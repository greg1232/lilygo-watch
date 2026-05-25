# Design: Playing audio files over WiFi

Status: design exploration — not yet implemented.

## What we want to enable

Beyond recording and replaying short voice memos, we'd like the watch to play **arbitrary audio**: a song clip, an alarm sound, a tone the kid recorded on a phone, a "tap the watch face and it tells you a joke" effect. WiFi opens the door because the watch doesn't need to carry the audio file itself — it can pull from a server.

Three concrete user stories to design against:

1. **Short effect**: Press a button on the watch, hear a 2–5 second sound (a doorbell ding, a "good job" voice clip). Latency matters; bandwidth doesn't.
2. **Long-form play**: Pick a song or a 3-minute story, play it through to the end. Latency tolerable; total bytes matter.
3. **Stream**: Play an internet radio station continuously until you stop it. No fixed length.

These three sound like one feature but they sit on different ends of every tradeoff. We need to choose where on each axis we want to land.

## Hardware constraints

The numbers that decide everything:

| Resource | Available | Comment |
|---|---|---|
| Flash app partition | 3 MB total, ~2.6 MB free | Where the firmware lives |
| Flash FATFS partition | 9.9 MB | Available for persistent files |
| PSRAM | 8 MB | Where big buffers live |
| SRAM | 320 KB usable | Stack / small buffers / DMA |
| WiFi throughput | ~5 Mbps real | More than any audio format needs |
| I2S DAC | MAX98357A | 16-bit, ≤96 kHz, single channel mono |
| CPU | 240 MHz dual-core LX7 | Plenty for any audio decoder |

The single hard limit is the **9.9 MB FATFS partition**. Everything else is in surplus.

## Audio format tradeoffs

How many seconds fit in 9.9 MB:

| Format | Bitrate | Bytes / second | Seconds in 9.9 MB | Use case |
|---|---|---|---|---|
| WAV (16 kHz, 16-bit, mono) | 256 kbps | 32 KB | ~310 sec / 5 min | What `audio_record()` produces |
| WAV (44.1 kHz, 16-bit, stereo) | 1411 kbps | 176 KB | ~56 sec | CD-quality, but speaker is mono |
| MP3 (128 kbps) | 128 kbps | 16 KB | ~620 sec / 10 min | Standard music quality |
| MP3 (64 kbps) | 64 kbps | 8 KB | ~1240 sec / 20 min | "AM radio" quality, fine for kid stories |
| AAC (64 kbps) | 64 kbps | 8 KB | ~1240 sec | Slightly better than MP3 at same bitrate |
| Opus (32 kbps) | 32 kbps | 4 KB | ~2480 sec / 40 min | Best per-bit; great for voice |
| Opus (24 kbps) | 24 kbps | 3 KB | ~3300 sec / 55 min | Voice-only, very compact |

Practical reading: with WAV, the watch can hold a 5-minute clip; with MP3-64 it's 20 minutes; with Opus it's nearly an hour. **Compressed formats let you fit real content.**

Decoding cost:
- WAV: zero — it's raw samples.
- MP3: ~30% of one core for a 128 kbps stream. Easy.
- AAC: similar to MP3.
- Opus: ~50% of one core. Still easy. Best quality-per-bit.

PSRAM (8 MB) can hold a much larger uncompressed buffer than flash — we can decode an entire MP3 file at boot, holding the PCM in PSRAM. But that takes time and battery; better to decode-as-you-play.

## Approach 1 — Download then play

Pull the whole file over HTTPS, store it in FATFS (or PSRAM for one-shot), then play from local storage.

```
[server] ──HTTPS──▶ [watch FATFS] ──decode──▶ [I2S amp]
```

**Good for:**
- Short, frequently-played effects (the doorbell ding, "good job" voice clip).
- Once-pre-loaded content (story tracks, alarm tones, lullabies that should always work even offline).
- Reliable playback — no buffer underruns mid-track.

