#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
// Reine Zeichenhelfer. Aufruf nur aus dem GUI-Task.
namespace DisplayUI {
void attach(Adafruit_GFX& display);
void centeredText(const String& text,int y,uint8_t size,uint16_t color,int maxChars);
void centeredTextAt(const String& text,int centerX,int y,uint8_t size,uint16_t color,int maxChars);
void centeredPlayerTitle(const String& name, int centerY=120);
void playerTitleAt(const String& name,int centerX,int centerY,uint8_t fontSize);
void updateArc(bool upper,int percent,int& previous,uint16_t active,uint16_t inactive);
void drawWifiIcon(int cx,int cy,int strength);
void drawCustomIcon(const String& icon,int cx,int cy,int state,uint16_t color,uint8_t size);
void drawRepeatIcon(int cx,int cy,bool enabled);
void drawShuffleIcon(int cx,int cy,int mode);
}
