#pragma once
#include <stddef.h>
#include <stdint.h>

// MP3-Frame-Metadaten ohne Abhaengigkeit von Playerzustand/Arduino.
uint32_t mp3HeaderBitrate(const uint8_t* frame, size_t length);
uint32_t mp3HeaderDuration(const uint8_t* frame, size_t length, uint32_t sampleRate);
