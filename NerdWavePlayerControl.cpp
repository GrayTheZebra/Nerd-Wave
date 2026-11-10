#include "NerdWaveLog.h"
#include "NerdWavePlayerControl.h"
#include "SourceMetadata.h"

namespace NerdWavePlayerControl {
volatile bool pauseRequested = false;
bool repeatAll = false;
uint8_t shuffleMode = 0;

void togglePause() { pauseRequested = !pauseRequested; }
void toggleRepeat() { repeatAll = !repeatAll; }
void cycleShuffle() { shuffleMode = (shuffleMode + 1) % 3; }

bool changeTrack(NavigationContext& ctx, int direction, bool restartCurrent) {
  if(restartCurrent) return ctx.startTrack(ctx.selectedMp3Path,ctx.playingName,false);
  if(shuffleMode != 0) {
    String path,name;
    if(!ctx.pickRandomTrack(path,name)) return false;
    return ctx.startTrack(path,name,true);
  }
  // After changing sources the ctx.browser may still show root, although the
  // resumed song lives in a subfolder. Synchronize before navigating.
  if(ctx.selectedMp3Path.length() &&
     (ctx.browser.path()!=ctx.parentFolder(ctx.selectedMp3Path) ||
      !ctx.browser.current() || ctx.browser.current()->path!=ctx.selectedMp3Path)) {
    if(!ctx.browser.focusTrack(ctx.selectedMp3Path)) {
      NerdWaveLog::output.printf("[NAV] Titelkontext nicht wiederherstellbar: %s\n", ctx.selectedMp3Path.c_str());
      return false;
    }
  }
  const int originalIndex = ctx.browser.index();
  const int total = ctx.browser.total();
  if (direction == 0 || total <= 0) return false;
  for (int i = originalIndex + direction; i >= 0 && i < total; i += direction) {
    if (!ctx.browser.move(direction)) break;
    const MusicItem* item = ctx.browser.current();
    if (!item) break;
    String lowerName = item->name;
    lowerName.toLowerCase();
    if (!item->directory && lowerName.endsWith(".mp3")) {
      String path = item->path, name = item->name;
      if (ctx.startTrack(path, name,false)) return true;
      break;
    }
  }
  // Am Ordnerende optional wieder zum Anfang/Ende springen.
  if(repeatAll && !ctx.decodeError && total>0) {
    int edge = direction>0 ? 0 : total-1;
    ctx.browser.move(edge-ctx.browser.index());
    for(int i=edge;i>=0 && i<total;i+=direction) {
      const MusicItem* it=ctx.browser.current();
      if(it && ctx.isMp3File(*it)) {
        String path=it->path,name=it->name;
        if(ctx.startTrack(path,name,false)) return true;
        break;
      }
      if(i+direction>=0 && i+direction<total) ctx.browser.move(direction);
    }
  }
  // Bei Fehler oder am Ordnerende Auswahl zuruecksetzen.
  while (ctx.browser.index() != originalIndex) {
    int step = ctx.browser.index() < originalIndex ? 1 : -1;
    if (!ctx.browser.move(step)) break;
  }
  NerdWaveLog::output.println("[NAV] Kein weiterer MP3-Titel in diesem Ordner");
  return false;
}
}
