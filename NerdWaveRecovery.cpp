#include "NerdWaveLog.h"
#include "NerdWaveRecovery.h"
#include "NerdWaveNetwork.h"
#include "NerdWaveSdCard.h"
#include "NerdWaveTrackCatalog.h"
#include "CopyManager.h"
#include "SourceMetadata.h"

namespace NerdWaveRecovery {
bool nasPending=false;
uint32_t nasRetryAt=0;
uint8_t nasFailures=0;
NasPhase nasPhase=NasPhase::Waiting;
const char* nasPhaseLabel() {
  switch(nasPhase) {
    case NasPhase::CheckingApi: return "Pruefe NAS-API";
    case NasPhase::ApiUnavailable: return "NAS-API offline";
    case NasPhase::WifiReconnect: return "Verbinde WLAN";
    case NasPhase::WifiFailed: return "WLAN nicht bereit";
    case NasPhase::WifiReady: return "WLAN verbunden";
    case NasPhase::RestoringTrack: return "Lade letzten Titel";
    default: return "Warte auf NAS";
  }
}
uint32_t secondsToNextNasRetry(uint32_t retryAt) {
  constexpr uint32_t retryMs=5000;
  uint32_t elapsed=(uint32_t)(millis()-retryAt);
  return elapsed>=retryMs?0:(retryMs-elapsed+999)/1000;
}
namespace {
constexpr uint32_t NAS_RETRY_MS=5000;
constexpr uint32_t SD_RETRY_MS=5000;
constexpr uint8_t NAS_RECOVERY_FAILURE_THRESHOLD=2;
}
void poll(Context& c) {
  // Keine WiFiManager-Portale oder NVS-Aenderungen waehrend des NAS-Retry.
  if(c.musicSource==0 && c.nasPending && !c.startingTrack && !c.decoderRunning &&
     !c.playerRunning && (uint32_t)(millis()-c.nasRetryAt)>=NAS_RETRY_MS) {
    c.nasRetryAt=millis();
    nasPhase=NasPhase::CheckingApi;
    c.redraw=true;
    NerdWaveLog::output.println("[NAS] Wiederverbindungsversuch");
    if(c.browser.begin()) {
      c.nasPending=false;
      c.nasFailures=0;
      nasPhase=NasPhase::RestoringTrack;
      NerdWaveLog::output.println("[NAS] Browser wieder erreichbar");
      if(c.autoStartLast && !c.selectedMp3Path.length()) {
        String path=c.prefs.getString(trackPathKey(0),"");
        String name=c.prefs.getString(trackNameKey(0),"");
        if(path.length()) {
          if(!name.length()) name=path.substring(path.lastIndexOf('/')+1);
          if(NerdWaveTrackCatalog::trackExists(c.api,path)) {
            if(c.startTrack(path,name)) { c.browser.focusTrack(path); c.page=1; NerdWaveLog::output.println("[NAS] Autostart nachgeholt"); }
          } else if(c.api.error().length()) {
            c.nasPending=true;
            NerdWaveLog::output.printf("[NAS] Titelcheck verschoben: %s\n",c.api.error().c_str());
          } else {
            NerdWaveLog::output.println("[NAS] Gespeicherter Titel nicht gefunden");
          }
        }
      }
      c.redraw=true;
    } else {
      nasPhase=NasPhase::ApiUnavailable;
      c.redraw=true;
      NerdWaveLog::output.printf("[NAS] API noch nicht bereit: %s\n",c.browser.error().c_str());
      if(c.nasFailures<NAS_RECOVERY_FAILURE_THRESHOLD) ++c.nasFailures;
      if(c.nasFailures>=NAS_RECOVERY_FAILURE_THRESHOLD && c.useWifi &&
         !c.startingTrack && !c.decoderRunning && !c.playerRunning &&
         CopyManager::state()==CopyManager::State::Idle) {
        c.nasFailures=0;
        nasPhase=NasPhase::WifiReconnect;
        c.redraw=true;
        c.wifiReady=NerdWaveNetwork::recoverWifi();
        nasPhase=c.wifiReady?NasPhase::WifiReady:NasPhase::WifiFailed;
        c.redraw=true;
        if(c.wifiReady) NerdWaveLog::output.println("[NAS] WLAN wieder bereit, NAS-API beim naechsten Zyklus pruefen");
        else NerdWaveLog::output.println("[NAS] WLAN-Recovery erfolglos; regulaerer Retry bleibt aktiv");
        c.nasRetryAt=millis();
      }
    }
  }
  // Kein SD-Mount waehrend laufendem Decoder oder Kopier-Task.
  if(c.musicSource==3 && c.sdRecoveryPending && !c.sdMounted &&
     !c.startingTrack && !c.decoderRunning && !c.playerRunning &&
     (uint32_t)(millis()-c.sdRetryAt)>=SD_RETRY_MS) {
    NerdWaveLog::output.printf("[SDREC] Neuer Mountversuch nach %lu ms\n",(unsigned long)(millis()-c.sdRetryAt));
    if(NerdWaveSdCard::tryMount()) {
      NerdWaveLog::output.println("[SDREC] SD wieder bereit; Browser wird neu eingelesen");
      if(c.browser.begin()) {
        c.page=0;
        if(c.sdAutostartPending && c.autoStartLast) {
          c.sdAutostartPending=false;
          String path=c.prefs.getString(trackPathKey(3),"");
          String name=c.prefs.getString(trackNameKey(3),"");
          if(!name.length()) name=path.substring(path.lastIndexOf('/')+1);
          bool savedValid=NerdWaveTrackCatalog::trackExists(c.api,path);
          if(!savedValid) {
            path="";name="";
            if(NerdWaveTrackCatalog::firstPlayable(c.api,path,name))
              NerdWaveLog::output.printf("[SDREC] Fallback: %s\n",path.c_str());
          }
          if(path.length() && c.startTrack(path,name)) {
            c.browser.focusTrack(path);
            c.page=1;
            NerdWaveLog::output.println(savedValid?"[SDREC] Letzten Titel gestartet":"[SDREC] Ersatz-Titel gestartet");
          }
        }
      } else {
        NerdWaveLog::output.printf("[SDREC] Browser nicht lesbar: %s\n",c.browser.error().c_str());
        c.sdRecoveryPending=true;
        c.sdRetryAt=millis();
      }
      c.redraw=true;
    }
  }
}
}
