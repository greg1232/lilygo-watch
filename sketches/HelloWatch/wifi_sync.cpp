#include "wifi_sync.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// Use wifi_secrets.h if it exists; otherwise the build still succeeds
// and WiFi simply does nothing at boot.
#if __has_include("wifi_secrets.h")
  #include "wifi_secrets.h"
#else
  #define WIFI_SSID     ""
  #define WIFI_PASSWORD ""
#endif

static bool synced = false;

bool wifi_time_synced() { return synced; }

void wifi_sync_time(uint32_t timeout_ms) {
  if (WIFI_SSID[0] == '\0') {
    Serial.println("WiFi: no credentials (create wifi_secrets.h to enable)");
    return;
  }

  Serial.printf("WiFi: connecting to '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("WiFi: connect failed (status=%d)\n", WiFi.status());
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  Serial.printf("WiFi: connected, IP %s, RSSI %d\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());

  // The system TZ env var is already set in HelloWatch.ino setup().
  // Pass 0 offsets here — localtime() respects TZ regardless.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Wait briefly for SNTP to fill in the system clock.
  struct tm tm;
  if (getLocalTime(&tm, 5000)) {
    synced = true;
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    Serial.printf("WiFi: NTP synced to %s PT\n", buf);
  } else {
    Serial.println("WiFi: NTP sync timed out");
  }

  // Disconnect and power off the radio — saves ~70 mA when not in use.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi: disconnected");
}
