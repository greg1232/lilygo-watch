# lilygo-watch

Arduino sketches for the LilyGo T-Watch S3 (ESP32-S3, 240×240 ST7789, FT6336U touch, AXP2101 PMU). Detailed docs live in [`docs/`](docs/README.md).

## Apps

- **Clock** — 12-hour Pacific time (DST aware), date, top-right battery indicator.
- **Memory game** — 4×3 grid, 6 hand-drawn symbol pairs, haptic buzz on match, rainbow celebration on win.
- **Voice memo** — 5-second mic record + speaker playback via I²S.
- **About** — JABROS branding, robot mascot, brothers' bio.

Navigation:
```
                 Voice          (swipe up from clock)
                   ↕
   Memory  ←→  Clock            (swipe left/right)
                   ↕
                 About          (swipe down from clock)
```

Crown button toggles screen on/off. Watch auto-sleeps after 2 minutes of inactivity (light sleep, ~1 mA on battery).

## Hardware setup

- Board: `esp32:esp32:twatchs3` (default tool options work).
- Connection: USB-C cable from the watch to a Mac. Serial port appears as `/dev/cu.usbmodem11201` (number may vary).

## Build & flash

Requires `arduino-cli` and the ESP32 board core:

```sh
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino" "XPowersLib" "Adafruit GFX Library"
```

To flash, run the helper script — it bakes the current epoch into the build so the clock starts at the right time:

```sh
./flash.sh
```

Tune `BUFFER_SEC` env var if the clock is consistently fast or slow after flashing (it compensates for compile + upload + boot time).

## Monitor serial

```sh
arduino-cli monitor -p /dev/cu.usbmodem11201 -c baudrate=115200
# or simply:
cat /dev/cu.usbmodem11201
```

## Documentation

- [docs/hardware.md](docs/hardware.md) — pin map, peripherals, PMU rails, hardware quirks.
- [docs/architecture.md](docs/architecture.md) — module layout, init order, sleep/wake flow.
- [docs/build.md](docs/build.md) — toolchain setup, `flash.sh`, BUILD_EPOCH, monitoring.
- [docs/apps.md](docs/apps.md) — per-app layout and state machines.
- [docs/modules.md](docs/modules.md) — per-module reference.
- [docs/adding-an-app.md](docs/adding-an-app.md) — recipe for adding a new app.
