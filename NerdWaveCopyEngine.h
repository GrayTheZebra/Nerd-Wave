#pragma once
#include <Arduino.h>
#include "MusicApi.h"
namespace NerdWaveCopyEngine {
struct Context {
  int& mediaDestination;
  int& mediaOrigin;
  String& mediaResult;
  volatile bool& autoNextPending;
  volatile bool& autoNextEnabled;
  volatile bool& stopRequested;
  volatile bool& pauseRequested;
  volatile bool& decoderRunning;
  volatile bool& playerRunning;
  bool& startingTrack;
  int& mediaCopyMode;
  String& mediaChosenPath;
  volatile bool& mediaAbort;
  uint8_t& musicSource;
  volatile bool& playerActive;
  volatile bool& playbackPaused;
  String& selectedMp3Path;
  String& copyResumePath;
  String& copyResumeName;
  bool& copyResumeAfter;
  String& playingName;
  MusicApi& api;
  bool& nerdUseWifi;
  int& mediaStage;
  bool& redraw;
  bool& lanReady;
  bool (*connectNetwork)();
  void (*ampOff)();
};
void execute(Context& c);
}
