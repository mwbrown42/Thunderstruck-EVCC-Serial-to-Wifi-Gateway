#!/usr/bin/env python3
"""
Compresses data/index.html using gzip and generates include/WebAssets.h
for zero-heap-allocation PROGMEM web serving on ESP32-S3.
"""
import os
import gzip

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX_HTML = os.path.join(PROJECT_ROOT, "data", "index.html")
WEB_ASSETS_H = os.path.join(PROJECT_ROOT, "include", "WebAssets.h")

def main():
    print(f"Reading {INDEX_HTML}...")
    with open(INDEX_HTML, "rb") as f:
        raw_data = f.read()

    print(f"Compressing ({len(raw_data)} bytes)...")
    gz_data = gzip.compress(raw_data, compresslevel=9)
    print(f"Compressed size: {len(gz_data)} bytes ({len(gz_data)/len(raw_data)*100:.1f}%)")

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <Arduino.h>")
    lines.append("")
    lines.append("// Gzipped web dashboard asset for instant, zero-heap-allocation streaming")
    lines.append(f"static const uint32_t INDEX_HTML_GZ_LEN = {len(gz_data)};")
    lines.append("static const uint8_t INDEX_HTML_GZ[] PROGMEM = {")

    row = []
    for i, b in enumerate(gz_data):
        row.append(f"0x{b:02x}")
        if len(row) == 16 or i == len(gz_data) - 1:
            lines.append("    " + ", ".join(row) + ("," if i < len(gz_data) - 1 else ""))
            row = []

    lines.append("};")
    lines.append("")

    with open(WEB_ASSETS_H, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print(f"Successfully generated {WEB_ASSETS_H}!")

if __name__ == "__main__":
    main()
