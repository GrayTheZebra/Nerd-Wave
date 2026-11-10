#include "SourceMetadata.h"

const char* sourceName(uint8_t source) {
  switch (source) {
    case 0: return "NAS";
    case 1: return "USB";
    case 2: return "BT";
    case 3: return "SD";
    default: return "?";
  }
}

const char* trackPathKey(uint8_t source) {
  return source == 3 ? "sdPath" : source == 1 ? "usbPath" : "nasPath";
}

const char* trackNameKey(uint8_t source) {
  return source == 3 ? "sdName" : source == 1 ? "usbName" : "nasName";
}
