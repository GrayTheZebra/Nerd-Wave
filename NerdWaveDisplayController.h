#pragma once
#include <Arduino.h>
#include <Adafruit_GC9A01A.h>
#include "Browser.h"
#include "NerdWaveMenus.h"
namespace NerdWaveDisplayController {
struct Context {
  Adafruit_GC9A01A& tft;
  int page;
  bool& redraw;
  uint64_t bytesPlayed;
  uint32_t playbackBaseSeconds;
  uint32_t audioSampleRate;
  uint32_t estimatedDurationSec;
  uint8_t musicSource;
  bool muted;
  int volumePercent;
  bool nerdUseWifi;
  bool repeatAll;
  uint8_t shuffleMode;
  const String& playingName;
  bool btConnected;
  bool btPlaying;
  bool btPairing;
  bool playbackPaused;
  bool playerActive;
  bool startingTrack;
  bool decodeDone;
  MusicBrowser& browser;
  int& lastPage;
  int& lastProgress;
  int& lastVolume;
  String& shownPlayerTitle;
  String& shownTime;
  String& shownSource;
  String& shownState;
  int& shownWifiStatus;
  int& shownRepeat;
  int& shownShuffle;
  int mediaStage;
  int mediaChoice;
  int mediaOrigin;
  int mediaDestination;
  int mediaCopyMode;
  int mediaIndex;
  int mediaSelection;
  int settingsIndex;
  bool mediaInfo;
  bool sdMounted;
  bool ledEditing;
  bool ledEnabled;
  bool irCapture;
  bool resetConfirmYes;
  bool autoStartLast;
  uint8_t resetConfirm;
  uint8_t requestedMusicSource;
  uint8_t networkMode;
  uint8_t ledEffect;
  uint8_t ledBrightness;
  uint8_t ledBaseBrightness;
  uint8_t ledSensitivity;
  uint8_t ledSettingsIndex;
  uint16_t ledCount;
  const String& mediaPath;
  const String& mediaChosenName;
  const String& mediaResult;
  const MusicPage& mediaList;
  uint32_t underruns;
  uint32_t i2sErrors;
  uint32_t maxI2sWriteUs;
  uint64_t ringFill;
  bool (*copyListAt)(int,MusicItem&);
};
void render(Context& c);
}
