# Build & flash workflow

## One-time setup

Install `arduino-cli` (we use Homebrew on macOS):

```sh
brew install arduino-cli
```

Initialise the config and add Espressif's board manager URL:

```sh
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
```

Install the ESP32 board core (this is the chunky one — ~1 GB of toolchain):

```sh
arduino-cli core install esp32:esp32
```

Install the libraries used by the sketch:

```sh
arduino-cli lib install "GFX Library for Arduino"
arduino-cli lib install "XPowersLib"
arduino-cli lib install "Adafruit GFX Library"     # for the FreeMono24pt7b font header
arduino-cli lib install "Adafruit DRV2605 Library"
```

`ESP_I2S` (used by the voice memo app) ships *inside* the ESP32 core, so it doesn't need a separate install.

The `FreeMono24pt7b.h` font file is committed in `sketches/HelloWatch/` — the `Adafruit GFX Library` install is only there so the toolchain finds its `gfxfont.h` header.

## The flash script

`flash.sh` in the repo root does compile + upload in one command. It also bakes the *current host time* into the firmware so the clock displays the right time after boot.

```sh
./flash.sh
```

What it does:

```
EPOCH = now + 35              # buffer for compile + upload + boot
arduino-cli compile --fqbn esp32:esp32:twatchs3 \
  --build-property "compiler.cpp.extra_flags=-DBUILD_EPOCH=${EPOCH}L" \
  sketches/HelloWatch
arduino-cli upload -p /dev/cu.usbmodem11201 --fqbn esp32:esp32:twatchs3 \
  sketches/HelloWatch
```

The `BUILD_EPOCH` macro is a compile-time integer (seconds since 1970 UTC). `HelloWatch.ino` calls `settimeofday()` with it during `setup()`, so the system clock starts roughly correct without WiFi NTP or RTC persistence.

`BUFFER_SEC` is tunable via env var:

```sh
BUFFER_SEC=45 ./flash.sh   # if your machine compiles slowly
BUFFER_SEC=20 ./flash.sh   # if your machine is faster than mine
```

The watch will briefly show a time `BUFFER_SEC` seconds in the future, then real time catches up. If the displayed time is consistently *behind* real time, raise `BUFFER_SEC`; if *ahead*, lower it.

`PORT` is also tunable:

```sh
PORT=/dev/cu.usbmodem21101 ./flash.sh
```

## Direct arduino-cli (without the script)

If you don't need the time baked in:

```sh
arduino-cli compile --fqbn esp32:esp32:twatchs3 sketches/HelloWatch
arduino-cli upload  -p /dev/cu.usbmodem11201 --fqbn esp32:esp32:twatchs3 sketches/HelloWatch
```

The clock will start at the Unix epoch (1970) and you'll see year `1970` in the date — harmless but visible.

## Serial monitor

```sh
arduino-cli monitor -p /dev/cu.usbmodem11201 -c baudrate=115200
```

Or, if that gives you no output, the raw alternatives:

```sh
stty -f /dev/cu.usbmodem11201 115200 cs8 -cstopb -parenb
cat /dev/cu.usbmodem11201
```

```sh
screen /dev/cu.usbmodem11201 115200   # exit with Ctrl-A then K
```

## Common errors

### `could not open port /dev/cu.usbmodem11201`

The watch went into light sleep — USB CDC suspends with it. **Press the crown** to wake it; the port will reappear within a second. If that gets annoying mid-debug, the build is configured to skip sleep while VBUS is detected, so just keeping the cable plugged in usually keeps the port alive.

If the port disappears entirely (not just sleep), the watch may have rebooted unexpectedly or the USB CDC failed. Plug-cycle the cable.

### `'GestureType' does not name a type`

Arduino auto-prototype hoisting: the preprocessor injects function prototypes at the top of `.ino` files, *before* enum definitions. If a function in the `.ino` uses an enum as its argument or return type, define the enum in a header that `.ino` includes. (We hit this twice during development — `AppId` and `GestureType` both ended up in `apps.h` / `gestures.h`.)

### Display lights up blank or stays black

ALDO2 wasn't enabled before `gfx->begin()`. Check `hardware_init()` ordering. The display also blanks on `power_set_screen(false)` — that's normal; press the crown to wake.

### Haptic motor feels silent

You're driving an ERM with LRA-tuned waveforms. Make sure `HAPTIC.useERM()` + `HAPTIC.selectLibrary(1)` are both called.

### Audio playback is silent or distorted

Common causes:
- Forgot to enable BLDO2 before `HAPTIC.begin()` — no, that's haptic. For audio, both I2S peripherals run from the main 3.3V; they should always have power. But the speaker amp needs the I2S clock running, which means `audio_init()` must succeed.
- PSRAM not enabled in the board build — the `ps_malloc` inside `recordWAV()` falls back to regular heap, which can OOM for longer recordings.
- Sample rate mismatch — both `MIC.begin()` and `SPK.begin()` use `AUDIO_SAMPLE_RATE` (16 kHz). Don't change one without the other.

## Build sizes

Approximate as of this writing:

- Sketch: ~470 KB of 3 MB app partition (15 %)
- RAM: ~24 KB of 320 KB static + heap (7 %)

Plenty of headroom for additional apps.

## Resetting the watch into download mode

You shouldn't need this — `arduino-cli upload` toggles DTR/RTS to put the chip into bootloader mode automatically. If a sketch is wedged enough that it doesn't respond:

1. Remove the back cover and unplug the battery.
2. Plug in USB.
3. Hold the **BOOT** button inside the case.
4. Tap the crown briefly.
5. Release BOOT — the watch is now in download mode and `arduino-cli upload` will work.
6. After upload, hold the crown to power down, then press it to power back on.

The procedure is documented with photos in LilyGo's own README.
