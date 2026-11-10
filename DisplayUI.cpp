#include "DisplayUI.h"
#include "DisplayText.h"
#include "NerdWaveSkins.h"
#include <math.h>
namespace DisplayUI {
static Adafruit_GFX* screen=nullptr;
void attach(Adafruit_GFX& display){screen=&display;}
#define tft (*screen)
#define C_BG (NerdWaveSkins::current().bg)
#define C_WHITE (NerdWaveSkins::current().text)
#define C_CYAN (NerdWaveSkins::current().accent)
#define C_YELLOW (NerdWaveSkins::current().secondary)
#define C_SELECT (NerdWaveSkins::current().selected)
#define C_CYAN_DIM (NerdWaveSkins::current().dimAccent)
#define C_YELLOW_DIM (NerdWaveSkins::current().dimSecondary)

void centeredText(const String& text, int y, uint8_t size, uint16_t color, int maxChars) {
  String shown=lcdText(text,(size_t)maxChars);
  tft.setTextSize(size);
  tft.setTextColor(color);
  int width=(int)shown.length()*6*size;
  tft.setCursor(max(0,(240-width)/2),y);
  tft.print(shown);
}



void centeredTextAt(const String& text,int centerX,int y,uint8_t size,uint16_t color,int maxChars) {
 String shown=lcdText(text,(size_t)maxChars);tft.setTextSize(size);tft.setTextColor(color);
 const int fit=max(1,240/(6*(int)max((uint8_t)1,size)));if((int)shown.length()>fit){shown=shown.substring(0,max(0,fit-2))+"..";}
 int width=(int)shown.length()*6*size;
 // Die Zeile bleibt innerhalb der sichtbaren horizontalen Displaygrenzen.
 tft.setCursor(max(0,min(240-width,centerX-width/2)),y);tft.print(shown);
}
void playerTitleAt(const String& name,int centerX,int centerY,uint8_t fontSize) {
 String clean=lcdText(displayTitle(name),100);
 uint8_t size=fontSize?fontSize:3;
 int limit=192/(6*size);
 if((int)clean.length()>limit) clean=clean.substring(0,max(0,limit-2))+"..";
 int16_t bx,by;uint16_t bw,bh;tft.setTextSize(size);tft.getTextBounds(clean,0,0,&bx,&by,&bw,&bh);
 tft.setTextColor(NerdWaveSkins::layout().elementColor[2]);tft.setCursor(max(0,min(240-(int)bw,centerX-(int)bw/2-bx)),centerY-(int)bh/2-by);tft.print(clean);
}

// ============== 06_DisplayHelpers ==============
// Nerd-Wave v0.5.77: extracted without behavioral changes
// Titelaufbereitung in DisplayText.cpp ausgelagert.

void centeredPlayerTitle(const String& name,int centerY) {
  String clean=lcdText(displayTitle(name),100);
  // GFX Standardfont: 6x8 px pro Zeichen; kurze Titel groesser darstellen.
  uint8_t size=(clean.length()<=10)?3:2;
  int limit=192/(6*size);
  if((int)clean.length()>limit) {
    clean=clean.substring(0,max(0,limit-2))+"..";
  }
  int16_t bx,by;
  uint16_t bw,bh;
  tft.setTextSize(size);
  tft.getTextBounds(clean,0,0,&bx,&by,&bw,&bh);
  // Titel geometrisch auf der Displaymitte (y=120) ausrichten.
  int cursorX=120-(int)bw/2-bx;
  int cursorY=centerY-(int)bh/2-by;
  tft.setTextColor(C_WHITE);
  tft.setCursor(cursorX,cursorY);
  tft.print(clean);
}

// Segment 0 beginnt links. Oben verlaeuft der Bogen ueber den oberen Rand,
// unten ueber den unteren Rand. Die freien 8 Grad je Ende ergeben 16 Grad Abstand.
constexpr int ARC_SEGMENTS=60;
constexpr int ARC_RADIUS_IN=107;
constexpr int ARC_RADIUS_OUT=112;
static void drawArcSegment(bool upper, int segment, uint16_t ink) {
  const float start=upper?188.0f:172.0f;
  const float sweep=upper?164.0f:-164.0f;
  float a0=(start+sweep*segment/ARC_SEGMENTS)*PI/180.0f;
  float a1=(start+sweep*(segment+1)/ARC_SEGMENTS)*PI/180.0f;
  for(int r=ARC_RADIUS_IN;r<=ARC_RADIUS_OUT;r++) {
    int x0=120+lroundf(cosf(a0)*r);
    int y0=120+lroundf(sinf(a0)*r);
    int x1=120+lroundf(cosf(a1)*r);
    int y1=120+lroundf(sinf(a1)*r);
    tft.drawLine(x0,y0,x1,y1,ink);
  }
}

void updateArc(bool upper, int percent, int& previous,
                      uint16_t active, uint16_t inactive) {
  percent=constrain(percent,0,100);
  int filled=(percent*ARC_SEGMENTS+50)/100;
  int priorFilled=previous<0?-1:(previous*ARC_SEGMENTS+50)/100;
  if(priorFilled==filled && previous>=0) return;
  for(int i=0;i<ARC_SEGMENTS;i++) {
    bool wasFilled=i<priorFilled;
    bool isFilled=i<filled;
    if(previous>=0 && wasFilled==isFilled) continue;
    drawArcSegment(upper,i,isFilled?active:inactive);
  }
  previous=percent;
}

// Drei Statussymbole im oberen Mittelbereich: WLAN, Repeat, Shuffle.
// Normale Shuffle-Pfeile = nur aktueller Ordner;
// Shuffle mit Ordnersymbol = inklusive Unterordner.
// Keine Bilddateien, alles mit GFX-Primitiven.
static int iconOriginX=0,iconOriginY=0,iconScale=1;
static int sx(int x){return iconOriginX+(x-iconOriginX)*iconScale;}
static int sy(int y){return iconOriginY+(y-iconOriginY)*iconScale;}
static void iconDrawLine(int x0,int y0,int x1,int y1,uint16_t color){tft.drawLine(sx(x0),sy(y0),sx(x1),sy(y1),color);}
static void iconLine(int x0,int y0,int x1,int y1,uint16_t c,int weight=2) {
  for(int i=0;i<weight*iconScale;i++) tft.drawLine(sx(x0),sy(y0)+i,sx(x1),sy(y1)+i,c);
}
static void arrowHead(int x,int y,int dx,int dy,uint16_t c) {
  // Spitze bei (x,y), Richtung (dx,dy), wobei dx/dy in {-1,0,1}.
  int px=-dy, py=dx;
  iconDrawLine(x,y,x-dx*5+px*3,y-dy*5+py*3,c);
  iconDrawLine(x,y,x-dx*5-px*3,y-dy*5-py*3,c);
}
static bool iconOverride=false;static uint16_t iconOverrideColor=0;static uint8_t iconOverrideSize=1;
void drawWifiIcon(int cx,int cy,int strength) {
  iconOriginX=cx; iconOriginY=cy;iconScale=iconOverride?iconOverrideSize:NerdWaveSkins::layout().iconSize;
  uint16_t dim=iconOverride?uint16_t(iconOverrideColor>>1):C_CYAN_DIM, on=iconOverride?iconOverrideColor:NerdWaveSkins::layout().elementColor[1];
  // Funkboegen ohne float/trig (12, 9, 6 Pixel Durchmesser).
  for(int level=0;level<3;level++) {
    int r=13-level*4;
    uint16_t col=(strength>=(level+1))?on:dim;
    // Winkel 210..330 Grad; Reihenfolge ergibt geschichtete WLAN-Boegen.
    for(int a=210;a<=330;a+=6) {
      float a0=a*PI/180.0f, a1=(a+6)*PI/180.0f;
      iconDrawLine(cx+lroundf(cosf(a0)*r),cy+lroundf(sinf(a0)*r),
                   cx+lroundf(cosf(a1)*r),cy+lroundf(sinf(a1)*r),col);
    }
  }
  tft.fillCircle(sx(cx),sy(cy+2),2*iconScale,strength>0?on:dim);
}
void drawRepeatIcon(int cx,int cy,bool enabled) {
  iconOriginX=cx; iconOriginY=cy;iconScale=iconOverride?iconOverrideSize:NerdWaveSkins::layout().iconSize;
  uint16_t c=enabled?(iconOverride?iconOverrideColor:NerdWaveSkins::layout().elementColor[1]):(iconOverride?uint16_t(iconOverrideColor>>1):C_CYAN_DIM);
  int x=cx-13,y=cy-9;
  // Zwei gegengerichtete Pfeile in einem breiten Wiederholungsrahmen.
  iconLine(x+2,y+2,x+22,y+2,c);
  arrowHead(x+22,y+2,1,0,c);
  iconLine(x+22,y+14,x+2,y+14,c);
  arrowHead(x+2,y+14,-1,0,c);
  iconLine(x+2,y+2,x+2,y+7,c,1);
  iconLine(x+22,y+9,x+22,y+14,c,1);
}
void drawShuffleIcon(int cx,int cy,int mode) {
  iconOriginX=cx; iconOriginY=cy;iconScale=iconOverride?iconOverrideSize:NerdWaveSkins::layout().iconSize;
  uint16_t c=mode?(iconOverride?iconOverrideColor:NerdWaveSkins::layout().elementColor[1]):(iconOverride?uint16_t(iconOverrideColor>>1):C_YELLOW_DIM);
  // Gekreuzte Pfeile = Shuffle im aktuellen Ordner.
  int x=cx-13,y=cy-9;
  iconLine(x,y+3,x+7,y+3,c);
  iconLine(x+7,y+3,x+18,y+15,c);
  iconLine(x+18,y+15,x+24,y+15,c);
  arrowHead(x+24,y+15,1,0,c);
  iconLine(x,y+15,x+7,y+15,c);
  iconLine(x+7,y+15,x+18,y+3,c);
  iconLine(x+18,y+3,x+24,y+3,c);
  arrowHead(x+24,y+3,1,0,c);
  if(mode==2) {
    // Zweites Icon fuer Shuffle ueber Unterordner: kleines Ordnersymbol.
    // Ein schwarzer Bereich verhindert Ueberschneidung mit den Pfeilen.
    int fx=cx+4,fy=cy+4;
    tft.fillRect(sx(fx-2),sy(fy-2),19*iconScale,16*iconScale,C_BG);
    tft.drawRect(sx(fx),sy(fy+2),15*iconScale,10*iconScale,(iconOverride?iconOverrideColor:NerdWaveSkins::layout().elementColor[1]));
    iconDrawLine(fx,fy,fx+6,fy,(iconOverride?iconOverrideColor:NerdWaveSkins::layout().elementColor[1]));
    iconDrawLine(fx+6,fy,fx+8,fy+2,(iconOverride?iconOverrideColor:NerdWaveSkins::layout().elementColor[1]));
  }
}

void drawCustomIcon(const String& icon,int cx,int cy,int state,uint16_t color,uint8_t size){
 iconOverride=true;iconOverrideColor=color;iconOverrideSize=size;
 if(icon=="wifi")drawWifiIcon(cx,cy,state);else if(icon=="repeat")drawRepeatIcon(cx,cy,state!=0);else if(icon=="shuffle")drawShuffleIcon(cx,cy,state);
 iconOverride=false;
}
#undef tft
}
