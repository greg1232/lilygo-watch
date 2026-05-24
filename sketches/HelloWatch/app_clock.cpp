#include "apps.h"
#include "hardware.h"
#include "battery.h"
#include "colors.h"
#include "FreeMono24pt7b.h"
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

static int clock_last_second = -1;
static int clock_last_minute = -1;

static void draw_centered(const char *s, int16_t baseline_y, uint16_t color,
                          int16_t erase_y, int16_t erase_h) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, baseline_y, &x1, &y1, &w, &h);
  int16_t x = (DISP_WIDTH - (int16_t)w) / 2 - x1;
  gfx->fillRect(0, erase_y, DISP_WIDTH, erase_h, COLOR_BLACK);
  gfx->setTextColor(color);
  gfx->setCursor(x, baseline_y);
  gfx->print(s);
}

void clock_enter() {
  gfx->fillScreen(COLOR_BLACK);
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_GREY);
  // "Pacific Time" (144px wide) ends at x=192; battery icon starts at x=202.
  const char *header = "Pacific Time";
  gfx->setCursor((DISP_WIDTH - (int)strlen(header) * 12) / 2, 20);
  gfx->print(header);
  clock_last_second = -1;
  clock_last_minute = -1;
  battery_invalidate();
  battery_refresh(true);
}

void clock_tick() {
  time_t now = time(nullptr);
  struct tm tm_local;
  localtime_r(&now, &tm_local);
  if (tm_local.tm_sec == clock_last_second) return;
  clock_last_second = tm_local.tm_sec;

  int hour12 = tm_local.tm_hour % 12;
  if (hour12 == 0) hour12 = 12;
  const char *ampm = (tm_local.tm_hour < 12) ? "AM" : "PM";

  char big[8];
  char small_[12];
  snprintf(big, sizeof(big), "%d:%02d", hour12, tm_local.tm_min);
  snprintf(small_, sizeof(small_), "%02d %s", tm_local.tm_sec, ampm);

  // Wipe both time lines together.
  gfx->fillRect(0, 80, DISP_WIDTH, 110, COLOR_BLACK);

  // Custom GFX fonts honor setTextSize() as a pixel multiplier — must
  // explicitly set to 1 here so the 24pt font doesn't render at 2x.
  gfx->setFont(&FreeMono24pt7b);
  gfx->setTextSize(1);
  draw_centered(big, 128, COLOR_GREEN, 80, 60);

  // Seconds line: built-in font at size 3 — unambiguous digits.
  gfx->setFont(nullptr);
  gfx->setTextSize(3);
  int small_w = (int)strlen(small_) * 18;
  gfx->setTextColor(COLOR_DIM_GREEN);
  gfx->setCursor((DISP_WIDTH - small_w) / 2, 155);
  gfx->print(small_);

  if (tm_local.tm_min != clock_last_minute) {
    clock_last_minute = tm_local.tm_min;
    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a %b %d %Y", &tm_local);

    // Redraw header — erase only the text region (battery icon lives at x>=200).
    gfx->setFont(nullptr);
    gfx->setTextSize(2);
    gfx->fillRect(0, 10, 200, 25, COLOR_BLACK);
    gfx->setTextColor(COLOR_GREY);
    const char *header = "Pacific Time";
    gfx->setCursor((DISP_WIDTH - (int)strlen(header) * 12) / 2, 20);
    gfx->print(header);

    // Date
    gfx->fillRect(0, 200, DISP_WIDTH, 30, COLOR_BLACK);
    gfx->setTextColor(COLOR_CYAN);
    gfx->setCursor((DISP_WIDTH - (int)strlen(date_buf) * 12) / 2, 210);
    gfx->print(date_buf);
    Serial.printf("%s %s PT\n", date_buf, big);
  }

  // Refresh battery icon roughly every 30 seconds.
  if (tm_local.tm_sec == 0 || tm_local.tm_sec == 30) {
    battery_refresh(false);
  }
}
