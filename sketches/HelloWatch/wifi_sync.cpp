#include "wifi_sync.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
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

// Convert a struct tm interpreted as UTC into Unix epoch (seconds since
// 1970-01-01 UTC). We can't use mktime() — it interprets tm as local
// time, which would apply the PT offset we don't want. timegm() is a
// GNU extension and isn't reliably available.
static time_t tm_utc_to_epoch(const struct tm &t) {
  static const int days_in_month[] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
  int year = t.tm_year + 1900;
  long days = 0;
  for (int y = 1970; y < year; y++) {
    days += 365;
    if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) days++;
  }
  for (int m = 0; m < t.tm_mon; m++) {
    days += days_in_month[m];
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (m == 1 && leap) days++;
  }
  days += t.tm_mday - 1;
  return (time_t)days * 86400L + t.tm_hour * 3600L + t.tm_min * 60L + t.tm_sec;
}

// Fetch the current UTC time from the Date: header of an HTTPS response.
// Returns true on success and writes the Unix epoch to *out_epoch.
// Uses HEAD so no body is transferred. Skips cert verification — we trust
// the timestamp at face value (worst case it's bogus and the sanity
// check rejects it).
static bool fetch_https_time(time_t *out_epoch) {
  WiFiClientSecure tls;
  tls.setInsecure();
  tls.setTimeout(5);  // seconds

  HTTPClient http;
  if (!http.begin(tls, "https://www.google.com/")) {
    Serial.println("WiFi: HTTPS begin failed");
    return false;
  }
  http.setTimeout(5000);
  const char *want_header[] = {"Date"};
  http.collectHeaders(want_header, 1);

  int code = http.sendRequest("HEAD");
  if (code <= 0) {
    Serial.printf("WiFi: HTTPS HEAD failed (%d / %s)\n",
                  code, http.errorToString(code).c_str());
    http.end();
    return false;
  }

  String date_str = http.header("Date");
  http.end();
  if (date_str.length() == 0) {
    Serial.println("WiFi: no Date header in HTTPS response");
    return false;
  }

  // Parse RFC 1123 date — "Mon, 25 May 2026 19:43:00 GMT"
  struct tm tm = {};
  if (!strptime(date_str.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm)) {
    Serial.printf("WiFi: could not parse Date '%s'\n", date_str.c_str());
    return false;
  }
  *out_epoch = tm_utc_to_epoch(tm);
  return true;
}

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

  // Set TZ now so localtime() works once the system clock is set.
  setenv("TZ", tz, 1);
  tzset();

  // Fetch authoritative UTC from Google's HTTPS Date header. We
  // historically tried SNTP (configTzTime) here but at least one
  // network returns NTP responses ~11.6h before reality — the cause
  // was never fully diagnosed. HTTPS over TCP/443 is more robust and
  // works through any NAT.
  time_t utc_epoch = 0;
  if (fetch_https_time(&utc_epoch)) {
    // Sanity check: time should be at or after build_epoch (time only
    // moves forward, and the firmware was just built).
    if (build_epoch > 0 && utc_epoch < build_epoch - 60) {
      Serial.printf("WiFi: HTTPS time %lld is before build_epoch %lld "
                    "— implausible, rejecting\n",
                    (long long)utc_epoch, (long long)build_epoch);
    } else {
      struct timeval tv = { .tv_sec = utc_epoch, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
      synced = true;
      struct tm tm;
      time_t now = time(nullptr);
      localtime_r(&now, &tm);
      char buf[32];
      strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S %p", &tm);
      Serial.printf("WiFi: time synced via HTTPS to %s PT (epoch=%lld)\n",
                    buf, (long long)utc_epoch);
    }
  } else {
    Serial.println("WiFi: HTTPS time fetch failed");
  }

  // Disconnect and power off the radio — saves ~70 mA when not in use.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi: disconnected");
}
