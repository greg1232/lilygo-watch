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

// Playback gain. The PDM mic captures at a low level and the MAX98357A's
// hardware gain pin is fixed on this board, so we amplify in software.
// Each sample is multiplied by PLAYBACK_GAIN and clipped to int16 range.
// Tune in 1-step increments — too high gives distortion on loud passages.
#define PLAYBACK_GAIN 4

bool audio_play() {
  if (!ready || recording == nullptr) return false;
  Serial.printf("Playing %u bytes (gain %dx)\n",
                (unsigned)recording_len, PLAYBACK_GAIN);

  // Copy the recording into a scratch buffer and amplify the PCM samples.
  // We don't modify `recording` so the next play sounds the same as this one
  // instead of compounding the gain.
  uint8_t *scratch = (uint8_t *)ps_malloc(recording_len);
  if (!scratch) scratch = (uint8_t *)malloc(recording_len);
  if (!scratch) {
    Serial.println("Gain alloc failed — playing un-amplified");
    SPK.playWAV(recording, recording_len);
    return true;
  }
  memcpy(scratch, recording, recording_len);

  // Skip the 44-byte WAV header, then apply gain to each 16-bit sample.
  int16_t *samples = (int16_t *)(scratch + 44);
  size_t n_samples = (recording_len - 44) / sizeof(int16_t);
  for (size_t i = 0; i < n_samples; i++) {
    int32_t s = (int32_t)samples[i] * PLAYBACK_GAIN;
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    samples[i] = (int16_t)s;
  }

  SPK.playWAV(scratch, recording_len);
  free(scratch);
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
