#include "NerdWaveCopyProgress.h"
#include "DisplayUI.h"
using DisplayUI::centeredText;
namespace { constexpr uint16_t C_BG=0x0000, C_WHITE=0xFFFF, C_CYAN=0x07FF, C_YELLOW=0xFFE0; }
bool NerdWaveCopyProgress::drawn=false;
static String copyProgressName="";
static int copyProgressPercent=-1;
static uint32_t copyProgressFiles=UINT32_MAX;

// Vom Hintergrundtask nur numerische Daten und kurzen Dateinamen publizieren.
// TFT und Arduino-Encoder bleiben exklusiv im loopTask.
void NerdWaveCopyProgress::reset() {
  NerdWaveCopyProgress::drawn=false;
  copyProgressName="";
  copyProgressPercent=-1;
  copyProgressFiles=UINT32_MAX;
}

void NerdWaveCopyProgress::render(Adafruit_GC9A01A& tft,const String& name,uint64_t done,uint64_t total,uint32_t files) {
  tft.setTextWrap(false);
  if(!NerdWaveCopyProgress::drawn) {
    tft.fillScreen(C_BG);
    centeredText("KOPIERE",45,2,C_YELLOW,18);
    tft.drawRect(30,126,180,15,C_CYAN);
    centeredText("VOL lang: Abbruch",212,1,C_WHITE,25);
    NerdWaveCopyProgress::drawn=true;
  }

  String label=name;
  if(label.length()>18) label=label.substring(0,16)+"..";
  if(label!=copyProgressName) {
    tft.fillRect(12,82,216,25,C_BG);
    centeredText(label,86,2,C_WHITE,18);
    copyProgressName=label;
    copyProgressPercent=-1; // Neuer Titel: Balken wieder bei 0 beginnen.
    tft.fillRect(32,128,176,11,C_BG);
  }

  int pct=total ? (int)min((uint64_t)100,(done*100)/total) : 0;
  if(pct!=copyProgressPercent) {
    // Nur den Balken und seine Prozentanzeige aktualisieren.
    // Bei einer neuen Datei wird der innere Balken oben geloescht.
    const int oldWidth=max(0,copyProgressPercent)*176/100;
    const int newWidth=pct*176/100;
    if(newWidth>oldWidth) tft.fillRect(32+oldWidth,128,newWidth-oldWidth,11,C_CYAN);
    else if(newWidth<oldWidth) tft.fillRect(32+newWidth,128,oldWidth-newWidth,11,C_BG);
    tft.fillRect(80,150,80,20,C_BG);
    centeredText(String(pct)+"%",154,2,C_WHITE,12);
    copyProgressPercent=pct;
  }

  if(files!=copyProgressFiles) {
    tft.fillRect(22,184,196,20,C_BG);
    centeredText(String(files)+" Datei(en)",187,1,C_CYAN,24);
    copyProgressFiles=files;
  }
}
