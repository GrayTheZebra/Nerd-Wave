#pragma once
#include <Arduino.h>

// Ein gemeinsames GPIOA-Snapshot fuer beide Encoder; keine Interruptleitung.
namespace McpInputs {
  bool begin();
  bool read(uint8_t &bits);
  bool ready();
}
