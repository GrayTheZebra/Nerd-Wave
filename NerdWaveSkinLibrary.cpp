#include "NerdWaveSkinLibrary.h"
#include <Preferences.h>
#include <ArduinoJson.h>
namespace NerdWaveSkinLibrary {
namespace {
constexpr int SLOTS=4;
Preferences prefs;
bool open(){return prefs.begin("nw-skin-lib",false);}
String slotKey(int i){return String("skin")+String(i);}
String nameKey(int i){return String("name")+String(i);}
String clean(const String& name){String n=name;n.trim();return n;}
bool valid(const String& n){if(n.isEmpty()||n.length()>32)return false;for(size_t i=0;i<n.length();i++)if((uint8_t)n[i]<32 || n[i]=='/' || n[i]=='\\')return false;return true;}
}
String list(){JsonDocument doc;JsonArray arr=doc["skins"].to<JsonArray>();if(open()){
 for(int i=0;i<SLOTS;i++){String n=prefs.getString(nameKey(i).c_str(),"");if(n.length()){JsonObject e=arr.add<JsonObject>();e["id"]=n;e["name"]=n;}}
 prefs.end();}String out;serializeJson(doc,out);return out;}
bool save(const String& name,const String& payload,bool overwrite,String& error){String n=clean(name);if(!valid(n)){error="Name muss 1 bis 32 Zeichen haben";return false;}
 if(payload.length()>10000){error="Skin zu gross (maximal 10 KB)";return false;}
 JsonDocument test;if(deserializeJson(test,payload)||test["schema"].as<int>()!=3||!test["fields"].is<JsonArrayConst>()||!test["background"].is<JsonObjectConst>()){error="Ungueltiges Skin-Template";return false;}
 if(!open()){error="NVS nicht verfuegbar";return false;}
 int slot=-1,empty=-1;for(int i=0;i<SLOTS;i++){String cur=prefs.getString(nameKey(i).c_str(),"");if(cur==n)slot=i;if(cur.isEmpty()&&empty<0)empty=i;}
 if(slot>=0&&!overwrite){error="Name bereits vergeben – Aktualisieren waehlen";prefs.end();return false;}
 if(slot<0)slot=empty;if(slot<0){error="Maximal vier eigene Skins moeglich";prefs.end();return false;}
 if(prefs.putString(slotKey(slot).c_str(),payload)!=payload.length()){error="NVS-Speichern fehlgeschlagen";prefs.end();return false;}
 prefs.putString(nameKey(slot).c_str(),n);prefs.end();return true;}
bool load(const String& name,String& json,String& error){String n=clean(name);if(!open()){error="NVS nicht verfuegbar";return false;}
 bool found=false;for(int i=0;i<SLOTS;i++)if(prefs.getString(nameKey(i).c_str(),"")==n){json=prefs.getString(slotKey(i).c_str(),"");found=json.length()>0;break;}prefs.end();if(!found)error="Skin nicht gefunden";return found;}
bool remove(const String& name,String& error){String n=clean(name);if(!open()){error="NVS nicht verfuegbar";return false;}
 bool found=false;for(int i=0;i<SLOTS;i++)if(prefs.getString(nameKey(i).c_str(),"")==n){prefs.remove(nameKey(i).c_str());prefs.remove(slotKey(i).c_str());found=true;break;}prefs.end();if(!found)error="Skin nicht gefunden";return found;}
}
