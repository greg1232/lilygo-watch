# Adding a new app

Step-by-step recipe to add a new app to the watch. We'll use a hypothetical `app_stopwatch` as the example.

## 1. Reserve an `AppId`

Edit `apps.h`:

```cpp
enum AppId {
  APP_CLOCK = 0,
  APP_MEMORY = 1,
  APP_VOICE = 2,
  APP_ABOUT = 3,
  APP_STOPWATCH = 4,   // ← new
};
```

Add forward declarations for the entry points the new app exposes. At minimum every app needs `<name>_enter()` and `<name>_tick()`; add `<name>_tap()` if the app responds to taps:

```cpp
void stopwatch_enter();
void stopwatch_tick();
void stopwatch_tap(int16_t x, int16_t y);
```

## 2. Wire it into the dispatcher

Edit `apps.cpp`. Three switch statements need a new `case`:

```cpp
void app_enter_current() {
  switch (current_app) {
    case APP_CLOCK:     clock_enter(); break;
    case APP_MEMORY:    memory_enter(); break;
    case APP_VOICE:     voice_enter(); break;
    case APP_ABOUT:     about_enter(); break;
    case APP_STOPWATCH: stopwatch_enter(); break;   // ← new
  }
}

void app_switch(AppId next) {
  // ...
  const char *name = (next == APP_CLOCK)     ? ">> Clock"
                   : (next == APP_MEMORY)    ? ">> Memory"
                   : (next == APP_VOICE)     ? ">> Voice"
                   : (next == APP_ABOUT)     ? ">> About"
                                             : ">> Stopwatch";   // ← new
  // ...
}

void app_tick() {
  switch (current_app) {
    case APP_CLOCK:     clock_tick(); break;
    case APP_MEMORY:    memory_tick(); break;
    case APP_VOICE:     voice_tick(); break;
    case APP_ABOUT:     about_tick(); break;
    case APP_STOPWATCH: stopwatch_tick(); break;   // ← new
  }
}

void app_handle_tap(int16_t x, int16_t y) {
  switch (current_app) {
    case APP_MEMORY:    memory_tap(x, y); break;
    case APP_VOICE:     voice_tap(x, y); break;
    case APP_STOPWATCH: stopwatch_tap(x, y); break;   // ← new
    default: break;
  }
}
```

## 3. Bind a gesture to enter your app

Edit `HelloWatch.ino`'s `loop()` switch on the gesture. With the current nav grid (Clock at the center, Memory/Voice/About at the four cardinal directions), there's no free direction from Clock left. You have a few options:

- **Replace** an existing mapping — e.g. swipe-down from Clock now goes to Stopwatch, and About becomes a long-press destination.
- **Add a second-level swipe** — e.g. from Memory, swipe up to reach Stopwatch (no current mapping uses Memory + up).
- **Convert to linear cycling** — replace the cardinal grid with `swipe_left = next app, swipe_right = prev app`. Scales better past 4 apps.

For this example we'll add Memory + swipe-up = Stopwatch:

```cpp
case GESTURE_SWIPE_UP:
  if      (current_app == APP_CLOCK)  app_switch(APP_VOICE);
  else if (current_app == APP_ABOUT)  app_switch(APP_CLOCK);
  else if (current_app == APP_MEMORY) app_switch(APP_STOPWATCH);   // ← new
  break;
case GESTURE_SWIPE_DOWN:
  if      (current_app == APP_VOICE)     app_switch(APP_CLOCK);
  else if (current_app == APP_CLOCK)     app_switch(APP_ABOUT);
  else if (current_app == APP_STOPWATCH) app_switch(APP_MEMORY);   // ← new
  break;
```

(Document the new mapping in `apps.md`'s navigation diagram.)

## 4. Create `app_stopwatch.cpp`

Template:

```cpp
#include "apps.h"
#include "hardware.h"   // for gfx, haptic_play, etc.
#include "colors.h"
#include <Arduino.h>

// --- per-app state ---
static bool running = false;
static uint32_t start_ms = 0;
static uint32_t accumulated_ms = 0;

// --- helpers ---
static void render() {
  gfx->fillScreen(COLOR_BLACK);
  // ... draw your UI ...
}

// --- entry points ---
void stopwatch_enter() {
  render();
}

void stopwatch_tick() {
  if (!running) return;
  // Update the displayed time roughly every 100 ms.
}

void stopwatch_tap(int16_t x, int16_t y) {
  // Translate tap into start/stop/reset.
  // Optionally: haptic_play(1) for a satisfying tactile response.
}
```

Files in the sketch dir are auto-compiled — no Makefile to touch.

## 5. Compile and flash

```sh
./flash.sh
```

Watch for compile errors. The most common gotchas:

- **`'StopwatchState' does not name a type`** in the `.ino` — if you use a custom enum or type in the `.ino`, put its definition in a header (`stopwatch.h`?) that's included before any function that references it. Arduino's auto-prototype generator hoists declarations and breaks if it sees a function returning an undeclared type.
- **Forgetting `setTextSize(1)` before a custom GFX font** — the previous app's last `setTextSize()` carries over. Set it explicitly.
- **Forgetting to erase before redrawing** — if the previous content has different widths or heights than the new, you'll see ghost pixels. Either fillRect the bounds, or use a `getTextBounds()`-based wider erase.

## 6. Test paths to cover

- App switch on every direction works and the new app renders fresh.
- Sleeping (auto or via crown) and waking re-renders the new app correctly — i.e. `*_enter()` re-renders fully from scratch, not assumes-prior-state.
- Auto-sleep still kicks in after 2 minutes of no input.
- If your app uses haptic, audio, or shared resources, make sure they're cleanly released — the apps share `gfx` and the I²C buses, no need to deinit those.

## 7. Document and commit

- Add a section to `docs/apps.md` describing the layout and state machine.
- Update the navigation diagram in `docs/apps.md` and `docs/architecture.md`.
- Commit with a message that explains *why*, not just *what*.

## Common app-architecture patterns

### Static-content app (like About)
- `*_enter()` draws everything, `*_tick()` is a no-op, `*_tap()` is optional.
- Cheapest possible app. ~30 lines of code.

### Per-tick-updating app (like Clock)
- `*_enter()` draws the static parts (header, etc.).
- `*_tick()` redraws only what changed (digits per second, date per minute).
- Use timestamps to dedupe per-tick redraws.

### Interactive app (like Memory)
- `*_enter()` initializes game state and renders.
- `*_tick()` handles time-based transitions (mismatch flip-back).
- `*_tap()` is the entire game logic.

### Blocking app (like Voice)
- `*_tap()` does heavy synchronous work (record / play).
- `*_tick()` is a no-op or handles post-blocking state cleanup.
- Watch is unresponsive during the blocking call — acceptable for short operations, painful for long ones.
