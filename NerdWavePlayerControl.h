#pragma once
#include <Arduino.h>
#include "Browser.h"

namespace NerdWavePlayerControl {
extern volatile bool pauseRequested;
extern bool repeatAll;
extern uint8_t shuffleMode;

void togglePause();
void toggleRepeat();
void cycleShuffle();

// Nur die Auswahl des naechsten Titels; Audio-Lifecycle verbleibt in der Runtime.
struct NavigationContext {
  MusicBrowser& browser;
  String& selectedMp3Path;
  String& playingName;
  uint64_t& bytesPlayed;
  volatile bool& decodeError;
  bool (*startTrack)(const String&, const String&, bool);
  bool (*pickRandomTrack)(String&, String&);
  String (*parentFolder)(const String&);
  bool (*isMp3File)(const MusicItem&);
};
bool changeTrack(NavigationContext& ctx, int direction, bool restartCurrent = false);
}
