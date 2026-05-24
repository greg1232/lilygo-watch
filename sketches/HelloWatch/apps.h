#pragma once

#include <stdint.h>

enum AppId { APP_CLOCK = 0, APP_MEMORY = 1 };

extern AppId current_app;

// Render the current app's screen from scratch (called on boot, on app
// switch, and on wake from sleep).
void app_enter_current();

// Switch to a different app and render its screen.
void app_switch(AppId next);

// Per-frame dispatch from loop().
void app_tick();

// Forward a tap to the current app (no-op for apps that don't take taps).
void app_handle_tap(int16_t x, int16_t y);

// --- Per-app entry points (implemented in app_*.cpp) ---
void clock_enter();
void clock_tick();

void memory_enter();
void memory_tick();
void memory_tap(int16_t x, int16_t y);
