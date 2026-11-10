#include "NerdWaveLog.h"
#include "InputManager.h"
#include <IRremote.hpp>
namespace InputManager {
void beginIr(uint8_t pin){IrReceiver.begin(pin, DISABLE_LED_FEEDBACK);}
static Preferences* prefsPtr=nullptr;
static int* pagePtr=nullptr;
static bool* redrawPtr=nullptr;
static bool* ledTogglePtr=nullptr;
static bool* ledNextPtr=nullptr;
void bind(Preferences& prefs,int& page,bool& redraw,bool& ledToggle,bool& ledNext){prefsPtr=&prefs;pagePtr=&page;redrawPtr=&redraw;ledTogglePtr=&ledToggle;ledNextPtr=&ledNext;}
const char* const irActionName[IR_ACTIONS]={
  "Play / Pause","Naechster Titel","Vorheriger Titel","Lauter","Leiser","Stumm",
  "Dateibrowser","Einstellungen","Menue hoch","Menue runter","Auswaehlen","Zurueck",
  "LED Ein/Aus","LED Effekt weiter",
  "Quelle NAS","Quelle USB","Quelle SD","Quelle BT"
};
IrBinding irBindings[IR_ACTIONS];
int irLearnIndex=0;
bool irCapture=false;
bool irDirty=false;
uint32_t irLastActionAt=0;
int irVirtualStep=0;
ButtonEvent irVirtualPress=ButtonEvent::None;
int irVolumeStep=0;
bool irTogglePause=false,irToggleMute=false;
int irTrackStep=0;
bool irOpenBrowser=false,irOpenSettings=false;
int8_t irDirectSource=-1;
void loadIrBindings(){
  for(int i=0;i<IR_ACTIONS;i++) {
    char k[16];
    snprintf(k,sizeof(k),"ir%d_p",i); irBindings[i].proto=prefsPtr->getUChar(k,0);
    snprintf(k,sizeof(k),"ir%d_a",i); irBindings[i].address=prefsPtr->getUShort(k,0);
    snprintf(k,sizeof(k),"ir%d_c",i); irBindings[i].command=prefsPtr->getUShort(k,0);
  }
}
void storeIrBinding(int i) {
  char k[16];
  snprintf(k,sizeof(k),"ir%d_p",i); prefsPtr->putUChar(k,irBindings[i].proto);
  snprintf(k,sizeof(k),"ir%d_a",i); prefsPtr->putUShort(k,irBindings[i].address);
  snprintf(k,sizeof(k),"ir%d_c",i); prefsPtr->putUShort(k,irBindings[i].command);
}
void handleIr() {
  if(!IrReceiver.decode()) return;
  const auto &d=IrReceiver.decodedIRData;
  const bool repeat=(d.flags & IRDATA_FLAGS_IS_REPEAT)!=0;
  const uint8_t proto=(uint8_t)d.protocol;
  const uint16_t addr=(uint16_t)d.address, cmd=(uint16_t)d.command;
  if(*pagePtr==4 && irCapture && !repeat && proto!=0) {
    for(int j=0;j<IR_ACTIONS;j++) {
      if(j!=irLearnIndex && irBindings[j].proto==proto && irBindings[j].address==addr && irBindings[j].command==cmd) {
        irBindings[j]={}; storeIrBinding(j);
        NerdWaveLog::output.printf("[IR] Bisherige Zuordnung %s entfernt\n",irActionName[j]);
      }
    }
    irBindings[irLearnIndex]={proto,addr,cmd};
    storeIrBinding(irLearnIndex);
    NerdWaveLog::output.printf("[IR] Gelernt: %s -> proto=%u addr=0x%X cmd=0x%X\n",irActionName[irLearnIndex],proto,addr,cmd);
    irCapture=false;*redrawPtr=true;
  } else if(*pagePtr!=4 && proto!=0) {
    for(int i=0;i<IR_ACTIONS;i++) {
      const auto &b=irBindings[i];
      if(!b.proto || b.proto!=proto || b.address!=addr || b.command!=cmd) continue;
      // Repeat nur fuer die Lautstaerke zulassen; andere Aktionen nicht mehrfach ausloesen.
      if(repeat && i!=IR_VOL_UP && i!=IR_VOL_DOWN) break;
      if(repeat && (uint32_t)(millis()-irLastActionAt)<110) break;
      irLastActionAt=millis();
      NerdWaveLog::output.printf("[IR] Aktion: %s%s\n",irActionName[i],repeat?" (Repeat)":"");
      switch(i) {
        case IR_PLAY: irTogglePause=true; break;
        case IR_NEXT: irTrackStep=1; break;
        case IR_PREV: irTrackStep=-1; break;
        case IR_VOL_UP: irVolumeStep+=2; break;
        case IR_VOL_DOWN: irVolumeStep-=2; break;
        case IR_MUTE: irToggleMute=true; break;
        case IR_BROWSER: irOpenBrowser=true; break;
        case IR_SETTINGS: irOpenSettings=true; break;
        case IR_UP: irVirtualStep=-1; break;
        case IR_DOWN: irVirtualStep=1; break;
        case IR_SELECT: irVirtualPress=ButtonEvent::Short; break;
        case IR_BACK: irVirtualPress=ButtonEvent::Long; break;
        case IR_LED_TOGGLE: *ledTogglePtr=true; break;
        case IR_LED_NEXT: *ledNextPtr=true; break;
        case IR_SOURCE_NAS: irDirectSource=0; break;
        case IR_SOURCE_USB: irDirectSource=1; break;
        case IR_SOURCE_SD:  irDirectSource=3; break;
        case IR_SOURCE_BT:  irDirectSource=2; break;
        default: break;
      }
      break;
    }
  }
  IrReceiver.resume();
}


}
