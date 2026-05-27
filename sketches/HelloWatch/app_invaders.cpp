#include "apps.h"
#include "hardware.h"
#include "colors.h"
#include <Arduino.h>

// Space Invaders — kid-friendly. Tap anywhere to slide the ship to that
// column AND fire a bullet. Wipe out all the aliens to win. If an alien
// reaches the ship row, or an alien bullet hits the ship, game over.

#define ALIEN_COLS    6
#define ALIEN_ROWS    3
#define ALIEN_W       26
#define ALIEN_H       14
#define ALIEN_GAP_X   6
#define ALIEN_GAP_Y   8
#define ALIEN_STEP    400  // ms between formation moves
#define ALIEN_FIRE_MS 1800 // average ms between alien shots
#define BULLET_W      3
#define BULLET_H      8
#define SHIP_W        28
#define SHIP_H        12
#define SHIP_Y        222
#define PBULLET_STEP  50   // ms between player-bullet position updates
#define ABULLET_STEP  120
#define MAX_BULLETS   6

#define FORMATION_TOP    50
#define FORMATION_LEFT   (((DISP_WIDTH - (ALIEN_COLS * (ALIEN_W + ALIEN_GAP_X) - ALIEN_GAP_X)) / 2))
#define FORMATION_WIDTH  (ALIEN_COLS * (ALIEN_W + ALIEN_GAP_X) - ALIEN_GAP_X)
#define FORMATION_HEIGHT (ALIEN_ROWS * (ALIEN_H + ALIEN_GAP_Y) - ALIEN_GAP_Y)
#define DANGER_Y         (SHIP_Y - 10)

enum InvState { INV_PLAYING, INV_WON, INV_LOST };

struct Bullet {
  int16_t x, y;
  bool active;
  bool from_player;
};

static bool aliens[ALIEN_ROWS][ALIEN_COLS];
static int16_t formation_x;
static int16_t formation_y;
static int8_t  formation_dir;   // +1 or -1
static int16_t ship_x;
static int16_t ship_x_prev;
static Bullet  bullets[MAX_BULLETS];
static int     score;
static InvState inv_state;
static uint32_t last_alien_step_ms;
static uint32_t last_bullet_step_ms;
static uint32_t last_alien_fire_ms;

static uint16_t row_color(int r) {
  static const uint16_t cols[3] = { COLOR_RED, COLOR_YELLOW, COLOR_GREEN };
  return cols[r % 3];
}

static int16_t alien_x(int col) { return formation_x + col * (ALIEN_W + ALIEN_GAP_X); }
static int16_t alien_y(int row) { return formation_y + row * (ALIEN_H + ALIEN_GAP_Y); }

static void draw_one_alien(int row, int col, bool alive) {
  int16_t x = alien_x(col);
  int16_t y = alien_y(row);
  gfx->fillRect(x, y, ALIEN_W, ALIEN_H, COLOR_BLACK);
  if (!alive) return;
  uint16_t c = row_color(row);
  // Body — pixelated alien: domed top, two "legs" at bottom corners.
  gfx->fillRect(x + 4, y + 2, ALIEN_W - 8, ALIEN_H - 6, c);
  gfx->fillRect(x, y + 6, 4, ALIEN_H - 6, c);
  gfx->fillRect(x + ALIEN_W - 4, y + 6, 4, ALIEN_H - 6, c);
  gfx->fillRect(x + 2, y + ALIEN_H - 4, 4, 4, c);
  gfx->fillRect(x + ALIEN_W - 6, y + ALIEN_H - 4, 4, 4, c);
  // Two black eyes
  gfx->fillRect(x + 7, y + 5, 3, 3, COLOR_BLACK);
  gfx->fillRect(x + ALIEN_W - 10, y + 5, 3, 3, COLOR_BLACK);
}

static void draw_all_aliens() {
  // Erase the whole formation strip + a little buffer below for the previous
  // position when stepping down.
  gfx->fillRect(0, FORMATION_TOP - 4,
                DISP_WIDTH, formation_y - FORMATION_TOP + FORMATION_HEIGHT + 8,
                COLOR_BLACK);
  for (int r = 0; r < ALIEN_ROWS; r++)
    for (int c = 0; c < ALIEN_COLS; c++)
      draw_one_alien(r, c, aliens[r][c]);
}

