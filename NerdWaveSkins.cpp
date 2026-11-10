#include "NerdWaveSkins.h"
#include <ArduinoJson.h>
namespace NerdWaveSkins {
namespace {
const Theme classic{0x0000,0xFFFF,0x07FF,0xFFE0,0x18C3,0x0351,0x7A80};
const Theme amber{0x0800,0xBC09,0xFD20,0xAB40,0x3080,0x5900,0x51A0};
const Layout standardLayout{27,68,120,145,178,120,120,120,120,120,1,0,2,1,{0xFFE0,0x07FF,0xFFFF,0xFFFF,0x07FF},31,1};
struct LegacyLayout { uint8_t sourceY,iconsY,titleY,timeY,stateY; };
struct PreviousLayout { uint8_t sourceY,iconsY,titleY,timeY,stateY,sourceX,iconsX,titleX,timeX,stateX,sourceFont,titleFont,timeFont,stateFont; };
bool validLayout(const Layout& l) {
 const uint8_t coords[]={l.sourceX,l.iconsX,l.titleX,l.timeX,l.stateX,l.sourceY,l.iconsY,l.titleY,l.timeY,l.stateY};
 for(uint8_t v:coords) if(v>239) return false;
 return l.sourceFont>=1&&l.sourceFont<=3 && l.titleFont<=3 &&
        l.timeFont>=1&&l.timeFont<=3 && l.stateFont>=1&&l.stateFont<=3 &&
        l.iconSize>=1&&l.iconSize<=2 && (l.visibleMask & ~uint8_t(31))==0;
}
const char* const parts[]={"source","icons","title","time","state"};

Layout activeLayout=standardLayout;
Decoration decorationState;
Theme active=classic;
Preferences* storage=nullptr;
String selected="classic";
uint16_t to565(uint32_t rgb){return uint16_t(((rgb>>19)&31)<<11 | ((rgb>>10)&63)<<5 | ((rgb>>3)&31));}
uint32_t toRgb(uint16_t c){uint32_t r=(c>>11)&31,g=(c>>5)&63,b=c&31;return ((r*255/31)<<16)|((g*255/63)<<8)|(b*255/31);}
bool parseColor(JsonVariantConst v,uint16_t& output){if(!v.is<const char*>())return false;String s=v.as<String>();if(s.length()!=7||s[0]!='#')return false;for(int i=1;i<7;i++)if(!isxdigit((unsigned char)s[i]))return false;output=to565(strtoul(s.c_str()+1,nullptr,16));return true;}
bool parseY(JsonVariantConst v,uint8_t& output,int low,int high) {
 if(v.isNull())return true; // Old schema-1 skins remain compatible.
 if(!v.is<int>())return false;
 int y=v.as<int>();if(y<low||y>high)return false;output=uint8_t(y);return true;
}
bool decode(const String& input,Theme& out,Layout& outLayout,String& name,String& error){
 if(input.length()>4096){error="Skin ist zu groß (max. 4096 Bytes)";return false;}
 JsonDocument doc; if(deserializeJson(doc,input)){error="Ungültiges JSON";return false;}
 if(doc["schema"].as<int>()!=1){error="Skin-Schema muss 1 sein";return false;}
 name=doc["name"].as<String>();name.trim();if(name.isEmpty()||name.length()>32){error="Name muss 1 bis 32 Zeichen haben";return false;}
 if(doc["display"].as<String>()!="GC9A01"||doc["width"].as<int>()!=240||doc["height"].as<int>()!=240){error="Skin benötigt GC9A01 mit 240 x 240";return false;}
 JsonObjectConst colors=doc["colors"].as<JsonObjectConst>();
 if(colors.isNull()||!parseColor(colors["background"],out.bg)||!parseColor(colors["text"],out.text)||!parseColor(colors["accent"],out.accent)||!parseColor(colors["secondary"],out.secondary)||!parseColor(colors["selection"],out.selected)||!parseColor(colors["dimAccent"],out.dimAccent)||!parseColor(colors["dimSecondary"],out.dimSecondary)){error="Sieben Farben als #RRGGBB erforderlich";return false;}
 outLayout=standardLayout;
 JsonVariantConst layout=doc["layout"];
 if(!layout.isNull()) {
  if(!layout.is<JsonObjectConst>() ||
     !parseY(layout["sourceY"],outLayout.sourceY,0,239) ||
     !parseY(layout["iconsY"],outLayout.iconsY,0,239) ||
     !parseY(layout["titleY"],outLayout.titleY,0,239) ||
     !parseY(layout["timeY"],outLayout.timeY,0,239) ||
     !parseY(layout["stateY"],outLayout.stateY,0,239) ||
     !parseY(layout["sourceX"],outLayout.sourceX,0,239) ||
     !parseY(layout["iconsX"],outLayout.iconsX,0,239) ||
     !parseY(layout["titleX"],outLayout.titleX,0,239) ||
     !parseY(layout["timeX"],outLayout.timeX,0,239) ||
     !parseY(layout["stateX"],outLayout.stateX,0,239) ||
     !parseY(layout["sourceFont"],outLayout.sourceFont,1,3) ||
     !parseY(layout["titleFont"],outLayout.titleFont,0,3) ||
     !parseY(layout["timeFont"],outLayout.timeFont,1,3) ||
     !parseY(layout["stateFont"],outLayout.stateFont,1,3) ||
     !parseY(layout["iconSize"],outLayout.iconSize,1,2)) {
    error="Layoutposition außerhalb des sicheren Bereichs";return false;
  }
 }
 JsonObjectConst elements=doc["elements"].as<JsonObjectConst>();
 if(!elements.isNull()) {
  for(int i=0;i<5;i++) {
   JsonObjectConst e=elements[parts[i]].as<JsonObjectConst>();
   if(e.isNull())continue;
   if(!e["visible"].isNull()) {
    if(!e["visible"].is<bool>()){error="Ungültige Sichtbarkeit";return false;}
    if(e["visible"].as<bool>())outLayout.visibleMask|=(1<<i);
    else outLayout.visibleMask &= ~(1<<i);
   }
   if(!e["color"].isNull()&&!parseColor(e["color"],outLayout.elementColor[i])){error="Ungültige Elementfarbe";return false;}
  }
 }
 return validLayout(outLayout);
}
uint16_t mix565(uint16_t a,uint16_t b,uint16_t t){
 uint32_t r=(((a>>11)&31)*(255-t)+((b>>11)&31)*t)/255;
 uint32_t g=(((a>>5)&63)*(255-t)+((b>>5)&63)*t)/255;
 uint32_t bl=((a&31)*(255-t)+(b&31)*t)/255;
 return uint16_t((r<<11)|(g<<5)|bl);
}
void resetBands(){
 decorationState.bandCount=1;
 decorationState.bandStart[0]=0;
 decorationState.bandEnd[0]=239;
 decorationState.bandColor[0]=active.bg;
 for(uint8_t i=1;i<Decoration::MAX_BANDS;i++){
  decorationState.bandStart[i]=0;
  decorationState.bandEnd[i]=0;
  decorationState.bandColor[i]=active.bg;
 }
}
bool validBands(const Decoration& d){
 if(d.bandCount<1 || d.bandCount>Decoration::MAX_BANDS) return false;
 for(uint8_t i=0;i<d.bandCount;i++){
  if(d.bandStart[i]>239 || d.bandEnd[i]>239 || d.bandStart[i]>d.bandEnd[i]) return false;
 }
 return true;
}
void loadDecoration(){
 decorationState=Decoration(); decorationState.first=active.bg;decorationState.second=active.bg; resetBands();
 if(!storage)return;
 String data=storage->getString("skinDecor","");if(!data.length())return;
 JsonDocument d;if(deserializeJson(d,data))return;
 int mode=d["mode"]|0,dir=d["direction"]|0;
 if(mode<0||mode>2||dir<0||dir>1)return;
 decorationState.mode=mode;decorationState.direction=dir;
 uint16_t col;if(parseColor(d["first"],col))decorationState.first=col;
 if(parseColor(d["second"],col))decorationState.second=col;
 JsonArrayConst bands=d["bands"].as<JsonArrayConst>();
 if(!bands.isNull()){
  uint8_t n=0;
  for(JsonObjectConst band : bands){
   if(n>=Decoration::MAX_BANDS) break;
   int start=band["start"]|0,end=band["end"]|0;
   if(start<0||start>239||end<0||end>239||start>end) continue;
   uint16_t bandCol=active.bg;
   if(!parseColor(band["color"],bandCol)) continue;
   decorationState.bandStart[n]=uint8_t(start);
   decorationState.bandEnd[n]=uint8_t(end);
   decorationState.bandColor[n]=bandCol;
   n++;
  }
  if(n>0) decorationState.bandCount=n;
 }
 for(int i=0;i<5;i++){
  JsonVariantConst e=d["elements"][parts[i]];
  if(parseColor(e["background"],col))decorationState.bgColor[i]=col;
  decorationState.bgEnabled[i]=e["bgEnabled"]|false;
  int pad=e["padding"]|0;if(pad>=0&&pad<=2)decorationState.bgPadding[i]=pad;
  String txt=e["text"]|"";if(txt.length()<=96)decorationState.text[i]=txt;
 }
}
String decorationJson(const Decoration& val){
 JsonDocument d;d["mode"]=val.mode;d["direction"]=val.direction;
 char color[8];snprintf(color,sizeof(color),"#%06lX",(unsigned long)toRgb(val.first));d["first"]=color;
 snprintf(color,sizeof(color),"#%06lX",(unsigned long)toRgb(val.second));d["second"]=color;
 JsonArray bands=d["bands"].to<JsonArray>();
 for(uint8_t i=0;i<val.bandCount && i<Decoration::MAX_BANDS;i++){
  JsonObject b=bands.add<JsonObject>();
  b["start"]=val.bandStart[i];
  b["end"]=val.bandEnd[i];
  snprintf(color,sizeof(color),"#%06lX",(unsigned long)toRgb(val.bandColor[i]));
  b["color"]=color;
 }
 JsonObject els=d["elements"].to<JsonObject>();for(int i=0;i<5;i++){
  JsonObject e=els[parts[i]].to<JsonObject>();e["bgEnabled"]=val.bgEnabled[i];e["padding"]=val.bgPadding[i];e["text"]=val.text[i];
  snprintf(color,sizeof(color),"#%06lX",(unsigned long)toRgb(val.bgColor[i]));e["background"]=color;
 }
 String out;serializeJson(d,out);return out;
}

}
void begin(Preferences& prefs){storage=&prefs;activeLayout=standardLayout;selected=prefs.getString("skinId","classic");if(selected=="amber")active=amber;else if(selected=="custom"){
 Theme t;Layout l;String n,e;if(decode(prefs.getString("skinData",""),t,l,n,e)){active=t;activeLayout=l;}else {active=classic; /* Eigene Felder funktionieren auch ohne altes skinData-Template. */}
 }else {selected="classic";active=classic;}
 const size_t stored=prefs.getBytesLength("skinLayout");
 if(stored==sizeof(Layout)) { Layout remembered; prefs.getBytes("skinLayout",&remembered,sizeof(remembered));if(validLayout(remembered))activeLayout=remembered; }
 else if(stored==sizeof(PreviousLayout)) { PreviousLayout old; prefs.getBytes("skinLayout",&old,sizeof(old));
  Layout migrated=standardLayout;
  migrated.sourceY=old.sourceY;migrated.iconsY=old.iconsY;migrated.titleY=old.titleY;migrated.timeY=old.timeY;migrated.stateY=old.stateY;
  migrated.sourceX=old.sourceX;migrated.iconsX=old.iconsX;migrated.titleX=old.titleX;migrated.timeX=old.timeX;migrated.stateX=old.stateX;
  migrated.sourceFont=old.sourceFont;migrated.titleFont=old.titleFont;migrated.timeFont=old.timeFont;migrated.stateFont=old.stateFont;
  if(validLayout(migrated))activeLayout=migrated;
 }
 else if(stored==sizeof(LegacyLayout)) { LegacyLayout old; prefs.getBytes("skinLayout",&old,sizeof(old)); Layout migrated=standardLayout;
  migrated.sourceY=old.sourceY;migrated.iconsY=old.iconsY;migrated.titleY=old.titleY;migrated.timeY=old.timeY;migrated.stateY=old.stateY;
  if(validLayout(migrated))activeLayout=migrated;
 }
 loadDecoration();
 // Built-in designs never inherit saved custom decorations or layouts.
 // Custom data in NVS remains untouched and is restored when selected.
 if(selected!="custom"){
   activeLayout=standardLayout;
   activeLayout.elementColor[0]=active.secondary;
   activeLayout.elementColor[1]=active.accent;
   activeLayout.elementColor[2]=active.text;
   activeLayout.elementColor[3]=active.text;
   activeLayout.elementColor[4]=active.accent;
   decorationState=Decoration();
   decorationState.first=active.bg;
   decorationState.second=active.bg;
   resetBands();
 }
}
const Theme& current(){return active;}
const Layout& layout(){return activeLayout;}
const Decoration& decoration(){return decorationState;}
uint16_t backgroundAt(int x,int y){
 if(decorationState.mode==0)return decorationState.first;
 const int pos=decorationState.direction?x:y;
 if(decorationState.mode==1){
  const uint16_t t=(uint16_t)constrain(pos*255/239,0,255);
  return mix565(decorationState.first,decorationState.second,t);
 }
 for(uint8_t i=0;i<decorationState.bandCount && i<Decoration::MAX_BANDS;i++){
  if(pos>=decorationState.bandStart[i] && pos<=decorationState.bandEnd[i]) return decorationState.bandColor[i];
 }
 return decorationState.bandColor[0];
}
bool setDecoration(const Decoration& next,bool save,String& error){
 if(next.mode>2||next.direction>1){error="Hintergrundmodus ungültig";return false;}
 if(!validBands(next)){error="Hintergrund-Bereiche ungültig";return false;}
 for(int i=0;i<5;i++)if(next.bgPadding[i]>2||next.text[i].length()>96){error="Elementdaten ungültig";return false;}
 if(save){if(!storage||storage->putString("skinDecor",decorationJson(next))==0){error="Design konnte nicht gespeichert werden";return false;}}
 decorationState=next;return true;
}
const char* activeName(){if(selected=="amber")return "Retro Amber";if(selected=="custom")return "Eigener Skin";return "Classic Cyan";}
bool selectBuiltin(const String& id){if(id!="classic"&&id!="amber")return false;selected=id;active=id=="amber"?amber:classic;
 // Fully restore default design; never carry custom gradients, backdrops,
 // text templates or custom positions into a built-in skin.
 activeLayout=standardLayout;
 decorationState=Decoration();
 decorationState.first=active.bg;
 decorationState.second=active.bg;
 resetBands();
 // Farbpalette des gewählten Skins auf die Elementfarben übertragen.
 // Positions- und Sichtbarkeitseinstellungen bleiben erhalten.
 activeLayout.elementColor[0]=active.secondary;
 activeLayout.elementColor[1]=active.accent;
 activeLayout.elementColor[2]=active.text;
 activeLayout.elementColor[3]=active.text;
 activeLayout.elementColor[4]=active.accent;
 if(storage) {storage->putString("skinId",id); /* Do not overwrite custom layout. */}
 return true;}
bool selectCustom(){
 if(!storage)return false;
 // Alte Farbschema-Daten optional laden; der neue Editor speichert seine
 // Hintergrund-/Felddefinitionen separat in skinDecor und freeFields.
 Theme t;Layout l;String name,error;
 if(decode(storage->getString("skinData",""),t,l,name,error)){
   active=t;activeLayout=l;
 }else{
   active=classic;activeLayout=standardLayout;
 }
 selected="custom";
 loadDecoration();
 return storage->putString("skinId","custom")>0;
}
String json(){JsonDocument d;d["active"]=selected;d["label"]=activeName();d["customAvailable"]=storage!=nullptr;JsonObject l=d["layout"].to<JsonObject>();l["sourceY"]=activeLayout.sourceY;l["iconsY"]=activeLayout.iconsY;l["titleY"]=activeLayout.titleY;l["timeY"]=activeLayout.timeY;l["stateY"]=activeLayout.stateY;l["sourceX"]=activeLayout.sourceX;l["iconsX"]=activeLayout.iconsX;l["titleX"]=activeLayout.titleX;l["timeX"]=activeLayout.timeX;l["stateX"]=activeLayout.stateX;l["sourceFont"]=activeLayout.sourceFont;l["titleFont"]=activeLayout.titleFont;l["timeFont"]=activeLayout.timeFont;l["stateFont"]=activeLayout.stateFont;l["iconSize"]=activeLayout.iconSize;
 JsonObject elements=d["elements"].to<JsonObject>();for(int i=0;i<5;i++) {JsonObject e=elements[parts[i]].to<JsonObject>();e["visible"]=(activeLayout.visibleMask & (1<<i))!=0;char rgb[8];snprintf(rgb,sizeof(rgb),"#%06lX",(unsigned long)toRgb(activeLayout.elementColor[i]));e["color"]=rgb;}
 d["decoration"]=serialized(decorationJson(decorationState));String r;serializeJson(d,r);return r;}
bool applyJson(const String& payload,String& error){Theme parsed;Layout parsedLayout;String n;if(!decode(payload,parsed,parsedLayout,n,error))return false; if(!storage){error="NVS nicht initialisiert";return false;} if(storage->putString("skinData",payload)==0){error="NVS-Speichern fehlgeschlagen";return false;}storage->putString("skinId","custom");selected="custom";active=parsed;activeLayout=parsedLayout;loadDecoration();
 JsonDocument templateDoc;if(!deserializeJson(templateDoc,payload)){
  JsonObjectConst ext=templateDoc["decoration"].as<JsonObjectConst>();
  if(!ext.isNull()){
   int mode=ext["mode"]|0,dir=ext["direction"]|0;
   uint16_t v1,v2;if(mode>=0&&mode<=2&&dir>=0&&dir<=1&&parseColor(ext["first"],v1)&&parseColor(ext["second"],v2)){
    decorationState.mode=mode;decorationState.direction=dir;decorationState.first=v1;decorationState.second=v2;
   }
   JsonArrayConst bands=ext["bands"].as<JsonArrayConst>();
   if(!bands.isNull()){
    uint8_t n=0;
    for(JsonObjectConst band : bands){
      if(n>=Decoration::MAX_BANDS) break;
      int start=band["start"]|0,end=band["end"]|0;
      uint16_t bc;
      if(start<0||start>239||end<0||end>239||start>end||!parseColor(band["color"],bc)) continue;
      decorationState.bandStart[n]=uint8_t(start);
      decorationState.bandEnd[n]=uint8_t(end);
      decorationState.bandColor[n]=bc;
      n++;
    }
    if(n>0) decorationState.bandCount=n;
   }
  }
  for(int i=0;i<5;i++){
   JsonObjectConst el=templateDoc["elements"][parts[i]].as<JsonObjectConst>();if(el.isNull())continue;
   uint16_t color;if(parseColor(el["background"],color))decorationState.bgColor[i]=color;
   decorationState.bgEnabled[i]=el["bgEnabled"]|false;
   int pad=el["padding"]|0;if(pad>=0&&pad<=2)decorationState.bgPadding[i]=pad;
   String txt=el["text"]|"";if(txt.length()<=96)decorationState.text[i]=txt;
  }
  storage->putString("skinDecor",decorationJson(decorationState));
 }
 if(storage)storage->putBytes("skinLayout",&activeLayout,sizeof(activeLayout));return true;}
bool setLayout(const Layout& next,bool save,String& error){
 if(!validLayout(next)){error="Layoutwert außerhalb des gültigen Bereichs";return false;}
 if(save && (!storage || storage->putBytes("skinLayout",&next,sizeof(next))!=sizeof(next))){error="Layout konnte nicht gespeichert werden";return false;}
 activeLayout=next;return true;
}

}
