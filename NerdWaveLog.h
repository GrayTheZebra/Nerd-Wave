#pragma once
#include <Arduino.h>
#include <Print.h>

// Spiegelung der Firmware-Serialausgabe in einen begrenzten RAM-Ringpuffer.
// Keine SD-/Flash-Schreibzugriffe, keine Blockierung durch HTTP-Clients.
namespace NerdWaveLog {
  class MirroredPrint final : public Print {
  public:
    size_t write(uint8_t b) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    using Print::write;
  };
  extern MirroredPrint output;
  // Nur waehrend setup(): Bildschirm-Protokoll auf der Setup-Task.
  void setBootLineCallback(void (*callback)(const char*));
  // Text ab einer absoluten Byte-Position, maximal maxBytes Zeichen.
  // next gibt die Position nach dem letzten zurueckgegebenen Byte an.
  String read(uint32_t from, uint16_t maxBytes, uint32_t& next, bool& truncated);
}