static void draw_ship(int16_t x) {
  // Erase previous position
  gfx->fillRect(ship_x_prev - SHIP_W / 2, SHIP_Y, SHIP_W, SHIP_H, COLOR_BLACK);
  // Hull
  gfx->fillRoundRect(x - SHIP_W / 2, SHIP_Y + 4, SHIP_W, SHIP_H - 4, 2, COLOR_CYAN);
  // Cannon (turret pointing up)
  gfx->fillRect(x - 2, SHIP_Y, 4, 4, COLOR_CYAN);
  // Underside detail
  gfx->drawRect(x - SHIP_W / 2, SHIP_Y + 4, SHIP_W, SHIP_H - 4, COLOR_WHITE);
  ship_x_prev = x;
}

static void draw_score() {
  gfx->fillRect(0, 0, DISP_WIDTH, 24, COLOR_BLACK);
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setCursor(6, 4);
  char buf[24];
  snprintf(buf, sizeof(buf), "Score: %d", score);
  gfx->print(buf);
}

static void erase_bullet(const Bullet &b) {
  gfx->fillRect(b.x, b.y, BULLET_W, BULLET_H, COLOR_BLACK);
}
static void draw_bullet(const Bullet &b) {
  uint16_t c = b.from_player ? COLOR_YELLOW : COLOR_RED;
  gfx->fillRect(b.x, b.y, BULLET_W, BULLET_H, c);
}

static int find_bullet_slot(bool from_player) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].from_player = from_player;
      return i;
    }
  }
  return -1;
}

static void fire_player_bullet() {
  int i = find_bullet_slot(true);
  if (i < 0) return;
  bullets[i].x = ship_x - BULLET_W / 2;
  bullets[i].y = SHIP_Y - BULLET_H;
  draw_bullet(bullets[i]);
}

static void fire_alien_bullet_random() {
  // Find a random column that has at least one live alien; shoot from the lowest one.
  int candidate_cols[ALIEN_COLS];
  int n = 0;
  for (int c = 0; c < ALIEN_COLS; c++) {
    for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
      if (aliens[r][c]) { candidate_cols[n++] = c; break; }
    }
  }
  if (n == 0) return;
  int col = candidate_cols[random(n)];
  int row = -1;
  for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
    if (aliens[r][col]) { row = r; break; }
  }
  if (row < 0) return;

  int i = find_bullet_slot(false);
  if (i < 0) return;
  bullets[i].x = alien_x(col) + ALIEN_W / 2 - BULLET_W / 2;
  bullets[i].y = alien_y(row) + ALIEN_H;
  draw_bullet(bullets[i]);
}

static int count_live_aliens() {
  int n = 0;
  for (int r = 0; r < ALIEN_ROWS; r++)
    for (int c = 0; c < ALIEN_COLS; c++)
      if (aliens[r][c]) n++;
  return n;
}

static void show_overlay(const char *line1, const char *line2, uint16_t color) {
  gfx->fillRect(20, 90, DISP_WIDTH - 40, 90, COLOR_BLACK);
  gfx->drawRect(20, 90, DISP_WIDTH - 40, 90, color);
  gfx->setFont(nullptr);
  gfx->setTextSize(3);
  gfx->setTextColor(color);
  int n = (int)strlen(line1);
  gfx->setCursor((DISP_WIDTH - n * 18) / 2, 110);
  gfx->print(line1);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_GREY);
  n = (int)strlen(line2);
  gfx->setCursor((DISP_WIDTH - n * 6) / 2, 150);
  gfx->print(line2);
}

static void end_game(bool won) {
  inv_state = won ? INV_WON : INV_LOST;
  if (won) {
    static const uint8_t win[] = { 14, 14, 14, 70 };
    haptic_sequence(win, 4);
    show_overlay("YOU WIN!", "tap to play again", COLOR_GREEN);
  } else {
    static const uint8_t lose[] = { 16, 47 };
    haptic_sequence(lose, 2);
    show_overlay("GAME OVER", "tap to play again", COLOR_RED);
  }
}

