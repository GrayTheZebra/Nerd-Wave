#pragma once
#include <Arduino.h>
#include "MusicApi.h"
#include "InputManager.h"
namespace NerdWaveCopyUi {
struct Context {
  int& mediaStage;
  int& mediaChoice;
  bool& redraw;
  int& mediaOrigin;
  bool& sdMounted;
  String& mediaPath;
  int& mediaCopyMode;
  String& mediaChosenPath;
  String& mediaChosenName;
  int& mediaDestination;
  int& mediaSelection;
  MusicPage& mediaList;
  int& mediaIndex;
  bool& mediaInfo;
  int& mediaReturnPage;
  uint32_t& browserActivityAt;
  uint8_t& musicSource;
  String& selectedMp3Path;
  String& lastTrackPath;
  String& mediaResult;
  int& page;
  bool (*mountSd)();
  bool (*mountUsb)();
  bool (*usbMounted)();
  bool (*loadFolder)(const String&);
  bool (*listAt)(int,MusicItem&);
  void (*execute)();
};
void handle(Context& c,int steps,ButtonEvent button);
// Im Kopiermenue: 5 Sekunden Inaktivitaet, auch auf Ergebnis- und Info-Seite.
void markActivity();
bool idleExpired();
void returnToPlayer(int& page,int& mediaStage,bool& mediaInfo,bool& mediaNotice,bool& redraw,uint32_t& browserActivityAt);
}
