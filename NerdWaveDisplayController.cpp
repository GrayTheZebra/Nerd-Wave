#include "NerdWaveDisplayController.h"
#include "NerdWaveViewState.h"
#include "NerdWaveRecovery.h"
#include "NerdWaveBtUart.h"
#include "SourceMetadata.h"
#include "DisplayText.h"
#include "DisplayUI.h"
#include "NerdWaveSkins.h"
#include "NerdWaveFields.h"
#include "NerdWaveLog.h"
#include "NerdWaveUsb.h"
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <new>
#include <cstring>
using namespace DisplayUI;
#define C_BG (NerdWaveSkins::current().bg)
#define C_WHITE (NerdWaveSkins::current().text)
#define C_CYAN (NerdWaveSkins::current().accent)
#define C_YELLOW (NerdWaveSkins::current().secondary)
#define C_SELECT (NerdWaveSkins::current().selected)
#define C_CYAN_DIM (NerdWaveSkins::current().dimAccent)
#define C_YELLOW_DIM (NerdWaveSkins::current().dimSecondary)

// Nur im Dateibrowser benoetigt; Player-Titel bleibt statisch.
static bool needsScroll(const String& raw, size_t width) {
  return lcdText(raw, 120).length() > width;
}

// Nur die innere Flaeche der selektierten Browserzeile aktualisieren.
// 192x20 Pixel Canvas: verhindert Full-Refresh und schneidet Text pixelgenau ab.
static void drawBrowserMarquee(Adafruit_GC9A01A& display, const String& raw, int y, uint32_t stamp) {
  static GFXcanvas16 line(192, 20);
  if(!line.getBuffer()) return;
  const String title=lcdText(raw, 120);
  const int pixelWidth=(int)title.length()*12; // GFX 6px * Schriftgroesse 2
  const int overflow=max(0,pixelWidth-192);
  int offset=0;
  if(overflow) {
    // 40 ms pro Pixel, 600 ms Pause an beiden Endpunkten.
    const uint32_t move=(uint32_t)overflow*40U;
    const uint32_t period=move*2U+1200U;
    const uint32_t phase=stamp%period;
    if(phase<600U) offset=0;
    else if(phase<600U+move) offset=(int)((phase-600U)/40U);
    else if(phase<1200U+move) offset=overflow;
    else offset=overflow-(int)((phase-1200U-move)/40U);
  }
  line.fillScreen(C_SELECT);
  line.setTextWrap(false);
  line.setTextSize(2);
  line.setTextColor(C_YELLOW);
  line.setCursor(overflow ? -offset : (192-pixelWidth)/2, 2);
  line.print(title);
  display.drawRGBBitmap(24,y, line.getBuffer(),192,20);
}


static void updatePlayerIcons(NerdWaveDisplayController::Context& c,Adafruit_GFX& tft,bool force) {
  const auto& layout=NerdWaveSkins::layout();
  auto& nerdUseWifi=c.nerdUseWifi;
  auto& shownWifiStatus=c.shownWifiStatus;
  auto& shownRepeat=c.shownRepeat;
  auto& shownShuffle=c.shownShuffle;
  auto& repeatAll=c.repeatAll;
  auto& shuffleMode=c.shuffleMode;
  int wifi=nerdUseWifi?0:3;
  if(nerdUseWifi && WiFi.status()==WL_CONNECTED) {
    int r=WiFi.RSSI();
    wifi=(r>=-60)?3:(r>=-75)?2:1;
  }
  if(!(layout.visibleMask & 2))return;
  // Drei Statussymbole; Abstand und Zeichengröße folgen dem Skin.
  const int spacing=layout.iconSize==2?66:42;
  const int area=layout.iconSize==2?68:40;
  if(force || wifi!=shownWifiStatus) {
    if(&tft==&c.tft)tft.fillRect(layout.iconsX-spacing-area/2,layout.iconsY-area/2,area,area,C_BG);
    drawWifiIcon(layout.iconsX-spacing,layout.iconsY,wifi);
    shownWifiStatus=wifi;
  }
  if(force || (int)repeatAll!=shownRepeat) {
    if(&tft==&c.tft)tft.fillRect(layout.iconsX-area/2,layout.iconsY-area/2,area,area,C_BG);
    drawRepeatIcon(layout.iconsX,layout.iconsY,repeatAll);
    shownRepeat=repeatAll;
  }
  if(force || (int)shuffleMode!=shownShuffle) {
    if(&tft==&c.tft)tft.fillRect(layout.iconsX+spacing-area/2,layout.iconsY-area/2,area,area,C_BG);
    drawShuffleIcon(layout.iconsX+spacing,layout.iconsY,shuffleMode);
    shownShuffle=shuffleMode;
  }
}




