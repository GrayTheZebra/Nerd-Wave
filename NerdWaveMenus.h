#pragma once
#include <Arduino.h>
#include <Adafruit_GC9A01A.h>
#include <Preferences.h>
#include "MusicApi.h"
namespace NerdWaveMenus {
extern uint32_t settingsActivityAt;
extern bool resetConfirmYes;
extern uint8_t resetConfirm;
extern int settingsReturnPage;
extern bool mediaListReady;
extern int mediaSelection;
extern MusicPage mediaList;
extern String mediaResult;
extern String mediaChosenName;
extern String mediaChosenPath;
extern String mediaPath;
extern int mediaDestination;
extern int mediaOrigin;
extern int mediaChoice;
extern int mediaStage;
extern int mediaCopyMode;
extern bool mediaNotice;
extern bool mediaInfo;
extern int mediaReturnPage;
extern int mediaIndex;
extern int settingsIndex;
// Menu model has borrowed references; called only on Arduino loopTask.
struct View {
 Adafruit_GC9A01A& tft;
 int page, mediaStage, mediaChoice, mediaOrigin, mediaDestination, mediaCopyMode, mediaIndex, mediaSelection, settingsIndex;
 bool mediaInfo, sdMounted, ledEditing, ledEnabled, irCapture, resetConfirmYes, btPairing, btConnected, repeatAll, autoStartLast, nerdUseWifi;
 uint8_t resetConfirm, musicSource, requestedMusicSource, shuffleMode, networkMode, ledEffect, ledBrightness, ledBaseBrightness, ledSensitivity, ledSettingsIndex;
 uint16_t ledCount;
 const String& mediaPath; const String& mediaChosenName; const String& mediaResult;
 const MusicPage& mediaList;
 uint64_t ringLevel, underruns, i2sErrors, maxI2sWriteUs;
 bool (*copyListAt)(int,MusicItem&);
};
void render(const View& view);
// Common cyclic navigation, preserving modulo behavior for negative increments.
int wrapIndex(int index, int increment, int count);
// Sichtbarer Menueindex -> fester Aktionsindex (LAN aus: Punkt 5 auslassen).
int settingsCount();
int settingsAction(int visibleIndex);
}
