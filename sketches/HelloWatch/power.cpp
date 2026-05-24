#include "power.h"
#include "hardware.h"
#include "apps.h"
#include "colors.h"
#include <Arduino.h>
#include <esp_sleep.h>

bool screen_on = true;
uint32_t last_activity_ms = 0;

void power_mark_activity() {
  last_activity_ms = millis();
}

void power_set_screen(bool on) {
  if (on == screen_on) return;
  if (on) {
    PMU.enableALDO2();
    delay(30);
    gfx->begin();
    app_enter_current();
    digitalWrite(DISP_BL, HIGH);
    Serial.println("Screen ON");
  } else {
    digitalWrite(DISP_BL, LOW);
    PMU.disableALDO2();
    Serial.println("Screen OFF");
  }
  screen_on = on;
}

bool power_idle_timeout_elapsed() {
  return screen_on && (millis() - last_activity_ms > INACTIVITY_TIMEOUT_MS);
}

void power_enter_light_sleep_if_possible() {
  // Skip light sleep while plugged into USB — sleep suspends USB CDC,
  // which makes the serial port disappear and breaks dev flashing.
  if (PMU.isVbusIn()) {
    delay(100);
    return;
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PMU_INT, 0);
  esp_light_sleep_start();
}
