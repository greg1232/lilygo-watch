#pragma once
#include <stdint.h>

// Connect to WiFi (credentials from wifi_secrets.h), trigger an NTP
// sync, then disconnect and power down the radio. Blocking, bounded
// by timeout_ms. Safe to call even without wifi_secrets.h — falls back
// to a no-op so the rest of the firmware still builds + runs.
void wifi_sync_time(uint32_t timeout_ms);

// Did the most-recent sync attempt actually update the system clock?
bool wifi_time_synced();
