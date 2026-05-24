# Books

A children's book about the watch the Jasbros built.

## Contents

- **[how-the-jasbros-built-a-watch.md](how-the-jasbros-built-a-watch.md)** — 10-page picture book for a 6-year-old. Tells the story of building the watch piece by piece, with each page matching one feature in the code.
- **[pictures/](pictures/)** — one SVG illustration per page, embedded in the book.
- **[print.css](print.css)** — page-break-aware stylesheet used when generating the printable PDF.
- **[make_pdf.sh](make_pdf.sh)** — converts the markdown + SVGs into a single printable PDF.

## Reading on screen

GitHub renders the markdown and SVGs inline. Just open `how-the-jasbros-built-a-watch.md`.

## Printing

Generate a PDF with:

```sh
./make_pdf.sh
```

This produces `how-the-jasbros-built-a-watch.pdf` in this directory (about 350 KB, 10 pages, US Letter, with each story page on its own sheet).

### Requirements

- **pandoc**: `brew install pandoc`
- **Google Chrome** (or Chromium): used in headless mode to render SVGs and print to PDF. Most Macs already have it.

### Tuning

- Want A4 instead of Letter? Edit `print.css` and change `size: letter;` to `size: A4;`.
- Want margins different? Adjust `margin: 0.6in;` in the `@page` rule.
- The body font is Comic Sans MS — friendly for kids. Swap if you'd rather a different feel.

### Why this pipeline

Markdown → HTML (pandoc) → PDF (Chrome headless) keeps the SVGs as vectors all the way through. Other markdown-to-PDF tools rasterize the SVGs and the lines come out fuzzy. Chrome's print engine renders SVG natively at full resolution.
