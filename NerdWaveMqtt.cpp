#include "NerdWaveLog.h"
#include "NerdWaveMqtt.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "NerdWaveSources.h"
namespace NerdWaveMqtt {
namespace {
WiFiClient transport;
PubSubClient mqtt(transport);
Preferences* store=nullptr;
Actions act{};
bool enabled=false;
String host,user,pass,prefix="nerdwave";
uint16_t port=1883;
uint32_t retryAt=0, lastPublish=0;
String lastState;
String topic(const char* tail){return prefix+"/"+tail;}
const char* sourceNames[]={"NAS","USB","Bluetooth","SD"};
String deviceId(){uint64_t mac=ESP.getEfuseMac();char id[15];snprintf(id,sizeof(id),"%012llx",(unsigned long long)mac);return String(id);}
void publishEntity(const char* component,const String& objectId,JsonDocument& d){
 String payload;serializeJson(d,payload);
 const String t="homeassistant/"+String(component)+"/"+objectId+"/config";
 if(!mqtt.publish(t.c_str(),payload.c_str(),true))
   NerdWaveLog::output.printf("[MQTT] Discovery fehlgeschlagen: %s (%u Bytes)\n",t.c_str(),(unsigned)payload.length());
}
void common(JsonDocument& d,const String& id,const String& slug,const char* name){
 d["name"]=name;
 d["unique_id"]="nerdwave_"+id+"_"+slug;
 d["availability_topic"]=topic("availability");
 d["payload_available"]="online";d["payload_not_available"]="offline";
 JsonObject dev=d["device"].to<JsonObject>();
 dev["identifiers"].to<JsonArray>().add("nerdwave_"+id);
 dev["name"]="Nerd-Wave";dev["manufacturer"]="DIY";dev["model"]="ESP32-S3 Audioplayer";
}
void announce(){
 const String id=deviceId();
 const String base="nerdwave_"+id;
 // Discovery als MQTT media_player wird von Home Assistant nicht unterstuetzt.
 // Einen ggf. retained gespeicherten Alt-Eintrag entfernen.
 mqtt.publish(("homeassistant/media_player/"+base+"/config").c_str(),"",true);
 {
  JsonDocument d;common(d,id,"status","Wiedergabe");
  d["state_topic"]=topic("playback/state");
  d["icon"]="mdi:play-circle-outline";
  publishEntity("sensor",base+"_status",d);
 }
 {
  JsonDocument d;common(d,id,"title","Titel");
  d["state_topic"]=topic("state");
  d["value_template"]="{{ value_json.media_title }}";
  d["icon"]="mdi:music-note";
  publishEntity("sensor",base+"_title",d);
 }
 {
  JsonDocument d;common(d,id,"volume","Lautstärke");
  d["command_topic"]=topic("volume/set");
  d["state_topic"]=topic("volume/percent");
  d["min"]=0;d["max"]=100;d["step"]=1;
  d["mode"]="slider";d["icon"]="mdi:volume-high";
  publishEntity("number",base+"_volume",d);
 }
 {
  JsonDocument d;common(d,id,"source","Quelle");
  d["command_topic"]=topic("source/set");
  d["state_topic"]=topic("source/state");
  JsonArray opts=d["options"].to<JsonArray>();
  for(uint8_t i : {uint8_t(0),uint8_t(1),uint8_t(3),uint8_t(2)})if(NerdWaveSources::enabled(i))opts.add(sourceNames[i]);
  publishEntity("select",base+"_source",d);
 }
 {
  JsonDocument d;common(d,id,"tracknumber","Titelnummer");
  d["command_topic"]=topic("track/number/set");d["min"]=1;d["max"]=10000;d["step"]=1;
  d["mode"]="box";d["icon"]="mdi:numeric";
  publishEntity("number",base+"_tracknumber",d);
 }
 {
  JsonDocument d;common(d,id,"trackpath","Titelname oder -pfad");
  d["command_topic"]=topic("track/path/set");d["mode"]="text";
  d["min"]=0;d["max"]=255;d["icon"]="mdi:file-music-outline";
  publishEntity("text",base+"_trackpath",d);
 }
 struct Button {const char* id;const char* name;const char* payload;const char* icon;};
 static const Button buttons[]={
  {"play","Play","PLAY","mdi:play"},
  {"pause","Pause","PAUSE","mdi:pause"},
  {"next","Nächster Titel","NEXT","mdi:skip-next"},
  {"previous","Vorheriger Titel","PREVIOUS","mdi:skip-previous"}
 };
 for(const auto& b:buttons){
  JsonDocument d;common(d,id,b.id,b.name);
  d["command_topic"]=topic("command");d["payload_press"]=b.payload;d["icon"]=b.icon;
  publishEntity("button",base+"_"+b.id,d);
 }
 NerdWaveLog::output.println("[MQTT] HA-Discovery: Sensoren, Lautstaerke, Quelle, 4 Tasten als Nerd-Wave");
}
bool cachedPlaying=false,cachedPaused=false;
void inbound(char* key,byte* data,unsigned int len){
 if(len>320)return;
 String value;value.reserve(len);for(unsigned i=0;i<len;i++)value+=(char)data[i];value.trim();
 const String t(key);
 if(t=="homeassistant/status"){if(value=="online")announce();return;}
 if(t==topic("command")){
   if(value=="NEXT" && act.next)act.next(1);
   else if(value=="PREVIOUS" && act.next)act.next(-1);
   else if((value=="PLAY"||value=="PAUSE"||value=="STOP") && act.toggle){
     // Runtime-Status wird in poll aktualisiert; keine blockierenden Audio-Aufrufe.
     if(value=="PLAY" && !cachedPlaying)act.toggle();
     else if((value=="PAUSE"||value=="STOP") && cachedPlaying && !cachedPaused)act.toggle();
   }
 }else if(t==topic("track/number/set") && act.playRequest){
   bool valid=value.length()>0;for(unsigned i=0;i<value.length();i++)if(value[i]<'0'||value[i]>'9')valid=false;
   if(valid && value.toInt()>0 && value.toInt()<=10000){
     bool ok=act.playRequest(value,true);mqtt.publish(topic("track/result").c_str(),ok?"ok":"not_found",false);
     NerdWaveLog::output.printf("[MQTT] Titelnummer %s: %s\n",value.c_str(),ok?"OK":"nicht gefunden");
   }
 }else if(t==topic("track/path/set") && act.playRequest){
   const bool ok=act.playRequest(value,false);
   mqtt.publish(topic("track/result").c_str(),ok?"ok":"not_found",false);
   NerdWaveLog::output.printf("[MQTT] Titelpfad: %s\n",ok?"OK":"nicht gefunden");
 }else if(t==topic("volume/set") && act.volume){
   int v=value.toInt();if(v>=0&&v<=100)act.volume(v);
 }else if(t==topic("source/set") && act.source){
   for(uint8_t i=0;i<4;i++)if(value==sourceNames[i] && NerdWaveSources::enabled(i)){act.source(i);break;}
 }
}
}
void begin(Preferences& prefs,Actions actions){store=&prefs;act=actions;
 enabled=prefs.getBool("mqttOn",false);host=prefs.getString("mqttHost","");
 port=prefs.getUShort("mqttPort",1883);user=prefs.getString("mqttUser","");pass=prefs.getString("mqttPass","");prefix=prefs.getString("mqttTopic","nerdwave");
 mqtt.setServer(host.c_str(),port);mqtt.setCallback(inbound);mqtt.setBufferSize(1536);mqtt.setSocketTimeout(2);
}
void poll(const State& state,bool networkReady){
 cachedPlaying=state.playing;cachedPaused=state.paused;
 if(!enabled||host.isEmpty()||!networkReady){if(mqtt.connected())mqtt.disconnect();return;}
 if(!mqtt.connected()){
   if((int32_t)(millis()-retryAt)<0)return;
   retryAt=millis()+10000;
   const String id="nerdwave_"+deviceId();
   bool ok=user.length()?mqtt.connect(id.c_str(),user.c_str(),pass.c_str(),topic("availability").c_str(),0,true,"offline"):
                         mqtt.connect(id.c_str(),topic("availability").c_str(),0,true,"offline");
   if(!ok){NerdWaveLog::output.printf("[MQTT] Verbindung fehlgeschlagen rc=%d\n",mqtt.state());return;}
   mqtt.publish(topic("availability").c_str(),"online",true);
   mqtt.subscribe(topic("command").c_str());mqtt.subscribe(topic("volume/set").c_str());mqtt.subscribe(topic("source/set").c_str());mqtt.subscribe(topic("track/number/set").c_str());mqtt.subscribe(topic("track/path/set").c_str());mqtt.subscribe("homeassistant/status");
   announce();lastState="";lastPublish=0;NerdWaveLog::output.println("[MQTT] verbunden, HA-Discovery gesendet");
 }
 mqtt.loop();
 JsonDocument d;d["state"]=state.playing?(state.paused?"paused":"playing"):"idle";
 d["title"]=state.title;d["media_title"]=state.title;
 d["media_position"] = state.elapsed;d["media_duration"] = state.duration;
 d["source"]=sourceNames[min((int)state.source,3)];d["volume_level"]=(float)constrain(state.volume,0,100)/100.0f;
 String payload;serializeJson(d,payload);
 if(payload!=lastState || millis()-lastPublish>30000){
   mqtt.publish(topic("state").c_str(),payload.c_str(),true);
   mqtt.publish(topic("playback/state").c_str(),state.playing?(state.paused?"paused":"playing"):"idle",true);
   String vol=String((float)constrain(state.volume,0,100)/100.0f,3);
   mqtt.publish(topic("volume/state").c_str(),vol.c_str(),true);
   const String percent=String(constrain(state.volume,0,100));mqtt.publish(topic("volume/percent").c_str(),percent.c_str(),true);
   mqtt.publish(topic("source/state").c_str(),sourceNames[min((int)state.source,3)],true);
   lastState=payload;lastPublish=millis();
 }
}
bool save(bool on,const String& newHost,uint16_t newPort,const String& newUser,const String& newPass,const String& newPrefix,String& error){
 if(on && (newHost.isEmpty()||newHost.length()>120)){error="MQTT-Host fehlt oder ist zu lang";return false;}
 if(newPrefix.length()<2||newPrefix.length()>56||newPrefix.startsWith("/")||newPrefix.endsWith("/")||newPrefix.indexOf('+')>=0||newPrefix.indexOf('#')>=0||newPrefix.indexOf(' ')>=0){error="Ungueltiger MQTT-Topic-Praefix";return false;}
 if(newUser.length()>80||newPass.length()>120){error="MQTT-Zugangsdaten zu lang";return false;}
 if(mqtt.connected())mqtt.disconnect();
 enabled=on;host=newHost;port=newPort;user=newUser;pass=newPass;prefix=newPrefix;
 store->putBool("mqttOn",enabled);store->putString("mqttHost",host);store->putUShort("mqttPort",port);
 store->putString("mqttUser",user);store->putString("mqttPass",pass);store->putString("mqttTopic",prefix);
 mqtt.setServer(host.c_str(),port);retryAt=0;return true;
}
void configJson(String& output){JsonDocument d;d["enabled"]=enabled;d["host"]=host;d["port"]=port;d["username"]=user;d["prefix"]=prefix;d["connected"]=mqtt.connected();d["passwordSet"]=pass.length()>0;serializeJson(d,output);}
bool connected(){return mqtt.connected();}
}
