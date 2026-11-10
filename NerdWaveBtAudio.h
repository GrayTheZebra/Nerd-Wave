#pragma once
#include <Arduino.h>
#include "driver/i2s.h"
namespace NerdWaveBtAudio {
extern volatile bool btStopRequested, btTaskRunning, btRxRunning;
extern uint32_t btAudioStartAt;
constexpr uint32_t BT_RETRY_INTERVAL_MS = 5000;
extern volatile uint32_t btRxBytes, btTxBytes, btI2sErrors, btUnderruns;
extern volatile uint32_t btOverflows, btStarves, btBufferLevel;
// Verwendet unveraendert den Legacy-I2S-Installer der Runtime.
bool start(volatile bool& playerActive, const bool& mp3I2sInstalled,
           bool (*configureI2S)(i2s_port_t, bool, uint32_t),
           void (*ampOff)(), void (*ampOn)());
void requestStop();
}
