#include "apps.h"
#include "hardware.h"
#include "colors.h"
#include "FreeMono24pt7b.h"
#include <Arduino.h>

// Small robot logo, drawn with primitives. Origin (cx, cy) is the centre
// of the head.
static void draw_robot_logo(int16_t cx, int16_t cy) {
  // Antennas with red tips
  gfx->drawLine(cx - 11, cy - 28, cx - 11, cy - 19, COLOR_WHITE);
  gfx->drawLine(cx + 11, cy - 28, cx + 11, cy - 19, COLOR_WHITE);
  gfx->fillCircle(cx - 11, cy - 30, 3, COLOR_RED);
  gfx->fillCircle(cx + 11, cy - 30, 3, COLOR_RED);

  // Head — rounded rectangle
  gfx->fillRoundRect(cx - 24, cy - 19, 48, 32, 5, COLOR_GREY);
  gfx->drawRoundRect(cx - 24, cy - 19, 48, 32, 5, COLOR_WHITE);

  // Eyes (with highlight specks)
  gfx->fillCircle(cx - 10, cy - 6, 5, COLOR_CYAN);
  gfx->fillCircle(cx + 10, cy - 6, 5, COLOR_CYAN);
  gfx->fillCircle(cx - 9, cy - 7, 2, COLOR_WHITE);
  gfx->fillCircle(cx + 11, cy - 7, 2, COLOR_WHITE);

  // Mouth — friendly green line
  gfx->drawLine(cx - 7, cy + 6, cx + 7, cy + 6, COLOR_GREEN);
  gfx->drawPixel(cx - 7, cy + 5, COLOR_GREEN);
  gfx->drawPixel(cx + 7, cy + 5, COLOR_GREEN);

  // Body
  gfx->fillRect(cx - 14, cy + 15, 28, 18, COLOR_DARK_GREY);
  gfx->drawRect(cx - 14, cy + 15, 28, 18, COLOR_WHITE);
  // Chest LED
  gfx->fillCircle(cx, cy + 24, 3, COLOR_YELLOW);

  // Arms
  gfx->drawLine(cx - 14, cy + 18, cx - 22, cy + 26, COLOR_WHITE);
  gfx->drawLine(cx + 14, cy + 18, cx + 22, cy + 26, COLOR_WHITE);
}

static void draw_centered_str(int16_t y, const char *s, uint8_t size, uint16_t color) {
  gfx->setFont(nullptr);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  int char_w = 6 * size;
  int n = (int)strlen(s);
  gfx->setCursor((DISP_WIDTH - n * char_w) / 2, y);
  gfx->print(s);
}

void about_enter() {
  gfx->fillScreen(COLOR_BLACK);

  // Brand title "JABROS" in the bold mono font, centred horizontally.
  gfx->setFont(&FreeMono24pt7b);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_CYAN);
  const char *brand = "JABROS";
  int16_t bx, by;
  uint16_t bw, bh;
  gfx->getTextBounds(brand, 0, 45, &bx, &by, &bw, &bh);
  int16_t title_x = (DISP_WIDTH - (int16_t)bw) / 2 - bx;
  gfx->setCursor(title_x, 45);
  gfx->print(brand);

  // Tiny "TM" superscript next to the title
  gfx->setFont(nullptr);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_GREY);
  gfx->setCursor(title_x + (int)bw + 3, 22);
  gfx->print("TM");

  // Robot mascot
  draw_robot_logo(DISP_WIDTH / 2, 115);

  // Credit
  draw_centered_str(170, "by Tejas Diamos", 2, COLOR_WHITE);

  // Bio — three lines at size 1 (5x7 font)
  draw_centered_str(195, "Two brothers learning",   1, COLOR_GREY);
  draw_centered_str(208, "the world by building",   1, COLOR_GREY);
  draw_centered_str(221, "advanced robots.",        1, COLOR_GREY);

  Serial.println("About: JABROS™ by Tejas Diamos");
}

void about_tick() {
  // Static screen — nothing to update.
}
