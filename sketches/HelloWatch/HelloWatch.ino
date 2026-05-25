// LilyGo T-Watch S3 — main entry point.
//
// Apps live in separate translation units:
//   app_clock.cpp    — digital Pacific-time clock
//   app_memory.cpp   — memory-match game
//
// Supporting modules:
//   hardware.{h,cpp} — display, PMU, haptic globals + init
//   gestures.{h,cpp} — FT6336U touch-panel polling and gesture detection
//   power.{h,cpp}    — screen on/off, light sleep, inactivity timer
//   battery.{h,cpp}  — top-right battery icon
//   apps.{h,cpp}     — AppId enum and current-app dispatch
//   colors.h         — RGB565 palette
//
// Navigation:
//   Swipe LEFT  from clock → Memory game
//   Swipe RIGHT from game  → Clock
//   Crown button            → toggle screen on/off (manual)
//   2 min idle              → auto-sleep
//
// Time source: BUILD_EPOCH is baked in at compile time by flash.sh.

#include "hardware.h"
#include "gestures.h"
#include "power.h"
#include "apps.h"
#include "audio.h"
#include "wifi_sync.h"
#include <time.h>
#include <sys/time.h>

#ifndef BUILD_EPOCH
#define BUILD_EPOCH 0
#endif
#define TZ_PACIFIC "PST8PDT,M3.2.0,M11.1.0"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nWatch booting...");

  hardware_init();
  audio_init();

  setenv("TZ", TZ_PACIFIC, 1);
  tzset();

  // Try to get accurate time via WiFi + NTP. If that fails (no WiFi
  // credentials, network down, etc.), fall back to the epoch baked in
  // at build time by flash.sh.
  wifi_sync_time(TZ_PACIFIC, (time_t)BUILD_EPOCH, 8000);
  if (!wifi_time_synced()) {
    struct timeval tv = { .tv_sec = (time_t)BUILD_EPOCH, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    Serial.println("Time: using BUILD_EPOCH fallback");
  }

  app_enter_current();
  power_mark_activity();
  Serial.println("Ready. Clock<->Memory: swipe left/right. Clock<->Voice: swipe up/down. Crown: toggle screen.");
}

void loop() {
  // Crown button → toggle screen
  PMU.getIrqStatus();
  if (PMU.isPekeyShortPressIrq()) {
    power_set_screen(!screen_on);
    PMU.clearIrqStatus();
    power_mark_activity();
  }

  if (!screen_on) {
    power_enter_light_sleep_if_possible();
    return;
  }

  int16_t tx, ty;
  GestureType g = poll_gesture(tx, ty);
  if (g != GESTURE_NONE) power_mark_activity();
  switch (g) {
    case GESTURE_SWIPE_LEFT:
      if (current_app == APP_CLOCK) app_switch(APP_MEMORY);
      break;
    case GESTURE_SWIPE_RIGHT:
      if (current_app == APP_MEMORY) app_switch(APP_CLOCK);
      else if (current_app == APP_VOICE) app_switch(APP_CLOCK);
      break;
    case GESTURE_SWIPE_UP:
      if      (current_app == APP_CLOCK) app_switch(APP_VOICE);
      else if (current_app == APP_ABOUT) app_switch(APP_CLOCK);
      break;
    case GESTURE_SWIPE_DOWN:
      if      (current_app == APP_VOICE) app_switch(APP_CLOCK);
      else if (current_app == APP_CLOCK) app_switch(APP_ABOUT);
      break;
    case GESTURE_TAP:
      Serial.printf("TAP at (%d, %d)\n", tx, ty);
      app_handle_tap(tx, ty);
      break;
    default:
      break;
  }

  app_tick();

  if (power_idle_timeout_elapsed()) {
    Serial.println("Auto-sleep (inactivity)");
    power_set_screen(false);
  }

  delay(20);
}
