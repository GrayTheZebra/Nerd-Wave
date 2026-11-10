#include "NerdWaveLog.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace NerdWaveLog {
namespace {
constexpr uint32_t CAPACITY = 16384;
char ring[CAPACITY];
uint32_t position = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
void store(const uint8_t* data, size_t length) {
  portENTER_CRITICAL(&mux);
  for(size_t i=0;i<length;i++) ring[position++ % CAPACITY] = (char)data[i];
  portEXIT_CRITICAL(&mux);
}
}
MirroredPrint output;
static void (*bootLineCallback)(const char*) = nullptr;
static char bootLine[192] = {};
static size_t bootLineLen = 0;
void setBootLineCallback(void (*callback)(const char*)) {
  bootLineCallback=callback;
  bootLineLen=0;
}
static void bootCollect(const uint8_t* data,size_t length) {
  if(!bootLineCallback) return;
  for(size_t i=0;i<length;i++) {
    const char c=(char)data[i];
    if(c=='\n') {
      if(bootLineLen) {bootLine[bootLineLen]='\0'; bootLineCallback(bootLine); bootLineLen=0;}
    } else if(c!='\r') {
      if(bootLineLen<sizeof(bootLine)-1) bootLine[bootLineLen++]=c;
    }
  }
}

size_t MirroredPrint::write(uint8_t b) {
  const size_t n=::Serial.write(b);
  store(&b,1);
  bootCollect(&b,1);
  return n;
}
size_t MirroredPrint::write(const uint8_t* data,size_t length) {
  if(!data || !length)return 0;
  const size_t n=::Serial.write(data,length);
  store(data,length);
  bootCollect(data,length);
  return n;
}
String read(uint32_t from,uint16_t maxBytes,uint32_t& next,bool& truncated) {
  const uint16_t limit=constrain((int)maxBytes,1,4096);
  String result;
  result.reserve(limit+1);
  portENTER_CRITICAL(&mux);
  const uint32_t end=position;
  const uint32_t oldest=end>CAPACITY?end-CAPACITY:0;
  truncated=from<oldest || from>end;
  uint32_t start=(from<oldest || from>end)?oldest:from;
  // Bei Erstaufruf nur den neuesten Ausschnitt zeigen, statt alte Bootlogs zu verlieren.
  if(end-start>limit){start=end-limit;truncated=true;}
  const uint32_t count=end-start;
  char local[4096];
  for(uint32_t i=0;i<count;i++)local[i]=ring[(start+i)%CAPACITY];
  next=end;
  portEXIT_CRITICAL(&mux);
  result.concat(local,count);
  return result;
}
}