void invaders_enter() {
  for (int r = 0; r < ALIEN_ROWS; r++)
    for (int c = 0; c < ALIEN_COLS; c++)
      aliens[r][c] = true;
  formation_x = FORMATION_LEFT;
  formation_y = FORMATION_TOP;
  formation_dir = 1;
  ship_x = DISP_WIDTH / 2;
  ship_x_prev = ship_x;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  score = 0;
  inv_state = INV_PLAYING;
  uint32_t now = millis();
  last_alien_step_ms = now;
  last_bullet_step_ms = now;
  last_alien_fire_ms = now;
  randomSeed((uint32_t)micros());

  gfx->fillScreen(COLOR_BLACK);
  draw_score();
  draw_all_aliens();
  // Force ship draw on a clean slate.
  ship_x_prev = ship_x;
  gfx->fillRect(ship_x - SHIP_W / 2, SHIP_Y, SHIP_W, SHIP_H, COLOR_BLACK);
  draw_ship(ship_x);
  Serial.println("Invaders: started");
}

static void step_aliens() {
  // Reverse + step down if the formation would go off-screen.
  int16_t next_x = formation_x + formation_dir * 6;
  if (next_x < 4 || next_x + FORMATION_WIDTH > DISP_WIDTH - 4) {
    formation_dir = -formation_dir;
    formation_y += 8;
  } else {
    formation_x = next_x;
  }
  draw_all_aliens();

  // If any alien crossed the danger line, game over.
  for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
    for (int c = 0; c < ALIEN_COLS; c++) {
      if (aliens[r][c] && alien_y(r) + ALIEN_H >= DANGER_Y) {
        end_game(false);
        return;
      }
    }
  }
}

static void step_bullets() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    erase_bullet(bullets[i]);
    if (bullets[i].from_player) bullets[i].y -= 8;
    else                        bullets[i].y += 5;
    if (bullets[i].y + BULLET_H < 0 || bullets[i].y > DISP_HEIGHT) {
      bullets[i].active = false;
      continue;
    }
    draw_bullet(bullets[i]);
  }
}

static void check_collisions() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    if (bullets[i].from_player) {
      // Player bullet vs aliens
      for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
          if (!aliens[r][c]) continue;
          int16_t ax = alien_x(c), ay = alien_y(r);
          if (bullets[i].x + BULLET_W > ax && bullets[i].x < ax + ALIEN_W &&
              bullets[i].y + BULLET_H > ay && bullets[i].y < ay + ALIEN_H) {
            aliens[r][c] = false;
            score += (ALIEN_ROWS - r) * 10;
            erase_bullet(bullets[i]);
            bullets[i].active = false;
            draw_one_alien(r, c, false);
            draw_score();
            haptic_play(1);
            if (count_live_aliens() == 0) {
              end_game(true);
              return;
            }
            goto next_bullet;
          }
        }
      }
    } else {
      // Alien bullet vs ship
      int16_t sx = ship_x - SHIP_W / 2;
      if (bullets[i].x + BULLET_W > sx && bullets[i].x < sx + SHIP_W &&
          bullets[i].y + BULLET_H > SHIP_Y && bullets[i].y < SHIP_Y + SHIP_H) {
        end_game(false);
        return;
      }
    }
    next_bullet:;
  }
}

void invaders_tick() {
  if (inv_state != INV_PLAYING) return;
  uint32_t now = millis();
  if (now - last_bullet_step_ms >= PBULLET_STEP) {
    last_bullet_step_ms = now;
    step_bullets();
    check_collisions();
    if (inv_state != INV_PLAYING) return;
  }
  if (now - last_alien_step_ms >= ALIEN_STEP) {
    last_alien_step_ms = now;
    step_aliens();
    if (inv_state != INV_PLAYING) return;
  }
  if (now - last_alien_fire_ms >= ALIEN_FIRE_MS) {
    last_alien_fire_ms = now;
    fire_alien_bullet_random();
  }
}

void invaders_tap(int16_t x, int16_t y) {
  if (inv_state != INV_PLAYING) {
    invaders_enter();
    return;
  }
  // Move ship toward tap, then fire.
  if (x < SHIP_W / 2) x = SHIP_W / 2;
  if (x > DISP_WIDTH - SHIP_W / 2) x = DISP_WIDTH - SHIP_W / 2;
  ship_x = x;
  draw_ship(ship_x);
  fire_player_bullet();
}
