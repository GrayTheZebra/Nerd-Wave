#pragma once
#include <Arduino.h>
#include <Preferences.h>
namespace NerdWaveWifiFilter {
void begin();
bool blocked(const String& bssid);
bool add(const String& bssid);
bool remove(const String& bssid);
String entries();
String name(const String& bssid);
bool setName(const String& bssid,const String& displayName);
// Connect to strongest permitted BSSID; requires saved STA credentials.
bool connectAllowed(uint32_t timeoutMs=12000);
// Detect roaming onto a blocked AP, rate-limited from main loop.
void poll();
}
