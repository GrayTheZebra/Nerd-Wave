#include "NerdWaveViewState.h"
namespace NerdWaveViewState {
int lastProgress=-1, lastVolume=-1, lastPage=-1;
String shownPlayerTitle="", shownTime="", shownState="", shownSource="";
int shownWifiStatus=-999, shownRepeat=-1, shownShuffle=-1;
void sourceChanged(int& page,bool& redraw) {
  // Einheitlich Player nach Quellenwechsel.
  page=1;
  redraw=true;
}
}
