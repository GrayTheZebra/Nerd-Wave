#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Encoders.h"
namespace InputManager {
enum IrAction : uint8_t { IR_PLAY, IR_NEXT, IR_PREV, IR_VOL_UP, IR_VOL_DOWN, IR_MUTE,
  IR_BROWSER, IR_SETTINGS, IR_UP, IR_DOWN, IR_SELECT, IR_BACK, IR_LED_TOGGLE, IR_LED_NEXT,
  IR_SOURCE_NAS, IR_SOURCE_USB, IR_SOURCE_SD, IR_SOURCE_BT, IR_ACTIONS };
extern const char* const irActionName[IR_ACTIONS];
struct IrBinding { uint8_t proto=0; uint16_t address=0; uint16_t command=0; };
extern IrBinding irBindings[IR_ACTIONS];
extern int irLearnIndex;
extern bool irCapture, irDirty;
extern uint32_t irLastActionAt;
extern int irVirtualStep, irVolumeStep, irTrackStep;
extern ButtonEvent irVirtualPress;
extern bool irTogglePause,irToggleMute,irOpenBrowser,irOpenSettings;
// -1: keine Quellenwahl. Die eigentliche Umschaltung erfolgt im Runtime-loopTask.
extern int8_t irDirectSource;
void beginIr(uint8_t pin);
void bind(Preferences& prefs,int& page,bool& redraw,bool& ledToggle,bool& ledNext);
void loadIrBindings();
void storeIrBinding(int i);
void handleIr();
}
