#pragma once
#include <Arduino.h>
#include <Adafruit_GC9A01A.h>
namespace NerdWaveCopyProgress {
extern bool drawn;
void reset();
void render(Adafruit_GC9A01A& tft,const String& name,uint64_t done,uint64_t total,uint32_t files);
}
