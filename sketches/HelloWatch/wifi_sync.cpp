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

void wifi_sync_time(const char *tz, time_t build_epoch, uint32_t timeout_ms) {
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

  // configTzTime() atomically sets TZ and starts NTP. Using configTime()
  // with 0 offsets would clobber TZ back to "GMT0" and the clock would
  // display UTC even after sync.
  // Google's anycast NTP IPs (bypasses local DNS that some captive
  // portals hijack to return wrong times). Falls back to pool.ntp.org
  // and time.google.com hostnames if the IPs are filtered.
  configTzTime(tz, "216.239.35.0", "time.google.com", "pool.ntp.org");
  // Belt-and-suspenders: re-apply TZ after configTzTime in case the
  // setenv inside it loses to a race with anything else.
  setenv("TZ", tz, 1);
  tzset();

  // Wait briefly for SNTP to fill in the system clock.
  struct tm tm;
  if (getLocalTime(&tm, 5000)) {
    time_t raw = time(nullptr);
    // Sanity check: NTP time should be at or after build_epoch (time
    // only moves forward, and the firmware was built at build_epoch).
    // If it's earlier, the response is implausible — could be a network
    // hijack, an SNTP/NAT bug, or a misbehaving stratum-N server.
    if (build_epoch > 0 && raw < build_epoch - 60) {
      Serial.printf("WiFi: NTP returned %lld but build was at %lld "
                    "(%ld sec earlier) — implausible, rejecting\n",
                    (long long)raw, (long long)build_epoch,
                    (long)(build_epoch - raw));
      struct timeval tv = { .tv_sec = build_epoch, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
      synced = false;
    } else {
      synced = true;
      char buf[32];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %p", &tm);
      Serial.printf("WiFi: NTP synced to %s PT (epoch=%lld)\n",
                    buf, (long long)raw);
    }
  } else {
    Serial.println("WiFi: NTP sync timed out");
  }

  // Disconnect and power off the radio — saves ~70 mA when not in use.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi: disconnected");
}
