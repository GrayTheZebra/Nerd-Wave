#pragma once
#include <Arduino.h>
#include <Preferences.h>
namespace NerdWaveWeb {
struct Context {
 Preferences& prefs;
 uint8_t& musicSource;
 uint8_t& networkMode;
 bool& repeatAll;
 uint8_t& shuffleMode;
 bool& autoStartLast;
 bool& networkRestartPending;
 uint8_t& bootNetworkMode;
 int& page;
 bool& redraw;
 String& playingName;
 String& selectedPath;
 uint64_t& bytesPlayed;
 volatile uint32_t& baseSeconds;
 volatile uint32_t& sampleRate;
 volatile uint32_t& durationSeconds;
 volatile bool& seeking;
 volatile uint32_t& seekDisplayDuration;
 bool (*seekTo)(uint32_t);
 volatile bool& playerActive;
 volatile bool& playbackPaused;
 volatile bool& btConnected;
 volatile bool& btPlaying;
 volatile int& volumePercent;
 bool (*playPath)(const String&, const String&);
 bool (*playPause)();
 bool (*stopPlayback)();
 bool (*skip)(int);
 void (*setVolume)(int);
 void (*requestSource)(uint8_t);
 void (*savePlayback)();
};
void begin(Context& ctx);
void poll();
}
