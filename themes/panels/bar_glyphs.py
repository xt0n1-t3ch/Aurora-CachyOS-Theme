# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Prints the runs of ink in the menu bar of a screenshot, so spacing can be read off a capture.

    bar_glyphs.py SHOT [LEFT RIGHT]

Between the columns LEFT (default 1500) and RIGHT (default: the image width), each run is printed as start-end, its
width and the gap before it.
"""
import sys

from PIL import Image

image = Image.open(sys.argv[1]).convert("RGB")
left = int(sys.argv[2]) if len(sys.argv) > 2 else 1500
right = int(sys.argv[3]) if len(sys.argv) > 3 else image.width
# A column has ink when at least two of its pixels in rows 5 to 26 are bright (a channel above 150) or saturated
# (a spread between channels above 90).
ink = []
for x in range(left, right):
    count = 0
    for y in range(5, 27):
        r, g, b = image.getpixel((x, y))
        if max(r, g, b) > 150 or max(r, g, b) - min(r, g, b) > 90:
            count += 1
    ink.append(count >= 2)
# A run ends after more than five empty columns; the padding lets the last run end.
runs, start, gap, end = [], None, 0, 0
for i, on in enumerate(ink + [False] * 8):
    if on:
        if start is None:
            start = i
        gap, end = 0, i
    elif start is not None:
        gap += 1
        if gap > 5:
            runs.append((left + start, left + end))
            start = None
previous = None
for a, b in runs:
    print(f"{a}-{b} w{b - a + 1}" + (f" gap {a - previous - 1}" if previous is not None else ""))
    previous = b
