#pragma once
#include <Arduino.h>

// ESP32-S3 <-> ESP32-WROOM: bestehendes UART-Protokoll ohne Funktionsaenderung.
namespace NerdWaveBtUart {
// Pinbelegung S3 UART1: RX GPIO42, TX GPIO47 (aus v0.5.99).
constexpr int BT_UART_RX = 42;
constexpr int BT_UART_TX = 47;
extern HardwareSerial btLink;
extern volatile bool btConnected;
extern volatile bool btPlaying;
extern bool btPairing;
extern bool btReady;
extern bool btUiDirty;
extern bool btUartStarted;

void btSend(const char* cmd);
void btPoll();
} // namespace NerdWaveBtUart
