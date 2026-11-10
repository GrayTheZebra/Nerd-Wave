#include "DisplayText.h"

String lcdText(const String& s, size_t maxChars) {
  String out;
  for (size_t i=0;i<s.length() && out.length()<maxChars;i++) {
    uint8_t c=(uint8_t)s[i];
    if (c>=32 && c<127) out+=(char)c;
    else if (c==0xC3 && i+1<s.length()) {
      uint8_t d=(uint8_t)s[++i];
      if (d==0xA4 || d==0x84) out+="ae";
      else if (d==0xB6 || d==0x96) out+="oe";
      else if (d==0xBC || d==0x9C) out+="ue";
      else if (d==0x9F) out+="ss";
      else out+='?';
    } else if ((c&0xC0)==0xC0) {
      while(i+1<s.length() && ((uint8_t)s[i+1]&0xC0)==0x80) ++i;
      out+='?';
    }
  }
  return out;
}

String displayTitle(const String& original) {
  String name=original;
  int slash=name.lastIndexOf('/');
  if(slash>=0) name=name.substring(slash+1);
  if(name.length()>4 && name.substring(name.length()-4).equalsIgnoreCase(".mp3"))
    name.remove(name.length()-4);
  return name;
}

