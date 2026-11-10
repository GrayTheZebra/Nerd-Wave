#include "NerdWaveLog.h"
#include "McpInputs.h"
#include "Config.h"
#include <Wire.h>

namespace {
  bool initialized = false;
  uint8_t cached = 0xFF;
  uint32_t lastPollUs = 0;
  uint32_t lastErrorMs = 0;

  bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(Config::MCP_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }
  bool readReg(uint8_t reg, uint8_t &value) {
    Wire.beginTransmission(Config::MCP_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(Config::MCP_ADDRESS, (uint8_t)1) != 1) return false;
    value = Wire.read();
    return true;
  }
}

bool McpInputs::begin() {
  Wire.begin(Config::MCP_SDA, Config::MCP_SCL);
  Wire.setClock(400000);
  // BANK=0, SEQOP=0 (Registerstandard); alle Port-A-Eingaenge mit Pullups.
  initialized = writeReg(0x00, 0xFF) && writeReg(0x0C, 0xFF)
                && readReg(0x12, cached);
  lastPollUs = micros();
  NerdWaveLog::output.printf("[MCP23017] %s, Adresse 0x%02X, SDA=%u SCL=%u\n",
                initialized ? "bereit" : "NICHT ERREICHBAR",
                Config::MCP_ADDRESS, Config::MCP_SDA, Config::MCP_SCL);
  return initialized;
}

bool McpInputs::ready() { return initialized; }

bool McpInputs::read(uint8_t &bits) {
  if (!initialized) return false;
  const uint32_t nowUs = micros();
  // Mindestens 1 ms Abstand: beide Encoder bekommen dasselbe Abbild.
  if ((uint32_t)(nowUs - lastPollUs) >= 1000) {
    lastPollUs = nowUs;
    uint8_t next;
    if (!readReg(0x12, next)) {
      uint32_t now = millis();
      if ((uint32_t)(now - lastErrorMs) >= 1000) {
        lastErrorMs = now;
        NerdWaveLog::output.println("[MCP23017] I2C-Lesefehler");
      }
      return false;
    }
    cached = next;
  }
  bits = cached;
  return true;
}
