#include "NerdWaveLog.h"
#include "NerdWaveNasConfig.h"
#include "NerdWaveMp3Stream.h"
#include "NerdWavePcmAudio.h"
#include "NerdWaveNetwork.h"
#include "NerdWaveUsb.h"
#include "Mp3FrameInfo.h"
#include "Config.h"
#include "MusicApi.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <EthernetESP32.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <cstring>
#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

namespace NerdWaveMp3Stream {
namespace {
Context* gContext = nullptr;
Context& ctx() { return *gContext; }
static constexpr size_t COMPRESSED_CAPACITY = 16384;
static constexpr size_t RING_CAPACITY = 512 * 1024;
static uint8_t compressed[COMPRESSED_CAPACITY];
static mp3d_sample_t decoded[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int16_t stereo[MINIMP3_MAX_SAMPLES_PER_FRAME];
static mp3dec_t mp3;
static bool ringWrite(const uint8_t* data, size_t count) {
  return NerdWavePcmAudio::write(data, count, ctx().decodeError, ctx().stopRequested);
}
static size_t ringLevel() { return NerdWavePcmAudio::level(); }
// ============== 04_StreamingAndDecode ==============
// Nerd-Wave v0.5.77: extracted without behavioral changes
// HTTP-Quelle mit exakter Content-Length, damit kurze Netzwerkpausen nicht als EOF gelten.
class Mp3HttpSource {
public:
  EthernetClient lanClient;
  WiFiClient wifiClient;
  Client& client;
  Mp3HttpSource():client(ctx().nerdUseWifi?static_cast<Client&>(wifiClient):static_cast<Client&>(lanClient)) {}
  int64_t remaining = -1;
  File localFile;
  bool local=false;
  bool sdLocal=false;
  // Diagnose ausschliesslich im Decoder-Task; kein geteilter mutable State.
  uint32_t diagOpenedAt=0, diagWindowAt=0, diagLastByteAt=0;
  uint32_t diagMaxGapMs=0, diagBytesWindow=0, diagTotalBytes=0;
  uint32_t diagReadCalls=0, diagEmptyPolls=0;
  void diagData(size_t got) {
    if(local || !got) return;
    const uint32_t now=millis();
    if(diagLastByteAt) {
      const uint32_t gap=now-diagLastByteAt;
      if(gap>diagMaxGapMs) diagMaxGapMs=gap;
    }
    diagLastByteAt=now;
    diagBytesWindow+=(uint32_t)got;
    diagTotalBytes+=(uint32_t)got;
    ++diagReadCalls;
    if(now-diagWindowAt>=5000) {
      uint32_t elapsed=now-diagWindowAt;
      NerdWaveLog::output.printf("[NETDIAG] RX=%lu B/s, Fenster=%lu B/%lu ms, MaxGap=%lu ms, LeerPolls=%lu, Rest=%lld\n",
        (unsigned long)((uint64_t)diagBytesWindow*1000/elapsed),
        (unsigned long)diagBytesWindow,(unsigned long)elapsed,
        (unsigned long)diagMaxGapMs,(unsigned long)diagEmptyPolls,(long long)remaining);
      diagWindowAt=now; diagBytesWindow=0; diagMaxGapMs=0; diagEmptyPolls=0;
    }
  }
  void close() {
    // Nach physischem Entfernen koennte das zugrundeliegende FAT/MSC-Handle
    // bereits ungueltig sein. Keinen erneuten Dateisystemzugriff erzwingen.
    if(local) {
      if(sdLocal || NerdWaveUsb::usbDisk.mounted()) localFile.close();
    } else {
      NerdWaveLog::output.printf("[NETDIAG] HTTP close: verbunden=%d, rest=%lld, empfangen=%lu B, seitOpen=%lu ms, MaxGapFenster=%lu ms\n",
        client.connected()?1:0,(long long)remaining,(unsigned long)diagTotalBytes,
        (unsigned long)(diagOpenedAt?millis()-diagOpenedAt:0),(unsigned long)diagMaxGapMs);
      client.stop();
    }
  }

  bool open() {
    if(ctx().musicSource==1 || ctx().musicSource==3) {
      local=true;
      sdLocal=(ctx().musicSource==3);
      if(sdLocal) {
        if(!ctx().sdMounted) {NerdWaveLog::output.println("[SD] Karte nicht bereit");return false;}
        localFile=SD_MMC.open(ctx().selectedMp3Path.c_str(),FILE_READ);
        if(!localFile || localFile.isDirectory()) {NerdWaveLog::output.printf("[SD] Datei nicht lesbar: %s\n",ctx().selectedMp3Path.c_str());return false;}
        remaining=localFile.size();
        NerdWaveLog::output.printf("[SD] MP3: %s (%lld Bytes)\n",ctx().selectedMp3Path.c_str(),(long long)remaining);
        return remaining>0;
      }
      if(!NerdWaveUsb::usbDisk.mounted()) { NerdWaveLog::output.println("[USB] Stick nicht bereit");return false; }
      localFile=NerdWaveUsb::usbDisk.open(ctx().selectedMp3Path.c_str(),FILE_READ);
      if(!localFile || localFile.isDirectory()) {NerdWaveLog::output.printf("[USB] Datei nicht lesbar: %s\n",ctx().selectedMp3Path.c_str());return false;}
      remaining=localFile.size();
      NerdWaveLog::output.printf("[USB] MP3: %s (%lld Bytes)\n",ctx().selectedMp3Path.c_str(),(long long)remaining);
      return remaining>0;
    }
    diagOpenedAt=diagWindowAt=diagLastByteAt=millis();
    NerdWaveLog::output.printf("[NETDIAG] HTTP open: Quelle=NAS, WLAN=%d, RSSI=%d dBm, Heap=%u\n",
      ctx().nerdUseWifi?1:0,ctx().nerdUseWifi?WiFi.RSSI():0,(unsigned)ESP.getFreeHeap());
    if(ctx().nerdUseWifi) NerdWaveNetwork::logWifiDetails("HTTP open");
    client.setTimeout(5000);
    NerdWaveLog::output.printf("[NET] MP3 via %s\n",ctx().nerdUseWifi?"WLAN":"LAN");
    const uint32_t connectAt=millis();
    if (!client.connect(NerdWaveNasConfig::address(), NerdWaveNasConfig::port())) {
      NerdWaveLog::output.printf("[NETDIAG] TCP connect FEHLER nach %lu ms, WLAN-Status=%d, RSSI=%d\n",
        (unsigned long)(millis()-connectAt),(int)WiFi.status(),ctx().nerdUseWifi?WiFi.RSSI():0);
      if(ctx().nerdUseWifi) NerdWaveNetwork::logWifiDetails("TCP connect Fehler");
      NerdWaveLog::output.println("[HTTP] Verbindung zum NAS fehlgeschlagen"); return false;
    }
    NerdWaveLog::output.printf("[NETDIAG] TCP connect OK nach %lu ms\n",(unsigned long)(millis()-connectAt));
    String requestPath = "/music" + MusicApi::encodePath(ctx().selectedMp3Path);
    NerdWaveLog::output.printf("[HTTP] GET %s\n", requestPath.c_str());
    client.print(String("GET ") + requestPath + " HTTP/1.0\r\nHost: " + NerdWaveNasConfig::host() +
                 "\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n");
    String status = client.readStringUntil('\n'); status.trim();
    NerdWaveLog::output.printf("[HTTP] %s\n", status.c_str());
    if (!status.startsWith("HTTP/1.") || status.indexOf(" 200 ") < 0) {
      NerdWaveLog::output.println("[HTTP] Erwartet HTTP 200"); client.stop(); return false;
    }
    while (true) {
      String line = client.readStringUntil('\n'); line.trim();
      if (!line.length()) break;
      if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
        remaining = atoll(line.substring(line.indexOf(':') + 1).c_str());
      }
      if (line.startsWith("Transfer-Encoding:") || line.startsWith("transfer-encoding:")) {
        if (line.indexOf("chunked") >= 0) {
          NerdWaveLog::output.println("[HTTP] Chunked-Transfer wird nicht unterstuetzt");
          client.stop(); return false;
        }
      }
    }
    if (remaining <= 0) {
      NerdWaveLog::output.println("[HTTP] Keine gueltige Content-Length"); client.stop(); return false;
    }
    NerdWaveLog::output.printf("[HTTP] MP3-Laenge: %lld Bytes\n", (long long)remaining);
    return true;
  }

  // Liest maximal cap Bytes; 0 nur bei echtem EOF oder Fehler.
  // Bei Timeouts wird bad gesetzt, nicht stillschweigend EOF gemeldet.
  size_t readSome(uint8_t* dst, size_t cap, bool& bad) {
    bad = false;
    if (remaining <= 0 || !cap) return 0;
    cap = min((size_t)1024,(size_t)min((int64_t)cap, remaining));
    if(local) {
      if(ctx().stopRequested || (sdLocal ? !ctx().sdMounted : !NerdWaveUsb::usbDisk.mounted()) || !localFile) {
        bad=true;
        NerdWaveLog::output.println(sdLocal?"[SD] Karte/Datei ungueltig":"[USB] Stick entfernt / Datei ungueltig");
        return 0;
      }
      size_t got=localFile.read(dst,cap);
      if(!got) {bad=true;NerdWaveLog::output.println(sdLocal?"[SD] Vorzeitiges Dateiende":"[USB] Vorzeitiges Dateiende");return 0;}
      remaining-=got;
      return got;
    }
    uint32_t lastDataMs = millis();
    while (!ctx().stopRequested) {
      int avail = client.available();
      if (avail > 0) {
        int got = client.read(dst, min(cap, (size_t)avail));
        if (got > 0) { remaining -= got; diagData((size_t)got); if(!ctx().nerdUseWifi) vTaskDelay(pdMS_TO_TICKS(1)); return (size_t)got; }
      }
      if (!client.connected() && client.available() == 0) {
        NerdWaveLog::output.printf("[NETDIAG] Abbruch: TCP geschlossen, ohne Daten seit %lu ms, Rest=%lld\n",
          (unsigned long)(millis()-diagLastByteAt),(long long)remaining);
        NerdWaveLog::output.println("[HTTP] Verbindung vor Dateiende getrennt"); bad = true; return 0;
      }
      if (millis() - lastDataMs > 5000) {
        NerdWaveLog::output.printf("[NETDIAG] Timeout: kein RX seit %lu ms, TCP.connected=%d, WiFi.status=%d, RSSI=%d, Rest=%lld\n",
          (unsigned long)(millis()-diagLastByteAt),client.connected()?1:0,
          (int)WiFi.status(),ctx().nerdUseWifi?WiFi.RSSI():0,(long long)remaining);
        if(ctx().nerdUseWifi) NerdWaveNetwork::logWifiDetails("HTTP Timeout");
        NerdWaveLog::output.println("[HTTP] 5 Sekunden ohne Daten (Timeout)"); bad = true; return 0;
      }
      ++ctx().netStalls;
      ++diagEmptyPolls;
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    bad = true; return 0;
  }

  bool skipBytes(size_t bytes) {
    uint8_t scratch[256];
    while (bytes) {
      bool bad = false;
      size_t got = readSome(scratch, min(bytes, sizeof(scratch)), bad);
      if (bad || got == 0) return false;
      bytes -= got;
    }
    return true;
  }
};

void decodeTaskImpl(void*) {
  ctx().decoderRunning = true;
  Mp3HttpSource source;
  if (!source.open()) {
    ctx().decodeError = true; ctx().decodeDone = true; ctx().decoderRunning = false; vTaskDelete(nullptr); return;
  }
  mp3dec_init(&mp3);
  const uint64_t mp3FileBytes = (uint64_t)source.remaining;
  uint64_t id3Bytes = 0;
  uint64_t validFrameBytes = 0;
  bool firstFrameChecked=false, bitrateChanged=false, durationLocked=false;
  uint32_t initialBitrate=0;
  uint32_t checkedFrames=0;
  bool bad = false;
  // ID3 anhand der ersten 10 Bytes pruefen, ohne auf dem Stream zu seeken.
  uint8_t id3[10];
  size_t id3Got = 0;
  while (id3Got < sizeof(id3)) {
    size_t n = source.readSome(id3 + id3Got, sizeof(id3) - id3Got, bad);
    if (bad || !n) { ctx().decodeError = true; break; }
    id3Got += n;
  }
  size_t have = 0;
  if (!ctx().decodeError && memcmp(id3, "ID3", 3) == 0) {
    uint32_t tagSize = ((id3[6]&127)<<21) | ((id3[7]&127)<<14) | ((id3[8]&127)<<7) | (id3[9]&127);
    uint32_t extra = (id3[5]&0x10) ? 10 : 0;
    id3Bytes = (uint64_t)tagSize + 10 + extra;
    if (!source.skipBytes((size_t)tagSize + extra)) ctx().decodeError = true;
    else NerdWaveLog::output.printf("[MP3] ID3 uebersprungen: %lu\n", (unsigned long)(tagSize + 10 + extra));
  } else if (!ctx().decodeError) {
    memcpy(compressed, id3, sizeof(id3));
    have = sizeof(id3);
  }
  uint32_t skipped = 0, lastProgress = millis();
  bool eof = false;
  while (!ctx().decodeError && !ctx().stopRequested) {
    if (!eof && have < 8192) {
      bool failed = false;
      size_t n = source.readSome(compressed + have, COMPRESSED_CAPACITY - have, failed);
      if (failed) { ctx().decodeError = true; break; }
      have += n;
      if (!n) eof = true;
    }
    if (!have) break;
    mp3dec_frame_info_t info = {};
    int samples = mp3dec_decode_frame(&mp3, compressed, (int)have, decoded, &info);
    if (info.frame_bytes > 0 && (size_t)info.frame_bytes <= have) {
      // Vor dem Entfernen des komprimierten Frames die Header-Metadaten auswerten.
      if(!firstFrameChecked && (size_t)info.frame_bytes>=4 && info.hz>0) {
        firstFrameChecked=true;
        uint32_t headerSec=mp3HeaderDuration(compressed,(size_t)info.frame_bytes,(uint32_t)info.hz);
        if(headerSec>=1 && headerSec<=86400) {
          ctx().estimatedDurationSec=headerSec;
          ctx().durationExact=true;
          durationLocked=true;
          NerdWaveLog::output.printf("[MP3] Dauer aus Xing/Info/VBRI: %lu s\n",(unsigned long)headerSec);
        }
      }
      if(info.hz>0 && info.frame_bytes>0 && checkedFrames<100) {
        uint32_t br=mp3HeaderBitrate(compressed,(size_t)info.frame_bytes);
        if(br) {
          if(!initialBitrate) initialBitrate=br;
          else if(br!=initialBitrate) bitrateChanged=true;
          checkedFrames++;
        }
      }
      have -= info.frame_bytes;
      memmove(compressed, compressed + info.frame_bytes, have);
    } else {
      if (!eof && have < COMPRESSED_CAPACITY) {
        bool failed = false;
        size_t n = source.readSome(compressed + have, COMPRESSED_CAPACITY - have, failed);
        if (failed) { ctx().decodeError = true; break; }
        have += n;
        if (!n) eof = true;
        continue;
      }
      if (eof) break;
      memmove(compressed, compressed + 1, --have);
      ++skipped;
      continue;
    }
    if (samples <= 0) { ++skipped; continue; }
    if ((info.hz != 44100 && info.hz != 48000) || (info.channels != 1 && info.channels != 2)) {
      NerdWaveLog::output.printf("[MP3] Format nicht unterstuetzt: %d Hz / %d Kanaele\n", info.hz, info.channels);
      ctx().decodeError = true; break;
    }
    if (!ctx().audioSampleRateReady) {
      ctx().audioSampleRate = (uint32_t)info.hz;
      ctx().audioSampleRateReady = true;
      NerdWaveLog::output.printf("[MP3] Abtastrate erkannt: %lu Hz / %d Kanaele\n", (unsigned long)ctx().audioSampleRate, info.channels);
    } else if ((uint32_t)info.hz != ctx().audioSampleRate) {
      NerdWaveLog::output.printf("[MP3] Abtastratenwechsel im Titel nicht unterstuetzt: %d Hz\n", info.hz);
      ctx().decodeError = true; break;
    }
    // Seek ohne Frame-Index: vor dem Ziel decodierte Frames verwerfen.
    // Dadurch bleiben VBR, ID3 und NAS/SD/USB identisch behandelt.
    if(ctx().seekStartSeconds && ctx().framesDecoded < (uint64_t)ctx().seekStartSeconds * ctx().audioSampleRate) {
      ctx().framesDecoded += (uint64_t)samples;
      if(ctx().framesDecoded >= (uint64_t)ctx().seekStartSeconds * ctx().audioSampleRate) {
        NerdWaveLog::output.printf("[SEEK] Ziel bei %lu s erreicht\n",(unsigned long)ctx().seekStartSeconds);
        ctx().seekStartSeconds=0;
      }
      continue;
    }
    for (int i = 0; i < samples; ++i) {
      int32_t left = decoded[i * info.channels];
      int32_t right = decoded[i * info.channels + (info.channels == 2 ? 1 : 0)];
      // Leiser machen, noch BEVOR PCM in den PSRAM-Ring kommt.
      // 100%-PCM in den Ring schreiben. Die Lautstaerke wird
      // unmittelbar vor I2S angewendet, damit Encoder live wirken.
      stereo[2*i] = (int16_t)left;
      stereo[2*i + 1] = (int16_t)right;
    }
    if (!ringWrite((const uint8_t*)stereo, (size_t)samples * 4)) {
      ctx().decodeError = true; break;
    }
    ctx().framesDecoded += samples;
    validFrameBytes += (uint32_t)info.frame_bytes;
    // Ohne Header die Dauer nur EINMAL bestimmen, nicht fortlaufend verschieben.
    if(!durationLocked && ctx().audioSampleRate && mp3FileBytes>id3Bytes) {
      if(!bitrateChanged && checkedFrames>=100 && initialBitrate) {
        uint64_t secs=((mp3FileBytes-id3Bytes)*8ULL+initialBitrate/2)/initialBitrate;
        if(secs>=1 && secs<=86400) {
          ctx().estimatedDurationSec=(uint32_t)secs;
          durationLocked=true;
          NerdWaveLog::output.printf("[MP3] CBR-Dauer: %lu s\n",(unsigned long)secs);
        }
      }
      // Ohne CBR / Xing erst nach 15 Sekunden decodiertem Audio festlegen.
      if(!durationLocked && ctx().framesDecoded >= (uint64_t)ctx().audioSampleRate*15 && validFrameBytes) {
        double approx=((double)(mp3FileBytes-id3Bytes)*ctx().framesDecoded)/
                      ((double)validFrameBytes*ctx().audioSampleRate);
        if(approx>=1.0 && approx<=86400.0) {
          ctx().estimatedDurationSec=(uint32_t)(approx+0.5);
          durationLocked=true;
          NerdWaveLog::output.printf("[MP3] Dauer geschaetzt (fixiert): %lu s\n",(unsigned long)ctx().estimatedDurationSec);
        }
      }
    }
    if (millis() - lastProgress > 5000) {
      lastProgress = millis();
      NerdWaveLog::output.printf("[DECODE] %.1f s | Ring %u/%u KB | Stack frei %u\n",
        ctx().framesDecoded / (float)ctx().audioSampleRate, (unsigned)(ringLevel() / 1024),
        (unsigned)(RING_CAPACITY / 1024), (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
  }
  NerdWaveLog::output.printf("[TASKDIAG] Decoder Ende: stop=%d, fehler=%d, datenRest=%lld, Laufzeit=%lu ms\n",
    ctx().stopRequested?1:0,ctx().decodeError?1:0,(long long)source.remaining,
    (unsigned long)(millis()-source.diagOpenedAt));
  source.close();
  if (!ctx().decodeError && !ctx().stopRequested && ctx().framesDecoded > 0) {
    ctx().estimatedDurationSec = (uint32_t)((ctx().framesDecoded + ctx().audioSampleRate/2) / ctx().audioSampleRate);
    ctx().durationExact = true;
  }
  ctx().decodeDone = true;
  NerdWaveLog::output.printf("[DECODE] Ende: %.2f s, uebersprungen=%lu, Fehler=%s, Netz-Wartezyklen=%lu\n",
    ctx().framesDecoded/(float)ctx().audioSampleRate, (unsigned long)skipped,
    ctx().decodeError ? "ja" : "nein", (unsigned long)ctx().netStalls);
  ctx().decoderRunning = false;
  vTaskDelete(nullptr);
}


} // namespace
void bind(Context& context) { gContext = &context; }
void decodeTask(void* arg) { decodeTaskImpl(arg); }
} // namespace NerdWaveMp3Stream
