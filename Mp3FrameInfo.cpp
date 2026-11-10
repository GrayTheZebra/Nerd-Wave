#include "Mp3FrameInfo.h"
#include <cstring>

uint32_t mp3ReadBe32(const uint8_t* p) {
  return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
uint32_t mp3HeaderBitrate(const uint8_t* f, size_t len) {
  if(len<4 || f[0]!=0xff || (f[1]&0xe0)!=0xe0) return 0;
  int ver=(f[1]>>3)&3, layer=(f[1]>>1)&3, ix=(f[2]>>4)&15;
  if(ver==1 || layer!=1 || ix==0 || ix==15) return 0; // nur MPEG Layer III
  static const uint16_t m1[]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
  static const uint16_t m2[]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
  return 1000UL*(ver==3?m1[ix]:m2[ix]);
}
uint32_t mp3HeaderDuration(const uint8_t* f, size_t len, uint32_t sampleRate) {
  if(len<8 || f[0]!=0xff || (f[1]&0xe0)!=0xe0 || !sampleRate) return 0;
  const int ver=(f[1]>>3)&3, layer=(f[1]>>1)&3;
  if(ver==1 || layer!=1) return 0;
  const uint32_t samplesPerFrame=ver==3?1152:576;
  const bool mono=((f[3]>>6)&3)==3;
  const size_t crc=(f[1]&1)?0:2;
  const size_t sideInfo=ver==3?(mono?17:32):(mono?9:17);
  const size_t xing=4+crc+sideInfo;
  if(xing+12<=len && (!memcmp(f+xing,"Xing",4) || !memcmp(f+xing,"Info",4))) {
    uint32_t flags=mp3ReadBe32(f+xing+4);
    if((flags&1)!=0) {
      uint32_t frames=mp3ReadBe32(f+xing+8);
      if(frames>0) return (uint32_t)(((uint64_t)frames*samplesPerFrame+sampleRate/2)/sampleRate);
    }
  }
  // Fraunhofer VBRI: 32 Bytes hinter dem MPEG-Header.
  const size_t vbri=4+32;
  if(vbri+18<=len && !memcmp(f+vbri,"VBRI",4)) {
    uint32_t frames=mp3ReadBe32(f+vbri+14);
    if(frames>0) return (uint32_t)(((uint64_t)frames*samplesPerFrame+sampleRate/2)/sampleRate);
  }
  return 0;
}
