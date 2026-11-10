#include "NerdWaveLog.h"
#include "NerdWaveWifiFilter.h"
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <ctype.h>
#include <stdio.h>
namespace NerdWaveWifiFilter {
namespace {
String deny;
uint32_t lastCheck=0;
String norm(String b) {
 b.trim();b.toUpperCase();
 if(b.length()!=17)return "";
 for(int i=0;i<17;i++)if((i%3)==2){if(b[i]!=':')return "";}else if(!isxdigit((unsigned char)b[i]))return "";
 return b;
}
bool has(const String& b){return deny.indexOf(String("|")+b+"|")>=0;}
void save(){Preferences p;if(p.begin("nw-wifi",false)){p.putString("blocked",deny);p.end();}}
// Read ESP-IDF STA credentials; never print or expose them to HTTP API.
bool credentials(String& ssid,String& pass){
 wifi_config_t cfg={};
 if(esp_wifi_get_config(WIFI_IF_STA,&cfg)!=ESP_OK)return false;
 ssid=String((const char*)cfg.sta.ssid);pass=String((const char*)cfg.sta.password);
 return !ssid.isEmpty();
}
}
void begin(){Preferences p;if(p.begin("nw-wifi",true)){deny=p.getString("blocked","");p.end();}lastCheck=millis();}
bool blocked(const String& bssid){String b=norm(bssid);return !b.isEmpty()&&has(b);}
bool add(const String& bssid){String b=norm(bssid);if(b.isEmpty())return false;if(has(b))return true;if(deny.length()+b.length()+2>350)return false;deny+="|"+b+"|";save();return true;}
bool remove(const String& bssid){String b=norm(bssid);if(b.isEmpty())return false;String needle=String("|")+b+"|";int i=deny.indexOf(needle);if(i<0)return true;deny.remove(i,needle.length());save();return true;}
// A separate NVS entry per access point. Hex MAC without separators fits NVS key limit.
String name(const String& bssid){
 String b=norm(bssid);if(b.isEmpty())return "";
 String key="n";for(size_t i=0;i<b.length();++i)if(b[i]!=':')key+=b[i];
 Preferences p;String result="";if(p.begin("nw-apnames",true)){result=p.getString(key.c_str(),"");p.end();}
 return result;
}
bool setName(const String& bssid,const String& displayName){
 String b=norm(bssid);String n=displayName;n.trim();
 if(b.isEmpty()||n.length()>40||n.indexOf('\n')>=0||n.indexOf('\r')>=0)return false;
 String key="n";for(size_t i=0;i<b.length();++i)if(b[i]!=':')key+=b[i];
 Preferences p;if(!p.begin("nw-apnames",false))return false;
 bool ok=n.isEmpty()?p.remove(key.c_str()):p.putString(key.c_str(),n)>0;
 p.end();return ok;
}
String entries(){String out;int cursor=0;while(cursor<(int)deny.length()){int a=deny.indexOf('|',cursor);if(a<0)break;int b=deny.indexOf('|',a+1);if(b<0)break;String item=deny.substring(a+1,b);if(item.length()==17){if(!out.isEmpty())out+=",";out+=item;}cursor=b+1;}return out;}
bool connectAllowed(uint32_t timeoutMs){
 String ssid,pass;
 if(!credentials(ssid,pass)){NerdWaveLog::output.println("[WIFI-FILTER] Keine gespeicherten Zugangsdaten");return false;}
 int n=WiFi.scanNetworks(false,true);
 int best=-1;int bestRssi=-200;
 for(int i=0;i<n;i++){
  if(WiFi.SSID(i)!=ssid||blocked(WiFi.BSSIDstr(i)))continue;
  if(WiFi.RSSI(i)>bestRssi){best=i;bestRssi=WiFi.RSSI(i);}
 }
 if(best<0){WiFi.scanDelete();NerdWaveLog::output.println("[WIFI-FILTER] Kein erlaubter Zugangspunkt erreichbar");WiFi.disconnect(false,false);return false;}
 String bssid=WiFi.BSSIDstr(best);int channel=WiFi.channel(best);
 uint8_t bytes[6];unsigned int b[6];
 if(sscanf(bssid.c_str(),"%2x:%2x:%2x:%2x:%2x:%2x",&b[0],&b[1],&b[2],&b[3],&b[4],&b[5])!=6){WiFi.scanDelete();return false;}
 for(int j=0;j<6;j++)bytes[j]=(uint8_t)b[j];
 WiFi.scanDelete();
 NerdWaveLog::output.printf("[WIFI-FILTER] Erlaubter AP: %s Kanal=%d RSSI=%d\n",bssid.c_str(),channel,bestRssi);
 WiFi.disconnect(false,false);delay(100);
 WiFi.begin(ssid.c_str(),pass.c_str(),channel,bytes,true);
 uint32_t start=millis();while(WiFi.status()!=WL_CONNECTED && millis()-start<timeoutMs)delay(50);
 bool ok=WiFi.status()==WL_CONNECTED && !blocked(WiFi.BSSIDstr());
 if(!ok)WiFi.disconnect(false,false);
 if(ok)WiFi.setSleep(false);
 return ok;
}
void poll(){
 if(millis()-lastCheck<10000)return;
 lastCheck=millis();
 if(WiFi.status()==WL_CONNECTED && blocked(WiFi.BSSIDstr())){
  NerdWaveLog::output.println("[WIFI-FILTER] Gesperrter Mesh-Knoten erkannt: Wechsel wird versucht");
  connectAllowed();
 }
}
} // namespace NerdWaveWifiFilter
