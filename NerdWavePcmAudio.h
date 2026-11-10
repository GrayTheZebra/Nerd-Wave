#pragma once
#include <Arduino.h>
#include "driver/i2s.h"

// PCM-Puffer fuer NAS/USB/SD und gemeinsam genutzter Legacy-I2S-Installer.
// Bluetooth besitzt weiterhin seinen eigenen PCM-Ring in NerdWaveBtAudio.
namespace NerdWavePcmAudio {
constexpr size_t CAPACITY = 512 * 1024;
extern bool nasI2SInstalled;
bool begin();
bool ready();
void reset(); // Nur aufrufen, wenn Decoder und Player gestoppt sind.
size_t level();
bool write(const uint8_t* data, size_t count,
           const volatile bool& decodeError, const volatile bool& stopRequested);
size_t read(uint8_t* dest, size_t count);
bool configureLegacyI2S(i2s_port_t port, bool input, uint32_t rate);
bool initOutput(uint32_t sampleRate);
void shutdownOutput();
}
