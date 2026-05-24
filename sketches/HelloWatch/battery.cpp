#include "battery.h"
#include "hardware.h"
#include "colors.h"

static const int16_t ICON_X = 202;
static const int16_t ICON_Y = 12;
static const int16_t ICON_W = 30;
static const int16_t ICON_H = 14;

static int8_t last_pct = -2;
static bool last_charging = false;

void battery_invalidate() {
  last_pct = -2;
}

static void draw_icon(uint8_t pct, bool charging) {
  // Erase a slightly larger area so the previous fill never bleeds through.
  gfx->fillRect(ICON_X - 1, ICON_Y - 1, ICON_W + 6, ICON_H + 2, COLOR_BLACK);
  gfx->drawRect(ICON_X, ICON_Y, ICON_W, ICON_H, COLOR_WHITE);
  gfx->fillRect(ICON_X + ICON_W, ICON_Y + 4, 3, 6, COLOR_WHITE);

  uint16_t fill_color;
  if (charging)         fill_color = COLOR_CYAN;
  else if (pct > 50)    fill_color = COLOR_GREEN;
  else if (pct > 20)    fill_color = COLOR_YELLOW;
  else                  fill_color = COLOR_RED;

  int16_t fill_w = ((ICON_W - 4) * (int)pct) / 100;
  if (fill_w > 0) gfx->fillRect(ICON_X + 2, ICON_Y + 2, fill_w, ICON_H - 4, fill_color);
}

void battery_refresh(bool force) {
  uint8_t pct = PMU.getBatteryPercent();
  bool charging = PMU.isCharging();
  if (force || pct != last_pct || charging != last_charging) {
    draw_icon(pct, charging);
    last_pct = pct;
    last_charging = charging;
  }
}
