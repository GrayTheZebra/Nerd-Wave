#pragma once
#include <Arduino.h>
#include <Preferences.h>

namespace NerdWaveVolume {
extern volatile int percent;
extern volatile bool muted;
extern bool dirty;
extern uint32_t changedAt;

void load(Preferences& prefs);
void saveIfDirty(Preferences& prefs);
void changeBy(int delta, uint32_t now);
void toggleMute();
void discardPendingSave();
}
