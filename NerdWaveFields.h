#pragma once
#include <Arduino.h>
#include <Preferences.h>
namespace NerdWaveFields {
constexpr uint8_t MAX_FIELDS=16;
struct Field {
 String id, text, type="text", style="linear";
 uint8_t width=100,height=10,maxTextWidth=230,borderWidth=1,radius=8;
 uint16_t arcStart=0,arcSweep=360;
 uint8_t x=120,y=120,size=2,padding=0;
 uint16_t color=0xffff,background=0;
 bool visible=true,bgEnabled=false;
};
void begin(Preferences& prefs);
bool enabled();
uint8_t count();
const Field& at(uint8_t i);
String json();
bool apply(const String& json,bool persist,String& error);
}
