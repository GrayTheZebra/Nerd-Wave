#pragma once
#include <Arduino.h>
namespace NerdWaveSkinLibrary {
String list();
bool save(const String& name,const String& json,bool overwrite,String& error);
bool load(const String& name,String& json,String& error);
bool remove(const String& name,String& error);
}
