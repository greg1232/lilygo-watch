#include "apps.h"
#include "hardware.h"
#include "colors.h"
#include "FreeMono24pt7b.h"
#include <Arduino.h>

// Spy-themed mascot — robot with fedora + sunglasses. Origin (cx, cy)
// is the centre of the head (the visible face, below the hat).
static void draw_agent_robot(int16_t cx, int16_t cy) {
  gfx->fillRoundRect(cx - 32, cy - 32, 64, 5, 2, COLOR_DARK_GREY);   // fedora brim
  gfx->fillRoundRect(cx - 20, cy - 44, 40, 14, 3, COLOR_DARK_GREY);  // fedora crown
  gfx->fillRect(cx - 20, cy - 32, 40, 3, COLOR_RED);                  // hat band

  gfx->fillRoundRect(cx - 24, cy - 22, 48, 32, 5, COLOR_GREY);
  gfx->drawRoundRect(cx - 24, cy - 22, 48, 32, 5, COLOR_WHITE);

  // Sunglasses
  gfx->fillRoundRect(cx - 19, cy - 12, 15, 9, 2, COLOR_BLACK);
  gfx->drawRoundRect(cx - 19, cy - 12, 15, 9, 2, COLOR_WHITE);
  gfx->fillRoundRect(cx + 4, cy - 12, 15, 9, 2, COLOR_BLACK);
  gfx->drawRoundRect(cx + 4, cy - 12, 15, 9, 2, COLOR_WHITE);
  gfx->drawLine(cx - 4, cy - 8, cx + 4, cy - 8, COLOR_WHITE);
  gfx->drawPixel(cx - 14, cy - 10, COLOR_WHITE);
  gfx->drawPixel(cx + 9, cy - 10, COLOR_WHITE);

  gfx->drawLine(cx - 5, cy + 4, cx + 5, cy + 4, COLOR_GREEN);  // smirk
  gfx->drawPixel(cx + 6, cy + 3, COLOR_GREEN);

  gfx->fillRect(cx - 14, cy + 14, 28, 18, COLOR_DARK_GREY);
  gfx->drawRect(cx - 14, cy + 14, 28, 18, COLOR_WHITE);
  gfx->drawLine(cx - 14, cy + 14, cx - 6, cy + 22, COLOR_WHITE);
  gfx->drawLine(cx + 14, cy + 14, cx + 6, cy + 22, COLOR_WHITE);
  gfx->fillCircle(cx, cy + 25, 3, COLOR_YELLOW);

  gfx->drawLine(cx - 14, cy + 17, cx - 22, cy + 25, COLOR_WHITE);
  gfx->drawLine(cx + 14, cy + 17, cx + 22, cy + 25, COLOR_WHITE);
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

// --- Secret-code state ---
// To unlock the hidden game: tap A.G.E.N.T. (it flashes), then tap
// Tejas within 5 seconds (also flashes). Wrong sequence quietly resets.
enum CodeState { CODE_IDLE, CODE_AGENT_HIT };
static CodeState code_state = CODE_IDLE;
static uint32_t code_armed_at = 0;
#define CODE_TIMEOUT_MS 5000UL

// Layout constants — must match the draw calls in about_enter().
#define AGENT_X 30
#define AGENT_Y 10
#define AGENT_W 180   // 10 chars * 18 px (size 3 font)
#define AGENT_H 24
#define CREDIT_Y 205
#define CREDIT_H 16
// "Tejas & Ojas" is 12 chars * 12 = 144 wide, centered → x in [48, 192].
// "Tejas" is the first 5 chars → x in [48, 108].
#define TEJAS_X 48
#define TEJAS_W 60   // 5 chars * 12 px (size 2 font)

static bool point_in_rect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh, int16_t pad = 6) {
  return (x >= rx - pad && x < rx + rw + pad && y >= ry - pad && y < ry + rh + pad);
}

static void draw_agent_title(uint16_t color) {
  gfx->fillRect(0, AGENT_Y - 2, DISP_WIDTH, AGENT_H + 4, COLOR_BLACK);
  draw_centered_str(AGENT_Y, "A.G.E.N.T.", 3, color);
}

static void draw_credit(uint16_t tejas_color) {
  // Erase the credit line area
  gfx->fillRect(0, CREDIT_Y - 2, DISP_WIDTH, CREDIT_H + 4, COLOR_BLACK);
  // Manually render "Tejas & Ojas" with "Tejas" optionally highlighted.
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  int x = TEJAS_X;
  gfx->setTextColor(tejas_color);
  gfx->setCursor(x, CREDIT_Y);
  gfx->print("Tejas");
  x += 5 * 12;
  gfx->setTextColor(COLOR_WHITE);
  gfx->setCursor(x, CREDIT_Y);
  gfx->print(" & Ojas");
}

void about_enter() {
  gfx->fillScreen(COLOR_BLACK);
  draw_agent_title(COLOR_CYAN);
  draw_centered_str(40, "Answering Gadget with", 1, COLOR_GREY);
  draw_centered_str(52, "Exciting New Technology", 1, COLOR_GREY);
  draw_agent_robot(DISP_WIDTH / 2, 125);
  draw_centered_str(180, "Agent of C.L.A.S.S.", 2, COLOR_RED);
  draw_credit(COLOR_WHITE);
  draw_centered_str(225, "Brothers", 1, COLOR_GREY);
  code_state = CODE_IDLE;
  Serial.println("About: A.G.E.N.T. — Tejas & Ojas Brothers");
}

void about_tick() {
  // Time out the unlock sequence if the user pauses too long
  if (code_state == CODE_AGENT_HIT &&
      (millis() - code_armed_at) > CODE_TIMEOUT_MS) {
    code_state = CODE_IDLE;
    Serial.println("Secret code timed out");
  }
}

void about_tap(int16_t x, int16_t y) {
  bool hit_agent = point_in_rect(x, y, AGENT_X, AGENT_Y, AGENT_W, AGENT_H);
  bool hit_tejas = point_in_rect(x, y, TEJAS_X, CREDIT_Y, TEJAS_W, CREDIT_H);

  if (hit_agent) {
    // Flash AGENT yellow briefly, then back to cyan. Arm the code.
    draw_agent_title(COLOR_YELLOW);
    haptic_play(1);  // strong click
    delay(220);
    draw_agent_title(COLOR_CYAN);
    code_state = CODE_AGENT_HIT;
    code_armed_at = millis();
    Serial.println("Code: AGENT armed");
    return;
  }

  if (hit_tejas && code_state == CODE_AGENT_HIT) {
    // Sequence complete — flash Tejas + buzz + launch.
    draw_credit(COLOR_YELLOW);
    haptic_play(14);  // strong buzz
    delay(350);
    draw_credit(COLOR_GREEN);
    delay(120);
    code_state = CODE_IDLE;
    Serial.println("Code: UNLOCKED → Invaders");
    app_switch(APP_INVADERS);
    return;
  }

  // Tap anywhere else: harmless, doesn't reset the code (timeout handles that).
}
