#include "NerdWaveSources.h"
#include "SourceMetadata.h"

namespace NerdWaveSources {
  static uint8_t mask = 0x0F;
  uint8_t enabledMask(){return mask;}
  bool enabled(uint8_t source){return source<4 && (mask & (1u<<source));}
  uint8_t firstEnabled(){for(uint8_t i=0;i<4;i++)if(enabled(i))return i;return 0;}
  bool configure(Preferences& prefs,uint8_t next){next &= 0x0F;if(!next)return false;mask=next;prefs.putUChar("srcMask",mask);return true;}
  uint8_t current = 0;
  uint8_t requested = 0;
  bool switchPending = false;

  void load(Preferences& prefs) {
    mask = prefs.getUChar("srcMask",0x0F)&0x0F;
    if(!mask)mask=0x01; // mindestens eine Quelle aktiv
    current = min((uint8_t)3, prefs.getUChar("musicSrc", 0));
    if(!enabled(current)) {current=firstEnabled();prefs.putUChar("musicSrc",current);}
    requested = current;
    switchPending = false;
  }

  void loadTrack(Preferences& prefs, String& path, String& name) {
    path = prefs.getString(trackPathKey(current), "");
    name = prefs.getString(trackNameKey(current), "");
  }

  void queueNext() {
    for(uint8_t i=0;i<4;i++){
      const uint8_t order[4]={0,1,3,2};
      uint8_t pos=0;
      while(pos<4 && order[pos]!=requested) ++pos;
      requested=order[(pos+1)%4];
      if(enabled(requested))break;
    }
    switchPending = (requested != current);
  }

  void clearPending() { switchPending = false; }

  void cancelPending() {
    requested = current;
    switchPending = false;
  }

  void commit(uint8_t target, Preferences& prefs, String& path, String& name) {
    if(!enabled(target))return;
    current = target;
    prefs.putUChar("musicSrc", current);
    loadTrack(prefs, path, name);
  }
}
