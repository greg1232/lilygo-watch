#pragma once

#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <XPowersLib.h>
#include <Adafruit_DRV2605.h>

// Display + PMU + haptic globals, set up by hardware_init().
extern Arduino_GFX *gfx;
extern XPowersAXP2101 PMU;
extern Adafruit_DRV2605 HAPTIC;
extern bool haptic_ready;

// One-shot initialisation of PMU rails, Wire/Wire1 buses, display, and DRV2605.
// Safe to call once from setup() — must precede any display or haptic use.
void hardware_init();

// Play a single DRV2605 ROM effect (library 1, ERM motor).
// Common picks: 1=strong click, 14=strong buzz, 24=sharp click, 47=long buzz.
void haptic_play(uint8_t effect);

// Play up to 7 chained effects (slot 7 reserved for the end marker).
void haptic_sequence(const uint8_t *effects, uint8_t n);
