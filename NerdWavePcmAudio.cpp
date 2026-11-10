#include "NerdWaveLog.h"
#include "NerdWavePcmAudio.h"
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstring>

namespace NerdWavePcmAudio {
namespace {
uint8_t* ring = nullptr;
SemaphoreHandle_t ringMutex = nullptr;
size_t readPos = 0, writePos = 0, ringUsed = 0;
constexpr gpio_num_t PIN_BCLK = GPIO_NUM_38;
constexpr gpio_num_t PIN_LRC = GPIO_NUM_39;
constexpr gpio_num_t PIN_DOUT = GPIO_NUM_40;
}
bool nasI2SInstalled = false;

bool begin() {
  if (!ring) ring = static_cast<uint8_t*>(heap_caps_malloc(CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!ringMutex) ringMutex = xSemaphoreCreateMutex();
  return ready();
}
bool ready() { return ring && ringMutex; }
void reset() {
  if (!ready()) return;
  xSemaphoreTake(ringMutex, portMAX_DELAY);
  readPos = writePos = ringUsed = 0;
  xSemaphoreGive(ringMutex);
}
size_t level() {
  if (!ringMutex) return 0;
  xSemaphoreTake(ringMutex, portMAX_DELAY);
  size_t v = ringUsed;
  xSemaphoreGive(ringMutex);
  return v;
}
bool write(const uint8_t* data, size_t count,
           const volatile bool& decodeError, const volatile bool& stopRequested) {
  if (!ready()) return false;
  while (count) {
    xSemaphoreTake(ringMutex, portMAX_DELAY);
    size_t freeBytes = CAPACITY - ringUsed;
    size_t part = std::min(count, std::min(freeBytes, CAPACITY - writePos));
    if (part) {
      memcpy(ring + writePos, data, part);
      writePos = (writePos + part) % CAPACITY;
      ringUsed += part;
    }
    xSemaphoreGive(ringMutex);
    data += part;
    count -= part;
    if (!part) vTaskDelay(pdMS_TO_TICKS(2));
    if (decodeError || stopRequested) return false;
  }
  return true;
}
size_t read(uint8_t* dest, size_t count) {
  if (!ready()) return 0;
  xSemaphoreTake(ringMutex, portMAX_DELAY);
  size_t part = std::min(count, std::min(ringUsed, CAPACITY - readPos));
  if (part) {
    memcpy(dest, ring + readPos, part);
    readPos = (readPos + part) % CAPACITY;
    ringUsed -= part;
  }
  xSemaphoreGive(ringMutex);
  return part;
}

// Originale Legacy-I2S-Konfiguration aus v0.5.113, unveraenderte Parameter.
bool configureLegacyI2S(i2s_port_t port, bool input, uint32_t rate) {
  i2s_config_t cfg = {};
  cfg.mode = input ? (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX)
                   : (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = rate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  esp_err_t e = i2s_driver_install(port, &cfg, 0, nullptr);
  if(e != ESP_OK) { NerdWaveLog::output.printf("[I2S] Port %d install=%d (%s)\n", (int)port, (int)e, esp_err_to_name(e)); return false; }
  i2s_pin_config_t pins = {};
  pins.bck_io_num = (int)(input ? GPIO_NUM_3 : PIN_BCLK);
  pins.ws_io_num = (int)(input ? GPIO_NUM_6 : PIN_LRC);
  pins.data_out_num = input ? I2S_PIN_NO_CHANGE : (int)PIN_DOUT;
  pins.data_in_num = input ? (int)GPIO_NUM_46 : I2S_PIN_NO_CHANGE;
  e = i2s_set_pin(port, &pins);
  if(e != ESP_OK) {
    NerdWaveLog::output.printf("[I2S] Port %d pins=%d (%s)\n", (int)port, (int)e, esp_err_to_name(e));
    i2s_driver_uninstall(port);
    return false;
  }
  return true;
}
bool initOutput(uint32_t sampleRate) {
  if(nasI2SInstalled) return true;
  if(!configureLegacyI2S(I2S_NUM_0, false, sampleRate)) return false;
  nasI2SInstalled = true;
  return true;
}
void shutdownOutput() {
  i2s_driver_uninstall(I2S_NUM_0);
  nasI2SInstalled = false;
}
}
