#pragma once

#include <stdint.h>

// Inactivity threshold for auto-sleep.
#define INACTIVITY_TIMEOUT_MS 120000UL  // 2 minutes

extern bool screen_on;
extern uint32_t last_activity_ms;

// Bookkeep input activity (tap, swipe, button). Resets the auto-sleep timer.
void power_mark_activity();

// Turn the LCD on (ALDO2 + backlight + display re-init + app re-render)
// or off. No-op if already in the requested state.
void power_set_screen(bool on);

// Returns true if the auto-sleep timeout has elapsed since the last activity.
bool power_idle_timeout_elapsed();

// Enter ESP32 light sleep until the PMU INT pin fires (crown press).
// Returns immediately if VBUS is present (USB CDC must stay alive for dev).
void power_enter_light_sleep_if_possible();
