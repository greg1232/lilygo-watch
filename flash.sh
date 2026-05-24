#!/usr/bin/env bash
# Compile + upload the HelloWatch sketch, baking in current epoch + a buffer
# that approximates compile + upload + boot time on this machine.
set -euo pipefail

SKETCH="sketches/HelloWatch"
FQBN="esp32:esp32:twatchs3"
PORT="${PORT:-/dev/cu.usbmodem11201}"
# Empirically warm-cache compile+upload+boot ~35s on this Mac
BUFFER_SEC="${BUFFER_SEC:-35}"

EPOCH=$(( $(date +%s) + BUFFER_SEC ))
echo "Baking epoch $EPOCH ($(date -r "$EPOCH"))"

arduino-cli compile --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=-DBUILD_EPOCH=${EPOCH}L" \
  "$SKETCH"

arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo "Done at $(date). Watch should display ~${BUFFER_SEC}s in the future briefly,"
echo "then catch up. Tune BUFFER_SEC env var if it's consistently off."
