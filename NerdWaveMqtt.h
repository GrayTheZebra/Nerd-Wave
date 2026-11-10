#pragma once
#include <Arduino.h>
#include <Preferences.h>
namespace NerdWaveMqtt {
struct State {
 uint8_t source; uint8_t mask; bool playing; bool paused; int volume;
 bool repeat; uint8_t shuffle; String title; uint32_t elapsed; uint32_t duration;
};
struct Actions {bool (*toggle)(); bool (*next)(int); void (*volume)(int); void (*source)(uint8_t); bool (*repeat)(); void (*shuffle)(uint8_t); bool (*playRequest)(const String&,bool);};
void begin(Preferences& prefs,Actions actions);
void poll(const State& state,bool networkReady);
bool save(bool enabled,const String& host,uint16_t port,const String& user,const String& password,const String& prefix,String& error);
void configJson(String& output);
bool connected();
}
