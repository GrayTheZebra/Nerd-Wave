#include "NerdWaveFields.h"
#include "NerdWaveSkins.h"
#include <ArduinoJson.h>
namespace NerdWaveFields {
namespace {
Field fields[MAX_FIELDS];uint8_t length=0;Preferences* store=nullptr;bool active=false;
uint16_t to565(uint32_t v){return uint16_t((((v>>19)&31)<<11)|(((v>>10)&63)<<5)|((v>>3)&31));}
String hex(uint16_t c){char buf[8];uint32_t r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;snprintf(buf,8,"#%02lX%02lX%02lX",(unsigned long)r,(unsigned long)g,(unsigned long)b);return String(buf);}
bool color(JsonVariantConst v,uint16_t& output){if(!v.is<const char*>())return false;String s=v.as<String>();if(s.length()!=7||s[0]!='#')return false;for(int i=1;i<7;i++)if(!isxdigit((unsigned char)s[i]))return false;output=to565(strtoul(s.c_str()+1,nullptr,16));return true;}
void defaultFields(){const auto& l=NerdWaveSkins::layout();length=7;
 const char* ids[]={"source","title","time","state"};const char* templates[]={"QUELLE: %quelle%","%titel%","%zeit%","%status%"};
 const uint8_t xs[]={l.sourceX,l.titleX,l.timeX,l.stateX},ys[]={l.sourceY,l.titleY,l.timeY,l.stateY};const uint8_t sz[]={l.sourceFont,l.titleFont?l.titleFont:2,l.timeFont,l.stateFont};const uint8_t indices[]={0,2,3,4};
 for(int i=0;i<4;i++){Field f;f.id=ids[i];f.text=templates[i];f.x=xs[i];f.y=ys[i];f.size=sz[i];f.color=l.elementColor[indices[i]];f.visible=true;fields[i]=f;}
 const char* symbols[]={"wifi","repeat","shuffle"};for(int i=0;i<3;i++){Field f;f.id=String("icon_")+symbols[i];f.type="icon";f.text=symbols[i];f.x=uint8_t(constrain((int)l.iconsX+(i-1)*(l.iconSize==2?66:42),0,239));f.y=l.iconsY;f.size=l.iconSize;f.color=l.elementColor[1];f.visible=!!(l.visibleMask&2);fields[4+i]=f;}}
}
void begin(Preferences& prefs){store=&prefs;defaultFields();active=true;String raw=prefs.getString("freeFields","");if(raw.length()){String error;if(!apply(raw,false,error)){defaultFields();active=true;}}}
bool enabled(){return active;}
uint8_t count(){return length;}
const Field& at(uint8_t i){return fields[i];}
String json(){JsonDocument doc;doc["schema"]=2;JsonArray arr=doc["fields"].to<JsonArray>();for(int i=0;i<length;i++){const Field& f=fields[i];JsonObject e=arr.add<JsonObject>();e["id"]=f.id;e["type"]=f.type;e["text"]=f.text;e["x"]=f.x;e["y"]=f.y;e["size"]=f.size;e["visible"]=f.visible;e["color"]=hex(f.color);e["bgEnabled"]=f.bgEnabled;e["bgColor"]=hex(f.background);e["padding"]=f.padding;e["style"]=f.style;e["width"]=f.width;e["height"]=f.height;e["maxTextWidth"]=f.maxTextWidth;e["arcStart"]=f.arcStart;e["arcSweep"]=f.arcSweep;e["borderWidth"]=f.borderWidth;e["radius"]=f.radius;}String s;serializeJson(doc,s);return s;}
bool apply(const String& raw,bool persist,String& error){
 if(raw.length()>8192){error="Template zu groß (max. 8192 Bytes)";return false;}
 JsonDocument doc;if(deserializeJson(doc,raw)){error="JSON ungültig";return false;}
 if(doc["schema"].as<int>()!=2||!doc["fields"].is<JsonArrayConst>()){error="Schema 2 mit fields-Array erforderlich";return false;}
 JsonArrayConst arr=doc["fields"].as<JsonArrayConst>();if(arr.size()>MAX_FIELDS){error="Maximal 12 Elemente";return false;}
 Field next[MAX_FIELDS];int n=0;
 for(JsonVariantConst v:arr){if(!v.is<JsonObjectConst>()){error="Ungültiger Eintrag";return false;}JsonObjectConst e=v.as<JsonObjectConst>();Field f;
 f.id=e["id"].as<String>();f.text=e["text"].as<String>();f.type=e["type"].as<String>();if(f.id.isEmpty()||f.id.length()>32||f.text.length()>120||(f.type!="text" && f.type!="icon" && f.type!="gauge" && f.type!="shape")||(f.type=="icon" && f.text!="wifi" && f.text!="repeat" && f.text!="shuffle")){error="Ungültiges Feld (ID, Typ oder Text)";return false;}
 for(int i=0;i<n;i++)if(next[i].id==f.id){error="Doppelte Feld-ID";return false;}
 if(!e["x"].is<int>()||!e["y"].is<int>()||!e["size"].is<int>()||!e["padding"].is<int>()||!e["visible"].is<bool>()||!e["bgEnabled"].is<bool>()){error="Ungültige Feldparameter";return false;}
 int x=e["x"].as<int>(),y=e["y"].as<int>(),size=e["size"].as<int>(),pad=e["padding"].as<int>();if(x<0||x>239||y<0||y>239||size<1||size>(f.type=="icon"?2:3)||pad<0||pad>2||!color(e["color"],f.color)||!color(e["bgColor"],f.background)){error="Feldwert außerhalb des Bereichs";return false;}
 if(f.type=="text"){
  // Alte Templates ohne maxTextWidth behalten die bisherige Begrenzung.
  int maxWidth=e["maxTextWidth"]|230;
  if(maxWidth<12||maxWidth>239){error="Textbreite muss zwischen 12 und 239 Pixel liegen";return false;}
  f.maxTextWidth=(uint8_t)maxWidth;
 }
 if(f.type=="shape"){
   if(f.text!="rect" && f.text!="ellipse"){error="Unbekannte Form";return false;}
   int w=e["width"]|80,h=e["height"]|50,border=e["borderWidth"]|1,r=e["radius"]|8;
   if(w<4||w>239||h<4||h>239||border<0||border>20||r<0||r>110){error="Ungueltige Form-Abmessungen";return false;}
   f.width=w;f.height=h;f.borderWidth=border;f.radius=r;
   int start=e["arcStart"]|0,sweep=e["arcSweep"]|360;
   if(start<0||start>359||sweep<1||sweep>360){error="Ungueltiger Ausschnittwinkel";return false;}
   f.arcStart=start;f.arcSweep=sweep;
 }
 if(f.type=="gauge"){
  if(f.text!="progress" && f.text!="volume"){error="Ungueltiger Anzeigetyp";return false;}
  f.style=e["style"]|"linear";
  if(f.style!="linear" && f.style!="segments" && f.style!="ring"){error="Ungueltiges Balkendesign";return false;}
  int w=e["width"]|100,h=e["height"]|10;
  if(w<12||w>200||h<2||h>80){error="Ungueltige Abmessungen";return false;}
  f.width=w;f.height=h;
  int arcStart=e["arcStart"]|0,arcSweep=e["arcSweep"]|360;
  if(arcStart<0||arcStart>359||arcSweep<1||arcSweep>360){error="Ungueltiger Bogenwinkel";return false;}
  f.arcStart=arcStart;f.arcSweep=arcSweep;
 }
 f.x=x;f.y=y;f.size=size;f.padding=pad;f.visible=e["visible"].as<bool>();f.bgEnabled=e["bgEnabled"].as<bool>();next[n++]=f;}
 if(persist && (!store || store->putString("freeFields",raw)==0)){error="NVS konnte nicht speichern";return false;}
 length=n;for(int i=0;i<n;i++)fields[i]=next[i];active=true;return true;
}
}
