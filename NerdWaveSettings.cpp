#include "NerdWaveLog.h"
#include "NerdWaveSettings.h"

namespace NerdWaveSettings {
uint8_t loadNetworkMode(Preferences& prefs) {
  const uint8_t mode = prefs.getUChar("netMode", 0);
  // Alte Prioritaetsmodi einmalig auf einen festen Transport abbilden.
  return (mode == 1 || mode == 3) ? 1 : 0;
}

void saveNetworkMode(Preferences& prefs, uint8_t mode) {
  prefs.putUChar("netMode", mode == 1 ? 1 : 0);
}

const char* modeName(uint8_t mode) {
  return mode == 1 ? "WLAN" : "LAN";
}

void loadPlayback(Preferences& prefs, bool& repeatAll, uint8_t& shuffleMode, bool& autoStartLast) {
  repeatAll = prefs.getBool("repeat", false);
  const uint8_t savedShuffle = prefs.getUChar("shuffle", 0);
  shuffleMode = savedShuffle <= 2 ? savedShuffle : 2;
  autoStartLast = prefs.getBool("autostart", false);
}

void savePlayback(Preferences& prefs, bool repeatAll, uint8_t shuffleMode, bool autoStartLast) {
  prefs.putBool("repeat", repeatAll);
  prefs.putUChar("shuffle", shuffleMode);
  prefs.putBool("autostart", autoStartLast);
  NerdWaveLog::output.printf("[NVS] Repeat=%d Shuffle=%u Autostart=%d\n", repeatAll, shuffleMode, autoStartLast);
}
}
