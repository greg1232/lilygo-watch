// LilyGo T-Watch S3 — Clock + Memory game with swipe navigation
//
// Navigation:
//   Swipe LEFT  from clock → Memory game
//   Swipe RIGHT from game  → Clock
//
// Memory: tap pairs of cards (4 cols x 3 rows) to find matching symbols.

#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <XPowersLib.h>
#include <Adafruit_DRV2605.h>
#include <esp_sleep.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "FreeMono24pt7b.h"

// --- Colors (RGB565) ---
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED   0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_DIM_GREEN 0x03E0
#define COLOR_BLUE  0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_ORANGE 0xFD20
#define COLOR_PURPLE 0xA01F
#define COLOR_CYAN  0x07FF
#define COLOR_GREY  0x7BEF
#define COLOR_DARK_GREY 0x39E7
#define COLOR_CARD_BACK 0x18C3
#define COLOR_CARD_MATCHED 0x0320

#define FT6336U_ADDR 0x38

// Touch panel orientation relative to display. Toggle these if a tap
// registers on the wrong side / corner of the screen.
#define TOUCH_FLIP_X 1
#define TOUCH_FLIP_Y 1
#define TOUCH_SWAP_XY 0

enum AppId { APP_CLOCK = 0, APP_MEMORY = 1 };

enum GestureType {
  GESTURE_NONE,
  GESTURE_TAP,
  GESTURE_SWIPE_LEFT,
  GESTURE_SWIPE_RIGHT,
  GESTURE_SWIPE_UP,
  GESTURE_SWIPE_DOWN
};

// Forward declarations (used by screen_set_on() before defs)
void clock_enter();
void memory_enter();

#ifndef BUILD_EPOCH
#define BUILD_EPOCH 0
#endif
#define TZ_PACIFIC "PST8PDT,M3.2.0,M11.1.0"

XPowersAXP2101 PMU;
Adafruit_DRV2605 HAPTIC;
static bool haptic_ready = false;

// Play a single waveform effect from DRV2605 library 1 (see datasheet table).
// 1=strong click, 14=strong buzz, 24=sharp click, 70=transition ramp up.
static void haptic_play(uint8_t effect) {
  if (!haptic_ready) return;
  HAPTIC.setWaveform(0, effect);
  HAPTIC.setWaveform(1, 0);  // end of sequence
  HAPTIC.go();
}

// Play up to 7 chained effects (slot 7 is reserved for end marker).
static void haptic_sequence(const uint8_t *effects, uint8_t n) {
  if (!haptic_ready) return;
  if (n > 7) n = 7;
  for (uint8_t i = 0; i < n; i++) HAPTIC.setWaveform(i, effects[i]);
  HAPTIC.setWaveform(n, 0);
  HAPTIC.go();
}

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    DISP_DC, DISP_CS, DISP_SCK, DISP_MOSI, GFX_NOT_DEFINED);

Arduino_GFX *gfx = new Arduino_ST7789(
    bus, GFX_NOT_DEFINED, 0, true, DISP_WIDTH, DISP_HEIGHT, 0, 0, 0, 0);

// --- Touch + gesture detection ---
struct Gesture {
  bool active;
  int16_t start_x, start_y, last_x, last_y;
  uint32_t start_ms;
};
Gesture gesture = { false, 0, 0, 0, 0, 0 };

static bool ft_read(int16_t &x, int16_t &y) {
  Wire1.beginTransmission(FT6336U_ADDR);
  Wire1.write(0x02);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((uint8_t)FT6336U_ADDR, (uint8_t)5) != 5) return false;
  uint8_t num = Wire1.read();
  uint8_t xh = Wire1.read();
  uint8_t xl = Wire1.read();
  uint8_t yh = Wire1.read();
  uint8_t yl = Wire1.read();
  if (num == 0 || num > 2) return false;
  int16_t raw_x = ((xh & 0x0F) << 8) | xl;
  int16_t raw_y = ((yh & 0x0F) << 8) | yl;
#if TOUCH_SWAP_XY
  int16_t t = raw_x; raw_x = raw_y; raw_y = t;
#endif
#if TOUCH_FLIP_X
  raw_x = (DISP_WIDTH - 1) - raw_x;
#endif
#if TOUCH_FLIP_Y
  raw_y = (DISP_HEIGHT - 1) - raw_y;
#endif
  x = raw_x;
  y = raw_y;
  return true;
}

