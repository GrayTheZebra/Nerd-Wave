#pragma once
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

// Gemeinsame Playback-Task fuer NAS, USB und SD. Alle Zustandsreferenzen
// zeigen auf die unveraenderten Variablen der Runtime bzw. ihrer Module.
namespace NerdWavePlayback {
struct Context {
  volatile bool& playerRunning;
  volatile bool& stopRequested;
  volatile bool& decodeDone;
  volatile bool& decodeError;
  volatile bool& audioSampleRateReady;
  volatile uint32_t& audioSampleRate;
  volatile bool& playerActive;
  volatile bool& playbackPaused;
  volatile bool& pauseRequested;
  uint32_t& i2sErrors;
  uint64_t& bytesPlayed;
  uint32_t& underruns;
  uint32_t& maxI2sWriteUs;
  volatile bool& muted;
  volatile int& volumePercent;
  volatile uint16_t& audioLevelQ15;
  volatile bool& autoNextEnabled;
  volatile bool& autoNextPending;
  void (*ampOff)();
  void (*ampOn)();
  void (*captureSpectrum)(const int16_t*, size_t, uint32_t);
};
void playbackTask(void* parameter);
}
