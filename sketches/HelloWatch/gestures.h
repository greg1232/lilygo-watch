#pragma once

#include <stdint.h>

enum GestureType {
  GESTURE_NONE,
  GESTURE_TAP,
  GESTURE_SWIPE_LEFT,
  GESTURE_SWIPE_RIGHT,
  GESTURE_SWIPE_UP,
  GESTURE_SWIPE_DOWN
};

// Read the FT6336U touch panel once and report a finished gesture (if any).
// On GESTURE_TAP, tap_x / tap_y are filled with display-space coordinates.
// On swipes, the coordinates are unspecified.
GestureType poll_gesture(int16_t &tap_x, int16_t &tap_y);