static GestureType poll_gesture(int16_t &tap_x, int16_t &tap_y) {
  int16_t x, y;
  if (ft_read(x, y)) {
    if (!gesture.active) {
      gesture.active = true;
      gesture.start_x = x;
      gesture.start_y = y;
      gesture.start_ms = millis();
    }
    gesture.last_x = x;
    gesture.last_y = y;
    return GESTURE_NONE;
  }
  if (!gesture.active) return GESTURE_NONE;
  gesture.active = false;
  int16_t dx = gesture.last_x - gesture.start_x;
  int16_t dy = gesture.last_y - gesture.start_y;
  int16_t adx = abs(dx);
  int16_t ady = abs(dy);
  if (adx > 60 && adx > ady) return dx > 0 ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
  if (ady > 60 && ady > adx) return dy > 0 ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
  if (adx < 25 && ady < 25) {
    tap_x = gesture.start_x;
    tap_y = gesture.start_y;
    return GESTURE_TAP;
  }
  return GESTURE_NONE;
}

// --- App framework ---
AppId current_app = APP_CLOCK;

// --- Screen on/off (crown button via AXP2101 power-key IRQ) ---
static bool screen_on = true;

// --- Inactivity auto-sleep ---
#define INACTIVITY_TIMEOUT_MS 120000UL  // 2 minutes
static uint32_t last_activity_ms = 0;

// --- Battery indicator ---
static int8_t battery_last_pct = -2;
static bool battery_last_charging = false;

static void draw_battery_icon(int16_t x, int16_t y, uint8_t pct, bool charging) {
  const int16_t bw = 30, bh = 14;
  // Erase a slightly larger area so the previous fill never bleeds through
  gfx->fillRect(x - 1, y - 1, bw + 6, bh + 2, COLOR_BLACK);
  gfx->drawRect(x, y, bw, bh, COLOR_WHITE);
  gfx->fillRect(x + bw, y + 4, 3, 6, COLOR_WHITE);

  uint16_t fill_color;
  if (charging) fill_color = COLOR_CYAN;
  else if (pct > 50) fill_color = COLOR_GREEN;
  else if (pct > 20) fill_color = COLOR_YELLOW;
  else fill_color = COLOR_RED;

  int16_t fill_w = ((bw - 4) * (int)pct) / 100;
  if (fill_w > 0) gfx->fillRect(x + 2, y + 2, fill_w, bh - 4, fill_color);
}

// Read PMU and redraw icon if state changed
static void refresh_battery_icon(bool force) {
  uint8_t pct = PMU.getBatteryPercent();
  bool charging = PMU.isCharging();
  if (force || pct != battery_last_pct || charging != battery_last_charging) {
    draw_battery_icon(202, 12, pct, charging);
    battery_last_pct = pct;
    battery_last_charging = charging;
  }
}

static void enter_app() {
  if (current_app == APP_CLOCK) clock_enter();
  else memory_enter();
}

static void screen_set_on(bool on) {
  if (on == screen_on) return;
  if (on) {
    PMU.enableALDO2();
    delay(30);
    gfx->begin();
    enter_app();
    digitalWrite(DISP_BL, HIGH);
    Serial.println("Screen ON");
  } else {
    digitalWrite(DISP_BL, LOW);
    PMU.disableALDO2();
    Serial.println("Screen OFF");
  }
  screen_on = on;
}

// --- Clock app ---
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
  // "Pacific Time" (144px wide) ends at x=192; battery icon starts at x=202 — no overlap
  const char *header = "Pacific Time";
  gfx->setCursor((DISP_WIDTH - (int)strlen(header) * 12) / 2, 20);
  gfx->print(header);
  clock_last_second = -1;
  clock_last_minute = -1;
  battery_last_pct = -2;  // force redraw
  refresh_battery_icon(true);
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

  // CRITICAL: custom GFX fonts honor textsize as a pixel multiplier.
  // Leftover textsize=2 from the header would render this at 2x and overflow.
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

    // Redraw header — erase only the text region, not the battery icon at x=200+
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

  // Refresh battery icon roughly every 30 seconds
  if (tm_local.tm_sec == 0 || tm_local.tm_sec == 30) {
    refresh_battery_icon(false);
  }
}

// --- Memory app ---
#define MEM_COLS 4
#define MEM_ROWS 3
#define MEM_N (MEM_COLS * MEM_ROWS)
#define CARD_W (DISP_WIDTH / MEM_COLS)
#define CARD_H (DISP_HEIGHT / MEM_ROWS)

struct Card {
  uint8_t symbol;
  bool flipped;
  bool matched;
};
Card mem_cards[MEM_N];
int8_t mem_first = -1;
int8_t mem_second = -1;
uint32_t mem_resolve_at = 0;
uint8_t mem_matched_pairs = 0;

static uint16_t SYMBOL_COLORS[6] = {
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
  // stem
  gfx->fillTriangle(cx - 2, cy + lobe/2, cx + 2, cy + lobe/2, cx, cy + r, COLOR_GREEN);
}

