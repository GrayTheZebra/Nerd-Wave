#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Browser.h"
#include "MusicApi.h"

// Wiederherstellung fehlgeschlagener NAS- und SD-Quellen im Arduino-loopTask.
// Speicher-/Player-Zustaende bleiben im bisherigen Besitzer; keine parallelen Tasks.
namespace NerdWaveRecovery {
extern bool nasPending;
extern uint32_t nasRetryAt;
extern uint8_t nasFailures;

struct Context {
  uint8_t& musicSource;
  bool& nasPending;
  uint32_t& nasRetryAt;
  uint8_t& nasFailures;
  bool& sdRecoveryPending;
  bool& sdMounted;
  uint32_t& sdRetryAt;
  bool& sdAutostartPending;
  bool& startingTrack;
  volatile bool& decoderRunning;
  volatile bool& playerRunning;
  bool& useWifi;
  bool& wifiReady;
  bool& autoStartLast;
  String& selectedMp3Path;
  Preferences& prefs;
  MusicBrowser& browser;
  MusicApi& api;
  int& page;
  bool& redraw;
  bool (*startTrack)(const String&,const String&);
};
// Nur Anzeigestatus: keine Aenderung der Retry-/Netzwerklogik.
// Beim NAS-Ausfall zeigt die UI die letzte Aktion und den naechsten Versuch.
enum class NasPhase : uint8_t { Waiting, CheckingApi, ApiUnavailable, WifiReconnect, WifiFailed, WifiReady, RestoringTrack };
extern NasPhase nasPhase;
const char* nasPhaseLabel();
uint32_t secondsToNextNasRetry(uint32_t retryAt);
void poll(Context& ctx);
}
