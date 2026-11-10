#include "NerdWaveLog.h"
#include "NerdWaveSdCard.h"

#include <SD_MMC.h>
#include "Config.h"

namespace NerdWaveSdCard {
bool mounted = false;
bool recoveryPending = false;
uint32_t retryAt = 0;

bool tryMount() {
  if(mounted) return true;
  SD_MMC.setPins(Config::SD_CLK, Config::SD_CMD, Config::SD_D0);
  for(uint8_t attempt=1;attempt<=3;attempt++) {
    NerdWaveLog::output.printf("[SD] Mount Versuch %u/3\n",(unsigned)attempt);
    if(SD_MMC.begin("/sdcard",true)) {
      mounted=true;
      recoveryPending=false;
      NerdWaveLog::output.printf("[SD] Bereit: %llu MiB\n", (unsigned long long)(SD_MMC.cardSize()/1048576ULL));
      return true;
    }
    SD_MMC.end();
    delay(500);
  }
  recoveryPending=true;
  retryAt=millis();
  NerdWaveLog::output.println("[SD] SD-Karte nicht erreichbar, automatischer Retry vorgesehen");
  return false;
}
}
