#pragma once
#include <stdint.h>
#include <time.h>

// Connect to WiFi (credentials from wifi_secrets.h), set the system
// timezone, trigger an NTP sync, then disconnect and power down the
// radio. Blocking, bounded by timeout_ms. Safe to call even without
// wifi_secrets.h — falls back to a no-op so the rest of the firmware
// still builds + runs.
//
// `tz` is a POSIX TZ string, e.g. "PST8PDT,M3.2.0,M11.1.0".
//
// `build_epoch` is the Unix epoch baked in at compile time. If the
// NTP response returns a time more than 60 seconds *before* this
// epoch, the response is rejected as bogus (some networks hijack
// outbound UDP/123 and return their gateway's wrong clock). Pass 0
// to disable the sanity check.
void wifi_sync_time(const char *tz, time_t build_epoch, uint32_t timeout_ms);

// Did the most-recent sync attempt actually update the system clock?
bool wifi_time_synced();
