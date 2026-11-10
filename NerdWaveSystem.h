#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <Adafruit_GC9A01A.h>

// Boot-Hardware, Startanzeige, NVS-Migration und expliziter Reset.
namespace NerdWaveSystem {
void onboardRgbOff();
void initHardwarePins();
void showBootScreen(Adafruit_GC9A01A& tft);
void renderWifiSetup(Adafruit_GC9A01A& tft);
void migrateLegacyTrack(Preferences& prefs);
void performReset(Preferences& prefs, bool factoryReset,
                  bool& repeatAll, uint8_t& shuffleMode,
                  bool& autoStartLast, uint8_t& networkMode,
                  String& lastTrackPath, String& lastTrackName);
}
