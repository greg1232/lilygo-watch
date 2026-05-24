# Modules

Per-module reference. Each module is a `.h` interface plus a `.cpp` implementation, both in `sketches/HelloWatch/`.

---

## hardware

**Files:** `hardware.h`, `hardware.cpp`

Owns the long-lived peripheral globals (`gfx`, `PMU`, `HAPTIC`) and the one-shot init that brings them up.

### API

```cpp
extern Arduino_GFX *gfx;
extern XPowersAXP2101 PMU;
extern Adafruit_DRV2605 HAPTIC;
extern bool haptic_ready;

void hardware_init();
void haptic_play(uint8_t effect);
void haptic_sequence(const uint8_t *effects, uint8_t n);
```

### What `hardware_init()` does

1. `Wire.begin(SDA, SCL)` — main I²C bus at 10/11.
2. `PMU.begin(Wire, ...)` — initialize the AXP2101.
3. Enable ALDO2 at 3.3 V (LCD VDD).
4. Enable BLDO2 at 3.3 V (DRV2605 enable pin).
5. Clear and enable PMU short-press + long-press IRQs.
6. `HAPTIC.begin(&Wire)`, `useERM()`, `selectLibrary(1)`, `setMode(INTTRIG)`.
7. `Wire1.begin(TP_SDA, TP_SCL)` — separate I²C bus for the touch panel.
8. `pinMode(PMU_INT, INPUT_PULLUP)` — used as light-sleep wake source later.
9. Display backlight LOW, `gfx->begin()`, `fillScreen(BLACK)`, backlight HIGH (so we don't show garbage during the init flash).

### Haptic helpers

`haptic_play(uint8_t effect)` plays a single ROM effect from library 1 (ERM).
`haptic_sequence(const uint8_t *effects, uint8_t n)` plays up to 7 chained effects.

Both are no-ops if `haptic_ready == false`.

Useful effect numbers (ERM library 1):
| # | Description |
|---|---|
| 1 | Strong Click - 100% |
| 14 | Strong Buzz - 100% |
| 24 | Sharp Click - 100% |
| 47 | Buzz 1 - 100% (longest) |
| 52 | Pulsing Strong 1 |
| 70 | Transition Ramp Up Long Sharp 1 |
| 84 | Long Double Sharp Click |

Full table is in the DRV2605 datasheet.

---

## gestures

**Files:** `gestures.h`, `gestures.cpp`

Reads the FT6336U capacitive touch panel on `Wire1` and classifies each touch sequence into a tap or a swipe.

### API

```cpp
enum GestureType {
  GESTURE_NONE,
  GESTURE_TAP,
  GESTURE_SWIPE_LEFT, SWIPE_RIGHT, SWIPE_UP, SWIPE_DOWN
};
GestureType poll_gesture(int16_t &tap_x, int16_t &tap_y);
```

`poll_gesture()` is meant to be called every iteration of the main loop. It returns `GESTURE_NONE` while a finger is down or no finger is down. On the *release* of a touch, it computes the dx/dy from start to last position and returns one of:
- `GESTURE_TAP` if the motion was small (<25 px in both axes) — `tap_x`/`tap_y` filled in.
- `GESTURE_SWIPE_LEFT/RIGHT` if dx is large (>60 px) and dominates dy.
- `GESTURE_SWIPE_UP/DOWN` if dy is large (>60 px) and dominates dx.
- `GESTURE_NONE` for ambiguous motion (e.g. medium-distance diagonal).

### Touch orientation

The FT6336U is physically mounted 180° from the display. `ft_read()` applies `TOUCH_FLIP_X` and `TOUCH_FLIP_Y` to swap coordinates into display space. Macros are at the top of `gestures.cpp` and easy to flip if a future board ships differently mounted.

### Implementation detail

The internal `Gesture` struct holds start position, last position, and timestamp. State is just `active`/`!active`. No queue — if you process gestures slower than the user makes them, you'll miss some. In practice, polling every 20 ms is plenty fast.

---

## power

**Files:** `power.h`, `power.cpp`

Screen on/off, the 2-minute inactivity timer, and ESP32 light sleep.

### API

```cpp
extern bool screen_on;
extern uint32_t last_activity_ms;
#define INACTIVITY_TIMEOUT_MS 120000UL

void power_mark_activity();
void power_set_screen(bool on);
bool power_idle_timeout_elapsed();
void power_enter_light_sleep_if_possible();
```

### `power_set_screen(true)`

- `PMU.enableALDO2()` — LCD VDD back on
- `delay(30)` — wait for the rail to come up
- `gfx->begin()` — re-init the display (it lost its state when ALDO2 was cut)
- `app_enter_current()` — redraw whatever app was active
- `digitalWrite(DISP_BL, HIGH)` — backlight on
- `screen_on = true`

### `power_set_screen(false)`

- `digitalWrite(DISP_BL, LOW)` — backlight off
- `PMU.disableALDO2()` — LCD VDD off
- `screen_on = false`

### `power_enter_light_sleep_if_possible()`

- If VBUS is detected (USB plugged in), `delay(100)` and return — skip sleep so the USB CDC doesn't drop.
- Otherwise: configure `ext0_wakeup` on `PMU_INT` (active-low) and call `esp_light_sleep_start()`. Blocks until the crown is pressed and the PMU pulls its INT line low.

After waking, control returns to `loop()`, which on its next iteration sees the PMU short-press IRQ and calls `power_set_screen(true)`.

---

## battery

**Files:** `battery.h`, `battery.cpp`

Tiny battery icon in the top-right of the clock face. 30×14 px outline + filled fill bar + small cap rectangle. Color-coded by level:

| Level | Color |
|---|---|
| Charging | Cyan |
| >50 % | Green |
| 20–50 % | Yellow |
| ≤20 % | Red |

### API

```cpp
void battery_invalidate();         // force redraw on next refresh
void battery_refresh(bool force);  // read PMU, redraw if state changed
```

### When refreshed

- Once unconditionally in `clock_enter()` (with `force=true`).
- In `clock_tick()`, on `tm_sec == 0` or `tm_sec == 30` — about twice a minute.

The "invalidate" call is needed when re-entering the clock app after a sleep cycle, because the cached `last_pct` value matches what was on screen *before* sleep, but the display was wiped during ALDO2-off.

### Positioning

Hard-coded to (202, 12). The "Pacific Time" header is centered in the full screen width, but its actual rendered width (144 px from x=48 to x=192) leaves the right side free.

---

## audio

**Files:** `audio.h`, `audio.cpp`

Wraps the ESP_I2S library to expose record / play helpers.

### API

```cpp
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_REC_SECONDS 5

void audio_init();
bool audio_record();
bool audio_play();
bool audio_has_recording();
void audio_clear();
```

### `audio_init()`

- Creates two `I2SClass` instances at file scope: one for the PDM microphone (RX), one for the PCM speaker (TX). These auto-allocate to I2S0 and I2S1 on the ESP32-S3.
- Mic: `setPinsPdmRx(MIC_SCK, MIC_DAT)` + `begin(I2S_MODE_PDM_RX, 16000, 16-bit, MONO)`.
- Speaker: `setPins(I2S_BCLK, I2S_WCLK, I2S_DOUT)` + `begin(I2S_MODE_STD, 16000, 16-bit, MONO, SLOT_LEFT)`.
- Sets a `ready` flag on success; all other functions no-op if not ready.

### `audio_record()`

Calls `MIC.recordWAV(AUDIO_REC_SECONDS, &recording_len)` — the library allocates a buffer (via `ps_malloc` if PSRAM is available), writes a 44-byte WAV header, and fills it with PCM samples. **Blocking** — control returns only after the full duration elapses.

If a previous recording exists, it's freed first.

### `audio_play()`

Calls `SPK.playWAV(recording, recording_len)`. Parses the WAV header, then streams samples to the I2S TX. Also **blocking**.

### Buffer sizing

5 s × 16 kHz × 2 bytes = 160 KB + 44-byte header. Allocated dynamically; freed on `audio_clear()` or the next `audio_record()` call.

### Future: nonblocking record

`I2SClass::readBytes(buffer, n)` returns whatever it has and is non-blocking. A chunked loop reading 200 ms slices into a pre-allocated buffer would let the UI tick between chunks for a level meter or stop button. Not implemented in v1.

---

## apps

**Files:** `apps.h`, `apps.cpp`

App registry + dispatcher.

### API

```cpp
enum AppId { APP_CLOCK, APP_MEMORY, APP_VOICE, APP_ABOUT };
extern AppId current_app;

void app_enter_current();
void app_switch(AppId next);
void app_tick();
void app_handle_tap(int16_t x, int16_t y);
```

### Implementation

Just four switch statements. Adding a fifth app means adding the enum value plus a case in each switch. See [adding-an-app.md](adding-an-app.md) for the full recipe.

### Why no class hierarchy

We have four apps. A `class App` with virtual methods would be ~80 lines of plumbing for the same behavior. The switch statements are honest about the shape — most apps don't need every callback, and the per-switch `default` case lets us skip the noop.
