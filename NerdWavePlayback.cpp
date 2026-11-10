#include "NerdWaveLog.h"
#include "NerdWavePlayback.h"
#include "NerdWavePcmAudio.h"
#include "driver/i2s.h"
#include <cstring>
#include <algorithm>

namespace NerdWavePlayback {
namespace {
constexpr size_t CHUNK_BYTES = 4096;
constexpr size_t PREBUFFER_BYTES = 128 * 1024;
constexpr size_t RING_CAPACITY = NerdWavePcmAudio::CAPACITY;
// Bestehender statischer DMA-Zwischenpuffer, unveraendert 4096 Bytes.
uint8_t playbackBuf[CHUNK_BYTES];
}
void playbackTask(void* parameter) {
  auto* ctx = static_cast<Context*>(parameter);
  auto& playerRunning = ctx->playerRunning;
  auto& stopRequested = ctx->stopRequested;
  auto& decodeDone = ctx->decodeDone;
  auto& decodeError = ctx->decodeError;
  auto& audioSampleRateReady = ctx->audioSampleRateReady;
  auto& audioSampleRate = ctx->audioSampleRate;
  auto& playerActive = ctx->playerActive;
  auto& playbackPaused = ctx->playbackPaused;
  auto& pauseRequested = ctx->pauseRequested;
  auto& i2sErrors = ctx->i2sErrors;
  auto& bytesPlayed = ctx->bytesPlayed;
  auto& underruns = ctx->underruns;
  auto& maxI2sWriteUs = ctx->maxI2sWriteUs;
  auto& muted = ctx->muted;
  auto& volumePercent = ctx->volumePercent;
  auto& audioLevelQ15 = ctx->audioLevelQ15;
  auto& autoNextEnabled = ctx->autoNextEnabled;
  auto& autoNextPending = ctx->autoNextPending;

  playerRunning = true;
  ctx->ampOff();
  NerdWaveLog::output.println("[PLAY] Warte auf 128 KB PCM-Vorlauf...");
  while (!stopRequested && !decodeDone && NerdWavePcmAudio::level() < PREBUFFER_BYTES) vTaskDelay(pdMS_TO_TICKS(10));
  if (stopRequested || decodeError || NerdWavePcmAudio::level() == 0 || !audioSampleRateReady) {
    NerdWaveLog::output.println("[PLAY] Keine Daten / Decoderfehler"); playerRunning=false; vTaskDelete(nullptr); return;
  }
  NerdWaveLog::output.printf("[I2S] Starte mit %lu Hz\n", (unsigned long)audioSampleRate);
  if (!NerdWavePcmAudio::initOutput(audioSampleRate)) { decodeError = true; playerRunning=false; vTaskDelete(nullptr); return; }
  playerActive = true;
  playbackPaused = false;
  ctx->ampOn();
  NerdWaveLog::output.println("[PLAY] I2S gestartet, NAS-Wiedergabe laeuft");
  uint32_t startMs = millis(), lastReport = millis();
  bool waitingForData = false;
  bool bufferingStoppedI2s = false;
  uint32_t rebufferEvents = 0;
  // Bei leerem Ring nach kurzen Netzwerkstockungen erst wieder Vorlauf sammeln.
  // Kleine Restmengen am Titelende duerfen dennoch abgespielt werden.
  constexpr size_t REBUFFER_BYTES = 64 * 1024;
  size_t aligned = 0;
  while (!stopRequested) {
    if (pauseRequested) {
      if (!playbackPaused) {
        // Nur die Audio-Task darf den I2S-Treiber anhalten.
        ctx->ampOff();
        if (!bufferingStoppedI2s) i2s_stop(I2S_NUM_0);
        playbackPaused = true;
        playerActive = false;
        NerdWaveLog::output.println("[PLAY] Pause");
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (playbackPaused) {
      esp_err_t resumed = bufferingStoppedI2s ? ESP_OK : i2s_start(I2S_NUM_0);
      if (resumed != ESP_OK) {
        NerdWaveLog::output.printf("[I2S] Resume fehlgeschlagen: %d\n", resumed);
        ++i2sErrors; decodeError = true; break;
      }
      playbackPaused = false;
      playerActive = true;
      if (!bufferingStoppedI2s) ctx->ampOn();
      NerdWaveLog::output.println("[PLAY] Weiter");
    }
    // Laufender Stream leer? Nicht jeden kleinen Netzwerkburst einzeln
    // abspielen: I2S pausieren und zuerst wieder 64 KB PCM sammeln.
    // Die bereits im playbackBuf liegenden Samples bleiben erhalten.
    if (waitingForData && !decodeDone && NerdWavePcmAudio::level() < REBUFFER_BYTES) {
      if (!bufferingStoppedI2s) {
        ctx->ampOff();
        i2s_stop(I2S_NUM_0);
        bufferingStoppedI2s = true;
        ++rebufferEvents;
        NerdWaveLog::output.printf("[REBUFFER] NAS/Audio-Puffer leer, Ereignis=%lu, Ring=%u KB\n",
                      (unsigned long)rebufferEvents, (unsigned)(NerdWavePcmAudio::level()/1024));
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (bufferingStoppedI2s) {
      if (decodeError || (decodeDone && NerdWavePcmAudio::level()==0 && aligned==0)) break;
      esp_err_t resumeError=i2s_start(I2S_NUM_0);
      if (resumeError!=ESP_OK) {
        NerdWaveLog::output.printf("[REBUFFER] I2S-Neustart fehlgeschlagen: %d\n",(int)resumeError);
        ++i2sErrors;decodeError=true;break;
      }
      i2s_zero_dma_buffer(I2S_NUM_0);
      ctx->ampOn();
      bufferingStoppedI2s=false;
      waitingForData=false;
      NerdWaveLog::output.printf("[REBUFFER] Fortsetzung mit %u KB PCM\n",(unsigned)(NerdWavePcmAudio::level()/1024));
    }
    size_t got = NerdWavePcmAudio::read(playbackBuf + aligned, CHUNK_BYTES - aligned);
    aligned += got;
    if (aligned == CHUNK_BYTES || (decodeDone && NerdWavePcmAudio::level() == 0 && aligned > 0)) {
      // Lautstaerke im internen DMA-Zwischenpuffer anwenden.
      // Keine Rueckwirkung auf den PCM-Ring im PSRAM.
      // Pegel aus tatsaechlich abgespieltem PCM VOR der Lautstaerkeregelung.
      // Nur ein Wert pro I2S-Block, keine LED-/SPI-Operation im Audio-Task.
      uint32_t levelSum=0;uint32_t levelSamples=0;
      const int16_t *levelPCM=reinterpret_cast<const int16_t*>(playbackBuf);
      for(size_t j=0;j<aligned/sizeof(int16_t);j+=8){
        int32_t v=levelPCM[j];levelSum+=(uint32_t)(v<0?-v:v);++levelSamples;
      }
      if(levelSamples)audioLevelQ15=(uint16_t)min((uint32_t)32767,levelSum/levelSamples);
      ctx->captureSpectrum(levelPCM,aligned,audioSampleRate);
      const int vol = muted ? 0 : volumePercent;
      int16_t* pcm16 = reinterpret_cast<int16_t*>(playbackBuf);
      for (size_t i = 0; i < aligned / sizeof(int16_t); ++i) {
        pcm16[i] = (int16_t)(((int32_t)pcm16[i] * vol) / 100);
      }
      size_t offset = 0;
      while (offset < aligned) {
        size_t sent = 0;
        uint32_t began = micros();
        esp_err_t err = i2s_write(I2S_NUM_0, playbackBuf + offset, aligned - offset, &sent, pdMS_TO_TICKS(1000));
        uint32_t took = micros() - began;
        if (took > maxI2sWriteUs) maxI2sWriteUs = took;
        if (err != ESP_OK || !sent) { ++i2sErrors; decodeError = true; break; }
        offset += sent;
      }
      bytesPlayed += offset;
      aligned = 0;
      waitingForData = false;
      if (decodeError) break;
    } else if (!got) {
      if (decodeDone && NerdWavePcmAudio::level() == 0) break;
      if (!waitingForData) { ++underruns; waitingForData = true; }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (millis() - lastReport >= 5000) {
      lastReport = millis();
      NerdWaveLog::output.printf("[STATUS] Zeit=%.1f s | Ring=%u/%u KB | Underruns=%lu | I2S=%lu | MaxWrite=%lu us\n",
        bytesPlayed / (audioSampleRate*4.0f), (unsigned)(NerdWavePcmAudio::level()/1024),
        (unsigned)(RING_CAPACITY/1024), (unsigned long)underruns,
        (unsigned long)i2sErrors, (unsigned long)maxI2sWriteUs);
    }
  }
  // Der Rebuffer-Zweig kann I2S gestoppt haben. Vor dem abschliessenden
  // Nullblock fuer einheitlichen Cleanup wieder aktivieren.
  if (bufferingStoppedI2s) {
    i2s_start(I2S_NUM_0);
    bufferingStoppedI2s=false;
  }
  // Automatische Fortsetzung nur bei vollstaendigem, fehlerfreiem Titel.
  // Browser-/Titelwechsel setzt stopRequested und unterdrueckt Auto-Next.
  const bool naturalEnd = !stopRequested && !decodeError && decodeDone && NerdWavePcmAudio::level() == 0;
  ctx->ampOff();
  // Wird z.B. beim Titelwechsel direkt aus Pause heraus gestoppt.
  if (playbackPaused) {
    i2s_start(I2S_NUM_0);
    playbackPaused = false;
  }
  memset(playbackBuf, 0, sizeof(playbackBuf));
  size_t sent = 0;
  i2s_write(I2S_NUM_0, playbackBuf, sizeof(playbackBuf), &sent, pdMS_TO_TICKS(1000));
  i2s_stop(I2S_NUM_0);
  NerdWavePcmAudio::shutdownOutput();
  playerActive = false;
  pauseRequested = false;
  NerdWaveLog::output.printf("[PLAY] Fertig: %llu PCM-Bytes, %.2f s, Laufzeit %lu ms, Underruns=%lu, I2S-Fehler=%lu, Rebuffer=%lu\n",
    (unsigned long long)bytesPlayed, bytesPlayed/(audioSampleRate*4.0f),
    (unsigned long)(millis()-startMs), (unsigned long)underruns,
    (unsigned long)i2sErrors, (unsigned long)rebufferEvents);
  if (naturalEnd && autoNextEnabled) {
    autoNextPending = true;
    NerdWaveLog::output.println("[AUTO] Titel beendet, naechster Titel angefordert");
  }
  playerRunning = false;
  vTaskDelete(nullptr);
}

}
