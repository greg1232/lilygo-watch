#include "audio.h"
#include <Arduino.h>
#include <ESP_I2S.h>

// Pin definitions come from the lilygo_twatch_s3 board variant:
//   MIC_SCK=44, MIC_DAT=47  (PDM microphone, SPM1423HM4H-B)
//   I2S_BCLK=48, I2S_WCLK=15, I2S_DOUT=46  (MAX98357A class-D amp)

static I2SClass MIC;
static I2SClass SPK;
static bool ready = false;

static uint8_t *recording = nullptr;
static size_t recording_len = 0;

void audio_init() {
  // PDM mic on I2S0
  MIC.setPinsPdmRx(MIC_SCK, MIC_DAT);
  if (!MIC.begin(I2S_MODE_PDM_RX, AUDIO_SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Mic (PDM RX) init FAILED");
    return;
  }

  // PCM speaker on I2S1
  SPK.setPins(I2S_BCLK, I2S_WCLK, I2S_DOUT);
  if (!SPK.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO,
                 I2S_STD_SLOT_LEFT)) {
    Serial.println("Speaker (I2S TX) init FAILED");
    return;
  }

  ready = true;
  Serial.println("Audio ready (16 kHz / 16-bit / mono)");
}

bool audio_record() {
  if (!ready) return false;
  audio_clear();
  recording = MIC.recordWAV(AUDIO_REC_SECONDS, &recording_len);
  if (recording == nullptr) {
    Serial.println("recordWAV returned null");
    return false;
  }
  Serial.printf("Recorded %u bytes (%us)\n",
                (unsigned)recording_len, (unsigned)AUDIO_REC_SECONDS);
  return true;
}

bool audio_play() {
  if (!ready || recording == nullptr) return false;
  Serial.printf("Playing %u bytes\n", (unsigned)recording_len);
  SPK.playWAV(recording, recording_len);
  return true;
}

bool audio_has_recording() {
  return recording != nullptr;
}

void audio_clear() {
  if (recording) {
    free(recording);
    recording = nullptr;
    recording_len = 0;
  }
}
