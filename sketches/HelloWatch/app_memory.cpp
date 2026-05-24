#include "apps.h"
#include "hardware.h"
#include "colors.h"
#include <Arduino.h>
#include <math.h>

#define MEM_COLS 4
#define MEM_ROWS 3
#define MEM_N    (MEM_COLS * MEM_ROWS)
#define CARD_W   (DISP_WIDTH / MEM_COLS)
#define CARD_H   (DISP_HEIGHT / MEM_ROWS)

struct Card {
  uint8_t symbol;
  bool flipped;
  bool matched;
};

static Card mem_cards[MEM_N];
static int8_t mem_first = -1;
static int8_t mem_second = -1;
static uint32_t mem_resolve_at = 0;
static uint8_t mem_matched_pairs = 0;

static const uint16_t SYMBOL_COLORS[6] = {
  COLOR_RED, COLOR_YELLOW, COLOR_ORANGE, COLOR_WHITE, COLOR_GREEN, COLOR_PURPLE
};
static const char *SYMBOL_NAMES[6] = {
  "heart", "star", "sun", "moon", "clover", "diamond"
};

static void draw_heart(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  int16_t lobe = r * 5 / 12;
  gfx->fillCircle(cx - r/2, cy - r/4, lobe, c);
  gfx->fillCircle(cx + r/2, cy - r/4, lobe, c);
  gfx->fillTriangle(cx - r, cy - r/4, cx + r, cy - r/4, cx, cy + r, c);
}

static void draw_star(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  int16_t s = r / 3;
  gfx->fillTriangle(cx, cy - r, cx - s, cy, cx + s, cy, c);
  gfx->fillTriangle(cx, cy + r, cx - s, cy, cx + s, cy, c);
  gfx->fillTriangle(cx - r, cy, cx, cy - s, cx, cy + s, c);
  gfx->fillTriangle(cx + r, cy, cx, cy - s, cx, cy + s, c);
  gfx->fillCircle(cx, cy, s, c);
}

static void draw_sun(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  gfx->fillCircle(cx, cy, r * 2 / 3 - 2, c);
  for (int i = 0; i < 8; i++) {
    float a = i * (M_PI / 4);
    int16_t x1 = cx + (int16_t)(cos(a) * (r * 0.7f));
    int16_t y1 = cy + (int16_t)(sin(a) * (r * 0.7f));
    int16_t x2 = cx + (int16_t)(cos(a) * r);
    int16_t y2 = cy + (int16_t)(sin(a) * r);
    gfx->drawLine(x1, y1, x2, y2, c);
    gfx->drawLine(x1, y1 + 1, x2, y2 + 1, c);
    gfx->drawLine(x1 + 1, y1, x2 + 1, y2, c);
  }
}

static void draw_moon(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  gfx->fillCircle(cx, cy, r, c);
  gfx->fillCircle(cx + r / 3, cy - r / 6, r - 2, COLOR_BLACK);
}

static void draw_clover(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  int16_t lobe = r * 5 / 12;
  gfx->fillCircle(cx - r/2, cy - r/3, lobe, c);
  gfx->fillCircle(cx + r/2, cy - r/3, lobe, c);
  gfx->fillCircle(cx, cy - r * 3 / 4, lobe, c);
  gfx->fillCircle(cx, cy, lobe, c);
  gfx->fillTriangle(cx - 2, cy + lobe/2, cx + 2, cy + lobe/2, cx, cy + r, COLOR_GREEN);
}

static void draw_diamond(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  gfx->fillTriangle(cx, cy - r, cx - r, cy, cx + r, cy, c);
  gfx->fillTriangle(cx, cy + r, cx - r, cy, cx + r, cy, c);
  gfx->drawLine(cx - r/2, cy - r/2, cx + r/3, cy - r/6, COLOR_WHITE);
}

static void draw_symbol(uint8_t sym, int16_t cx, int16_t cy, int16_t r) {
  uint16_t c = SYMBOL_COLORS[sym];
  switch (sym) {
    case 0: draw_heart(cx, cy, r, c); break;
    case 1: draw_star(cx, cy, r, c); break;
    case 2: draw_sun(cx, cy, r, c); break;
    case 3: draw_moon(cx, cy, r, c); break;
    case 4: draw_clover(cx, cy, r, c); break;
    case 5: draw_diamond(cx, cy, r, c); break;
  }
}

