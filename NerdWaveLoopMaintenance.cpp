#include "NerdWaveLog.h"
#include "NerdWaveLoopMaintenance.h"
#include "NerdWaveUsb.h"
#include "NerdWaveBtAudio.h"
#include "NerdWaveBtUart.h"
#include <esp_heap_caps.h>

namespace NerdWaveLoopMaintenance {

void pollUsb(uint8_t musicSource, bool playbackBusy,
             volatile bool& stopRequested, volatile bool& pauseRequested,
             volatile bool& autoNextPending, int& page, bool& redraw,
             void (*ampOff)(), void (*refreshBrowser)()) {
  using namespace NerdWaveUsb;
  if(!hostStarted) return;
  bool usbMountedNow=usbDisk.mounted();
  if(previouslyMounted && !usbMountedNow) {
    NerdWaveLog::output.println("[USB] Entfernt: Wiedergabe wird gestoppt");
    previouslyMounted=false;
    removalPending=true;
    browserRefreshPending=(musicSource==1);
    if(musicSource==1) {
      ampOff();
      autoNextPending=false;
      stopRequested=true;
      pauseRequested=false;
    }
    redraw=true;
  }
  // Erst auf Taskende warten; kein FAT-Zugriff mit altem File-Handle.
  if(removalPending && !playbackBusy) {
    removalPending=false;
    if(browserRefreshPending && musicSource==1) {
      refreshBrowser();
      browserRefreshPending=false;
      page=0;
      NerdWaveLog::output.println("[USB] Browser geleert: Stick fehlt");
    }
    redraw=true;
  }
  // FAT32-Mount kann blockieren: nur waehrend USB als Quelle aktiv ist.
  // Wechsel zu USB stoesst ebenfalls einen Mountversuch an.
  if(musicSource==1 && !usbMountedNow && !removalPending && !playbackBusy) {
    if(tryMount()) {
      usbMountedNow=true;
      NerdWaveLog::output.println("[USB] Eingesteckt / FAT32 wieder bereit");
    }
  }
  if(!previouslyMounted && usbMountedNow) {
    previouslyMounted=true;
    everMounted=true;
    NerdWaveLog::output.println("[USB] Datentraeger verfuegbar");
    if(musicSource==1 && !removalPending) {
      refreshBrowser();
      page=0;
      redraw=true;
      NerdWaveLog::output.println("[USB] Browser aktualisiert; kein automatischer Start");
    }
  }
}

void pollBluetooth(uint8_t musicSource, bool sourceSwitchPending,
                   volatile bool& playerActive, const bool& nasI2SInstalled,
                   bool& redraw, bool (*configureI2S)(i2s_port_t, bool, uint32_t),
                   void (*ampOff)(), void (*ampOn)()) {
  using namespace NerdWaveBtAudio;
  using namespace NerdWaveBtUart;
  // BT-Recovery nur wenn kein Task mehr aktiv und kein Quellenwechsel ansteht.
  if(musicSource==2 && !sourceSwitchPending && !btStopRequested &&
     !btTaskRunning && !btRxRunning &&
     millis()-btAudioStartAt >= BT_RETRY_INTERVAL_MS) {
    NerdWaveLog::output.printf("[BT-REC] Audiotask inaktiv; neuer Start (I2S-Fehler=%lu, Heap=%u, MP3-I2S=%d)\n",
                  (unsigned long)btI2sErrors,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                  (int)nasI2SInstalled);
    if(!start(playerActive,nasI2SInstalled,configureI2S,ampOff,ampOn))
      NerdWaveLog::output.println("[BT-REC] Audiotask konnte nicht erzeugt werden");
  }
  if(btUartStarted) {
    btPoll();
    if(btUiDirty) { btUiDirty=false; redraw=true; }
  }
  if(musicSource==2 && btUartStarted) {
    static uint32_t btStatusAt=0;
    if(millis()-btStatusAt>=3000) { btStatusAt=millis(); btSend("STATUS"); }
  }
  if(musicSource==2) {
    static uint32_t btStatsAt=0;
    if(millis()-btStatsAt>=5000) {
      btStatsAt=millis();
      NerdWaveLog::output.printf("[BT-PCM] connected=%d playing=%d task=%d rxTask=%d active=%d RX=%lu KB TX=%lu KB Puffer=%lu B Overflows=%lu Starves=%lu RX-Timeouts=%lu I2S-Fehler=%lu\n",
        (int)btConnected,(int)btPlaying,(int)btTaskRunning,(int)btRxRunning,(int)playerActive,
        (unsigned long)(btRxBytes/1024),(unsigned long)(btTxBytes/1024),
        (unsigned long)btBufferLevel,(unsigned long)btOverflows,
        (unsigned long)btStarves,(unsigned long)btUnderruns,(unsigned long)btI2sErrors);
    }
  }
}
}
