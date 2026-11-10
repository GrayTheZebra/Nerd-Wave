#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <IPAddress.h>
namespace NerdWaveNasConfig {
void load(Preferences& prefs);
bool set(Preferences& prefs, const String& host, uint16_t port);
String host();
uint16_t port();
IPAddress address();
}