static void draw_diamond(int16_t cx, int16_t cy, int16_t r, uint16_t c) {
  gfx->fillTriangle(cx, cy - r, cx - r, cy, cx + r, cy, c);
  gfx->fillTriangle(cx, cy + r, cx - r, cy, cx + r, cy, c);
  // highlight
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
    // Effect 14 = "Strong Buzz - 100%" — sustained, very noticeable
    haptic_play(14);
    mem_first = -1;
    mem_second = -1;
    if (mem_matched_pairs == MEM_N / 2) {
      // Game complete — celebratory rumble: triple ramp + buzz
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

// --- Main ---
static void switch_app(AppId next) {
  if (next == current_app) return;
  current_app = next;
  if (next == APP_CLOCK) {
    Serial.println(">> Clock");
    clock_enter();
  } else {
    Serial.println(">> Memory");
    memory_enter();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nWatch booting...");

  Wire.begin(SDA, SCL);
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, SDA, SCL)) {
    PMU.setALDO2Voltage(3300);
    PMU.enableALDO2();
    // BLDO2 powers the DRV2605 enable pin — must be on before drv.begin()
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();
    // Enable crown / power-key short-press IRQ for screen on/off toggle
    PMU.clearIrqStatus();
    PMU.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);
  } else {
    Serial.println("PMU init FAILED");
  }

  // Haptic driver — needs PMU's BLDO2 powered first
  delay(20);
  if (HAPTIC.begin(&Wire)) {
    HAPTIC.useLRA();                       // T-Watch-S3 uses an LRA actuator
    HAPTIC.selectLibrary(6);               // ROM library tuned for LRA motors
    HAPTIC.setMode(DRV2605_MODE_INTTRIG);  // play on .go()
    haptic_ready = true;
    Serial.println("Haptic ready (LRA, lib 6)");
  } else {
    Serial.println("Haptic init FAILED");
  }

  Wire1.begin(TP_SDA, TP_SCL);

  // PMU INT is active-low open-drain; used as light-sleep wake source
  pinMode(PMU_INT, INPUT_PULLUP);

  pinMode(DISP_BL, OUTPUT);
  digitalWrite(DISP_BL, LOW);
  gfx->begin();
  gfx->fillScreen(COLOR_BLACK);
  digitalWrite(DISP_BL, HIGH);

  setenv("TZ", TZ_PACIFIC, 1);
  tzset();
  struct timeval tv = { .tv_sec = (time_t)BUILD_EPOCH, .tv_usec = 0 };
  settimeofday(&tv, nullptr);

  clock_enter();
  last_activity_ms = millis();
  Serial.println("Ready. Swipe LEFT for Memory game; swipe RIGHT to return.");
  Serial.println("Crown button: toggle screen. Auto-sleep after 2 min idle.");
}

void loop() {
  // Poll crown button (PMU power-key IRQ) for screen toggle
  PMU.getIrqStatus();
  if (PMU.isPekeyShortPressIrq()) {
    screen_set_on(!screen_on);
    PMU.clearIrqStatus();
    last_activity_ms = millis();
  }

  if (!screen_on) {
    // Skip light sleep while plugged into USB — sleep suspends USB CDC,
    // which makes the serial port disappear and breaks dev flashing.
    if (PMU.isVbusIn()) {
      delay(100);
      return;
    }
    // Enter light sleep until the crown is pressed (PMU INT pin goes low).
    // Drops ESP32 to ~1mA. Code resumes here on wake; next loop iteration
    // sees the button IRQ and re-enables the display.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PMU_INT, 0);
    esp_light_sleep_start();
    return;
  }

  int16_t tx, ty;
  GestureType g = poll_gesture(tx, ty);
  if (g != GESTURE_NONE) last_activity_ms = millis();
  switch (g) {
    case GESTURE_SWIPE_LEFT:
      if (current_app == APP_CLOCK) switch_app(APP_MEMORY);
      break;
    case GESTURE_SWIPE_RIGHT:
      if (current_app == APP_MEMORY) switch_app(APP_CLOCK);
      break;
    case GESTURE_TAP:
      Serial.printf("TAP at (%d, %d)\n", tx, ty);
      if (current_app == APP_MEMORY) memory_tap(tx, ty);
      break;
    default:
      break;
  }

  if (current_app == APP_CLOCK) clock_tick();
  else memory_tick();

  // Auto-sleep after INACTIVITY_TIMEOUT_MS with no taps, swipes, or button.
  if (screen_on && (millis() - last_activity_ms > INACTIVITY_TIMEOUT_MS)) {
    Serial.println("Auto-sleep (inactivity)");
    screen_set_on(false);
  }

  delay(20);
}
