#include "hardware.h"
#include "colors.h"

XPowersAXP2101 PMU;
Adafruit_DRV2605 HAPTIC;
bool haptic_ready = false;

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    DISP_DC, DISP_CS, DISP_SCK, DISP_MOSI, GFX_NOT_DEFINED);

Arduino_GFX *gfx = new Arduino_ST7789(
    bus, GFX_NOT_DEFINED, 0, true, DISP_WIDTH, DISP_HEIGHT, 0, 0, 0, 0);

void haptic_play(uint8_t effect) {
  if (!haptic_ready) return;
  HAPTIC.setWaveform(0, effect);
  HAPTIC.setWaveform(1, 0);
  HAPTIC.go();
}

void haptic_sequence(const uint8_t *effects, uint8_t n) {
  if (!haptic_ready) return;
  if (n > 7) n = 7;
  for (uint8_t i = 0; i < n; i++) HAPTIC.setWaveform(i, effects[i]);
  HAPTIC.setWaveform(n, 0);
  HAPTIC.go();
}

void hardware_init() {
  Wire.begin(SDA, SCL);
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, SDA, SCL)) {
    PMU.setALDO2Voltage(3300);  // LCD VDD
    PMU.enableALDO2();
    PMU.setBLDO2Voltage(3300);  // DRV2605 enable
    PMU.enableBLDO2();
    PMU.clearIrqStatus();
    PMU.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);
  } else {
    Serial.println("PMU init FAILED");
  }

  delay(20);  // let BLDO2 settle before DRV2605 wakes up
  if (HAPTIC.begin(&Wire)) {
    HAPTIC.useERM();
    HAPTIC.selectLibrary(1);
    HAPTIC.setMode(DRV2605_MODE_INTTRIG);
    haptic_ready = true;
    Serial.println("Haptic ready (ERM, lib 1)");
  } else {
    Serial.println("Haptic init FAILED");
  }

  Wire1.begin(TP_SDA, TP_SCL);

  // PMU INT is active-low open-drain; used as light-sleep wake source.
  pinMode(PMU_INT, INPUT_PULLUP);

  pinMode(DISP_BL, OUTPUT);
  digitalWrite(DISP_BL, LOW);
  gfx->begin();
  gfx->fillScreen(COLOR_BLACK);
  digitalWrite(DISP_BL, HIGH);
}
