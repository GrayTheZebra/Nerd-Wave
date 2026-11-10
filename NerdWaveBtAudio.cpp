#include "NerdWaveLog.h"
#include "NerdWaveBtAudio.h"
#include "NerdWaveBtUart.h"
#include "NerdWaveVolume.h"
#include "LedController.h"
#include <esp_heap_caps.h>

namespace NerdWaveBtAudio {
volatile bool btStopRequested=false, btTaskRunning=false, btRxRunning=false;
uint32_t btAudioStartAt=0;
static constexpr gpio_num_t BT_BCLK=GPIO_NUM_3, BT_WS=GPIO_NUM_6, BT_DIN=GPIO_NUM_46;
static volatile bool* playerActiveFlag=nullptr;
static const bool* mp3I2sInstalled=nullptr;
static bool (*i2sConfigure)(i2s_port_t, bool, uint32_t)=nullptr;
static void (*amplifierOff)()=nullptr;
static void (*amplifierOn)()=nullptr;
volatile uint32_t btRxBytes=0,btTxBytes=0,btI2sErrors=0,btUnderruns=0;
static bool btStartI2S() {
  // Wie im nachweislich sauberen Referenztest: RX0 Slave, TX1 Master.
  if(!i2sConfigure(I2S_NUM_0, true, 44100)) return false;
  if(!i2sConfigure(I2S_NUM_1, false, 44100)) {
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_1);
  return true;
}
// Entkoppelt RX (WROOM-Takt) und TX (S3-Takt). RX darf niemals
// durch einen blockierenden I2S-Schreibvorgang ausgebremst werden.
static constexpr size_t BT_BUFFER_SIZE=65536;
static constexpr size_t BT_PREFILL=12288; // ca. 70 ms bei 44,1 kHz Stereo
static constexpr size_t BT_CHUNK=1024;
static uint8_t* btBuffer=nullptr;
static size_t btReadPos=0,btWritePos=0,btUsed=0;
static portMUX_TYPE btBufferMux=portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t btOverflows=0,btStarves=0;
volatile uint32_t btBufferLevel=0;
static void btClearBuffer() {
  portENTER_CRITICAL(&btBufferMux);
  btReadPos=btWritePos=btUsed=0;
  btBufferLevel=0;
  portEXIT_CRITICAL(&btBufferMux);
}
static void btPush(const uint8_t* data,size_t length) {
  length &= ~(size_t)3;
  portENTER_CRITICAL(&btBufferMux);
  // Bei langsamerem TX werden die aeltesten Frames verworfen, nicht neue.
  while(length>BT_BUFFER_SIZE-btUsed) {
    const size_t drop=BT_CHUNK;
    btReadPos=(btReadPos+drop)%BT_BUFFER_SIZE;
    btUsed-=drop;
    btOverflows++;
  }
  const size_t first=min(length,BT_BUFFER_SIZE-btWritePos);
  memcpy(btBuffer+btWritePos,data,first);
  if(first<length)memcpy(btBuffer,data+first,length-first);
  btWritePos=(btWritePos+length)%BT_BUFFER_SIZE;
  btUsed+=length;
  btBufferLevel=btUsed;
  portEXIT_CRITICAL(&btBufferMux);
}
static bool btPop(uint8_t* dst,size_t length) {
  bool ok=false;
  portENTER_CRITICAL(&btBufferMux);
  if(btUsed>=length) {
    const size_t first=min(length,BT_BUFFER_SIZE-btReadPos);
    memcpy(dst,btBuffer+btReadPos,first);
    if(first<length)memcpy(dst+first,btBuffer,length-first);
    btReadPos=(btReadPos+length)%BT_BUFFER_SIZE;
    btUsed-=length;
    btBufferLevel=btUsed;
    ok=true;
  }
  portEXIT_CRITICAL(&btBufferMux);
  return ok;
}
static void btReceiveTask(void*) {
  btRxRunning=true;
  alignas(4) uint8_t pcm[BT_CHUNK];
  while(!btStopRequested) {
    size_t got=0;
    const esp_err_t e=i2s_read(I2S_NUM_0,pcm,sizeof(pcm),&got,pdMS_TO_TICKS(50));
    if(e==ESP_ERR_TIMEOUT){btUnderruns++;continue;}
    if(e!=ESP_OK){btI2sErrors++;vTaskDelay(pdMS_TO_TICKS(1));continue;}
    if(!got)continue;
    btRxBytes+=got;
    // PCM nicht vom asynchron gemeldeten AVRCP-PLAYING-Status abhaengig machen.
    // CONNECTED genuegt: Ein verspaetetes IDLE darf gueltige Samples nicht verwerfen.
    if(NerdWaveBtUart::btConnected)btPush(pcm,got);
    else if(btBufferLevel)btClearBuffer();
  }
  btRxRunning=false;
  vTaskDelete(nullptr);
}
static void btAudioTask(void*) {
  btTaskRunning=true;
  // Der entkoppelte PCM-Ring ist kein I2S-DMA-Puffer. Er darf bei
  // fragmentiertem internem Heap im vorhandenen PSRAM liegen.
  const size_t freeInternal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  const size_t largestInternal=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  btBuffer=(uint8_t*)heap_caps_malloc(BT_BUFFER_SIZE,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(!btBuffer) {
    NerdWaveLog::output.printf("[BT-I2S] Interner 64KB-Block nicht verfuegbar: frei=%u B, groesster Block=%u B; versuche PSRAM\n",
      (unsigned)freeInternal,(unsigned)largestInternal);
    btBuffer=(uint8_t*)heap_caps_malloc(BT_BUFFER_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(btBuffer) NerdWaveLog::output.printf("[BT-I2S] PCM-Ring 64KB im PSRAM reserviert (PSRAM frei=%u B)\n",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  } else {
    NerdWaveLog::output.printf("[BT-I2S] PCM-Ring 64KB intern reserviert (vorher groesster Block=%u B)\n",(unsigned)largestInternal);
  }
  if(!btBuffer){
    NerdWaveLog::output.printf("[BT-I2S] Kein zusammenhaengender 64KB PCM-Puffer verfuegbar: intern=%u B, groesster intern=%u B, groesster PSRAM=%u B\n",
      (unsigned)freeInternal,(unsigned)largestInternal,
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    btI2sErrors++;btTaskRunning=false;vTaskDelete(nullptr);return;
  }
  if(!btStartI2S()) {NerdWaveLog::output.printf("[BT-I2S] Start fehlgeschlagen; Portzustand pruefen, MP3-I2S installiert=%d, Heap=%u\n",(int)*mp3I2sInstalled,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));free(btBuffer);btBuffer=nullptr;btI2sErrors++;amplifierOff();btTaskRunning=false;vTaskDelete(nullptr);return;}
  if(xTaskCreatePinnedToCore(btReceiveTask,"BTRX",4096,nullptr,6,nullptr,0)!=pdPASS) {
    NerdWaveLog::output.println("[BT-I2S] RX-Task fehlt");i2s_driver_uninstall(I2S_NUM_1);i2s_driver_uninstall(I2S_NUM_0);free(btBuffer);btBuffer=nullptr;btI2sErrors++;amplifierOff();btTaskRunning=false;vTaskDelete(nullptr);return;
  }
  NerdWaveLog::output.println("[BT-I2S] Zwei Tasks, LEGACY RX=I2S0 Slave, TX=I2S1 Master, Puffer=64KB, Vorlauf=12KB");
  alignas(4) int16_t pcm[BT_CHUNK/2];
  bool running=false;
  while(!btStopRequested) {
    if(!NerdWaveBtUart::btConnected) {
      if(running){amplifierOff();*playerActiveFlag=false;audioLevelQ15=0;running=false;}
      btClearBuffer();
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if(!running) {
      if(btBufferLevel<BT_PREFILL){vTaskDelay(pdMS_TO_TICKS(4));continue;}
      running=true;amplifierOn();*playerActiveFlag=true;
    }
    if(!btPop((uint8_t*)pcm,sizeof(pcm))) {
      btStarves++;amplifierOff();*playerActiveFlag=false;audioLevelQ15=0;
      running=false;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    // Erst am entkoppelten Ausgang die Analyse und Lautstaerke berechnen.
    uint32_t sum=0,count=0;
    for(size_t i=0;i<BT_CHUNK/2;i+=8){int32_t v=pcm[i];sum+=(uint32_t)(v<0?-v:v);count++;}
    if(count)audioLevelQ15=(uint16_t)min((uint32_t)32767,sum/count);
    spectrumCapture(pcm,sizeof(pcm),44100);
    const int vol=NerdWaveVolume::muted?0:NerdWaveVolume::percent;
    for(size_t i=0;i<BT_CHUNK/2;i++)pcm[i]=(int16_t)(((int32_t)pcm[i]*vol)/100);
    size_t sent=0;
    esp_err_t e=i2s_write(I2S_NUM_1,pcm,sizeof(pcm),&sent,pdMS_TO_TICKS(100));
    btTxBytes+=sent;
    if(e!=ESP_OK || sent!=sizeof(pcm))btI2sErrors++;
  }
  amplifierOff();*playerActiveFlag=false;audioLevelQ15=0;
  while(btRxRunning) vTaskDelay(pdMS_TO_TICKS(5));
  i2s_stop(I2S_NUM_1);
  i2s_driver_uninstall(I2S_NUM_1);
  i2s_driver_uninstall(I2S_NUM_0);
  free(btBuffer);btBuffer=nullptr;
  btTaskRunning=false;
  NerdWaveLog::output.println("[BT-I2S] RX/TX beendet und freigegeben");
  vTaskDelete(nullptr);
}

void requestStop() { btStopRequested=true; }

bool start(volatile bool& active, const bool& mp3Installed,
           bool (*configure)(i2s_port_t, bool, uint32_t),
           void (*off)(), void (*on)()) {
  if(btTaskRunning || btRxRunning || !configure || !off || !on) return false;
  playerActiveFlag=&active;
  mp3I2sInstalled=&mp3Installed;
  i2sConfigure=configure;
  amplifierOff=off;
  amplifierOn=on;
  btStopRequested=false;
  btAudioStartAt=millis();
  return xTaskCreatePinnedToCore(btAudioTask,"NerdBT",8192,nullptr,4,nullptr,0)==pdPASS;
}
} // namespace NerdWaveBtAudio
