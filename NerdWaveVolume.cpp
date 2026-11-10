#include "NerdWaveLog.h"
#include "NerdWaveVolume.h"

namespace NerdWaveVolume {
volatile int percent = 20;
volatile bool muted = false;
bool dirty = false;
uint32_t changedAt = 0;

void load(Preferences& prefs) {
  percent = constrain((int)prefs.getUChar("volume", 20), 0, 100);
  NerdWaveLog::output.printf("[NVS] Lautstaerke geladen: %d%%\n", (int)percent);
}

void saveIfDirty(Preferences& prefs) {
  if (!dirty) return;
  prefs.putUChar("volume", (uint8_t)constrain((int)percent, 0, 100));
  dirty = false;
  NerdWaveLog::output.printf("[NVS] Lautstaerke gespeichert: %d%%\n", (int)percent);
}

void changeBy(int delta, uint32_t now) {
  const int next = constrain((int)percent + delta, 0, 100);
  if (next != (int)percent) {
    percent = next;
    dirty = true;
    changedAt = now;
  }
}

void toggleMute() { muted = !muted; }
void discardPendingSave() { dirty = false; }
}
