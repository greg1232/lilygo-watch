# Apps

Four apps as of this writing. Each is a single `.cpp` file implementing the contract from `apps.h`: `<name>_enter()`, `<name>_tick()`, and optionally `<name>_tap(x, y)`.

## Navigation summary

```
                 Voice            (swipe up from clock)
                   ↕
   Memory  ←→  Clock              (swipe left/right)
                   ↕
                 About            (swipe down from clock)
```

The crown button toggles screen on/off; it doesn't change apps.

---

## Clock — `app_clock.cpp`

The default app. Shown on boot.

### Layout (240×240)

```
+--------------------------------+
|     Pacific Time      [BATT]   |   y=20 (header) + y=12 battery icon
|                                |
|                                |
|          1:42                  |   FreeMono24pt7b, y baseline=128
|                                |
|         07 PM                  |   default font @ size 3, y top=155
|                                |
|       Sun May 24 2026          |   default font @ size 2, y top=210
+--------------------------------+
```

### State

- `clock_last_second`, `clock_last_minute` track what's currently on screen so we only redraw on change.
- The seconds line redraws every second; the big HH:MM and the date redraw on the minute boundary.
- The battery icon refreshes on seconds 0 and 30 of each minute (twice a minute).

### Why a monospaced font for HH:MM

Proportional fonts shift centering between ticks (`1:11` is narrower than `1:22`), and the small per-tick centering shifts leave residual pixels at the edges that look like the digit is overdrawn. `FreeMono24pt7b` has uniform glyph widths, so the centered position is stable.

### Time source

`BUILD_EPOCH` is set at compile time by `flash.sh`. POSIX TZ string `PST8PDT,M3.2.0,M11.1.0` makes `localtime_r` produce correct Pacific time year-round including DST.

There's no RTC persistence yet — every reflash re-bakes the epoch. Once we wire up the onboard PCF8563, this stops mattering.

---

## Memory — `app_memory.cpp`

4×3 grid of cards. Tap to flip a card; tap a second to reveal; matches lock with a green border; mismatches flip back after ~900 ms. Find all 6 pairs to win.

### Layout

```
+--------------------------------+
|   ♥  |  ★  |  ☀  |  ☾  |        4 columns × 3 rows
|------+-----+-----+-----|
|   ☘  |  ◆  |  ?  |  ?  |        Each cell is 60 × 80 px
|------+-----+-----+-----|
|   ?  |  ?  |  ?  |  ?  |
+------+-----+-----+-----+
```

Cards face-down show a concentric-circle pattern in dark blue-grey. Face-up shows the symbol in its accent color. Matched cards keep their symbol visible plus a double-thickness green border.

### Symbols

All six are drawn from primitives (no bitmaps): heart, star (4-point), sun (circle + 8 rays), moon (crescent via two circles), clover (4-circle shamrock with stem), diamond (two filled triangles + highlight line).

### State machine

- `mem_first`, `mem_second` — indices of the up-to-two currently-flipped cards.
- `mem_resolve_at` — millis() timestamp when a mismatched pair should flip back; 0 means idle.
- `mem_matched_pairs` — counts wins.

### Match feedback

- **Per match**: DRV2605 effect `47` ("Buzz 1 - 100%") — the longest sustained ERM buzz.
- **Win**: a four-step sequence `{14, 14, 14, 70}` (three strong buzzes + a sharp ramp-up), then a rainbow celebration animation, then a reshuffle.

### Re-entry behavior

Switching back to Memory after going to Clock/Voice/About reshuffles the deck. State is not preserved across app switches in v1.

---

## Voice — `app_voice.cpp`

Tap to record 5 seconds, tap to play back. One recording at a time, held in RAM.

### Layout

```
+--------------------------------+
|         Voice Memo             |   y=18 title
|                                |
|                                |
|          ( REC )               |   big circular button, radius 55, y=120
|                                |
|                                |
|            X                   |   smaller clear button (only when there's a recording)
|                                |
|     tap to record (5s)         |   status line, y=232
+--------------------------------+
```

### State machine

```
       tap big
V_IDLE_EMPTY  ────────────────────→ V_RECORDING
       ↑                                  │ blocking 5s
       │                                  ↓
       │           tap big          V_IDLE_HAS_REC ─── tap X ─→ (back to IDLE_EMPTY)
       │                                  │
       │            blocking ~5s          ↓
       └────── V_PLAYING ←────── tap big
```

- **REC** button is red when idle-empty and during recording. Green/"PLAY" when there's a recording.
- **X** button only appears in `IDLE_HAS_REC`. Tap it (or its 8 px halo) to discard and re-record.
- The big button has a hit area of exactly its visible radius (55 px).

### Audio details

- Format: 16 kHz, 16-bit, mono. ~32 KB/sec.
- 5-second clip stored as a complete WAV buffer (44-byte header + 160 KB PCM).
- `recordWAV()` allocates via the I2S library's internal `malloc`; it uses PSRAM if `ps_malloc` succeeds.
- `playWAV()` parses the WAV header back out and pushes samples to the MAX98357A.

### Known limitations (v1)

- Blocking record means the watch is unresponsive for 5 seconds — swipes won't register, the button can't be pressed. Recording with progress indication requires switching to chunked `readBytes()` instead.
- No file persistence — power-cycling the watch clears the recording. Could be saved to LittleFS.
- Fixed 5-second duration. Could expose 3 / 5 / 10 second options.

---

## About — `app_about.cpp`

Static brand screen.

### Layout

```
+--------------------------------+
|         JASBROS  ™              |   bold cyan title + small TM superscript
|                                |
|         .--.--.                |   red antenna tips
|         |o  o|                 |   grey head + cyan eyes
|          [+]                   |   yellow chest LED
|        /[----]\                |   robot body + arms
|                                |
|      by Tejas Diamos           |   y=170, size 2
|                                |
|     Two brothers learning      |   y=195, size 1
|     the world by building      |   y=208
|        advanced robots.        |   y=221
+--------------------------------+
```

The robot is drawn with primitives (`fillRoundRect` + `fillCircle` + `drawLine`) — no bitmap dependency.

### Future: real photo logo

A 100×100 RGB565 thumbnail of the original photo would be ~20 KB and could replace `draw_robot_logo()` with a single `gfx->draw16bitRGBBitmap(x, y, logo_data, w, h)` call. Conversion is one shot via `image2cpp` or `lcd-image-converter`, output dropped in as `logo.h`.

### Why no tick?

`about_tick()` is intentionally a no-op — the screen is static. The framework still calls it every loop, but it returns immediately.
