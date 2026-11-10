#pragma once
#include <Arduino.h>
#include <Preferences.h>

// NVS-Verwaltung von allgemeinen Player-Einstellungen.
// Quellen- und Lautstaerke-NVS bleiben in den vorhandenen Modulen.
namespace NerdWaveSettings {
uint8_t loadNetworkMode(Preferences& prefs);
void saveNetworkMode(Preferences& prefs, uint8_t mode);
const char* modeName(uint8_t mode);
void loadPlayback(Preferences& prefs, bool& repeatAll, uint8_t& shuffleMode, bool& autoStartLast);
void savePlayback(Preferences& prefs, bool repeatAll, uint8_t shuffleMode, bool autoStartLast);
}
