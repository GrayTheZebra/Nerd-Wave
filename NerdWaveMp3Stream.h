#pragma once
#include <Arduino.h>

// MP3-Decoder und Quellenleser (NAS / USB / SD).
// Alle Zustandsfelder referenzieren weiterhin die bestehenden Runtime-Variablen.
namespace NerdWaveMp3Stream {
struct Context {
  bool& nerdUseWifi;
  uint8_t& musicSource;
  bool& sdMounted;
  String& selectedMp3Path;
  volatile bool& stopRequested;
  volatile bool& decoderRunning;
  volatile bool& decodeError;
  volatile bool& decodeDone;
  volatile uint32_t& audioSampleRate;
  volatile bool& audioSampleRateReady;
  uint64_t& framesDecoded;
  uint32_t& netStalls;
  volatile uint32_t& estimatedDurationSec;
  volatile bool& durationExact;
  volatile uint32_t& seekStartSeconds;
 };
void bind(Context& context);
void decodeTask(void*);
}
