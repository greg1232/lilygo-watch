# WiFi time sync

The watch connects to WiFi once at boot, fetches the current time from the `Date:` header of an HTTPS HEAD request to `www.google.com`, sets the system clock, and disconnects to save power. Net effect: the clock starts at the correct time (~2 second accuracy) even after long power-off, with no `BUILD_EPOCH` recompile dance.

## Why HTTPS instead of NTP?

The previous version used SNTP against `pool.ntp.org` / `time.google.com`. On at least one home network we developed against, those NTP responses came back consistently ~11.6 hours before reality, even though the same network's macOS `sntp(8)` worked fine. The root cause was never fully diagnosed — possibly NAT mangling UDP/123 packets in a way that confuses lwIP's SNTP path. Switching to HTTPS over TCP/443 sidesteps the issue entirely.

The Date header of any HTTPS response carries the server's UTC time. We just do a `HEAD /` (no body transfer), parse the RFC 1123 date, and `settimeofday()`. Same trick `curl`, browsers, and most system clocks use.

## Setting up credentials

1. Copy the template:
   ```sh
   cp sketches/HelloWatch/wifi_secrets.h.example sketches/HelloWatch/wifi_secrets.h
   ```
2. Edit `wifi_secrets.h` and set your SSID + password.
3. Flash as usual: `./flash.sh`.

`wifi_secrets.h` is gitignored — it stays on your machine only.

## What happens at boot

1. `wifi_sync_time(TZ_PACIFIC, BUILD_EPOCH, 8000)` is called from `setup()`.
2. WiFi connects in station mode. Timeout: 8 seconds.
3. `setenv("TZ", "PST8PDT,...")` + `tzset()` so `localtime()` produces Pacific time once the system clock is set.
4. **HTTPS HEAD to `https://www.google.com/`** with cert verification skipped (we're only reading a header, not authenticating). The `Date:` response header is parsed with `strptime()` and converted to a Unix epoch.
5. **Sanity check**: if the returned time is more than 60 seconds *before* `BUILD_EPOCH`, reject (time only moves forward; the firmware was just built).
6. On success, `settimeofday()` sets the system clock.
7. WiFi disconnects and the radio is powered off (`WiFi.mode(WIFI_OFF)`) — saves ~70 mA.
8. If WiFi failed, HTTPS failed, or sanity check rejected the time, `BUILD_EPOCH` (baked at compile time by `flash.sh`) is used so the clock is within a minute of correct.

Total boot delay: up to ~13 seconds in the worst case (8s WiFi + 5s NTP), typically 2–4 seconds when the network is healthy.

## Build without WiFi

If `wifi_secrets.h` doesn't exist, the build still succeeds (`wifi_sync.cpp` uses `__has_include` to detect it). `wifi_sync_time()` prints `WiFi: no credentials` and returns immediately, and the clock falls back to `BUILD_EPOCH`. This keeps fresh clones buildable for anyone who doesn't want to commit credentials.

## Verifying it works

Serial output on a successful sync:
```
WiFi: connecting to 'solus'....
WiFi: connected, IP 192.168.4.77, RSSI -69
WiFi: time synced via HTTPS to 2026-05-25 12:49:41 PM PT (epoch=1779738581)
WiFi: disconnected
```

On WiFi failure:
```
WiFi: connecting to 'solus'.........................
WiFi: connect failed (status=6)
Time: using BUILD_EPOCH fallback
```

`status=6` means `WL_DISCONNECTED` — usually a typo'd SSID, wrong password, or network out of range.

On HTTPS failure (no internet, captive portal, DNS broken):
```
WiFi: HTTPS HEAD failed (-1 / read Timeout)
WiFi: HTTPS time fetch failed
Time: using BUILD_EPOCH fallback
```

## Power impact

| Phase | Current draw |
|---|---|
| WiFi connecting | ~150 mA |
| WiFi idle / connected | ~70 mA |
| WiFi disconnected + radio off | ~0 mA (back to baseline) |
| Light sleep (screen off, on battery) | ~1 mA |

Connect-once-then-off keeps the battery hit minimal.

## Future: re-sync on a schedule

Right now the HTTPS sync only runs at boot. The ESP32's RTC drifts ~10 ppm = ~1 second per day. For multi-day battery operation, you'd want to re-sync once an hour or so. A small `power_set_screen` hook could trigger `wifi_sync_time()` on every wake from sleep.

## Security note

Credentials are stored in plain text in `wifi_secrets.h` and compiled into flash. If the watch is lost, anyone with `esptool.py` could dump flash and recover them. For higher security, store via the ESP32's NVS partition with encryption (`nvs_flash_init_partition_security`). Overkill for a home project; mentioned for completeness.
