// Nerd-Wave Bluetooth -> I2S: ESP32-WROOM-32 (Arduino ESP32 3.3.x)
// Voraussetzung: ESP32-A2DP von pschatzmann, klassischer ESP32 (NICHT S3).
// v0.5.60: AVRCP per UART; Bluetooth-Stack bleibt bei Quellenwechsel aktiv.
#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include "esp_gap_bt_api.h"
#include "driver/i2s.h"

constexpr int UART_RX=16, UART_TX=17;
constexpr int I2S_BCLK=26, I2S_WS=25, I2S_DOUT=22;
constexpr uint32_t AUDIO_RATE=44100; // A2DP-Sink Test mit 44,1 kHz SBC
HardwareSerial btUart(2);
BluetoothA2DPSink sink;
volatile bool btConnected=false, btPlaying=false;
bool pairing=false;
String commandLine;

// Kurze PCM-Schreibvorgaenge im Callback; Audio pruefen, bevor wir auf Queues umstellen.
volatile uint32_t audioBytes=0, audioErrors=0;
void pcmReceived(const uint8_t* pcm, uint32_t len) {
  if (!pcm || !len) return;
  size_t written=0;
  // Unterbrechung vermeiden: kein Serial, kein dynamischer Speicher im Callback.
  esp_err_t err=i2s_write(I2S_NUM_0, pcm, len, &written, pdMS_TO_TICKS(20));
  audioBytes += written;
  if (err!=ESP_OK || written!=len) audioErrors++;
}
void report(const char* message) {
  btUart.println(message);
  Serial.print("[BT] "); Serial.println(message);
}
void onConnection(esp_a2d_connection_state_t state, void*) {
  btConnected=state==ESP_A2D_CONNECTION_STATE_CONNECTED;
  if(!btConnected) btPlaying=false;
}
void onAudio(esp_a2d_audio_state_t state, void*) {
  btPlaying=state==ESP_A2D_AUDIO_STATE_STARTED;
}
void setPairing(bool enabled) {
  esp_err_t err=esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
       enabled ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE);
  if(err!=ESP_OK) { report("ERR:PAIR_MODE"); return; }
  pairing=enabled;
  report(enabled?"PAIRING:ON":"PAIRING:OFF");
}
void processCommand(const String& cmd) {
  if(cmd=="PAIR_ON") setPairing(true);
  else if(cmd=="PAIR_OFF") setPairing(false);
  else if(cmd=="STATUS") {
    report("READY"); report(pairing?"PAIRING:ON":"PAIRING:OFF");
    report(btConnected?"CONNECTED":"DISCONNECTED"); report(btPlaying?"PLAYING":"IDLE");
  } else if(cmd=="NEXT" || cmd=="PREV" || cmd=="TOGGLE" || cmd=="PLAY" || cmd=="PAUSE") {
    if(!btConnected) { report("ERR:NOT_CONNECTED"); return; }
    if(cmd=="NEXT") sink.next();
    else if(cmd=="PREV") sink.previous();
    else if(cmd=="PLAY") sink.play();
    else if(cmd=="PAUSE") sink.pause();
    else if(btPlaying) sink.pause();
    else sink.play();
  } else if(cmd=="DISCONNECT") {
    // Kompatibilitaet: S3 v0.5.59 sendet DISCONNECT beim Quellenwechsel.
    // sink.stop() wuerde den Bluetooth-Stack beenden und einen Reboot erfordern.
    if(btConnected) sink.pause();
    report("INFO:PAUSE_SENT");
  } else report("ERR:UNKNOWN_COMMAND");
}
void setup() {
  Serial.begin(115200);
  btUart.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  delay(300);
  Serial.println("=== NERD-WAVE BT I2S WROOM32 ===");
  i2s_config_t config={};
  config.mode=(i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate=AUDIO_RATE;
  config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format=I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags=0;
  config.dma_buf_count=8;
  config.dma_buf_len=256;
  config.use_apll=false;
  config.tx_desc_auto_clear=true;
  esp_err_t e=i2s_driver_install(I2S_NUM_0, &config, 0, nullptr);
  if(e!=ESP_OK) { Serial.printf("[I2S] driver_install Fehler=%d\n", e); while(true) delay(1000); }
  i2s_pin_config_t pins={};
  pins.bck_io_num=I2S_BCLK;
  pins.ws_io_num=I2S_WS;
  pins.data_out_num=I2S_DOUT;
  pins.data_in_num=I2S_PIN_NO_CHANGE;
  e=i2s_set_pin(I2S_NUM_0, &pins);
  if(e!=ESP_OK) { Serial.printf("[I2S] set_pin Fehler=%d\n",e); while(true) delay(1000); }
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("[I2S] MASTER 44100 Hz / 16 Bit Stereo; GPIO26/25/22");
  sink.set_auto_reconnect(false);
  sink.set_stream_reader(pcmReceived, false);
  sink.set_on_connection_state_changed(onConnection);
  sink.set_on_audio_state_changed(onAudio);
  sink.start("Nerd-Wave");
  setPairing(false);
  report("READY");
}
void loop() {
  while(btUart.available()) {
    char c=(char)btUart.read();
    if(c=='\n') {commandLine.trim(); if(commandLine.length()) processCommand(commandLine); commandLine="";}
    else if(c!='\r' && commandLine.length()<80) commandLine+=c;
  }
  static int prevState=-1;
  int state=int(btConnected)|(int(btPlaying)<<1);
  if(state!=prevState) {
    report(btConnected?"CONNECTED":"DISCONNECTED");
    report(btPlaying?"PLAYING":"IDLE");
    prevState=state;
  }
  static uint32_t last=0;
  if(millis()-last>=5000) {
    last=millis();
    Serial.printf("[I2S] PCM gesendet=%lu KB Fehler=%lu\n", (unsigned long)(audioBytes/1024), (unsigned long)audioErrors);
  }
  delay(5);
}
