#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Externe Konfigurationswerte fuer die vorhandene LED-Einstellungsoberflaeche.
// Die eigentliche NeoPixel-/FFT-Verarbeitung ist in LedController.cpp gekapselt.
constexpr uint16_t LED_MAX=300;
extern bool ledEnabled;
extern uint16_t ledCount;
extern uint8_t ledEffect;
extern uint8_t ledBrightness;
extern uint8_t ledBaseBrightness;
extern uint8_t ledSensitivity;
// 0..8: vier Musik-Effekte, Dauerlicht, Rainbow-Stripe und Rainbow-Gesamtfarbe (jeweils mit/ohne Musik)
constexpr uint8_t LED_EFFECT_COUNT=9;
extern uint32_t ledSolidColor; // RGB 0xRRGGBB
extern uint8_t ledRainbowSpeed; // 1..100
extern volatile uint16_t audioLevelQ15;

void ledLoad(Preferences& preferences);
void ledSave();
void ledBegin();
void ledResize();
void spectrumCapture(const int16_t* pcm,size_t byteCount,uint32_t sampleRate);
void ledTick(bool active);