static void draw_card(uint8_t idx) {
  int16_t col = idx % MEM_COLS;
  int16_t row = idx / MEM_COLS;
  int16_t x = col * CARD_W;
  int16_t y = row * CARD_H;
  Card &c = mem_cards[idx];

  if (c.flipped || c.matched) {
    gfx->fillRect(x, y, CARD_W, CARD_H, COLOR_BLACK);
    if (c.matched) {
      gfx->drawRect(x, y, CARD_W, CARD_H, COLOR_CARD_MATCHED);
      gfx->drawRect(x + 1, y + 1, CARD_W - 2, CARD_H - 2, COLOR_CARD_MATCHED);
    } else {
      gfx->drawRect(x, y, CARD_W, CARD_H, COLOR_DARK_GREY);
    }
    int16_t r = (CARD_W < CARD_H ? CARD_W : CARD_H) / 2 - 10;
    draw_symbol(c.symbol, x + CARD_W / 2, y + CARD_H / 2, r);
  } else {
    gfx->fillRect(x, y, CARD_W, CARD_H, COLOR_CARD_BACK);
    gfx->drawRect(x, y, CARD_W, CARD_H, COLOR_DARK_GREY);
    int16_t cx = x + CARD_W / 2;
    int16_t cy = y + CARD_H / 2;
    gfx->fillCircle(cx, cy, 4, COLOR_GREY);
    gfx->drawCircle(cx, cy, 12, COLOR_GREY);
    gfx->drawCircle(cx, cy, 20, COLOR_DARK_GREY);
  }
}

static void memory_shuffle() {
  uint8_t deck[MEM_N];
  for (int i = 0; i < MEM_N; i++) deck[i] = i / 2;
  for (int i = MEM_N - 1; i > 0; i--) {
    int j = random(i + 1);
    uint8_t t = deck[i]; deck[i] = deck[j]; deck[j] = t;
  }
  for (int i = 0; i < MEM_N; i++) {
    mem_cards[i].symbol = deck[i];
    mem_cards[i].flipped = false;
    mem_cards[i].matched = false;
  }
  mem_first = -1;
  mem_second = -1;
  mem_resolve_at = 0;
  mem_matched_pairs = 0;
}

void memory_enter() {
  randomSeed((uint32_t)micros());
  memory_shuffle();
  gfx->fillScreen(COLOR_BLACK);
  for (int i = 0; i < MEM_N; i++) draw_card(i);
}

static void memory_celebrate() {
  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < MEM_N; i++) {
      int16_t x = (i % MEM_COLS) * CARD_W;
      int16_t y = (i / MEM_COLS) * CARD_H;
      gfx->fillRect(x, y, CARD_W, CARD_H, SYMBOL_COLORS[(i + round) % 6]);
    }
    delay(200);
    for (int i = 0; i < MEM_N; i++) draw_card(i);
    delay(200);
  }
  memory_shuffle();
  for (int i = 0; i < MEM_N; i++) draw_card(i);
}

void memory_tap(int16_t x, int16_t y) {
  if (mem_resolve_at != 0) return;
  if (x < 0 || x >= DISP_WIDTH || y < 0 || y >= DISP_HEIGHT) return;
  int col = x / CARD_W;
  int row = y / CARD_H;
  if (col < 0 || col >= MEM_COLS || row < 0 || row >= MEM_ROWS) return;
  int idx = row * MEM_COLS + col;
  Card &c = mem_cards[idx];
  if (c.matched || c.flipped) return;
  c.flipped = true;
  draw_card(idx);
  Serial.printf("flip %d (%s)\n", idx, SYMBOL_NAMES[c.symbol]);

  if (mem_first < 0) {
    mem_first = idx;
    return;
  }
  mem_second = idx;
  if (mem_cards[mem_first].symbol == mem_cards[mem_second].symbol) {
    mem_cards[mem_first].matched = true;
    mem_cards[mem_second].matched = true;
    draw_card(mem_first);
    draw_card(mem_second);
    mem_matched_pairs++;
    Serial.printf("MATCH! pairs=%d\n", mem_matched_pairs);
    haptic_play(47);  // "Buzz 1 - 100%" — longest, strongest ERM buzz in lib 1
    mem_first = -1;
    mem_second = -1;
    if (mem_matched_pairs == MEM_N / 2) {
      static const uint8_t win_seq[] = { 14, 14, 14, 70 };
      haptic_sequence(win_seq, 4);
      memory_celebrate();
    }
  } else {
    mem_resolve_at = millis() + 900;
  }
}

void memory_tick() {
  if (mem_resolve_at != 0 && millis() >= mem_resolve_at) {
    mem_cards[mem_first].flipped = false;
    mem_cards[mem_second].flipped = false;
    draw_card(mem_first);
    draw_card(mem_second);
    mem_first = -1;
    mem_second = -1;
    mem_resolve_at = 0;
  }
}
