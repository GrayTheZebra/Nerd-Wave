#pragma once
#include <stdint.h>

// Quellennummern und NVS-Schluessel absichtlich kompatibel zu v0.5.77.
// Funktionen besitzen keinen Hardwarezugriff und keinen globalen Zustand.
const char* sourceName(uint8_t source);
const char* trackPathKey(uint8_t source);
const char* trackNameKey(uint8_t source);
