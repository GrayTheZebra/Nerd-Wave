#include "NerdWaveNasConfig.h"
namespace NerdWaveNasConfig {
static String currentHost="0.0.0.0";
static uint16_t currentPort=8080;
void load(Preferences& prefs){
  String candidate=prefs.getString("nasHost", "0.0.0.0");
  IPAddress ip;
  if(ip.fromString(candidate)) currentHost=candidate;
  uint32_t p=prefs.getUInt("nasPort",8080);
  if(p>=1 && p<=65535) currentPort=(uint16_t)p;
}
bool set(Preferences& prefs,const String& h,uint16_t p){
  IPAddress ip;
  if(!ip.fromString(h) || p==0) return false;
  currentHost=h;currentPort=p;
  prefs.putString("nasHost",currentHost);
  prefs.putUInt("nasPort",currentPort);
  return true;
}
String host(){return currentHost;}
uint16_t port(){return currentPort;}
IPAddress address(){IPAddress ip;ip.fromString(currentHost);return ip;}
}
