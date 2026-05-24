#pragma once

// Force-redraw the battery icon at top-right of the screen on next refresh.
void battery_invalidate();

// Re-read the PMU and redraw the icon if state changed (or if force==true).
// Color-coded: green >50%, yellow 20-50%, red <=20%, cyan when charging.
void battery_refresh(bool force);
