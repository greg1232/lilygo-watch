# Architecture

The sketch is split into small modules in `sketches/HelloWatch/`. Every file is in that one directory because Arduino sketches require it.

## File layout

```
HelloWatch.ino        — main: setup(), loop(), gesture → app dispatch
hardware.{h,cpp}      — display (gfx), PMU, HAPTIC globals; one-shot init
gestures.{h,cpp}      — FT6336U touch read + swipe/tap classification
power.{h,cpp}         — screen on/off, activity timer, light sleep
battery.{h,cpp}       — top-right battery icon
audio.{h,cpp}         — I2S mic + speaker, record/play helpers
apps.{h,cpp}          — AppId enum + current-app dispatcher
app_clock.cpp         — digital Pacific-time clock
app_memory.cpp        — memory-match game
app_voice.cpp         — record-then-playback voice memo
app_about.cpp         — JABROS branding screen
colors.h              — RGB565 palette (header-only)
FreeMono24pt7b.h      — bundled Adafruit font (used by clock + about)
```

## Module responsibilities

| Module | Owns / exposes | Talks to |
|---|---|---|
| `hardware` | `gfx`, `PMU`, `HAPTIC` globals; `haptic_play()`, `haptic_sequence()` | Wire (main I2C), SPI (display) |
| `gestures` | `GestureType`, `poll_gesture()` | Wire1 (touch I2C) |
| `power` | `screen_on`, `last_activity_ms`; toggle, sleep, idle-check | `hardware`, `apps` (re-enters current app on wake) |
| `battery` | `battery_refresh()`, `battery_invalidate()` | `hardware` (gfx + PMU) |
| `audio` | `audio_init()`, `audio_record()`, `audio_play()` | I2S0 (PDM RX), I2S1 (PCM TX) |
| `apps` | `current_app`, `app_switch()`, `app_tick()`, `app_handle_tap()` | individual `app_*.cpp` files |
| `app_*` | per-app `*_enter()`, `*_tick()`, `*_tap()` | `hardware`, sometimes `audio` / `battery` |

The dependency graph is acyclic: apps depend on hardware-ish modules, the dispatcher (`apps`) depends on each app, and `HelloWatch.ino` depends on everything.

## Init order (in `setup()`)

1. `Serial.begin(115200)` — USB CDC up.
2. `hardware_init()` — Wire/Wire1 buses, PMU + rails (ALDO2 for LCD, BLDO2 for haptic), PMU IRQs enabled, DRV2605 configured, PMU_INT pin as `INPUT_PULLUP`, display initialised, backlight on.
3. `audio_init()` — both I2S ports configured (mic on PDM RX, speaker on standard PCM TX).
4. Timezone + system clock set from `BUILD_EPOCH` macro.
5. `app_enter_current()` — renders the initial app (Clock).
6. `power_mark_activity()` — start the 2-minute idle timer.

Order matters:
- ALDO2 must be enabled before the display is initialised.
- BLDO2 must be enabled before `HAPTIC.begin()`.
- The display must be initialised before any app's `_enter()` runs.

## Main loop

`loop()` is small and fixed:

```
1. PMU.getIrqStatus()  → if short-press, toggle screen + mark activity
2. If screen is off    → power_enter_light_sleep_if_possible() and return
3. poll_gesture()      → if non-NONE, mark activity
4. Dispatch the gesture:
     SWIPE_LEFT/RIGHT/UP/DOWN → app_switch() per the nav grid
     TAP                       → app_handle_tap(x, y)
5. app_tick()          → run the current app's per-frame work
6. If idle > 2 min     → power_set_screen(false)
7. delay(20)
```

## App navigation

```
                 Voice
                   ↑ swipe-up
                   ↓ swipe-down
   Memory  ←→  Clock  ←→  Memory     (left/right between Clock and Memory)
                   ↑ swipe-up
                   ↓ swipe-down
                 About
```

- From any non-Clock app, a "back home" swipe returns to Clock. The directions are intentionally symmetric: if you swiped down to *get there*, swipe up to *leave*.
- Voice never connects to Memory directly — you go through Clock.

## Sleep and wake

- **Manual**: crown short-press toggles the screen. `screen_set_on(true)` re-enables ALDO2, calls `gfx->begin()`, then `app_enter_current()` to re-render whatever was on screen.
- **Auto**: after 2 minutes with no tap, swipe, or button, `power_set_screen(false)` runs.
- **When screen is off** and not on USB: `esp_light_sleep_start()` blocks until `PMU_INT` (GPIO 21) goes low (i.e. crown press fires PMU IRQ). On wake, control returns to the top of `loop()`, the IRQ check fires, and the screen comes back.
- **When screen is off but on USB**: we skip sleep (so USB CDC stays alive for `arduino-cli upload`). The watch idles in `delay(100)` loops.

## Hot paths and timing

| What | When | Cost |
|---|---|---|
| Touch poll | every 20 ms | ~1 ms I²C read |
| Clock redraw | once per second | ~10 ms (text only) |
| Battery refresh | seconds 0 and 30 of each minute | ~2 ms I²C + small redraw |
| Memory match check | only on tap | trivial |
| Audio record | blocking, 5 s | dominates the loop for that duration |
| Audio play | blocking, ~5 s | same |

The voice-memo recording and playback are intentionally blocking in v1 — the watch is unresponsive for the 5 seconds while it captures or plays. A chunked, nonblocking version would update the UI between sample chunks; that's an upgrade we left for later.

## Adding a fifth app

See [adding-an-app.md](adding-an-app.md).
