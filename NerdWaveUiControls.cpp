#include "NerdWaveLog.h"
#include "NerdWaveUiControls.h"
#include "LedController.h"
#include "InputManager.h"
#include "NerdWaveMenus.h"
using namespace InputManager;

namespace NerdWaveUiControls {
void handleLed(int step, ButtonEvent press, int& page, bool& ledEditing,
               uint8_t& ledSettingsIndex, uint32_t& settingsActivityAt, bool& redraw) {

    if(ledEditing){
      if(step){
        switch(ledSettingsIndex){
          case 1:ledCount=constrain((int)ledCount+step,1,(int)LED_MAX);break;
          case 3:ledBrightness=constrain((int)ledBrightness+step,0,100);break;
          case 4:ledBaseBrightness=constrain((int)ledBaseBrightness+step,0,20);break;
          case 5:ledSensitivity=constrain((int)ledSensitivity+step,1,100);break;
        }
        ledSave();redraw=true;
      }
      if(press==ButtonEvent::Short || press==ButtonEvent::Long){
        if(ledSettingsIndex==1) ledResize();
        ledEditing=false;ledSave();redraw=true;
      }
    } else {
      if(step){ledSettingsIndex=((int)ledSettingsIndex+step%7+7)%7;redraw=true;}
      if(press==ButtonEvent::Long){page=3;settingsActivityAt=millis();redraw=true;}
      else if(press==ButtonEvent::Short){
        switch(ledSettingsIndex){
          case 0:ledEnabled=!ledEnabled;break;
          case 1:case 3:case 4:case 5:ledEditing=true;break;
          case 2:ledEffect=(ledEffect+1)%LED_EFFECT_COUNT;break;
          case 6:page=3;settingsActivityAt=millis();break;
        }
        ledSave();redraw=true;
      }
   }
}

void handleIr(int step, ButtonEvent press, int& page,
              uint32_t& settingsActivityAt, bool& redraw) {

    if(step && !irCapture) {irLearnIndex=((irLearnIndex+step)%IR_ACTIONS+IR_ACTIONS)%IR_ACTIONS;redraw=true;}
    if(press==ButtonEvent::Long) {
      if(irCapture) irCapture=false;
      else {page=3;settingsActivityAt=millis();}
      redraw=true;
    } else if(press==ButtonEvent::Short && !irCapture) {
      irCapture=true;redraw=true;
      NerdWaveLog::output.printf("[IR] Warte auf Taste fuer %s\n",irActionName[irLearnIndex]);
    } else if(press==ButtonEvent::Double && !irCapture) {
      irBindings[irLearnIndex]={};storeIrBinding(irLearnIndex);redraw=true;
      NerdWaveLog::output.printf("[IR] Geloescht: %s\n",irActionName[irLearnIndex]);
   }
}

void returnToPlayerWhenIdle(int& page, uint8_t resetConfirm, bool playerRunning,
                            uint8_t musicSource, bool btPlaying, bool pauseRequested, bool hasRememberedTrack,
                            uint32_t settingsActivityAt, uint32_t browserActivityAt,
                            uint32_t browserReturnMs, bool& redraw) {
  // Einstellungen: bei 5 s Inaktivitaet waehrend laufender Musik zum Player.
  // Reset-Bestaetigung bleibt offen, damit kein Dialog automatisch verschwindet.
  if(page==3 && !resetConfirm && (playerRunning || (musicSource==2 && btPlaying)) && !pauseRequested &&
     (uint32_t)(millis()-settingsActivityAt)>=5000) {
    page=1;
    redraw=true;
    NerdWaveLog::output.println("[UI] Einstellungen inaktiv - zurueck zum Player");
  }
  // Bei laufender Wiedergabe oder einem gemerkten Titel nach 5 s
  // Browser-Inaktivitaet zum Player. Kein Autostart der Wiedergabe.
  if(page==0 && (playerRunning || (musicSource==2 && btPlaying) || hasRememberedTrack) && !pauseRequested &&
      (uint32_t)(millis()-browserActivityAt)>=browserReturnMs) {
    page=1;
    redraw=true;
    NerdWaveLog::output.println("[UI] Browser inaktiv - zurueck zum Player");
  }
 }
}
