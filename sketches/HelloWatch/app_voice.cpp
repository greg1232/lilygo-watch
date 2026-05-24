#include "apps.h"
#include "hardware.h"
#include "audio.h"
#include "colors.h"
#include <Arduino.h>

// Layout
#define BTN_CX (DISP_WIDTH / 2)
#define BTN_CY 120
#define BTN_R  55
#define CLEAR_CX (DISP_WIDTH / 2)
#define CLEAR_CY 200
#define CLEAR_R  18

enum VoiceState {
  V_IDLE_EMPTY,
  V_RECORDING,
  V_IDLE_HAS_REC,
  V_PLAYING,
};
static VoiceState v_state = V_IDLE_EMPTY;

static void draw_label_centered(int16_t cx, int16_t cy, const char *s, uint8_t size, uint16_t color) {
  gfx->setFont(nullptr);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  int char_w = 6 * size;
  int char_h = 8 * size;
  int n = (int)strlen(s);
  gfx->setCursor(cx - (n * char_w) / 2, cy - char_h / 2);
  gfx->print(s);
}

static void draw_big_button(uint16_t fill, const char *label) {
  // Erase the whole button band so the previous state's label is gone.
  gfx->fillRect(0, BTN_CY - BTN_R - 2, DISP_WIDTH, 2 * BTN_R + 4, COLOR_BLACK);
  gfx->fillCircle(BTN_CX, BTN_CY, BTN_R, fill);
  gfx->drawCircle(BTN_CX, BTN_CY, BTN_R, COLOR_WHITE);
  draw_label_centered(BTN_CX, BTN_CY, label, 3, COLOR_WHITE);
}

static void draw_clear_button(bool visible) {
  gfx->fillRect(CLEAR_CX - CLEAR_R - 2, CLEAR_CY - CLEAR_R - 2,
                2 * (CLEAR_R + 2), 2 * (CLEAR_R + 2), COLOR_BLACK);
  if (!visible) return;
  gfx->drawCircle(CLEAR_CX, CLEAR_CY, CLEAR_R, COLOR_GREY);
  draw_label_centered(CLEAR_CX, CLEAR_CY, "X", 2, COLOR_GREY);
}

static void draw_status(const char *s, uint16_t color) {
  // Status text below the clear button.
  gfx->fillRect(0, 225, DISP_WIDTH, 15, COLOR_BLACK);
  draw_label_centered(DISP_WIDTH / 2, 232, s, 1, color);
}

static void render_state() {
  switch (v_state) {
    case V_IDLE_EMPTY:
      draw_big_button(COLOR_RED, "REC");
      draw_clear_button(false);
      draw_status("tap to record (5s)", COLOR_GREY);
      break;
    case V_RECORDING:
      draw_big_button(COLOR_RED, "...");
      draw_clear_button(false);
      draw_status("recording...", COLOR_RED);
      break;
    case V_IDLE_HAS_REC:
      draw_big_button(COLOR_GREEN, "PLAY");
      draw_clear_button(true);
      draw_status("tap to play, X to clear", COLOR_GREY);
      break;
    case V_PLAYING:
      draw_big_button(COLOR_BLUE, "...");
      draw_clear_button(false);
      draw_status("playing...", COLOR_CYAN);
      break;
  }
}

void voice_enter() {
  gfx->fillScreen(COLOR_BLACK);
  // Title
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_WHITE);
  const char *title = "Voice Memo";
  gfx->setCursor((DISP_WIDTH - (int)strlen(title) * 12) / 2, 18);
  gfx->print(title);

  v_state = audio_has_recording() ? V_IDLE_HAS_REC : V_IDLE_EMPTY;
  render_state();
}

void voice_tick() {
  // Recording and playback are blocking from voice_tap(); nothing to do here.
}

static bool inside_circle(int16_t x, int16_t y, int16_t cx, int16_t cy, int16_t r) {
  int dx = x - cx, dy = y - cy;
  return (dx * dx + dy * dy) <= (int)r * r;
}

void voice_tap(int16_t x, int16_t y) {
  // Clear button (only meaningful when we have a recording)
  if (v_state == V_IDLE_HAS_REC && inside_circle(x, y, CLEAR_CX, CLEAR_CY, CLEAR_R + 8)) {
    audio_clear();
    v_state = V_IDLE_EMPTY;
    render_state();
    return;
  }

  if (!inside_circle(x, y, BTN_CX, BTN_CY, BTN_R)) return;

  switch (v_state) {
    case V_IDLE_EMPTY:
      v_state = V_RECORDING;
      render_state();
      if (audio_record()) v_state = V_IDLE_HAS_REC;
      else                v_state = V_IDLE_EMPTY;
      render_state();
      break;
    case V_IDLE_HAS_REC:
      v_state = V_PLAYING;
      render_state();
      audio_play();
      v_state = V_IDLE_HAS_REC;
      render_state();
      break;
    default:
      break;
  }
}
