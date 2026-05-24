# Hardware reference — LilyGo T-Watch S3

The T-Watch S3 is a wearable dev board around an ESP32-S3 with a 240×240 IPS display, capacitive touch, accelerometer, RTC chip, PMU, haptic motor, mic, speaker, IR LED, and a SubGHz LoRa radio. This page covers what we actually use and the gotchas we hit.

## Core specs

| Component | Part | Bus | Notes |
|---|---|---|---|
| MCU | Espressif ESP32-S3 | — | Dual-core Xtensa LX7, 240 MHz, USB CDC native |
| Flash | 16 MB QSPI | — | |
| PSRAM | 8 MB OPI | — | Required for audio buffers and JPEG decode |
| Display | ST7789V3 240×240 IPS | SPI | Driven via Arduino_GFX |
| Touch | FT6336U capacitive | I²C (Wire1) | Single-finger reliable |
| Accelerometer | Bosch BMA423 | I²C (Wire) | Unused so far — has hardware step counter |
| RTC | NXP PCF8563 | I²C (Wire) | Unused; could persist wall time across reboots |
| PMU | X-Powers AXP2101 | I²C (Wire) | Battery, rails, crown-key IRQ |
| Haptic | TI DRV2605L | I²C (Wire) | ERM motor — see "Haptic" gotcha below |
| PDM mic | SPM1423HM4H-B | I²S0 | 1-bit PDM, ESP32 decimates to PCM |
| PCM amp | MAX98357A (3.2 W class-D) | I²S1 | Drives the small onboard speaker |
| IR LED | IR12-21C | GPIO 2 | Unused; could be a TV remote |
| LoRa radio | Semtech SX1262 (this unit) | SPI | Unused; could send long-range messages |
| Crown | momentary push button | via AXP2101 | Generates short/long-press IRQs |

## Pin map (from `pins_arduino.h`)

Comes from the `lilygo_twatch_s3` board variant in arduino-esp32. We just refer to these symbols by name.

```
Display (ST7789, SPI):
  DISP_MOSI 13   DISP_SCK  18   DISP_CS  12
  DISP_DC   38   DISP_RST  -1   DISP_BL  45   (BL is a normal GPIO PWM/digital)

Touch (FT6336U, separate I2C bus):
  TP_SDA  39   TP_SCL  40   TP_INT  16

Shared I2C bus (PMU, RTC, accel, haptic):
  SDA  10   SCL  11

Interrupts on shared I2C peripherals:
  RTC_INT     17     (PCF8563)
  PMU_INT     21     (AXP2101)  — used as ext0 wake source for light sleep
  SENSOR_INT  14     (BMA423)

PDM microphone:
  MIC_SCK 44   MIC_DAT 47

I2S amplifier (MAX98357A):
  I2S_BCLK 48   I2S_WCLK 15   I2S_DOUT 46

IR transmitter:
  IR_SEND 2

UART → GPS connector:
  TX 42   RX 41

LoRa (SX1262) + SD share an SPI bus:
  SCK 3   MISO 4   MOSI 1
  LORA_CS 5   LORA_RST 8   LORA_BUSY 7   LORA_IRQ 9
```

## AXP2101 power rails

The PMU gates several rails. We touch these:

| Rail | Voltage | Powers | Our code |
|---|---|---|---|
| ALDO2 | 3.3 V | LCD VDD | Enabled in `hardware_init`; disabled in `power_set_screen(false)` to save power |
| BLDO2 | 3.3 V | DRV2605 enable pin | Enabled in `hardware_init`; never disabled |
| DC3 / BLDO1 | varies | GPS module | Untouched; GPS not present on this unit |

If you ever see a black screen on boot, ALDO2 is the first thing to check.

## Gotchas we hit

These are the non-obvious things that wasted time. Worth knowing.

### Display power is gated by the PMU
Just calling `gfx->begin()` will not light the LCD. You must `PMU.enableALDO2()` first (after `PMU.begin()`). On wake from sleep, you also have to re-init the display since cutting ALDO2 wipes its state.

### Touch panel is mounted 180° from the display
Raw FT6336U coordinates need both X and Y flipped before they match display pixels. `gestures.cpp` does this with `TOUCH_FLIP_X` / `TOUCH_FLIP_Y` macros. If a future board revision is mounted differently, those are the knobs.

### Custom GFX fonts multiply by textsize
`gfx->setTextSize(N)` is a pixel multiplier — for built-in 5×7 font it's the natural size knob, but for any custom GFX font (`FreeMono24pt7b` etc.), `setTextSize(2)` renders the 24 pt glyphs at 48 pt. Always set `setTextSize(1)` before drawing with a custom font, even if you think it's already 1.

### Haptic motor is ERM, not LRA
The DRV2605 defaults to ERM mode but the Adafruit `useERM()` call is needed if you've ever called `useLRA()`. We're using the ERM library (library 1). LRA settings drive the actual motor with the wrong waveforms and produce no sensation at all. The LilyGoLib source explicitly uses `useERM()` + `selectLibrary(1)`.

### Proportional fonts and per-tick recentering
If you redraw a digit string each tick by re-centering with `getTextBounds()`, a proportional font shifts text left/right because `1` is narrower than `2`. Fixed-rect erases miss the pixels from the previous tick. Use a monospaced font (`FreeMono24pt7b`) for changing-digit displays, or erase a wider region.

### USB CDC suspends in light sleep
`esp_light_sleep_start()` suspends the USB controller, which makes the serial port disappear from the host and breaks `arduino-cli upload`. `power_enter_light_sleep_if_possible()` checks `PMU.isVbusIn()` and skips sleep while plugged in — sleep only kicks in on battery.

### Crown long-press shuts down the watch
The AXP2101 has a hardware long-press timer (default 6 s) that cuts power to the ESP32. Don't disable it without thinking — it's the user's only emergency-off. We only consume the *short* press IRQ for screen toggle.

### Flash timing for the build-baked clock
`flash.sh` bakes the current host epoch into the firmware. The watch reaches `setup()` ~35 s after `flash.sh` starts (compile + upload + reset + USB enumerate + `Serial.begin` warmup). The script's `BUFFER_SEC` env var compensates; tune it if the clock is consistently slow or fast after flashing.
