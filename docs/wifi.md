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

1. `wifi_sync_time(TZ_PACIFIC, BUILD_EPOCH, 8000)` is called from `setup()`.
2. WiFi connects in station mode. Timeout: 8 seconds.
3. On success, `configTzTime()` is called — this atomically sets the system TZ and starts SNTP polling against Google's anycast IP, `time.google.com`, and `pool.ntp.org` in order.
4. We wait up to 5 seconds for the system clock to be set.
5. **Sanity check**: if the NTP-set time is more than 60 seconds *before* `BUILD_EPOCH`, the response is rejected as implausible (time only moves forward; the firmware was built at `BUILD_EPOCH`). The fallback below kicks in.
6. WiFi disconnects and the radio is powered off (`WiFi.mode(WIFI_OFF)`) — saves ~70 mA.
7. If WiFi failed, NTP timed out, or sanity check rejected the time, `BUILD_EPOCH` (baked at compile time by `flash.sh`) is used so the clock is within a minute of correct.

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

On WiFi failure:
```
WiFi: connecting to 'solus'.........................
WiFi: connect failed (status=6)
Time: using BUILD_EPOCH fallback
```

`status=6` means `WL_DISCONNECTED` — usually a typo'd SSID, wrong password, or network out of range.

On implausible NTP response (seen with at least one home network — root cause unknown but consistently reproducible):
```
WiFi: NTP returned 1779696230 but build was at 1779737945 (41715 sec earlier) — implausible, rejecting
WiFi: disconnected
Time: using BUILD_EPOCH fallback
```

This means SNTP got a response from the server, but the timestamp was earlier than the moment the firmware was compiled — physically impossible. Could be:
- A NAT/firewall mangling UDP/123 packets in transit
- An SNTP module quirk on this ESP32 core version
- The server itself returning bogus time (very unusual for major NTP services)

Curiously, the same network's Mac running `sntp pool.ntp.org` returns the correct time, so it's specific to the ESP32 SNTP path. The sanity check ensures the clock is still useful in this case.

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
