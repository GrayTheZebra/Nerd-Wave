#include "NerdWaveLog.h"
#include "NerdWaveNasConfig.h"
#include "CopyManager.h"
#include <cstring>
#include <memory>
#include <WiFi.h>
#include <WiFiClient.h>
#include <EthernetESP32.h>
#include <SD_MMC.h>
#include "Config.h"
#include "MusicApi.h"
#include "NerdWaveUsb.h"
#include "NerdWaveSdCard.h"

namespace {
  portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
  CopyManager::Progress progress;
  CopyManager::State jobState = CopyManager::State::Idle;
}

namespace CopyManager {
State state() {
  portENTER_CRITICAL(&mutex);
  State result=jobState;
  portEXIT_CRITICAL(&mutex);
  return result;
}

bool start(TaskFunction_t worker) {
  portENTER_CRITICAL(&mutex);
  if (jobState!=State::Idle) { portEXIT_CRITICAL(&mutex); return false; }
  progress=Progress{};
  jobState=State::Running;
  portEXIT_CRITICAL(&mutex);
  if (xTaskCreatePinnedToCore(worker,"NerdCopy",16384,nullptr,1,nullptr,1)!=pdPASS) {
    portENTER_CRITICAL(&mutex);
    jobState=State::Idle;
    portEXIT_CRITICAL(&mutex);
    return false;
  }
  return true;
}

void finish() {
  portENTER_CRITICAL(&mutex);
  jobState=State::Finished;
  portEXIT_CRITICAL(&mutex);
}

bool consumeFinished() {
  portENTER_CRITICAL(&mutex);
  bool finished=(jobState==State::Finished);
  if(finished) jobState=State::Idle;
  portEXIT_CRITICAL(&mutex);
  return finished;
}

void publish(const String& name,uint64_t done,uint64_t total,uint32_t files) {
  portENTER_CRITICAL(&mutex);
  strlcpy(progress.name,name.c_str(),sizeof(progress.name));
  progress.done=done;
  progress.total=total;
  progress.files=files;
  ++progress.serial;
  portEXIT_CRITICAL(&mutex);
}

void snapshot(Progress& output) {
  portENTER_CRITICAL(&mutex);
  output=progress;
  portEXIT_CRITICAL(&mutex);
}

// The runtime owns the current media selection; the worker only borrows its state.
namespace { TransferContext* context = nullptr; }
void setTransferContext(TransferContext& value) { context=&value; }

bool copySafePath(const String& path) {
  return path.startsWith("/") && path.indexOf("..")<0 && path.indexOf('\\')<0 && path.indexOf(':')<0;
}
bool copyReadyDestination() {
  return context->destination==3 ? (NerdWaveSdCard::mounted||NerdWaveSdCard::tryMount()) : NerdWaveUsb::usbDisk.mounted();
}
static bool copyExists(const String& path) {
  return context->destination==3 ? SD_MMC.exists(path.c_str()) : NerdWaveUsb::usbDisk.exists(path.c_str());
}
bool copyMkdir(const String& path) {
  if(copyExists(path)) return true;
  return context->destination==3 ? SD_MMC.mkdir(path.c_str()) : NerdWaveUsb::usbDisk.mkdir(path.c_str());
}
static bool copyMakeParents(const String& path) {
  for(int i=1;i<path.length();i++) {
    if(path[i]!='/')continue;
    String part=path.substring(0,i);
    if(!copyMkdir(part))return false;
  }
  return true;
}
static bool copyCheckAbort() {
  // Kein Zugriff auf Encoder im Worker; Langdruck setzt context->abort im loop().
  vTaskDelay(pdMS_TO_TICKS(1));
  return context->abort;
}
// Schreibt zuerst *.part. Bestehende Zieldateien bleiben unberuehrt.
// Ein abgebrochener Transfer wird geloescht, bevor eine weitere Aktion moeglich ist.
bool copyOneFile(uint8_t origin,const String& from,const String& to,uint32_t &files) {
  if(context->abort || !copySafePath(from) || !copySafePath(to) || !copyReadyDestination()) {context->result="Quelle/Ziel fehlt";return false;}
  if(copyExists(to)) {context->result="Datei existiert";return false;}
  if(!copyMakeParents(to)) {context->result="Ordner anlegen fehlgeschlagen";return false;}
  String partial=to+".part";
  if(copyExists(partial)) {context->result=".part existiert";return false;}
  // Zeitmessung nur fuer Diagnose; uebertraegt keine Daten und aendert keine Blockgroessen.
  uint64_t bytes=0;
  const uint32_t diagBeginUs=micros();
  uint32_t diagWindowStartMs=millis();
  uint64_t diagReadUs=0,diagWriteUs=0;
  uint32_t diagMaxReadUs=0,diagMaxWriteUs=0;
  uint32_t diagReadCalls=0,diagWriteCalls=0;
  // NAS-only: separate time spent waiting for bytes from the Client::read() call.
  uint64_t diagNetWaitUs=0,diagNetReadUs=0,diagNetAvailableTotal=0,diagNetReceivedTotal=0;
  uint32_t diagNetWaitCalls=0,diagNetReadCalls=0,diagNetEmptyWaits=0;
  uint32_t diagNetMaxWaitUs=0,diagNetMaxReadUs=0,diagNetMaxAvailable=0;
  uint64_t diagWindowBytes=0;
  auto diagReport=[&](bool finalReport,uint64_t transferred) {
    const uint32_t elapsedUs=(uint32_t)(micros()-diagBeginUs);
    const uint64_t measured=diagReadUs+diagWriteUs;
    const uint64_t otherUs=(uint64_t)elapsedUs>measured ? (uint64_t)elapsedUs-measured : 0;
    const uint32_t elapsedMs=elapsedUs/1000;
    const uint32_t kbps=elapsedMs ? (uint32_t)((transferred*1000ULL/1024ULL)/elapsedMs) : 0;
    NerdWaveLog::output.printf("[COPYDIAG] %s src=%s dst=%s size=%llu/%llu B elapsed=%lu ms speed=%lu KiB/s\n",
      finalReport?"FINAL":"WINDOW",origin==0?"NAS":(origin==3?"SD":"USB"),
      context->destination==3?"SD":"USB",(unsigned long long)transferred,
      (unsigned long long)bytes,(unsigned long)elapsedMs,(unsigned long)kbps);
    NerdWaveLog::output.printf("[COPYDIAG] READ=%llu ms WRITE=%llu ms OTHER=%llu ms MaxRead=%lu ms MaxWrite=%lu ms Calls=%lu/%lu\n",
      (unsigned long long)(diagReadUs/1000ULL),(unsigned long long)(diagWriteUs/1000ULL),
      (unsigned long long)(otherUs/1000ULL),(unsigned long)(diagMaxReadUs/1000),
      (unsigned long)(diagMaxWriteUs/1000),(unsigned long)diagReadCalls,(unsigned long)diagWriteCalls);
    if(origin==0) {
      NerdWaveLog::output.printf("[COPYNET] %s WAIT=%llu ms READ=%llu ms WaitCalls=%lu EmptyWaits=%lu ReadCalls=%lu\n",
        finalReport?"FINAL":"WINDOW",(unsigned long long)(diagNetWaitUs/1000ULL),
        (unsigned long long)(diagNetReadUs/1000ULL),(unsigned long)diagNetWaitCalls,
        (unsigned long)diagNetEmptyWaits,(unsigned long)diagNetReadCalls);
      NerdWaveLog::output.printf("[COPYNET] AvgRead=%lu B AvgAvailable=%lu B MaxAvailable=%lu B MaxWait=%lu ms MaxRead=%lu ms\n",
        (unsigned long)(diagNetReadCalls ? diagNetReceivedTotal/diagNetReadCalls : 0),
        (unsigned long)(diagNetReadCalls ? diagNetAvailableTotal/diagNetReadCalls : 0),
        (unsigned long)diagNetMaxAvailable,(unsigned long)(diagNetMaxWaitUs/1000),
        (unsigned long)(diagNetMaxReadUs/1000));
    }
    if(origin==0 && context->useWifi) {
      NerdWaveLog::output.printf("[COPYDIAG] WLAN BSSID=%s Kanal=%ld RSSI=%ld dBm Status=%d\n",
        WiFi.BSSIDstr().c_str(),(long)WiFi.channel(),(long)WiFi.RSSI(),(int)WiFi.status());
    }
    if(!finalReport) {
      const uint32_t nowMs=millis();
      const uint32_t windowMs=nowMs-diagWindowStartMs;
      NerdWaveLog::output.printf("[COPYDIAG] Fenster=%lu ms Bytes=%llu Rate=%lu KiB/s\n",
        (unsigned long)windowMs,(unsigned long long)diagWindowBytes,
        (unsigned long)(windowMs ? (uint32_t)(diagWindowBytes*1000ULL/1024ULL/windowMs):0));
      diagWindowBytes=0;
      diagWindowStartMs=nowMs;
    }
  };
  File input;
  EthernetClient eth;
  WiFiClient wifi;
  Client& conn=context->useWifi ? static_cast<Client&>(wifi) : static_cast<Client&>(eth);
  if(origin==0) {
    conn.setTimeout(5000);
    if(!conn.connect(NerdWaveNasConfig::address(),NerdWaveNasConfig::port())) {context->result="NAS nicht erreichbar";return false;}
    conn.print(String("GET /music")+MusicApi::encodePath(from)+" HTTP/1.0\r\nHost: "+NerdWaveNasConfig::host()+"\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n");
    String status=conn.readStringUntil('\n');status.trim();
    if(status.indexOf(" 200 ")<0) {context->result="NAS HTTP Fehler";conn.stop();return false;}
    bool hasLength=false,chunked=false,headerEnd=false;
    for(int i=0;i<48;i++) {
      String line=conn.readStringUntil('\n');line.trim();
      if(!line.length()) {headerEnd=true;break;}
      if(line.startsWith("Content-Length:") || line.startsWith("content-length:")) {bytes=strtoull(line.substring(line.indexOf(':')+1).c_str(),nullptr,10);hasLength=true;}
      if(line.startsWith("Transfer-Encoding:") || line.startsWith("transfer-encoding:")) if(line.indexOf("chunked")>=0) chunked=true;
    }
    if(!headerEnd || !hasLength || !bytes || chunked) {context->result="HTTP-Laenge ungueltig";conn.stop();return false;}
  } else {
    input=origin==3?SD_MMC.open(from.c_str(),FILE_READ):NerdWaveUsb::usbDisk.open(from.c_str(),FILE_READ);
    if(!input || input.isDirectory()) {context->result="Quelldatei fehlt";return false;}
    bytes=input.size();
  }
  File output=context->destination==3?SD_MMC.open(partial.c_str(),FILE_WRITE):NerdWaveUsb::usbDisk.open(partial.c_str(),FILE_WRITE);
  if(!output) {context->result="Ziel nicht beschreibbar";input.close();conn.stop();return false;}
  const String copyDisplayName=from.substring(from.lastIndexOf('/')+1);
  uint8_t buf[2048];uint64_t done=0;uint32_t previous=0;bool good=true;
  while(done<bytes) {
    if(copyCheckAbort()) {context->result="Abgebrochen";good=false;break;}
    if(!copyReadyDestination()) {context->result="Ziel entfernt";good=false;break;}
    size_t want=(size_t)min((uint64_t)sizeof(buf),bytes-done);
    size_t got=0;
    const uint32_t readStartUs=micros();
    if(origin==0) {
      const uint32_t waitStartUs=micros();
      uint32_t deadline=millis()+5000;
      bool waited=false;
      while(!conn.available() && conn.connected() && (int32_t)(millis()-deadline)<0) {
        waited=true;
        if(copyCheckAbort())break;
      }
      const uint32_t waitDurationUs=(uint32_t)(micros()-waitStartUs);
      diagNetWaitUs+=waitDurationUs;
      if(waitDurationUs>diagNetMaxWaitUs)diagNetMaxWaitUs=waitDurationUs;
      ++diagNetWaitCalls;
      if(waited)++diagNetEmptyWaits;
      if(context->abort) {context->result="Abgebrochen";good=false;break;}
      const int availableBeforeRead=conn.available();
      if(availableBeforeRead>0) {
        diagNetAvailableTotal+=(uint32_t)availableBeforeRead;
        if((uint32_t)availableBeforeRead>diagNetMaxAvailable)diagNetMaxAvailable=(uint32_t)availableBeforeRead;
      }
      const uint32_t clientReadStartUs=micros();
      int n=conn.read(buf,want);
      const uint32_t clientReadDurationUs=(uint32_t)(micros()-clientReadStartUs);
      diagNetReadUs+=clientReadDurationUs;
      if(clientReadDurationUs>diagNetMaxReadUs)diagNetMaxReadUs=clientReadDurationUs;
      ++diagNetReadCalls;
      if(n>0) {got=(size_t)n;diagNetReceivedTotal+=(uint32_t)n;}
    } else got=input.read(buf,want);
    const uint32_t readDurationUs=(uint32_t)(micros()-readStartUs);
    diagReadUs+=readDurationUs;
    if(readDurationUs>diagMaxReadUs)diagMaxReadUs=readDurationUs;
    ++diagReadCalls;
    if(!got) {context->result="Lese-/Schreibfehler";good=false;break;}
    const uint32_t writeStartUs=micros();
    const size_t written=output.write(buf,got);
    const uint32_t writeDurationUs=(uint32_t)(micros()-writeStartUs);
    diagWriteUs+=writeDurationUs;
    if(writeDurationUs>diagMaxWriteUs)diagMaxWriteUs=writeDurationUs;
    ++diagWriteCalls;
    if(written!=got) {context->result="Lese-/Schreibfehler";good=false;break;}
    done+=got;
    diagWindowBytes+=got;
    if(!context->progressDrawn || millis()-previous>200) {CopyManager::publish(copyDisplayName,done,bytes,files);previous=millis();}
    if((uint32_t)(millis()-diagWindowStartMs)>=5000U)diagReport(false,done);
  }
  const uint32_t flushStartUs=micros();
  output.flush();
  const uint32_t flushDurationUs=(uint32_t)(micros()-flushStartUs);
  diagWriteUs+=flushDurationUs;
  if(flushDurationUs>diagMaxWriteUs)diagMaxWriteUs=flushDurationUs;
  ++diagWriteCalls;
  output.close();input.close();conn.stop();
  diagReport(true,done);
  if(!good || done!=bytes) {
    if(context->destination==3) SD_MMC.remove(partial.c_str());else if(NerdWaveUsb::usbDisk.mounted()) NerdWaveUsb::usbDisk.remove(partial.c_str());
    return false;
  }
  bool renamed=context->destination==3?SD_MMC.rename(partial.c_str(),to.c_str()):NerdWaveUsb::usbDisk.rename(partial.c_str(),to.c_str());
  if(!renamed) {
    // Abgeschlossene Temp-Datei bei Rename-Fehler nicht liegen lassen.
    if(context->destination==3) SD_MMC.remove(partial.c_str());
    else if(NerdWaveUsb::usbDisk.mounted()) NerdWaveUsb::usbDisk.remove(partial.c_str());
    context->result="Umbenennen fehlgeschlagen";
    return false;
  }
  files++;CopyManager::publish(copyDisplayName,bytes,bytes,files);
  return true;
}
bool copyTree(uint8_t origin,const String& folder,const String& destination,uint32_t &files,int depth) {
  if(depth>12 || context->abort || !copySafePath(folder) || !copySafePath(destination)) {context->result="Ordner zu tief/Abbruch";return false;}
  if(!copyMkdir(destination)) {context->result="Zielordner fehlgeschlagen";return false;}
  uint16_t offset=0;
  for(;;) {
    std::unique_ptr<MusicPage> listing(new MusicPage());
    if(!listing || !context->api.fetchFrom(origin,folder,offset,*listing)) {context->result=context->api.error();return false;}
    if(!listing->count) return true;
    for(int i=0;i<listing->count;i++) {
      if(copyCheckAbort()) {context->result="Abgebrochen";return false;}
      MusicItem item=listing->items[i];
      String child=destination+"/"+item.name;
      if(!copySafePath(item.path) || item.name.indexOf('/')>=0 || item.name=="." || item.name=="..") {context->result="Ungueltiger Dateiname";return false;}
      if(item.directory) {
        if(!copyTree(origin,item.path,child,files,depth+1))return false;
      } else if(!copyOneFile(origin,item.path,child,files))return false;
    }
    offset+=listing->count;
    if(offset>=listing->total)return true;
  }
}

} // namespace CopyManager
