#include "NerdWaveLog.h"
#include "NerdWaveNasConfig.h"
#include "NerdWaveNetwork.h"
#include "MusicApi.h"
#include "Config.h"
#include <WiFi.h>
#include <EthernetESP32.h>
#include <ArduinoJson.h>
#include "NerdWaveUsb.h"
#include <SD_MMC.h>
using NerdWaveUsb::usbDisk;
extern uint8_t nerdMusicSource();
extern bool nerdSdMounted();

String MusicApi::encode(const String& s) {
  const char* hex = "0123456789ABCDEF";
  String out; out.reserve(s.length()*3);
  for (size_t i=0;i<s.length();++i) {
    uint8_t c=(uint8_t)s[i];
    if ((c>='a' && c<='z') || (c>='A' && c<='Z') ||
        (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='~') out+=(char)c;
    else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
  }
  return out;
}

// URL-Pfad: Slashes muessen Pfadtrenner bleiben, auch am Anfang.
// Umlaute/Leerzeichen bleiben UTF-8-percent-encoded.
String MusicApi::encodePath(const String& s) {
  String out = encode(s);
  out.replace("%2F", "/");
  return out;
}

bool MusicApi::fetch(const String& path, uint16_t offset, MusicPage& out) {
  return fetchFrom(nerdMusicSource(), path, offset, out);
}

bool MusicApi::fetchFrom(uint8_t source, const String& path, uint16_t offset, MusicPage& out) {
  error_="";
  if(source==1 || source==3) {
    bool sd = source==3;
    if(sd ? !nerdSdMounted() : !usbDisk.mounted()) {error_=sd?"SD-Karte fehlt":"USB-Stick fehlt";return false;}
    File folder=sd ? SD_MMC.open(path.c_str(),FILE_READ) : usbDisk.open(path.c_str(),FILE_READ);
    if(!folder || !folder.isDirectory()) {error_=sd?"SD-Ordner nicht lesbar":"USB-Ordner nicht lesbar";return false;}
    out.offset=offset;out.count=0;out.total=0;
    // Zwei Durchlaeufe: Gesamtzahl zaehlen und angeforderte Seite fuellen.
    File entry=folder.openNextFile();
    while(entry) {
      String name=String(entry.name());
      int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
      if(name.length() && !name.startsWith(".") && (entry.isDirectory() || name.endsWith(".mp3") || name.endsWith(".MP3") || name.endsWith(".Mp3"))) {
        if(out.total>=offset && out.count<Config::PAGE_SIZE) {
          MusicItem &it=out.items[out.count++];
          it.name=name;it.directory=entry.isDirectory();it.size=entry.size();
          it.path=(path=="/"?"/":path+"/")+name;
        }
        if(out.total<Config::MAX_TOTAL) ++out.total;
      }
      entry.close();entry=folder.openNextFile();
    }
    folder.close();
    NerdWaveLog::output.printf("[%s] Liste %s Offset=%u Anzahl=%u Gesamt=%u\n",sd?"SD":"USB",path.c_str(),offset,out.count,out.total);
    return true;
  }

  extern bool nerdUseWifi;
  // Eine fehlgeschlagene ENC28J60-Initialisierung darf keinen EthernetClient
  // erreichen: EthernetESP32 kann sonst bei client.connect() abstuerzen.
  if(!nerdUseWifi && !NerdWaveNetwork::lanUsable()) {
    error_="LAN nicht initialisiert";
    NerdWaveLog::output.println("[NET] NAS-Zugriff ausgesetzt: LAN-Treiber nicht bereit");
    return false;
  }
  if(nerdUseWifi && (WiFi.status()!=WL_CONNECTED || WiFi.localIP()==IPAddress(0,0,0,0))) {
    error_="WLAN nicht verbunden";
    NerdWaveLog::output.println("[NET] NAS-Zugriff ausgesetzt: WLAN nicht verbunden");
    return false;
  }
  EthernetClient lanClient;
  WiFiClient wifiClient;
  Client& client = nerdUseWifi ? static_cast<Client&>(wifiClient) : static_cast<Client&>(lanClient);
  NerdWaveLog::output.printf("[NET] API via %s\n",nerdUseWifi?"WLAN":"LAN");
  client.setTimeout(5000);
  // Bei schwankendem WLAN kurz warten und Verbindungsaufbau wiederholen.
  // Keinesfalls HTTP-Fehler (z.B. 404) blind wiederholen.
  bool connected = false;
  for (int attempt = 1; attempt <= 3; ++attempt) {
    if (client.connect(NerdWaveNasConfig::address(), NerdWaveNasConfig::port())) {
      connected = true;
      break;
    }
    NerdWaveLog::output.printf("[API] Verbindung fehlgeschlagen, Versuch %d/3\n", attempt);
    client.stop();
    if (attempt < 3) delay(500);
  }
  if (!connected) {
    error_="NAS nicht erreichbar"; return false;
  }
  String url="/api/list?path="+encode(path)+"&limit="+String(Config::PAGE_SIZE)+"&offset="+String(offset);
  client.print(String("GET ")+url+" HTTP/1.0\r\nHost: "+String(NerdWaveNasConfig::host())+"\r\nConnection: close\r\n\r\n");
  String status=client.readStringUntil('\n');status.trim();
  if (!status.startsWith("HTTP/1.") || status.indexOf(" 200 ")<0) {
    error_="HTTP: "+status; client.stop(); return false;
  }
  bool headerEnd=false;
  for (int i=0;i<40;i++) {
    String line=client.readStringUntil('\n');line.trim();
    if (!line.length()) {headerEnd=true;break;}
  }
  if (!headerEnd) {error_="Header unvollstaendig";client.stop();return false;}
  JsonDocument doc;
  DeserializationError err=deserializeJson(doc,client);
  client.stop();
  if (err) {error_=String("JSON: ")+err.c_str();return false;}
  int total=doc["total"] | -1;
  JsonArray items=doc["items"].as<JsonArray>();
  if (total<0 || total>Config::MAX_TOTAL || items.isNull() || items.size()>Config::PAGE_SIZE) {
    error_="Ungueltige Antwort";return false;
  }
  out.offset=offset;out.total=total;out.count=items.size();
  for(uint16_t i=0;i<out.count;i++) {
    JsonObject o=items[i];
    out.items[i].name=(const char*)(o["name"] | "");
    out.items[i].path=(const char*)(o["path"] | "");
    out.items[i].directory=String((const char*)(o["type"] | ""))=="directory";
    out.items[i].size=o["size"] | 0UL;
  }
  NerdWaveLog::output.printf("[API] %s Offset=%u Anzahl=%u Gesamt=%u\n",path.c_str(),offset,out.count,out.total);
  return true;
}
