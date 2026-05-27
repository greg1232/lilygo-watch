#include "apps.h"
#include <Arduino.h>

AppId current_app = APP_CLOCK;

void app_enter_current() {
  switch (current_app) {
    case APP_CLOCK:    clock_enter();    break;
    case APP_MEMORY:   memory_enter();   break;
    case APP_VOICE:    voice_enter();    break;
    case APP_ABOUT:    about_enter();    break;
    case APP_INVADERS: invaders_enter(); break;
  }
}

void app_switch(AppId next) {
  if (next == current_app) return;
  current_app = next;
  const char *name = (next == APP_CLOCK)    ? ">> Clock"
                   : (next == APP_MEMORY)   ? ">> Memory"
                   : (next == APP_VOICE)    ? ">> Voice"
                   : (next == APP_ABOUT)    ? ">> About"
                                            : ">> Invaders (unlocked!)";
  Serial.println(name);
  app_enter_current();
}

void app_tick() {
  switch (current_app) {
    case APP_CLOCK:    clock_tick();    break;
    case APP_MEMORY:   memory_tick();   break;
    case APP_VOICE:    voice_tick();    break;
    case APP_ABOUT:    about_tick();    break;
    case APP_INVADERS: invaders_tick(); break;
  }
}

void app_handle_tap(int16_t x, int16_t y) {
  switch (current_app) {
    case APP_MEMORY:   memory_tap(x, y);   break;
    case APP_VOICE:    voice_tap(x, y);    break;
    case APP_ABOUT:    about_tap(x, y);    break;
    case APP_INVADERS: invaders_tap(x, y); break;
    default: break;
  }
}
