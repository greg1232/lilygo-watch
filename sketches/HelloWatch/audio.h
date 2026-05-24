#pragma once

#include <stdint.h>
#include <stddef.h>

// Recording configuration — keep small enough to fit in regular heap if
// PSRAM allocation fails (5s @ 16 kHz @ 16-bit mono = 160 KB).
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_REC_SECONDS 5

// One-shot init of the PDM mic (I2S0) and PCM speaker (I2S1).
// Call once from setup() after hardware_init().
void audio_init();

// Blocking record: captures AUDIO_REC_SECONDS of audio into an internal
// WAV-formatted buffer. Returns true on success. Overwrites any prior
// recording.
bool audio_record();

// Blocking playback of the current recording. Returns false if nothing
// has been recorded yet.
bool audio_play();

bool audio_has_recording();
void audio_clear();
