#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Nerd-Wave v0.5.105: Auswahlzustand, Vormerkung und NVS der Musikquellen.
// Keine Audio-, Netzwerk- oder Dateisystemoperationen in diesem Modul.
namespace NerdWaveSources {
  extern uint8_t current;
  extern uint8_t requested;
  extern bool switchPending;

  // Bit 0 NAS, Bit 1 USB, Bit 2 BT-WROOM, Bit 3 SD.
  // Die Maske wird nach einem Neustart fuer die Hardwareinitialisierung wirksam.
  uint8_t enabledMask();
  bool enabled(uint8_t source);
  bool configure(Preferences& prefs, uint8_t mask);
  uint8_t firstEnabled();
  void load(Preferences& prefs);
  void loadTrack(Preferences& prefs, String& path, String& name);
  void queueNext();
  void clearPending();
  void cancelPending();
  void commit(uint8_t target, Preferences& prefs, String& path, String& name);
}
