#pragma once
#include <Arduino.h>
namespace NerdWaveViewState {
extern int lastProgress;
extern int lastVolume;
extern int lastPage;
extern String shownPlayerTitle;
extern String shownTime;
extern String shownSource;
extern String shownState;
extern int shownWifiStatus;
extern int shownRepeat;
extern int shownShuffle;
void sourceChanged(int& page,bool& redraw);
}
