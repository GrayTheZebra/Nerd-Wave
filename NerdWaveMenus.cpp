#include "NerdWaveMenus.h"
#include "DisplayUI.h"
#include "NerdWaveSkins.h"
#include "DisplayText.h"
#include "InputManager.h"
#include "LedController.h"
#include "NerdWaveSources.h"
#include "SourceMetadata.h"
#include "NerdWaveNetwork.h"
#include <WiFi.h>
#include <EthernetESP32.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
using namespace InputManager;
using DisplayUI::centeredText;
namespace NerdWaveMenus {
uint32_t settingsActivityAt = 0;
bool resetConfirmYes = false;
uint8_t resetConfirm = 0;
int settingsReturnPage = 0;
bool mediaListReady = false;
int mediaSelection = 0;
MusicPage mediaList;
String mediaResult = "";
String mediaChosenName = "";
String mediaChosenPath = "";
String mediaPath = "/";
int mediaDestination = 3;
int mediaOrigin = 0;
int mediaChoice = 0;
int mediaStage = 0;
int mediaCopyMode = 0;
bool mediaNotice = false;
bool mediaInfo = false;
int mediaReturnPage = 0;
int mediaIndex = 0;
int settingsIndex = 0;
static const char* modeName(uint8_t m) { return m==1 ? "WLAN" : "LAN"; }
#define C_BG (NerdWaveSkins::current().bg)
#define C_WHITE (NerdWaveSkins::current().text)
#define C_CYAN (NerdWaveSkins::current().accent)
#define C_YELLOW (NerdWaveSkins::current().secondary)
#define C_SELECT (NerdWaveSkins::current().selected)
int wrapIndex(int index,int increment,int count) { if(count<=0)return 0; return ((index+increment)%count+count)%count; }
// Action 11 is a read-only live network status row, inserted after network mode.
int settingsCount() { return 12; }
int settingsAction(int visibleIndex) {
  if(visibleIndex==6) return 11;
  return visibleIndex>6 ? visibleIndex-1 : visibleIndex;
}
static String currentNetworkAddress(bool useWifi) {
  if (useWifi) {
    if (WiFi.status()!=WL_CONNECTED) return "WLAN: offline";
    return String("WLAN: ")+WiFi.localIP().toString();
  }
  if (!NerdWaveNetwork::lanUsable()) return "LAN: offline";
  return String("LAN: ")+Ethernet.localIP().toString();
}
void render(const View& v) {
 auto& tft=v.tft;
  if(v.page==6) {
    centeredText("KOPIEREN",39,2,C_YELLOW,14);
    const String labels[5]={"Aktuellen Song", "Song kopieren", "Ordner kopieren", "SD Speicher", "Zurueck"};
    if(v.mediaStage==1) {
      centeredText("QUELLE WAEHLEN",66,2,C_CYAN,18);
      const char* names[]={"NAS", "USB", "SD", "Zurueck"};
      for(int i=0;i<4;i++) {int y=94+i*29;if(i==v.mediaChoice)tft.fillRoundRect(30,y-4,180,27,5,C_SELECT);centeredText(names[i],y,2,i==v.mediaChoice?C_YELLOW:C_WHITE,18);}
    } else if(v.mediaStage==2) {
      centeredText(v.mediaOrigin==0?"NAS":v.mediaOrigin==1?"USB":"SD",63,2,C_CYAN,15);
      String pathText=v.mediaPath;if(pathText.length()>25)pathText=".."+pathText.substring(pathText.length()-23);
      centeredText(pathText,84,1,C_WHITE,27);
      for(int i=0;i<3;i++) {
        int idx=v.mediaSelection-1+i;
        if(idx<0 || idx>=v.mediaList.total)continue;
        MusicItem item;
        if(v.copyListAt(idx,item)) {
          int y=108+i*32;
          if(idx==v.mediaSelection)tft.fillRoundRect(15,y-5,210,29,5,C_SELECT);
          String label=String(item.directory?"> ":"  ")+item.name;
          if(label.length()>17)label=label.substring(0,15)+"..";
          centeredText(label,y,2,idx==v.mediaSelection?C_YELLOW:C_WHITE,18);
        }
      }
      if(!v.mediaList.total)centeredText("Ordner leer",125,2,C_WHITE,20);
      centeredText("Lang: Ordner waehlen",213,1,C_CYAN,27);
    } else if(v.mediaStage==3) {
      centeredText("ZIEL WAEHLEN",60,2,C_CYAN,18);
      if(v.mediaCopyMode==2) {String current=v.mediaChosenName;if(current.length()>20)current=current.substring(0,18)+"..";centeredText(current,88,1,C_WHITE,26);}
      centeredText(v.mediaDestination==3?"SD-Karte":"USB-Stick",120,2,C_YELLOW,17);
      centeredText("VOL: Drehen",156,1,C_WHITE,22);
      centeredText(v.mediaCopyMode==2?"Klick: ins Root":"Klick: Kopieren",180,1,C_CYAN,24);
    } else if(v.mediaStage==4) {
      centeredText("ERGEBNIS",78,2,C_CYAN,18);
      String result=v.mediaResult;if(result.length()>20)result=result.substring(0,18)+"..";
      centeredText(result,119,2,C_WHITE,20);
      centeredText("VOL: Zurueck",182,1,C_CYAN,22);
    } else if(v.mediaInfo) {
      centeredText(v.sdMounted?"SD-KARTE":"SD FEHLT",74,2,C_CYAN,18);
      if(v.sdMounted) {
        const uint64_t total=SD_MMC.totalBytes(), used=SD_MMC.usedBytes();
        const uint64_t freeBytes=total>=used?total-used:0;
        centeredText("Gesamt: "+String((double)total/1073741824.0,1)+" GiB",112,2,C_WHITE,18);
        centeredText("Frei: "+String((double)freeBytes/1073741824.0,1)+" GiB",145,2,C_WHITE,18);
      }
      centeredText("VOL kurz: Zurueck",194,1,C_CYAN,26);
    } else {
      for(int i=0;i<5;i++) {const int y=67+i*32;if(i==v.mediaIndex)tft.fillRoundRect(12,y-5,216,29,6,C_SELECT);centeredText(labels[i],y,2,i==v.mediaIndex?C_YELLOW:C_WHITE,19);}
      centeredText("VOL: Drehen / Druecken",225,1,C_CYAN,29);
    }
    return;
  }
  if(v.page==4) {
    centeredText("IR ANLERNEN",36,2,C_YELLOW,12);
    centeredText(String(irLearnIndex+1)+" / "+String(IR_ACTIONS),58,1,C_CYAN,12);
    tft.fillRoundRect(16,91,208,48,7,C_SELECT);
    String item=lcdText(String(irActionName[irLearnIndex]),80);
    if(item.length()>18) item=item.substring(0,16)+"..";
    centeredText(item,109,2,C_YELLOW,10);
    if(v.irCapture) {
      centeredText("FB-Taste druecken",158,1,C_WHITE,13);
      centeredText("Lang: Abbrechen",180,1,C_CYAN,13);
    } else {
      centeredText(irBindings[irLearnIndex].proto?"Belegt":"Nicht belegt",157,1,C_WHITE,13);
      centeredText("Kurz: Lernen",179,1,C_CYAN,13);
      centeredText("Doppelt: Loeschen",196,1,C_CYAN,13);
      centeredText("Lang: Zurueck",213,1,C_CYAN,13);
    }
    return;
  }
  if(v.page==5) {
    centeredText("LED BELEUCHTUNG",40,2,C_YELLOW,12);
    const String labels[7]={
      String("LED: ")+(v.ledEnabled?"AN":"AUS"),
      String("Anzahl: ")+String(v.ledCount),
      String("Effekt: ")+(v.ledEffect==0?"VU":v.ledEffect==1?"DUAL VU":v.ledEffect==2?"PULSE":v.ledEffect==3?"SPEKTRUM":v.ledEffect==4?"DAUERLICHT":v.ledEffect==5?"RAINBOW":v.ledEffect==6?"RAINBOW MUSIK":v.ledEffect==7?"RAINBOW ALLE":"RAINBOW ALLE MUSIK"),
      String("Effektlicht: ")+String(v.ledBrightness)+"%",
      String("Grundlicht: ")+String(v.ledBaseBrightness)+"%",
      String("Empfindl.: ")+String(v.ledSensitivity),
      "Zurueck"
    };
    int first=min(max(0,(int)v.ledSettingsIndex-1),4);
    for(int i=0;i<3;i++){
      int idx=first+i,y=84+i*42;
      bool selected=idx==v.ledSettingsIndex;
      if(selected)tft.fillRoundRect(19,y-7,202,37,6,C_SELECT);
      String label=lcdText(labels[idx],80);
      if(label.length()>16)label=label.substring(0,14)+"..";
      centeredText(label,y,2,selected?C_YELLOW:C_WHITE,16);
    }
    centeredText(v.ledEditing?"Drehen = Wert | Kurz = OK":String(v.ledSettingsIndex+1)+" / 7",216,1,C_CYAN,13);
    return;
  }
  if(v.page==3) {
    if(v.resetConfirm) {
      centeredText("RESET?",48,2,C_YELLOW,13);
      centeredText(v.resetConfirm==1?"WLAN loeschen":"Werkseinstellungen",87,1,C_WHITE,28);
      centeredText("Wirklich ausfuehren?",109,1,C_CYAN,28);
      if(!v.resetConfirmYes) tft.fillRoundRect(30,135,180,34,5,C_SELECT);
      centeredText("NEIN",143,2,v.resetConfirmYes?C_WHITE:C_YELLOW,15);
      if(v.resetConfirmYes) tft.fillRoundRect(30,176,180,34,5,C_SELECT);
      centeredText("JA",184,2,v.resetConfirmYes?C_YELLOW:C_WHITE,15);
    } else {
      centeredText("EINSTELLUNGEN",40,2,C_YELLOW,13);
      // Menue-Reihenfolge: Quelle, BT, Wiedergabe, Netzwerk, LEDs, IR, Reset.
      const String labels[12]={
        String("Quelle: ")+(sourceName(v.requestedMusicSource)),
        v.musicSource==2 ? String("BT koppeln: ")+(v.btPairing?"AN":"AUS") : "BT koppeln",
        String("Repeat: ")+(v.repeatAll?"AN":"AUS"),
        String("Shuffle: ")+(v.shuffleMode==0?"AUS":v.shuffleMode==1?"ORDNER":"ALLES"),
        String("Autostart: ")+(v.autoStartLast?"AN":"AUS"),
        String("Netz: ")+(modeName(v.networkMode)),
        "LED Beleuchtung",
        "IR anlernen",
        "WLAN Reset",
        "Werksreset",
        "Zurueck",
        currentNetworkAddress(v.nerdUseWifi)
      };
      // Drei Zeilen, Auswahl zentral. Alle Eintraege bleiben erreichbar.
      constexpr int VISIBLE=3;
      const int count=settingsCount();
      int first=max(0,v.settingsIndex-1);
      first=min(first,count-VISIBLE);
      for(int i=0;i<VISIBLE;i++) {
        int idx=first+i;
        int action=settingsAction(idx);
        int y=84+i*42;
        bool active=(v.settingsIndex==idx);
        if(active) tft.fillRoundRect(19,y-7,202,37,6,C_SELECT);
        String label=lcdText(labels[action],80);
        if(action==11) {
          // A full IPv4 address must remain readable on the 240 px display.
          centeredText(label,y+5,1,active?C_YELLOW:C_WHITE,30);
        } else {
          if(label.length()>16) label=label.substring(0,14)+"..";
          centeredText(label,y,2,active?C_YELLOW:C_WHITE,16);
        }
      }
      centeredText(String(v.settingsIndex+1)+" / "+String(count),216,1,C_CYAN,16);
    }
  } else {
    centeredText("DIAGNOSE",44,2,C_YELLOW,15);
    centeredText("Ring "+String(v.ringLevel/1024)+" / 512 KB",80,1,C_WHITE,28);
    centeredText("Underruns "+String(v.underruns),101,1,C_WHITE,28);
    centeredText("I2S Fehler "+String(v.i2sErrors),122,1,C_WHITE,28);
    centeredText("MaxWrite "+String(v.maxI2sWriteUs)+" us",143,1,C_WHITE,28);
    centeredText("RAM "+String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),164,1,C_WHITE,28);
    centeredText(v.nerdUseWifi ? "WLAN "+String(WiFi.RSSI())+" dBm" : "LAN "+Ethernet.localIP().toString(),186,1,C_CYAN,28);
  }
}
}
