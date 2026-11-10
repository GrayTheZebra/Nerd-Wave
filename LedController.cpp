#include "NerdWaveLog.h"
#include "LedController.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

static constexpr uint8_t LED_PIN=1;
static Adafruit_NeoPixel ledStrip(1,LED_PIN,NEO_GRB+NEO_KHZ800);
bool ledEnabled=true;
uint16_t ledCount=60;
uint8_t ledEffect=1; // 0=VU, 1=Dual-VU, 2=Pulse, 3=Spektrum, 4=Dauerlicht, 5=Rainbow, 6=Rainbow Musik, 7=Rainbow alle, 8=Rainbow alle Musik
uint32_t ledSolidColor=0x00BFFF;
uint8_t ledRainbowSpeed=35;
uint8_t ledBrightness=35; // Effekt, Prozent
uint8_t ledBaseBrightness=3; // Standby, Prozent
uint8_t ledSensitivity=60; // 1..100
volatile uint16_t audioLevelQ15=0; // Audio-Task schreibt, loopTask liest
static float ledSmoothed=0.0f,ledPeak=0.0f,ledStandbyBlend=1.0f;
static uint32_t ledFrameAt=0;
// Preferences wird beim Initialisieren durch ledLoad() gesetzt.
static Preferences* ledPrefs = nullptr;
void ledLoad(Preferences& preferences) {
  ledPrefs=&preferences;
  ledEnabled=ledPrefs->getBool("ledOn",true);
  ledCount=constrain((int)ledPrefs->getUShort("ledCount",60),1,(int)LED_MAX);
  ledEffect=min((uint8_t)(LED_EFFECT_COUNT-1),ledPrefs->getUChar("ledEffect",1));
  ledBrightness=min((uint8_t)100,ledPrefs->getUChar("ledBright",35));
  ledBaseBrightness=min((uint8_t)20,ledPrefs->getUChar("ledBase",3));
  ledSensitivity=constrain((int)ledPrefs->getUChar("ledSens",60),1,100);
  ledSolidColor=ledPrefs->getUInt("ledColor",0x00BFFF)&0xFFFFFFUL;
  ledRainbowSpeed=constrain((int)ledPrefs->getUChar("ledSpeed",35),1,100);
}
void ledSave() {
  if (ledPrefs == nullptr) return;
  ledPrefs->putBool("ledOn",ledEnabled);ledPrefs->putUShort("ledCount",ledCount);
  ledPrefs->putUChar("ledEffect",ledEffect);ledPrefs->putUChar("ledBright",ledBrightness);
  ledPrefs->putUChar("ledBase",ledBaseBrightness);ledPrefs->putUChar("ledSens",ledSensitivity);
  ledPrefs->putUInt("ledColor",ledSolidColor);ledPrefs->putUChar("ledSpeed",ledRainbowSpeed);
}
void ledBegin(){
  // Nur einmal zu Beginn die NeoPixel-Hardware initialisieren.
  ledStrip.updateLength(ledCount);
  ledStrip.begin();
  ledStrip.clear();
  ledStrip.show();
  NerdWaveLog::output.printf("[LED] GPIO%d, Anzahl=%u, Effekt=%u\n",LED_PIN,(unsigned)ledCount,ledEffect);
}
void ledResize(){
  // Bei geaenderter Anzahl NUR Puffer anpassen. ESP32-RMT nicht erneut starten.
  // updateLength behaelt die bestehende Pin-/RMT-Initialisierung bei.
  ledStrip.clear();
  ledStrip.show();
  ledStrip.updateLength(ledCount);
  ledStrip.clear();
  ledFrameAt=0;
  NerdWaveLog::output.printf("[LED] Anzahl aktualisiert: %u (ohne erneutes begin)\n",(unsigned)ledCount);
}
// 256-Punkt-FFT: PCM wird zeitgleich mit der I2S-Ausgabe erfasst.
// Kurze Kopie unter Spinlock, eigentliche FFT erst im Arduino-loopTask.
static constexpr uint16_t FFT_N=256;
static portMUX_TYPE spectrumMux=portMUX_INITIALIZER_UNLOCKED;
static int16_t spectrumSamples[FFT_N]={0};
static uint16_t spectrumWrite=0;
static uint32_t spectrumSerial=0;
static uint8_t spectrumDecimPhase=0;
static volatile uint32_t spectrumSampleRate=48000;
static float fftRe[FFT_N],fftIm[FFT_N];
static float spectrumLevel[3]={0,0,0};
static float spectrumAgc[3]={150,100,60};
static uint32_t lastFftSerial=0;
void spectrumCapture(const int16_t* pcm,size_t byteCount,uint32_t sampleRate){
  spectrumSampleRate=sampleRate;
  // Pro Frame einen Mono-Wert; effektiv ~8 kHz Sampling fuer die FFT.
  const uint8_t decimation=sampleRate==44100?5:6;
  size_t frames=byteCount/4;
  for(size_t i=0;i<frames;i++){
    if(++spectrumDecimPhase<decimation)continue;
    spectrumDecimPhase=0;
    int32_t mono=((int32_t)pcm[2*i]+(int32_t)pcm[2*i+1])/2;
    portENTER_CRITICAL(&spectrumMux);
    spectrumSamples[spectrumWrite++]=(int16_t)mono;
    if(spectrumWrite==FFT_N){spectrumWrite=0;spectrumSerial++;}
    portEXIT_CRITICAL(&spectrumMux);
  }
}
static void spectrumAnalyze(bool active){
  if(!active){for(int b=0;b<3;b++)spectrumLevel[b]*=0.87f;return;}
  uint32_t serial;
  portENTER_CRITICAL(&spectrumMux);
  serial=spectrumSerial;
  if(serial!=lastFftSerial){
    // Letzte 256 Samples aus dem Ring in zeitlicher Reihenfolge.
    for(uint16_t i=0;i<FFT_N;i++)fftRe[i]=(float)spectrumSamples[(spectrumWrite+i)&(FFT_N-1)];
  }
  portEXIT_CRITICAL(&spectrumMux);
  if(serial==lastFftSerial)return;
  lastFftSerial=serial;
  for(uint16_t i=0;i<FFT_N;i++){
    fftRe[i]*=0.5f-0.5f*cosf(6.28318530718f*i/(FFT_N-1)); // Hann-Fenster
    fftIm[i]=0;
  }
  // Iterative Radix-2-FFT, keine zusaetzliche Bibliothek erforderlich.
  for(uint16_t i=1,j=0;i<FFT_N;i++){
    uint16_t bit=FFT_N>>1;
    for(;j&bit;bit>>=1)j^=bit;
    j^=bit;
    if(i<j){float t=fftRe[i];fftRe[i]=fftRe[j];fftRe[j]=t;}
  }
  for(uint16_t len=2;len<=FFT_N;len<<=1){
    float theta=-6.28318530718f/len;
    float wlenR=cosf(theta),wlenI=sinf(theta);
    for(uint16_t i=0;i<FFT_N;i+=len){
      float wr=1,wi=0;
      for(uint16_t j=0;j<len/2;j++){
        uint16_t a=i+j,b=a+len/2;
        float tr=wr*fftRe[b]-wi*fftIm[b];
        float ti=wr*fftIm[b]+wi*fftRe[b];
        fftRe[b]=fftRe[a]-tr;fftIm[b]=fftIm[a]-ti;
        fftRe[a]+=tr;fftIm[a]+=ti;
        float next=wr*wlenR-wi*wlenI;
        wi=wr*wlenI+wi*wlenR;wr=next;
      }
    }
  }
  float bandEnergy[3]={0,0,0};uint16_t bandBins[3]={0,0,0};
  const uint32_t sampleRate=spectrumSampleRate;
  const float fftRate=(float)sampleRate/(sampleRate==44100?5.0f:6.0f);
  for(uint16_t k=2;k<FFT_N/2;k++){
    float hz=k*fftRate/FFT_N;
    uint8_t band=hz<220?0:hz<1600?1:2;
    // Mittlere Spektralamplitude je Frequenzband.
    bandEnergy[band]+=sqrtf(fftRe[k]*fftRe[k]+fftIm[k]*fftIm[k]);
    bandBins[band]++;
  }
  for(int b=0;b<3;b++){
    float energy=bandBins[b]?bandEnergy[b]/bandBins[b]:0;
    // Automatische, langsam abfallende Bandreferenz; sanfte Attack/Release.
    spectrumAgc[b]=fmaxf(70.0f,fmaxf(energy,spectrumAgc[b]*0.994f));
    float level=fminf(1.0f,energy/fmaxf(70.0f,spectrumAgc[b])*0.88f);
    spectrumLevel[b]+=(level>spectrumLevel[b]?0.52f:0.18f)*(level-spectrumLevel[b]);
  }
}
void ledTick(bool active){
  if((uint32_t)(millis()-ledFrameAt)<40)return;
  ledFrameAt=millis();
  // In Pause/Stop keine Musikeffekte mit 0 Pegel rendern: die eingestellte
  // Grundhelligkeit gilt fuer ALLE Effekte, auch Rainbow-Musik (6/8).
  // Bei abgeschalteten LEDs (z.B. Shutdown) bleibt der Stripe dunkel.
  if(!active) {
    const float base=ledEnabled?(ledBaseBrightness/100.0f):0.0f;
    const uint8_t r=(uint8_t)(255.0f*base);
    const uint8_t g=(uint8_t)(190.0f*base);
    const uint8_t b=(uint8_t)(115.0f*base);
    for(uint16_t i=0;i<ledStrip.numPixels();i++) ledStrip.setPixelColor(i,r,g,b);
    ledStrip.show();
    ledSmoothed=0.0f;
    ledPeak=0.0f;
    ledStandbyBlend=1.0f;
    return;
  }
  spectrumAnalyze(active && ledEffect==3);
  float raw=(float)audioLevelQ15/32767.0f;
  // 1..100 Empfindlichkeit, bei 60 etwa Faktor 3.
  float gain=0.6f+(float)ledSensitivity*0.04f;
  float target=active?fminf(1.0f,raw*gain):0.0f;
  ledSmoothed+=(target>ledSmoothed?0.45f:0.11f)*(target-ledSmoothed);
  ledPeak=fmaxf(ledSmoothed,ledPeak-0.014f);
  float blendTarget=(active && ledEnabled)?0.0f:1.0f;
  ledStandbyBlend+=0.045f*(blendTarget-ledStandbyBlend);
  float effectFactor=(1.0f-ledStandbyBlend)*(ledEnabled?1.0f:0.0f);
  float baseFactor=ledStandbyBlend*(ledEnabled?1.0f:0.0f);
  float intensity=ledBrightness/100.0f;
  float ambient=ledBaseBrightness/100.0f;
  uint16_t n=ledStrip.numPixels();
  if(ledEffect>=4 && ledEffect<LED_EFFECT_COUNT){
    // Eigene Beleuchtungsmodi: ohne Musik auch bei gestoppter Wiedergabe sichtbar.
    // Kein Wechsel am I2S-/Decoder-Pfad, nur LED-Ausgabe.
    float power=ledEnabled?intensity:0.0f;
    if(ledEffect==6 || ledEffect==8){
      // Musikmodus: Pegel steuert die Helligkeit, bei Stille aus.
      power*=active?fminf(1.0f,ledSmoothed*1.8f):0.0f;
    }
    const uint8_t r=(ledSolidColor>>16)&0xff;
    const uint8_t g=(ledSolidColor>>8)&0xff;
    const uint8_t b=ledSolidColor&0xff;
    // Geschwindigkeit von ca. 0.6 bis 10.5 Umlaeufen je Minute.
    const uint32_t cycleMs=600000UL/(uint32_t)(ledRainbowSpeed+5);
    const uint32_t phase=(millis()%cycleMs)*65536UL/cycleMs;
    for(uint16_t i=0;i<n;i++){
      uint32_t rgb;
      if(ledEffect==4)rgb=ledStrip.Color(r,g,b);
      else rgb=ledStrip.gamma32(ledStrip.ColorHSV((uint16_t)(phase+((ledEffect==7 || ledEffect==8)?0:((uint32_t)i*65536UL/n)))));
      uint8_t rr=(uint8_t)(((rgb>>16)&0xff)*power);
      uint8_t gg=(uint8_t)(((rgb>>8)&0xff)*power);
      uint8_t bb=(uint8_t)((rgb&0xff)*power);
      ledStrip.setPixelColor(i,rr,gg,bb);
    }
    ledStrip.show();
    return;
  }
  for(uint16_t i=0;i<n;i++) {
    float pos=(i+0.5f)/(float)n;
    float level=ledSmoothed;
    bool lit=false;
    if(ledEffect==0) lit=pos<level;
    else if(ledEffect==1) {
      // Zwei identische Halbkreise: je ein Balken ab Mittelpunkt.
      float local=pos<0.5f?pos*2.0f:(1.0f-pos)*2.0f;
      lit=local<level;
    } else lit=true;
    float frac=ledEffect==2?ledSmoothed*ledSmoothed:(lit?1.0f:0.0f);
    if(ledEffect==3){
      // Drei Farbsektoren auf dem LED-Ring: Bass, Mitten, Hoehen.
      uint8_t band=(uint32_t)i*3/n;
      if(band>2)band=2;
      float bandPos=((float)((uint32_t)i*3 % n)+0.5f)/(float)n;
      frac=bandPos<fminf(1.0f,spectrumLevel[band]*(0.6f+ledSensitivity*0.018f))?1.0f:0.025f;
    }
    // Pegel-Farbskala: cyan/gruen bis gelb/rot bei hohen Pegeln.
    uint8_t r=level>0.83f?255:level>0.55f?220:20;
    uint8_t g=level>0.83f?35:level>0.55f?170:210;
    uint8_t b=level>0.55f?10:130;
    if(ledEffect==3){
      uint8_t band=(uint32_t)i*3/n;
      if(band==0){r=255;g=72;b=12;}       // Bass: Orange
      else if(band==1){r=18;g=220;b=82;}  // Mitten: Gruen
      else {r=25;g=115;b=255;}            // Hoehen: Blau
    }
    float e=effectFactor*intensity*frac;
    // Warmweiss als RGB-Mischung 255/190/115.
    uint8_t rr=(uint8_t)fminf(255.0f,r*e+255.0f*ambient*baseFactor);
    uint8_t gg=(uint8_t)fminf(255.0f,g*e+190.0f*ambient*baseFactor);
    uint8_t bb=(uint8_t)fminf(255.0f,b*e+115.0f*ambient*baseFactor);
    ledStrip.setPixelColor(i,rr,gg,bb);
  }
  ledStrip.show();
}

