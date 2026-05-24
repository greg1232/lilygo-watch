# WiFi + NTP time sync

The watch connects to WiFi once at boot, asks `pool.ntp.org` for the current time, and disconnects to save power. Net effect: the clock starts at the correct time even after long power-off, with no `BUILD_EPOCH` recompile dance.

## Setting up credentials

1. Copy the template:
   ```sh
   cp sketches/HelloWatch/wifi_secrets.h.example sketches/HelloWatch/wifi_secrets.h
   ```
2. Edit `wifi_secrets.h` and set your SSID + password.
3. Flash as usual: `./flash.sh`.

`wifi_secrets.h` is gitignored — it stays on your machine only.

## What happens at boot

1. `wifi_sync_time(8000)` is called from `setup()`.
2. WiFi connects in station mode. Timeout: 8 seconds.
3. On success, NTP is triggered against `pool.ntp.org` (fallback: `time.nist.gov`).
4. We wait up to 5 seconds for the system clock to be set.
5. WiFi disconnects and the radio is powered off (`WiFi.mode(WIFI_OFF)`) — saves ~70 mA.
6. If WiFi failed or NTP timed out, `BUILD_EPOCH` (baked at compile time by `flash.sh`) is used as the fallback so the clock is at least close.

Total boot delay: up to ~13 seconds in the worst case (8s WiFi + 5s NTP), typically 2–4 seconds when the network is healthy.

## Build without WiFi

If `wifi_secrets.h` doesn't exist, the build still succeeds (`wifi_sync.cpp` uses `__has_include` to detect it). `wifi_sync_time()` prints `WiFi: no credentials` and returns immediately, and the clock falls back to `BUILD_EPOCH`. This keeps fresh clones buildable for anyone who doesn't want to commit credentials.

## Verifying it works

Serial output on a successful sync:
```
WiFi: connecting to 'solus'....
WiFi: connected, IP 192.168.1.42, RSSI -52
WiFi: NTP synced to 2026-05-24 11:08:35 PT
WiFi: disconnected
```

On failure:
```
WiFi: connecting to 'solus'.........................
WiFi: connect failed (status=6)
Time: using BUILD_EPOCH fallback
```

`status=6` means `WL_DISCONNECTED` — usually a typo'd SSID, wrong password, or network out of range.

## Power impact

| Phase | Current draw |
|---|---|
| WiFi connecting | ~150 mA |
| WiFi idle / connected | ~70 mA |
| WiFi disconnected + radio off | ~0 mA (back to baseline) |
| Light sleep (screen off, on battery) | ~1 mA |

Connect-once-then-off keeps the battery hit minimal.

## Future: re-sync on a schedule

Right now NTP only runs at boot. The ESP32's RTC drifts ~10 ppm = ~1 second per day. For multi-day battery operation, you'd want to re-sync once an hour or so. A small `power_set_screen` hook could trigger `wifi_sync_time()` on every wake from sleep.

## Security note

Credentials are stored in plain text in `wifi_secrets.h` and compiled into flash. If the watch is lost, anyone with `esptool.py` could dump flash and recover them. For higher security, store via the ESP32's NVS partition with encryption (`nvs_flash_init_partition_security`). Overkill for a home project; mentioned for completeness.
