#include "gestures.h"
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>  // for DISP_WIDTH / DISP_HEIGHT

#define FT6336U_ADDR 0x38

// Touch panel orientation relative to display. Toggle these if a tap
// registers on the wrong side / corner of the screen.
#define TOUCH_FLIP_X 1
#define TOUCH_FLIP_Y 1
#define TOUCH_SWAP_XY 0

struct Gesture {
  bool active;
  int16_t start_x, start_y, last_x, last_y;
  uint32_t start_ms;
};
static Gesture g_state = { false, 0, 0, 0, 0, 0 };

static bool ft_read(int16_t &x, int16_t &y) {
  Wire1.beginTransmission(FT6336U_ADDR);
  Wire1.write(0x02);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((uint8_t)FT6336U_ADDR, (uint8_t)5) != 5) return false;
  uint8_t num = Wire1.read();
  uint8_t xh = Wire1.read();
  uint8_t xl = Wire1.read();
  uint8_t yh = Wire1.read();
  uint8_t yl = Wire1.read();
  if (num == 0 || num > 2) return false;
  int16_t raw_x = ((xh & 0x0F) << 8) | xl;
  int16_t raw_y = ((yh & 0x0F) << 8) | yl;
#if TOUCH_SWAP_XY
  int16_t t = raw_x; raw_x = raw_y; raw_y = t;
#endif
#if TOUCH_FLIP_X
  raw_x = (DISP_WIDTH - 1) - raw_x;
#endif
#if TOUCH_FLIP_Y
  raw_y = (DISP_HEIGHT - 1) - raw_y;
#endif
  x = raw_x;
  y = raw_y;
  return true;
}

GestureType poll_gesture(int16_t &tap_x, int16_t &tap_y) {
  int16_t x, y;
  if (ft_read(x, y)) {
    if (!g_state.active) {
      g_state.active = true;
      g_state.start_x = x;
      g_state.start_y = y;
      g_state.start_ms = millis();
    }
    g_state.last_x = x;
    g_state.last_y = y;
    return GESTURE_NONE;
  }
  if (!g_state.active) return GESTURE_NONE;
  g_state.active = false;
  int16_t dx = g_state.last_x - g_state.start_x;
  int16_t dy = g_state.last_y - g_state.start_y;
  int16_t adx = abs(dx);
  int16_t ady = abs(dy);
  if (adx > 60 && adx > ady) return dx > 0 ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
  if (ady > 60 && ady > adx) return dy > 0 ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
  if (adx < 25 && ady < 25) {
    tap_x = g_state.start_x;
    tap_y = g_state.start_y;
    return GESTURE_TAP;
  }
  return GESTURE_NONE;
}