void NerdWaveDisplayController::render(Context& c) {
  auto& tft=c.tft;
  const auto& layout=NerdWaveSkins::layout();
  auto& page=c.page;
  auto& redraw=c.redraw;
  auto& bytesPlayed=c.bytesPlayed;
  auto& audioSampleRate=c.audioSampleRate;
  auto& estimatedDurationSec=c.estimatedDurationSec;
  auto& musicSource=c.musicSource;
  auto& muted=c.muted;
  auto& volumePercent=c.volumePercent;
  auto& nerdUseWifi=c.nerdUseWifi;
  auto& repeatAll=c.repeatAll;
  auto& shuffleMode=c.shuffleMode;
  auto& playingName=c.playingName;
  auto& btConnected=c.btConnected;
  auto& btPlaying=c.btPlaying;
  auto& btPairing=c.btPairing;
  auto& playbackPaused=c.playbackPaused;
  auto& playerActive=c.playerActive;
  auto& startingTrack=c.startingTrack;
  auto& decodeDone=c.decodeDone;
  auto& browser=c.browser;
  auto& lastPage=c.lastPage;
  auto& lastProgress=c.lastProgress;
  auto& lastVolume=c.lastVolume;
  auto& shownPlayerTitle=c.shownPlayerTitle;
  auto& shownTime=c.shownTime;
  auto& shownSource=c.shownSource;
  auto& shownState=c.shownState;
  auto& shownWifiStatus=c.shownWifiStatus;
  auto& shownRepeat=c.shownRepeat;
  auto& shownShuffle=c.shownShuffle;
  auto& mediaStage=c.mediaStage;
  auto& mediaChoice=c.mediaChoice;
  auto& mediaOrigin=c.mediaOrigin;
  auto& mediaDestination=c.mediaDestination;
  auto& mediaCopyMode=c.mediaCopyMode;
  auto& mediaIndex=c.mediaIndex;
  auto& mediaSelection=c.mediaSelection;
  auto& settingsIndex=c.settingsIndex;
  auto& mediaInfo=c.mediaInfo;
  auto& sdMounted=c.sdMounted;
  auto& ledEditing=c.ledEditing;
  auto& ledEnabled=c.ledEnabled;
  auto& irCapture=c.irCapture;
  auto& resetConfirmYes=c.resetConfirmYes;
  auto& autoStartLast=c.autoStartLast;
  auto& resetConfirm=c.resetConfirm;
  auto& requestedMusicSource=c.requestedMusicSource;
  auto& networkMode=c.networkMode;
  auto& ledEffect=c.ledEffect;
  auto& ledBrightness=c.ledBrightness;
  auto& ledBaseBrightness=c.ledBaseBrightness;
  auto& ledSensitivity=c.ledSensitivity;
  auto& ledSettingsIndex=c.ledSettingsIndex;
  auto& ledCount=c.ledCount;
  auto& mediaPath=c.mediaPath;
  auto& mediaChosenName=c.mediaChosenName;
  auto& mediaResult=c.mediaResult;
  auto& mediaList=c.mediaList;
  auto& underruns=c.underruns;
  auto& i2sErrors=c.i2sErrors;
  auto& maxI2sWriteUs=c.maxI2sWriteUs;

  static uint16_t oldBg=0xFFFF,oldText=0xFFFF,oldAccent=0xFFFF,oldSecondary=0xFFFF,oldSelection=0xFFFF;
  static NerdWaveSkins::Layout oldLayout{};
  static uint32_t decorSignature=0;
  static bool lastCustomView=false;
  const auto& decor=NerdWaveSkins::decoration();uint32_t newDecorSignature=(uint32_t(decor.first)<<16)|decor.second;newDecorSignature^=uint32_t(decor.mode)<<8|uint32_t(decor.direction);for(int i=0;i<5;i++)newDecorSignature^=(uint32_t(decor.bgColor[i])<<(i%2?16:0))+(uint32_t(decor.bgEnabled[i])<<i)+(decor.text[i].length()<<i);
  bool decorChanged=decorSignature!=newDecorSignature;decorSignature=newDecorSignature;
  const bool skinChanged=c.redraw || decorChanged || (oldBg!=C_BG || oldText!=C_WHITE || oldAccent!=C_CYAN || oldSecondary!=C_YELLOW || oldSelection!=C_SELECT ||
    oldLayout.sourceY!=layout.sourceY || oldLayout.iconsY!=layout.iconsY || oldLayout.titleY!=layout.titleY || oldLayout.timeY!=layout.timeY || oldLayout.stateY!=layout.stateY || oldLayout.sourceX!=layout.sourceX || oldLayout.iconsX!=layout.iconsX || oldLayout.titleX!=layout.titleX || oldLayout.timeX!=layout.timeX || oldLayout.stateX!=layout.stateX || oldLayout.sourceFont!=layout.sourceFont || oldLayout.titleFont!=layout.titleFont || oldLayout.timeFont!=layout.timeFont || oldLayout.stateFont!=layout.stateFont || oldLayout.visibleMask!=layout.visibleMask || oldLayout.iconSize!=layout.iconSize ||
    memcmp(oldLayout.elementColor,layout.elementColor,sizeof(layout.elementColor))!=0);
  oldBg=C_BG;oldText=C_WHITE;oldAccent=C_CYAN;oldSecondary=C_YELLOW;oldSelection=C_SELECT;oldLayout=layout;
  bool pageChanged=(page!=lastPage) || skinChanged;
  String currentTitle=(page==1)?displayTitle(playingName):String();
  bool newPlayerTitle=(page==1 && currentTitle!=shownPlayerTitle);
  // Standard-Skins muessen die fest eingebaute Ansicht verwenden, auch wenn
  // noch gespeicherte Custom-Felder vorhanden sind.
  const bool customView=(strcmp(NerdWaveSkins::activeName(), "Eigener Skin") == 0) && NerdWaveFields::enabled();
  const bool rendererSwitched = (customView != lastCustomView);
  lastCustomView = customView;
  // Ein Canvas verhindert das sichtbare Loeschen vor jedem Sekundentick.
  // Bei fehlendem RAM faellt die Firmware auf den bewährten Direktpfad zurueck.
  static GFXcanvas16* skinCanvas=nullptr;
  static bool canvasAttempted=false;
  if(!canvasAttempted){
    canvasAttempted=true;
    skinCanvas=new (std::nothrow) GFXcanvas16(240,240);
    if(skinCanvas && !skinCanvas->getBuffer()) {delete skinCanvas;skinCanvas=nullptr;}
    NerdWaveLog::output.printf("[SKIN] Bildpuffer: %s\n",skinCanvas?"aktiv":"kein RAM, Direktmodus");
  }
  bool fullRefresh=rendererSwitched || pageChanged || (page==1 && newPlayerTitle) || (page!=1 && redraw);
  if(fullRefresh) {
    NerdWaveLog::output.printf("[DISPLAY-DIAG] Full-Refresh t=%lu ms page=%d prev=%d pageChanged=%u skinChanged=%u newTitle=%u redraw=%u decorChanged=%u\n",
      (unsigned long)millis(), page, lastPage, (unsigned)pageChanged,
      (unsigned)skinChanged, (unsigned)newPlayerTitle, (unsigned)redraw, (unsigned)decorChanged);
    if((page!=1 || !skinCanvas) && NerdWaveSkins::decoration().mode!=0) { for(int i=0;i<240;i++) {uint16_t c=NerdWaveSkins::backgroundAt(NerdWaveSkins::decoration().direction?i:0,NerdWaveSkins::decoration().direction?0:i);if(NerdWaveSkins::decoration().direction)tft.drawFastVLine(i,0,240,c);else tft.drawFastHLine(0,i,240,c);} }else if(page!=1 || !skinCanvas) tft.fillScreen(NerdWaveSkins::backgroundAt(0,0));
    tft.setTextWrap(false);
    lastPage=page;
    lastProgress=-1;
    lastVolume=-1;
    shownTime="";
    shownSource="";
    shownState="";
    shownPlayerTitle="";
    shownWifiStatus=-999; shownRepeat=-1; shownShuffle=-1;
  }
  redraw=false;
  if(page==1) {
    // Gemeinsames Ausgabeziel fuer Standard- und Custom-Skins.
    // Alle Elemente werden zunaechst auf demselben Canvas komponiert.
    Adafruit_GFX& tft = skinCanvas ? static_cast<Adafruit_GFX&>(*skinCanvas) : static_cast<Adafruit_GFX&>(c.tft);
    if(skinCanvas) DisplayUI::attach(tft);
    const uint32_t elapsedSec=musicSource==2?0:(uint32_t)(bytesPlayed/((uint64_t)audioSampleRate*4)) + c.playbackBaseSeconds;
    const uint32_t durationSec=musicSource==2?0:estimatedDurationSec;
    int progress=durationSec?(int)min((uint64_t)100,((uint64_t)elapsedSec*100)/durationSec):0;
    int vol=muted?0:volumePercent;
    bool freeProgress=false,freeVolume=false;
    if(customView)for(uint8_t n=0;n<NerdWaveFields::count();n++){
      const auto& field=NerdWaveFields::at(n);
      if(field.type=="gauge"){if(field.text=="progress")freeProgress=true;if(field.text=="volume")freeVolume=true;}
    }
    // Custom gauges replace the fixed arcs. Reset their previous-state cache
    // so removing a gauge restores the original arc cleanly.
    // Custom- und Standardskin zeichnen ihre Arcs auf dasselbe Ziel.
    // Standard-Elemente folgen nach dem Hintergrundaufbau weiter unten.
    if(customView) {
      // Ausschliesslich das Custom-Layout wird offscreen zusammengesetzt.
      // Die Zeichenoperationen und Ebenenreihenfolge bleiben unveraendert.
      // The field array is also the layer order (index 0: bottom).
      // Redraw the composite when dynamic values change, not on every loop.
      static String previousComposition;
      char elapsedText[20],totalText[20];
      snprintf(elapsedText,sizeof(elapsedText),"%02lu:%02lu",(unsigned long)(elapsedSec/60),(unsigned long)(elapsedSec%60));
      snprintf(totalText,sizeof(totalText),"%02lu:%02lu",(unsigned long)(durationSec/60),(unsigned long)(durationSec%60));
      String clockText=String(elapsedText)+" / "+(durationSec?String(totalText):String("--:--"));
      String statusText=musicSource==2?(btPlaying?"SPIELT":btConnected?"VERBUNDEN":"GETRENNT"):((playbackPaused || !playerActive)?"PAUSE":"PLAY");
      if(musicSource==0&&NerdWaveRecovery::nasPending)statusText=NerdWaveRecovery::nasPhaseLabel();
      // Nur sichtbare Aenderungen ausloesen: rohe RSSI-Werte schwanken auch im Stillstand.
      // Das WLAN-Symbol hat lediglich vier Zustaende. Ohne Symbol ist die RSSI
      // fuer die Bildkomposition bedeutungslos.
      int visibleWifiLevel=-1;
      for(uint8_t i=0;i<NerdWaveFields::count();i++){
        const auto& f=NerdWaveFields::at(i);
        if(f.visible && f.type=="icon" && f.text=="wifi"){
          visibleWifiLevel=nerdUseWifi?0:3;
          if(nerdUseWifi && WiFi.status()==WL_CONNECTED){
            const int r=WiFi.RSSI();
            visibleWifiLevel=r>=-60?3:r>=-75?2:1;
          }
          break;
        }
      }
      String signature=String(progress)+"|"+String(vol)+"|"+String(playingName)+"|"+String((int)musicSource)+"|"+
        statusText+"|"+String((int)repeatAll)+"|"+String((int)shuffleMode)+"|"+String((int)btConnected)+"|"+String((int)btPlaying)+"|"+
        String(visibleWifiLevel)+"|"+String(elapsedSec)+"|"+String(durationSec);
      if(fullRefresh || signature!=previousComposition) {
        previousComposition=signature;
        if(skinCanvas) DisplayUI::attach(tft);
        // Restore background first; every element is then composited bottom-to-top.
        const auto& decoration=NerdWaveSkins::decoration();
        if(decoration.mode==0)tft.fillScreen(NerdWaveSkins::backgroundAt(0,0));
        else for(int row=0;row<240;row++){
          if(decoration.direction==0)tft.drawFastHLine(0,row,240,NerdWaveSkins::backgroundAt(0,row));
          else for(int col=0;col<240;col++)tft.drawPixel(col,row,NerdWaveSkins::backgroundAt(col,row));
        }
        // Old perimeter meters stay available if a skin has no replacement gauge.
        // Draw them beneath all configurable layers.
        // Compositing zeichnet immer den kompletten Inhalt des Canvas neu.
        // Auch die Bogen-Caches muessen daher fuer diesen Durchlauf neu beginnen.
        lastProgress=-1;
        lastVolume=-1;
        if(!freeProgress)updateArc(true,progress,lastProgress,C_CYAN,C_CYAN_DIM);
        if(!freeVolume)updateArc(false,vol,lastVolume,C_YELLOW,C_YELLOW_DIM);
        for(uint8_t i=0;i<NerdWaveFields::count();i++){
          const auto& f=NerdWaveFields::at(i);
          if(!f.visible)continue;
          const int cx=(int)f.x,cy=(int)f.y;
          if(f.type=="shape"){
            const int w=(int)f.width,h=(int)f.height;
            const int x=cx-w/2,y=cy-h/2;
            const int thick=min((int)f.borderWidth,min(w,h)/2);
            if(f.text=="ellipse"){
              // Ellipse parametric geometry. Sweeps clockwise from 12 o'clock.
              // Full shapes use a horizontal-scan fill to avoid holes.
              const float rx=max(1.0f,(float)(w-1)/2.0f);
              const float ry=max(1.0f,(float)(h-1)/2.0f);
              const float tau=0.01745329252f;
              const int sweep=constrain((int)f.arcSweep,1,360);
              const int startAngle=(int)f.arcStart;
              if(f.bgEnabled && sweep==360){
                for(int dy=-(int)ry;dy<=(int)ry;dy++){
                  float t=(float)dy/ry;
                  const int dx=(int)(rx*sqrtf(max(0.0f,1.0f-t*t)));
                  tft.drawFastHLine(cx-dx,cy+dy,dx*2+1,f.background);
                }
              }else if(f.bgEnabled){
                // Filled elliptical sector: triangle fan (angles in steps <= 2 degrees).
                const int stepDegrees=2;
                for(int a=0;a<sweep;a+=stepDegrees){
                  const int b=min(sweep,a+stepDegrees);
                  const float a0=((float)startAngle+a-90.0f)*tau;
                  const float a1=((float)startAngle+b-90.0f)*tau;
                  tft.fillTriangle(cx,cy,cx+(int)roundf(cosf(a0)*rx),cy+(int)roundf(sinf(a0)*ry),
                    cx+(int)roundf(cosf(a1)*rx),cy+(int)roundf(sinf(a1)*ry),f.background);
                }
              }
              // Outline ellipse / outline sector, using short connected segments.
              if(thick){
                for(int inset=0;inset<thick;inset++){
                  const float irx=max(1.0f,rx-inset),iry=max(1.0f,ry-inset);
                  int prevx=0,prevy=0,firstx=0,firsty=0;
                  for(int a=0;a<=sweep;a+=2){
                    const int angle=min(sweep,a);
                    const float theta=((float)startAngle+angle-90.0f)*tau;
                    const int px=cx+(int)roundf(cosf(theta)*irx);
                    const int py=cy+(int)roundf(sinf(theta)*iry);
                    if(a==0){firstx=px;firsty=py;}else tft.drawLine(prevx,prevy,px,py,f.color);
                    prevx=px;prevy=py;
                  }
                  if(sweep<360){tft.drawLine(cx,cy,firstx,firsty,f.color);tft.drawLine(cx,cy,prevx,prevy,f.color);}
                }
              }
            }else{
              const int radius=min((int)f.radius,min(w,h)/2);
              if(f.bgEnabled)tft.fillRoundRect(x,y,w,h,radius,f.background);
              for(int stroke=0;stroke<thick;stroke++){
                const int sw=w-2*stroke,sh=h-2*stroke;
                if(sw<=0||sh<=0)break;
                tft.drawRoundRect(x+stroke,y+stroke,sw,sh,max(0,radius-stroke),f.color);
              }
            }
            continue;
          }
          if(f.type=="gauge"){
            const int pct=constrain(f.text=="volume"?vol:progress,0,100);
            const int w=max(12,(int)f.width),h=f.style=="ring"?w:max(2,(int)f.height);
            const int x=cx-w/2,y=cy-h/2;
            const uint16_t off=f.bgEnabled?f.background:NerdWaveSkins::backgroundAt(cx,cy);
            if(f.style=="ring"){
              const int thickness=max(1,(int)f.height/2);
              const int radius=max(1,w/2-thickness-1);
              for(int step=0;step<(int)f.arcSweep;step+=3){
                const float rad=((float)f.arcStart+(float)step-90.0f)*0.01745329252f;
                const int px=cx+(int)(cosf(rad)*radius),py=cy+(int)(sinf(rad)*radius);
                if(px>=0&&px<240&&py>=0&&py<240)tft.fillCircle(px,py,thickness,step<pct*(int)f.arcSweep/100?f.color:off);
              }
            }else if(f.style=="segments"){
              const int cells=max(1,w/12),gap=2,cellw=max(1,(w-(cells-1)*gap)/cells);
              for(int k=0;k<cells;k++)tft.fillRect(x+k*(cellw+gap),y,cellw,h,k*100<pct*cells?f.color:off);
            }else{
              tft.fillRect(x,y,w,h,off);
              if(pct>0)tft.fillRect(x,y,max(1,w*pct/100),h,f.color);
            }
            continue;
          }
          if(f.type=="icon"){
            int iconState=0;
            if(f.text=="wifi"){
              iconState=nerdUseWifi?0:3;
              if(nerdUseWifi&&WiFi.status()==WL_CONNECTED){const int r=WiFi.RSSI();iconState=r>=-60?3:r>=-75?2:1;}
            }else if(f.text=="repeat")iconState=repeatAll?1:0;
            else if(f.text=="shuffle")iconState=shuffleMode;
            
            drawCustomIcon(f.text,cx,cy,iconState,f.color,f.size);
            continue;
          }
          if(f.type=="text"){
            String v=f.text;
            const int titleRoom=max(1,(int)f.maxTextWidth/(6*(int)max((uint8_t)1,f.size)));
            v.replace("%titel%",displayTitle(playingName));v.replace("%quelle%",sourceName(musicSource));
            v.replace("%quelle_label%",String("QUELLE: ")+sourceName(musicSource));
            v.replace("%zeit%",musicSource==2?(btConnected?"Handy verbunden":"Bluetooth bereit"):clockText);
            v.replace("%zeit_aktuell%",elapsedText);v.replace("%zeit_gesamt%",durationSec?String(totalText):String("--:--"));
            v.replace("%status%",statusText);v.replace("%lautstaerke%",String(volumePercent));
            v.replace("%repeat%",repeatAll?"An":"Aus");v.replace("%shuffle%",String(shuffleMode));
            v.replace("%netzwerk%",nerdUseWifi?"WLAN":"LAN");
            String shown=lcdText(v,120);
            const int limit=max(1,(int)f.maxTextWidth/(6*(int)max((uint8_t)1,f.size)));
            if((int)shown.length()>limit){if(limit<=2)shown=shown.substring(0,limit);else shown=shown.substring(0,limit-2)+"..";}
            int16_t bx=0,by=0;uint16_t bw=0,bh=0;
            tft.setTextSize(f.size);tft.getTextBounds(shown,0,0,&bx,&by,&bw,&bh);
            const int x=cx-(int)bw/2,y=cy-(int)bh/2;
            
            tft.setTextColor(f.color);tft.setCursor(x-bx,y-by);tft.print(shown);
          }
        }
        if(skinCanvas) {
          DisplayUI::attach(c.tft);
          c.tft.drawRGBBitmap(0,0,skinCanvas->getBuffer(),240,240);
        }
      }
      shownPlayerTitle=currentTitle;
      return;
    }

    // Standardskin: kein partielles Loeschen mit C_BG mehr auf dem physischen
    // Display. Auch Text, Icons und Arcs werden in EINEM Durchlauf aufgebaut.
    static String previousStandardComposition;
    int wifiLevel=nerdUseWifi ? 0 : 3;
    if(nerdUseWifi && WiFi.status()==WL_CONNECTED) {
      const int r=WiFi.RSSI(); wifiLevel=r>=-60?3:r>=-75?2:1;
    }
    const String standardSignature=String(playingName)+"|"+String(elapsedSec)+"|"+String(durationSec)+"|"+
      String(progress)+"|"+String(vol)+"|"+String((int)musicSource)+"|"+
      String((int)playbackPaused)+"|"+String((int)playerActive)+"|"+String((int)startingTrack)+"|"+
      String((int)decodeDone)+"|"+String((int)btConnected)+"|"+String((int)btPlaying)+"|"+
      String((int)btPairing)+"|"+String((int)repeatAll)+"|"+String((int)shuffleMode)+"|"+String(wifiLevel)+"|"+
      String((int)NerdWaveRecovery::nasPending)+"|"+String(NerdWaveRecovery::nasPhaseLabel());
    if(skinCanvas && !fullRefresh && standardSignature==previousStandardComposition){
      DisplayUI::attach(c.tft);
      return;
    }
    previousStandardComposition=standardSignature;
    if(skinCanvas){
      // Der Standardskin besitzt einen eigenen einfarbigen Hintergrund.
      // Damit koennen Freiflaechen keine fremden Verlaeufe ueberdecken.
      tft.fillScreen(C_BG);
      lastProgress=-1; lastVolume=-1;
      shownSource=""; shownTime=""; shownState=""; shownPlayerTitle="";
      shownWifiStatus=-999; shownRepeat=-1; shownShuffle=-1;
    }
    updateArc(true,progress,lastProgress,C_CYAN,C_CYAN_DIM);
    updateArc(false,vol,lastVolume,C_YELLOW,C_YELLOW_DIM);
    if(layout.visibleMask & 2)updatePlayerIcons(c,tft,true);
    // Eigene kleine Quellenzeile oberhalb der Statussymbole (keine Audioaenderung).
    auto renderTemplate=[&](int idx,const String& fallback,const String& timeValue,const String& stateValue)->String {
      String v=NerdWaveSkins::decoration().text[idx];if(!v.length())return fallback;
      v.replace("%titel%",displayTitle(playingName));v.replace("%quelle%",sourceName(musicSource));
      v.replace("%zeit%",timeValue);v.replace("%status%",stateValue);
      v.replace("%lautstaerke%",String(volumePercent));
      v.replace("%quelle_label%",String("QUELLE: ")+sourceName(musicSource));
      if(v.length()>128)v.remove(128);return v;
    };
    String sourceLabel=renderTemplate(0,String("QUELLE: ")+sourceName(musicSource),String(),String());
    if((layout.visibleMask & 1) && (sourceLabel!=shownSource || fullRefresh)) {
      if(!skinCanvas)tft.fillRect(0,max(0,(int)layout.sourceY-3),240,min(240-(int)layout.sourceY+3,34),C_BG);
      if(!skinCanvas && decor.bgEnabled[0]){int f=layout.sourceFont;int w=min(240,(int)sourceLabel.length()*6*f+2*decor.bgPadding[0]);tft.fillRect(max(0,(int)layout.sourceX-w/2),layout.sourceY,w,min(240-(int)layout.sourceY,8*f+2*decor.bgPadding[0]),decor.bgColor[0]);}
      centeredTextAt(sourceLabel,layout.sourceX,layout.sourceY,layout.sourceFont,layout.elementColor[0],max(1,40/(int)layout.sourceFont));
      shownSource=sourceLabel;
    }

    if((fullRefresh || skinCanvas) && (layout.visibleMask & 4)) {
      const String titleText=renderTemplate(2,playingName,String(),String());
      playerTitleAt(titleText,layout.titleX,layout.titleY,layout.titleFont);
      shownPlayerTitle=currentTitle;
    }
    char timeText[48];
    if(durationSec) {
      snprintf(timeText,sizeof(timeText),"%02lu:%02lu / %02lu:%02lu",
        (unsigned long)(elapsedSec/60),(unsigned long)(elapsedSec%60),
        (unsigned long)(durationSec/60),(unsigned long)(durationSec%60));
    } else {
      snprintf(timeText,sizeof(timeText),"%02lu:%02lu / --:--",
        (unsigned long)(elapsedSec/60),(unsigned long)(elapsedSec%60));
    }
    String timeNow=musicSource==2 ? (btConnected?"Handy verbunden":"Bluetooth bereit") : String(timeText);
    timeNow=renderTemplate(3,timeNow,timeNow,String());
    if((layout.visibleMask & 8) && timeNow!=shownTime) {
      if(!skinCanvas)tft.fillRect(0,max(0,(int)layout.timeY-2),240,min(240-(int)layout.timeY+2,34),C_BG);
      if(!skinCanvas && decor.bgEnabled[3]){int f=layout.timeFont;int w=min(240,(int)timeNow.length()*6*f+2*decor.bgPadding[3]);tft.fillRect(max(0,(int)layout.timeX-w/2),layout.timeY,w,min(240-(int)layout.timeY,8*f+2*decor.bgPadding[3]),decor.bgColor[3]);}
      centeredTextAt(timeNow,layout.timeX,layout.timeY,layout.timeFont,layout.elementColor[3],max(1,40/(int)layout.timeFont));
      shownTime=timeNow;
    }
    String state=musicSource==2?(btPlaying?"SPIELT":btConnected?"VERBUNDEN":btPairing?"KOPPELN":"GETRENNT"):((playbackPaused || !playerActive)?"PAUSE":"PLAY");
    if(musicSource==0 && NerdWaveRecovery::nasPending) {
      uint32_t remaining=NerdWaveRecovery::secondsToNextNasRetry(NerdWaveRecovery::nasRetryAt);
      state=String(NerdWaveRecovery::nasPhaseLabel());
      if(remaining) state += String(" ")+String(remaining)+"s";
    }
    state=renderTemplate(4,state,timeNow,state);
    if((layout.visibleMask & 16) && state!=shownState) {
      // Recovery-Information benoetigt mehr Breite als der normale Player-Status.
      if(!skinCanvas)tft.fillRect(0,max(0,(int)layout.stateY-2),240,min(240-(int)layout.stateY+2,34),C_BG);
      if(!skinCanvas && decor.bgEnabled[4]){int f=layout.stateFont;int w=min(240,(int)state.length()*6*f+2*decor.bgPadding[4]);tft.fillRect(max(0,(int)layout.stateX-w/2),layout.stateY,w,min(240-(int)layout.stateY,8*f+2*decor.bgPadding[4]),decor.bgColor[4]);}
      centeredTextAt(state,layout.stateX,layout.stateY,layout.stateFont,layout.elementColor[4],max(1,40/(int)layout.stateFont));
      shownState=state;
    }
    if(skinCanvas){
      DisplayUI::attach(c.tft);
      c.tft.drawRGBBitmap(0,0,skinCanvas->getBuffer(),240,240);
    }
    return;
  }
  // Der Browser wird ausschliesslich bei Navigation/Statusaenderung gezeichnet.
  // Kein 750-ms-Refresh, kein periodisches fillRect(): verhindert Flackern.
  if(page==0) {
    const MusicItem* currentBrowserItem=browser.visibleItem(browser.index());
    const bool browserScroll=currentBrowserItem &&
      needsScroll(String(currentBrowserItem->directory?"> ":"  ")+currentBrowserItem->name,16);
    static uint32_t lastBrowserScrollTick=UINT32_MAX;
    const uint32_t browserTick=millis()/40U;
    const bool scrollChanged=browserScroll && browserTick!=lastBrowserScrollTick;
    lastBrowserScrollTick=browserTick;
    if(!fullRefresh) {
      if(scrollChanged && browser.total()>0 && !browser.error().length() &&
          (musicSource!=1 || NerdWaveUsb::usbDisk.mounted()) &&
          (musicSource!=3 || sdMounted)) {
        // Kein Loeschen des gesamten Bildes, nur selektierte Zeile im Canvas.
        const String activeName=String(currentBrowserItem->directory?"> ":"  ")+currentBrowserItem->name;
        const int first=min(max(0,browser.index()-1),max(0,browser.total()-3));
        const int y=91+(browser.index()-first)*35;
        drawBrowserMarquee(c.tft,activeName,y,millis());
      }
      return;
    }
    centeredText(musicSource==2?"BLUETOOTH":musicSource==3?"SD BROWSER":musicSource==1?"USB BROWSER":"NAS BROWSER",43,2,C_CYAN,12);
    centeredText(browser.path(),66,1,C_YELLOW,27);
    if(musicSource==1 && !NerdWaveUsb::usbDisk.mounted()) {
      centeredText("Kein USB Stick",108,2,0xF800,20);
      centeredText("Stick anschliessen",145,1,C_WHITE,25);
    } else if(musicSource==3 && !sdMounted) {
      centeredText("Keine SD Karte",108,2,0xF800,20);
      centeredText("Karte einsetzen",145,1,C_WHITE,25);
    } else if(musicSource==2) {
      centeredText(btConnected?"VERBUNDEN":"NICHT VERBUNDEN",105,2,C_CYAN,15);
      centeredText("Koppeln: Einstellungen",145,1,C_WHITE,28);
    } else if(browser.error().length()) {
      if(musicSource==0 && NerdWaveRecovery::nasPending) {
        centeredText("NAS OFFLINE",100,2,0xF800,14);
        centeredText(NerdWaveRecovery::nasPhaseLabel(),130,1,C_YELLOW,28);
        centeredText(String("Neuer Versuch in ")+String(NerdWaveRecovery::secondsToNextNasRetry(NerdWaveRecovery::nasRetryAt))+"s",151,1,C_WHITE,30);
      } else {
        centeredText(musicSource==3?"SD FEHLT":musicSource==1?"USB FEHLT":"NAS OFFLINE",105,2,0xF800,13);
        centeredText("NAV: erneut versuchen",144,1,C_WHITE,28);
      }
    } else {
      // Drei gut lesbare Zeilen. Die markierte Zeile ist mittig.
      constexpr int VISIBLE=3;
      int first=max(0,browser.index()-1);
      first=min(first,max(0,browser.total()-VISIBLE));
      for(int i=0;i<VISIBLE && first+i<browser.total();i++) {
        int idx=first+i, y=91+i*35;
        const MusicItem* item=browser.visibleItem(idx);
        if(!item) break;
        bool active=(idx==browser.index());
        if(active) tft.fillRoundRect(20,y-5,200,29,5,C_SELECT);
        // Standardschrift 2: 12 Pixel Breite je Zeichen. Max. 16 Zeichen.
        // Ein Verzeichnis ist durch das vorangestellte '>' erkennbar.
        String name=String(item->directory?"> ":"  ")+lcdText(item->name,80);
        if(active && name.length()>16) {
          drawBrowserMarquee(c.tft,name,y,millis());
        } else {
          if(name.length()>16) name=name.substring(0,14)+"..";
          centeredText(name,y,2,active?C_YELLOW:C_WHITE,16);
        }
      }
    }
    centeredText(String(browser.total()?browser.index()+1:0)+"/"+String(browser.total())+"  VOL "+String(muted?0:volumePercent)+"%",205,1,C_CYAN,30);
    return;
  }
  // Einstellungen/Diagnose: nur bei Ansichtwechsel oder Bedienung neu zeichnen.
  if(!fullRefresh) return;
  if(page==6 || page==4 || page==5 || page==3 || page==2) {
    NerdWaveMenus::View view{tft, page, mediaStage, mediaChoice, mediaOrigin, mediaDestination, mediaCopyMode, mediaIndex, mediaSelection, settingsIndex,
      mediaInfo, sdMounted, ledEditing, ledEnabled, irCapture, resetConfirmYes, btPairing, btConnected, repeatAll, autoStartLast, nerdUseWifi,
      resetConfirm, musicSource, requestedMusicSource, shuffleMode, networkMode, ledEffect, ledBrightness, ledBaseBrightness, ledSensitivity, ledSettingsIndex,
      ledCount, mediaPath, mediaChosenName, mediaResult, mediaList, c.ringFill, underruns, i2sErrors, maxI2sWriteUs, c.copyListAt};
    NerdWaveMenus::render(view);
  }
}

