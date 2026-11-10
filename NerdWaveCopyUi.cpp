#include "NerdWaveCopyUi.h"
#include "NerdWaveMenus.h"
#include "CopyManager.h"
namespace NerdWaveCopyUi {
namespace {
  constexpr uint32_t COPY_IDLE_MS=5000;
  uint32_t lastActivityMs=0;
}
void markActivity() { lastActivityMs=millis(); }
bool idleExpired() { return lastActivityMs && (uint32_t)(millis()-lastActivityMs)>=COPY_IDLE_MS; }
void returnToPlayer(int& page,int& mediaStage,bool& mediaInfo,bool& mediaNotice,bool& redraw,uint32_t& browserActivityAt) {
  page=1;mediaStage=0;mediaInfo=false;mediaNotice=false;
  browserActivityAt=millis();redraw=true;
}
void handle(Context& c,int vs,ButtonEvent volButton) {
    // Volumen-Encoder bedient die komplette Kopierverwaltung.
    if(c.mediaStage==1) {
      if(vs) {c.mediaChoice=NerdWaveMenus::wrapIndex(c.mediaChoice,vs,4);c.redraw=true;}
      if(volButton==ButtonEvent::Long || (volButton==ButtonEvent::Short && c.mediaChoice==3)) {c.mediaStage=0;c.redraw=true;}
      else if(volButton==ButtonEvent::Short) {
        c.mediaOrigin=c.mediaChoice==2?3:c.mediaChoice;
        if(c.mediaOrigin==3 && !c.sdMounted)c.mountSd();
        if(c.mediaOrigin==1 && !c.usbMounted())c.mountUsb();
        c.mediaPath="/";
        if(c.loadFolder("/"))c.mediaStage=2;else {c.mediaStage=4;}
        c.redraw=true;
      }
    } else if(c.mediaStage==2) {
      if(vs && c.mediaList.total) {c.mediaSelection=NerdWaveMenus::wrapIndex(c.mediaSelection,vs,c.mediaList.total);c.redraw=true;}
      if(volButton==ButtonEvent::Long) {
        if(c.mediaCopyMode==1 && c.mediaPath!="/") {
          c.mediaChosenPath=c.mediaPath;c.mediaChosenName=c.mediaPath.substring(c.mediaPath.lastIndexOf('/')+1);
          c.mediaDestination=c.mediaOrigin==3?1:3;c.mediaStage=3;
        } else if(c.mediaPath=="/")c.mediaStage=1;
        else {String parent=c.mediaPath.substring(0,c.mediaPath.lastIndexOf('/'));if(parent.isEmpty())parent="/";c.loadFolder(parent);}
        c.redraw=true;
      } else if(volButton==ButtonEvent::Short) {
        MusicItem chosen;
        if(c.listAt(c.mediaSelection,chosen)) {
          if(chosen.directory) {
            if(!c.loadFolder(chosen.path))c.mediaStage=4;
          } else if(c.mediaCopyMode==0) {
            c.mediaChosenPath=chosen.path;c.mediaChosenName=chosen.name;
            c.mediaDestination=c.mediaOrigin==3?1:3;c.mediaStage=3;
          }
          c.redraw=true;
        }
      }
    } else if(c.mediaStage==3) {
      if(vs) {c.mediaDestination=c.mediaDestination==3?1:3;c.redraw=true;}
      if(volButton==ButtonEvent::Long) {c.mediaStage=c.mediaCopyMode==2?0:2;c.redraw=true;}
      else if(volButton==ButtonEvent::Short) c.execute();
    } else if(c.mediaStage==4) {
      if(volButton==ButtonEvent::Short || volButton==ButtonEvent::Long) {c.mediaStage=0;c.redraw=true;}
    } else if(c.mediaInfo) {
      if(volButton==ButtonEvent::Short || volButton==ButtonEvent::Long) {c.mediaInfo=false;c.redraw=true;}
    } else {
      if(vs) {c.mediaIndex=NerdWaveMenus::wrapIndex(c.mediaIndex,vs,5);c.redraw=true;}
      if(volButton==ButtonEvent::Long || (volButton==ButtonEvent::Short && c.mediaIndex==4)) {
        c.page=c.mediaReturnPage;c.browserActivityAt=millis();c.redraw=true;
      } else if(volButton==ButtonEvent::Short && c.mediaIndex==0) {
        // Der zuletzt am Player gestartete Titel gehoert zur aktuellen Quelle.
        // BT besitzt keine lokale MP3-Datei, daher dort kein Kopieren.
        c.mediaCopyMode=2;
        c.mediaOrigin=c.musicSource;
        c.mediaChosenPath=c.selectedMp3Path.length()?c.selectedMp3Path:c.lastTrackPath;
        c.mediaChosenName=c.mediaChosenPath.substring(c.mediaChosenPath.lastIndexOf('/')+1);
        if(c.mediaOrigin==2 || !c.mediaChosenPath.length() || !CopyManager::copySafePath(c.mediaChosenPath)) {
          c.mediaResult=c.mediaOrigin==2?"BT ohne Datei":"Kein Song gewaehlt";
          c.mediaStage=4;
        } else {
          c.mediaDestination=c.mediaOrigin==3?1:3;
          c.mediaStage=3;
        }
        c.redraw=true;
      } else if(volButton==ButtonEvent::Short && (c.mediaIndex==1 || c.mediaIndex==2)) {
        c.mediaCopyMode=c.mediaIndex-1;c.mediaChoice=0;c.mediaStage=1;c.redraw=true;
      } else if(volButton==ButtonEvent::Short && c.mediaIndex==3) {
        c.mediaInfo=true;c.redraw=true;
      }
    }
}
}
