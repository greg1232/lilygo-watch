#!/usr/bin/env bash
# Convert the children's book (Markdown + SVG pictures) into a printable PDF.
#
# Pipeline:
#   1. pandoc renders the Markdown to a self-contained HTML file with the
#      page-break-aware print.css applied.
#   2. Chrome headless prints that HTML to a single PDF, rendering the SVGs
#      natively (no rasterization losses).
#
# Requirements:
#   - pandoc          brew install pandoc
#   - Google Chrome   any recent version
#
# Output:
#   how-the-jasbros-built-a-watch.pdf  (in this directory)
#
# Tuning:
#   To print a different book file, pass it as the first argument:
#     ./make_pdf.sh some-other-book.md

set -euo pipefail

cd "$(dirname "$0")"

INPUT="${1:-how-the-jasbros-built-a-watch.md}"
BASE="${INPUT%.md}"
HTML="${BASE}.html"
PDF="${BASE}.pdf"

# Pre-flight checks
command -v pandoc >/dev/null 2>&1 || {
  echo "Error: pandoc not found. Install with: brew install pandoc" >&2
  exit 1
}

CHROME=""
for candidate in \
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  "/Applications/Chromium.app/Contents/MacOS/Chromium" \
  "$(command -v google-chrome 2>/dev/null || true)" \
  "$(command -v chromium 2>/dev/null || true)"; do
  if [ -n "$candidate" ] && [ -x "$candidate" ]; then
    CHROME="$candidate"
    break
  fi
done
[ -n "$CHROME" ] || {
  echo "Error: Could not find Google Chrome or Chromium on this machine." >&2
  echo "Install Chrome from https://www.google.com/chrome/ and re-run." >&2
  exit 1
}

if [ ! -f "$INPUT" ]; then
  echo "Error: $INPUT not found in $(pwd)" >&2
  exit 1
fi

echo "Rendering $INPUT → $HTML ..."
pandoc "$INPUT" \
  --standalone \
  --metadata "title=How the Jasbros Built a Watch" \
  --css=print.css \
  --to=html5 \
  --embed-resources \
  --output="$HTML"

echo "Printing $HTML → $PDF via Chrome headless ..."
"$CHROME" \
  --headless \
  --disable-gpu \
  --no-pdf-header-footer \
  --print-to-pdf="$PDF" \
  --print-to-pdf-no-header \
  --virtual-time-budget=5000 \
  "file://$(pwd)/$HTML" 2>/dev/null

# Clean up the intermediate HTML (comment out to keep it for debugging)
rm -f "$HTML"

if [ -f "$PDF" ]; then
  SIZE=$(du -h "$PDF" | cut -f1)
  echo "Done. Wrote $PDF ($SIZE)."
  echo "Open with: open $PDF"
else
  echo "Error: $PDF was not produced. Check Chrome output above." >&2
  exit 1
fi
