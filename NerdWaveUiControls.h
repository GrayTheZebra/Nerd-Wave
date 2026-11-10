#pragma once
#include <Arduino.h>
#include "Encoders.h"
namespace NerdWaveUiControls {
void handleLed(int step, ButtonEvent press, int& page, bool& ledEditing,
               uint8_t& ledSettingsIndex, uint32_t& settingsActivityAt, bool& redraw);
void handleIr(int step, ButtonEvent press, int& page,
              uint32_t& settingsActivityAt, bool& redraw);
void returnToPlayerWhenIdle(int& page, uint8_t resetConfirm, bool playerRunning,
                            uint8_t musicSource, bool btPlaying, bool pauseRequested, bool hasRememberedTrack,
                            uint32_t settingsActivityAt, uint32_t browserActivityAt,
                            uint32_t browserReturnMs, bool& redraw);
}
