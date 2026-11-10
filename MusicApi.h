#pragma once
#include <Arduino.h>
struct MusicItem { String name; String path; bool directory=false; uint32_t size=0; };
struct MusicPage { uint16_t offset=0,count=0,total=0; MusicItem items[25]; };
class MusicApi {
public:
  bool fetch(const String& path, uint16_t offset, MusicPage& out);
  bool fetchFrom(uint8_t source, const String& path, uint16_t offset, MusicPage& out);
  String error() const { return error_; }
  static String encode(const String& s);
  static String encodePath(const String& s);
private:
  String error_;
};
