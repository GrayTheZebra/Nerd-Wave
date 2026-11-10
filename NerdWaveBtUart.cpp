#include "NerdWaveLog.h"
#include "NerdWaveBtUart.h"

namespace NerdWaveBtUart {
HardwareSerial btLink(1);
volatile bool btConnected=false, btPlaying=false;
bool btPairing=false, btReady=false, btUiDirty=false;
bool btUartStarted=false;
static String btLine;

void btSend(const char* cmd) {
  btLink.println(cmd);
  NerdWaveLog::output.printf("[S3->BT] %s\n", cmd);
}

void btPoll() {
  while (btLink.available()) {
    char c=(char)btLink.read();
    if(c=='\n') {
      btLine.trim();
      if(btLine.length()) {
        NerdWaveLog::output.printf("[BT->S3] %s\n",btLine.c_str());
        if(btLine=="READY")btReady=true;
        else if(btLine=="CONNECTED")btConnected=true;
        else if(btLine=="DISCONNECTED") {btConnected=false;btPlaying=false;}
        else if(btLine=="PLAYING")btPlaying=true;
        else if(btLine=="IDLE")btPlaying=false;
        else if(btLine=="PAIRING:ON")btPairing=true;
        else if(btLine=="PAIRING:OFF")btPairing=false;
        btUiDirty=true;
      }
      btLine="";
    } else if(c!='\r' && btLine.length()<120) btLine+=c;
  }
}
} // namespace NerdWaveBtUart
