#pragma once

#include <Arduino.h>

// SD_MMC (1-bit) und Retry-Zustand. Die Entscheidung ueber Browser-/Autostart-
// Recovery bleibt im Hauptloop; dieses Modul greift nie auf Audio-Tasks zu.
namespace NerdWaveSdCard {
extern bool mounted;
extern bool recoveryPending;
extern uint32_t retryAt;

// Unveraenderter Mount-Ablauf aus v0.5.95 (3 Versuche, 500 ms Abstand).
bool tryMount();
}
