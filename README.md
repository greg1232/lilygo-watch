# lilygo-watch

Arduino sketches for the LilyGo T-Watch S3 (ESP32-S3, 240×240 ST7789, FT6336U touch, AXP2101 PMU).

## Features

- **Digital clock** showing 12-hour Pacific time (DST aware) with date below.
- **Memory game**: 4×3 grid of 6 symbol pairs (heart, star, sun, moon, clover, diamond). Tap to flip; matches lock with a green border; full clear triggers a rainbow celebration and reshuffle.
- **Swipe navigation**: swipe LEFT on the clock to enter the game; swipe RIGHT in the game to return.
- **Screen on/off**: tap the crown to toggle the LCD power rail (ALDO2) + backlight off to save power; tap again to wake.

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

## Notable hardware quirks

- The display's VDD rail is gated by the AXP2101 PMU. Must enable `ALDO2` (3.3V) before the LCD will light up.
- The touch panel is mounted rotated 180° from the display, so X and Y are flipped (see `TOUCH_FLIP_X` / `TOUCH_FLIP_Y` macros in the sketch).
- Custom GFX fonts (`FreeMono24pt7b` etc.) honor `setTextSize()` as a *pixel multiplier* — always reset it to 1 before drawing with a custom font, otherwise the font renders 2× and overflows.