**Pre-requisites:**
- Mount the FATFS partition (it's already declared in the board config; we just need `FFat.begin()`).
- HTTP/HTTPS client (we already have `HTTPClient` from the time-sync work).
- An MP3 or Opus decoder library.

**Storage budget:**
- Cache ~5–10 effects of ~50 KB each (MP3 64 kbps × ~5 sec): ~500 KB used, plenty of room left.
- Or one 10-minute MP3 of a favorite story: ~5 MB used.

**Cons:**
- Has to be in flash before play. First-time playback waits for download.
- FATFS writes wear flash slowly; not a real concern for kid-scale use.

## Approach 2 — Stream live

Don't store anything. Open a TCP stream, decode chunks as they arrive, push directly to I2S.

```
[server] ──HTTPS──▶ [ring buffer] ──decode──▶ [I2S amp]
                       ▲
                       │
                  network in;
                  audio out drains
```

**Good for:**
- Internet radio (icecast/shoutcast streams).
- Long content where storage would never be enough (a whole album, a podcast).
- Anything the user picks fresh each time.

**Pre-requisites:**
- A ring buffer in PSRAM (~256 KB is plenty — that's ~16 seconds of MP3 128k buffered).
- A decoder that consumes from a stream callback, not a complete buffer.
- Background WiFi to stay connected during the whole play (battery cost).

**Cons:**
- Needs WiFi up the whole time → ~70 mA continuously, vs ~1 mA in light sleep. A 10-minute song costs ~12 mAh; with a typical small wearable battery (~300 mAh), that's 4% per song.
- Buffer underruns produce audible glitches. Need handle-failure logic.
- Latency: first sound after pressing play is ~1–2 seconds (connect + buffer fill).
- Won't work offline.

## Approach 3 — Hybrid (recommended)

Use both, by use case:

- **Short effects (≤30 sec)**: pre-download once to FATFS, cache permanently, play from flash. Sub-second latency, works offline.
- **Long content (songs, stories)**: stream live from a known URL. Skip the storage problem entirely.

A simple API splits cleanly:

```cpp
// Pre-loaded short effect, plays instantly.
audio_play_clip("ding.mp3");

// Streamed long content, takes ~1s to start.
audio_play_stream("https://music.example/song.mp3");
```

Under the hood both paths can share most of the decoder + I2S code.

## Library options

ESP32 has multiple paths:

### ESP32-audioI2S (schreibfaul1)
- Most popular Arduino library for this exact use case.
- One class, `Audio`, with `connecttohost()` for streams and `connecttoFS()` for local files.
- Built-in MP3, AAC, FLAC, Opus, M4A, WAV decoders.
- I2S output baked in — drop-in friendly with our existing MAX98357A wiring.
- Active community, well-maintained.
- Downside: it owns the I2S peripheral — would conflict with the voice memo app's speaker init. Need to coordinate.

### ESP-ADF (Espressif Audio Dev Framework)
- Official, more sophisticated. Pipeline of decoders, mixers, resamplers.
- Heavier — meant for full audio products like smart speakers.
- More to learn; probably overkill.

### Per-codec libraries
- `libhelix-mp3`, `libopus` etc. Smaller and more controllable but you write the I2S streaming yourself.
- Worth considering if we end up wanting tight control over latency or memory.

**Recommendation: ESP32-audioI2S** for v1. Replace it later if it constrains us.

## Memory plan

For the hybrid approach:

```
PSRAM (8 MB):
  256 KB    streaming ring buffer
  64 KB     decoder working memory
  remaining headroom for the rest of the firmware

FATFS (9.9 MB):
  ~500 KB   short effect clips
  remaining unused — could later hold downloaded songs

Flash app (3 MB):
  ~470 KB   current firmware
  + ~150 KB ESP32-audioI2S library
  + ~50 KB  HTTPS code (already there)
  → ~670 KB total, well under 3 MB
```

## UX

A new **Audio** app, swiped to (need to rethink nav with five apps — see "Navigation" below). Initial screen:

```
+----------------------------+
|   Audio                    |
|                            |
|   [ ding ]   [ doorbell ]  |   pre-downloaded clips,
|   [ horn ]   [ laugh ]     |   tap to play instantly
|                            |
|   ─────────────────────    |
|                            |
|   [ Stream: lofi radio ]   |   tap to start streaming;
|   [ Stream: kids stories ] |   tap again to stop
|                            |
+----------------------------+
```

Volume control: maybe the crown becomes a +/- when in this app, or two on-screen buttons.

A second screen could show "now playing" with a progress bar / signal strength for streaming.

## Navigation with five apps

Current grid:
```
        Voice
          ↕
  Memory ↔ Clock ↔ Memory
          ↕
        About
```

Out of room on the 4-directional grid. Two options:

**A) Move About into a long-press of the crown** — frees swipe-down from Clock for Audio.

**B) Switch to linear cycling** — `swipe-left = next app, swipe-right = prev app`. Apps cycle: Clock → Memory → Voice → Audio → About → Clock. Scales to any number of apps. Less direct than the cardinal grid but predictable.

Option B is the more sustainable choice and should probably happen before the 5th app, not after.

## Open questions

These are the design decisions to make before writing code:

1. **Which effects do we want pre-loaded by default?** (Doorbell, "good job", a laugh, a "ta-da!"? Pick 4–8.)
2. **Where do the effects live on a server?** (Just static files in this GitHub repo's `assets/`? A separate small HTTP server?)
3. **How do effects get on the watch the first time?** (Auto-download on first boot when WiFi is up? Or a one-shot "populate" routine?)
4. **What streaming URLs should be pre-set?** (LoFi Girl radio, a kid-friendly story podcast?)
5. **Do we expose volume control?** (Software gain like we have for voice playback, but with a slider.)
6. **Should streaming auto-pause when the screen goes to sleep?** (Probably yes — a 2-min auto-sleep into a still-streaming radio is wasteful and confusing.)
7. **What does "stop" look like for streaming?** (Crown press? Tap anywhere? Specific stop button?)

## Suggested next step

If the design lands, the implementation order I'd recommend:

1. **Mount FATFS** and verify we can read/write files. (Foundation for everything else.)
2. **Install ESP32-audioI2S** and play a hard-coded WAV from FATFS. (Proves the I2S path coexists with the voice-memo app.)
3. **Add a downloader** — fetch one file over HTTPS, save to FATFS. (Builds on the HTTPS time-sync code.)
4. **Build the Audio app** with a single hard-coded clip to start.
5. **Add streaming** as a second pathway.
6. **Pre-load 4–6 effects** from a known URL set.
7. **Switch to linear app navigation** before the app count outgrows the grid.

Each step is a self-contained PR that produces a useful artifact.
