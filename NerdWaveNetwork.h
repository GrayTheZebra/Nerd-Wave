#pragma once
#include <Arduino.h>

namespace NerdWaveNetwork {
void setLanEnabled(bool enabled);
bool lanEnabled();
// Nur true, wenn die LAN-Initialisierung erfolgreich abgeschlossen wurde.
bool lanUsable();
// Netzmodus: 0 LAN, 1 WLAN. Keine automatische Prioritaets-Umschaltung.
// Die referenzierten Zustandswerte gehoeren weiterhin der NerdWaveRuntime.
// callback wird ausschliesslich vom WiFiManager-Setup-AP aufgerufen.
bool connect(uint8_t mode, bool& useWifi, bool& lanReady, bool& wifiReady,
             void (*showSetupPortal)());
// Automatische NAS-Recovery: ohne WiFiManager/Setup-AP und ohne NVS-Aenderungen.
// Ausschliesslich bei gestoppter Audiowiedergabe aufrufen.
bool recoverWifi(uint32_t timeoutMs = 10000);
// Nur Diagnose: aktueller Mesh-Zugangspunkt, Kanal und Signalstaerke.
void logWifiDetails(const char* context);
}
