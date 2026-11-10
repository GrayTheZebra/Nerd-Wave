#pragma once
#include <Arduino.h>
#include <Preferences.h>
namespace NerdWaveSkins {
struct Theme { uint16_t bg, text, accent, secondary, selected, dimAccent, dimSecondary; };
struct Layout { uint8_t sourceY, iconsY, titleY, timeY, stateY; uint8_t sourceX, iconsX, titleX, timeX, stateX; uint8_t sourceFont, titleFont, timeFont, stateFont; uint16_t elementColor[5]; uint8_t visibleMask; uint8_t iconSize; };
struct Decoration {
  static constexpr uint8_t MAX_BANDS = 4;
  uint8_t mode=0, direction=0; // 0=solid, 1=gradient, 2=bands
  uint16_t first=0x0000, second=0x0000;
  uint8_t bandCount=1;
  uint8_t bandStart[MAX_BANDS]={0,0,0,0};
  uint8_t bandEnd[MAX_BANDS]={239,0,0,0};
  uint16_t bandColor[MAX_BANDS]={0,0,0,0};
  uint16_t bgColor[5]={0};
  uint8_t bgPadding[5]={0};
  bool bgEnabled[5]={false};
  String text[5];
};
void begin(Preferences& prefs);
const Theme& current();
const Layout& layout();
const Decoration& decoration();
uint16_t backgroundAt(int x,int y);
bool setDecoration(const Decoration& next,bool save,String& error);
const char* activeName();
String json();
bool applyJson(const String& payload, String& error);
bool selectBuiltin(const String& id);
bool selectCustom();
bool setLayout(const Layout& next, bool save, String& error);
}
