#include "apps.h"
#include <Arduino.h>

AppId current_app = APP_CLOCK;

void app_enter_current() {
  if (current_app == APP_CLOCK) clock_enter();
  else                          memory_enter();
}

void app_switch(AppId next) {
  if (next == current_app) return;
  current_app = next;
  Serial.println(next == APP_CLOCK ? ">> Clock" : ">> Memory");
  app_enter_current();
}

void app_tick() {
  if (current_app == APP_CLOCK) clock_tick();
  else                          memory_tick();
}

void app_handle_tap(int16_t x, int16_t y) {
  if (current_app == APP_MEMORY) memory_tap(x, y);
}
