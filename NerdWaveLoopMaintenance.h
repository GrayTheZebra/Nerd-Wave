#pragma once
#include <Arduino.h>
#include "driver/i2s.h"

// Periodische Systempflege, nur aus dem Arduino-loopTask aufrufen.
// Abhaengige Runtime-Zustaende werden als Referenzen uebergeben,
// damit weder die Audio-Tasks noch die Zustandsverwaltung dupliziert werden.
namespace NerdWaveLoopMaintenance {
void pollUsb(uint8_t musicSource, bool playbackBusy,
             volatile bool& stopRequested, volatile bool& pauseRequested,
             volatile bool& autoNextPending, int& page, bool& redraw,
             void (*ampOff)(), void (*refreshBrowser)());
void pollBluetooth(uint8_t musicSource, bool sourceSwitchPending,
                   volatile bool& playerActive, const bool& nasI2SInstalled,
                   bool& redraw, bool (*configureI2S)(i2s_port_t, bool, uint32_t),
                   void (*ampOff)(), void (*ampOn)());
}
