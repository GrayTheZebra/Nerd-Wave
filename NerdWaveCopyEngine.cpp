#include "NerdWaveLog.h"
#include "NerdWaveCopyEngine.h"
#include "NerdWaveCopyProgress.h"
#include "CopyManager.h"
#include "NerdWaveUsb.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace { NerdWaveCopyEngine::Context* context=nullptr; }
static void copyWorker(void*) {
  auto& c=*context;
  auto& mediaDestination=c.mediaDestination;
  auto& mediaOrigin=c.mediaOrigin;
  auto& mediaResult=c.mediaResult;
  auto& autoNextPending=c.autoNextPending;
  auto& autoNextEnabled=c.autoNextEnabled;
  auto& stopRequested=c.stopRequested;
  auto& pauseRequested=c.pauseRequested;
  auto& decoderRunning=c.decoderRunning;
  auto& playerRunning=c.playerRunning;
  auto& startingTrack=c.startingTrack;
  auto& mediaCopyMode=c.mediaCopyMode;
  auto& mediaChosenPath=c.mediaChosenPath;
  auto& mediaAbort=c.mediaAbort;
  auto& musicSource=c.musicSource;
  auto& playerActive=c.playerActive;
  auto& playbackPaused=c.playbackPaused;
  auto& selectedMp3Path=c.selectedMp3Path;
  auto& copyResumePath=c.copyResumePath;
  auto& copyResumeName=c.copyResumeName;
  auto& copyResumeAfter=c.copyResumeAfter;
  auto& playingName=c.playingName;
  auto& api=c.api;
  auto& nerdUseWifi=c.nerdUseWifi;
  auto& mediaStage=c.mediaStage;
  auto& redraw=c.redraw;
  auto& lanReady=c.lanReady;
  auto connectNetwork=c.connectNetwork;
  auto ampOff=c.ampOff;
  auto run=[&]() {
  if(mediaDestination==mediaOrigin) {mediaResult="Gleiche Quelle/Ziel";return;}
  // Ein USB-Ziel benoetigt keinen vorherigen Wechsel zur USB-Musikquelle.
  // Erst nach ausdruecklichem Kopierwunsch einbinden: Boot und NAS-Audio
  // bleiben frei von blockierenden USB-Mountversuchen.
  if(mediaDestination==1 && !NerdWaveUsb::usbDisk.mounted()) {
    if(!NerdWaveUsb::hostStarted) NerdWaveUsb::startHost();
    if(NerdWaveUsb::hostStarted) {
      NerdWaveLog::output.println("[USB] Kopierziel: Mount wird angefordert");
      NerdWaveUsb::tryMount(true);
    }
  }
  if(!CopyManager::copyReadyDestination()) {mediaResult="Ziel fehlt";return;}

  // Parallele Zugriffe durch Audio-Tasks auf dieselben FAT-Handles vermeiden.
  autoNextPending=false;autoNextEnabled=false;stopRequested=true;pauseRequested=false;
  uint32_t deadline=millis()+8000;
  while((decoderRunning || playerRunning || startingTrack) && (int32_t)(millis()-deadline)<0)delay(5);
  if(decoderRunning || playerRunning || startingTrack) {
    mediaResult="Player noch aktiv";return;
  }
  ampOff();
  if(mediaOrigin==0 && !lanReady && WiFi.status()!=WL_CONNECTED) {
    if(!connectNetwork()) {mediaResult="Netzwerk offline";return;}
  }
  // "Aktuellen Song" direkt im Ziel-Root ablegen. Bei den anderen
  // Kopiermodi die bisherige Ordnerstruktur unter /Musik beibehalten.
  const bool currentSongOnly=(mediaCopyMode==2);
  String base=currentSongOnly ? "/" : "/Musik";
  String target;
  if(currentSongOnly) {
    const int slash=mediaChosenPath.lastIndexOf('/');
    const String filename=mediaChosenPath.substring(slash+1);
    if(filename.isEmpty() || filename=="." || filename==".." ||
       filename.indexOf('\\')>=0 || filename.indexOf(':')>=0) {
      mediaResult="Dateiname ungueltig";return;
    }
    target="/"+filename;
  } else {
    target=base+mediaChosenPath;
  }
  if(!CopyManager::copySafePath(target) || !CopyManager::copyMkdir(base)) {
    mediaResult="Zielpfad ungueltig";return;
  }
  uint32_t files=0;
  bool result=mediaCopyMode!=1 ? CopyManager::copyOneFile(mediaOrigin,mediaChosenPath,target,files) : CopyManager::copyTree(mediaOrigin,mediaChosenPath,target,files,0);
  if(result)mediaResult=String(files)+" Datei(en) kopiert";
  
  NerdWaveLog::output.printf("[COPY] %s: %s\n",result?"OK":"FEHLER",mediaResult.c_str());
  };
  run();
  // Alle Dateihandles wurden geschlossen, Ergebnis jetzt an Hauptloop melden.
  CopyManager::finish();
  vTaskDelete(nullptr);
}

void NerdWaveCopyEngine::execute(Context& c) {
  context=&c;
  auto& mediaDestination=c.mediaDestination;
  auto& mediaOrigin=c.mediaOrigin;
  auto& mediaResult=c.mediaResult;
  auto& autoNextPending=c.autoNextPending;
  auto& autoNextEnabled=c.autoNextEnabled;
  auto& stopRequested=c.stopRequested;
  auto& pauseRequested=c.pauseRequested;
  auto& decoderRunning=c.decoderRunning;
  auto& playerRunning=c.playerRunning;
  auto& startingTrack=c.startingTrack;
  auto& mediaCopyMode=c.mediaCopyMode;
  auto& mediaChosenPath=c.mediaChosenPath;
  auto& mediaAbort=c.mediaAbort;
  auto& musicSource=c.musicSource;
  auto& playerActive=c.playerActive;
  auto& playbackPaused=c.playbackPaused;
  auto& selectedMp3Path=c.selectedMp3Path;
  auto& copyResumePath=c.copyResumePath;
  auto& copyResumeName=c.copyResumeName;
  auto& copyResumeAfter=c.copyResumeAfter;
  auto& playingName=c.playingName;
  auto& api=c.api;
  auto& nerdUseWifi=c.nerdUseWifi;
  auto& mediaStage=c.mediaStage;
  auto& redraw=c.redraw;
  auto& lanReady=c.lanReady;
  auto connectNetwork=c.connectNetwork;
  auto ampOff=c.ampOff;
  if(CopyManager::state()!=CopyManager::State::Idle)return;
  mediaAbort=false;
  mediaResult="";
  NerdWaveCopyProgress::reset();
  copyResumeAfter=musicSource!=2 && (playerRunning||playerActive) &&
                  !pauseRequested && !playbackPaused && selectedMp3Path.length();
  copyResumePath=selectedMp3Path;
  copyResumeName=playingName;
  static CopyManager::TransferContext transferContext{
    api, mediaDestination, mediaAbort, mediaResult, nerdUseWifi, NerdWaveCopyProgress::drawn
  };
  CopyManager::setTransferContext(transferContext);
  if(!CopyManager::start(copyWorker)) {
    mediaResult="Kopier-Task fehlt";
    mediaStage=4;
    redraw=true;
    NerdWaveLog::output.println("[COPY] Task konnte nicht gestartet werden");
  }
}



