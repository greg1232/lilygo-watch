# Documentation

Notes on how this codebase is structured and how to extend it.

- [hardware.md](hardware.md) — the T-Watch S3 board: pin map, peripherals, PMU rails, hardware quirks discovered along the way.
- [architecture.md](architecture.md) — source-file layout, what each module owns, init order, app dispatch, sleep/wake flow.
- [build.md](build.md) — installing `arduino-cli` + cores + libraries, what `flash.sh` does, the `BUILD_EPOCH` time-bake trick, serial monitor recipes.
- [apps.md](apps.md) — each app's UX, screen layout, state machine, and the gestures that reach it.
- [modules.md](modules.md) — per-module reference: `hardware`, `gestures`, `power`, `battery`, `audio`, `apps`.
- [adding-an-app.md](adding-an-app.md) — recipe for adding a new app from scratch.
- [wifi.md](wifi.md) — WiFi + NTP time sync setup, secrets file, power impact.

## Design

Forward-looking design docs for features not yet built:

- [design/audio-over-wifi.md](design/audio-over-wifi.md) — how the watch could play audio files: download, streaming, hybrid, format tradeoffs, recommendation.

The repo root [README.md](../README.md) has the quick-start. These docs go deeper.
