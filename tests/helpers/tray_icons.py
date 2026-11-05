# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""tray_icons.py SHOT: prints the centre x of each icon in the sandbox menu bar's right half of a capture, left to
right. An icon is a group of columns with ink between the middle of the bar and the clock."""
import sys

from PIL import Image

# RIGHT stops before the clock, which starts near x 2421. The bar runs tray, arrow, Quick Menu, Control Center, clock,
# so the last icon found is Control Center.
LEFT, RIGHT, TOP, BOTTOM = 1500, 2405, 5, 27
image = Image.open(sys.argv[1]).convert("RGB")
# A column has ink when at least two of its pixels are bright or colourful.
ink = []
for x in range(LEFT, RIGHT):
    count = 0
    for y in range(TOP, BOTTOM):
        r, g, b = image.getpixel((x, y))
        if max(r, g, b) > 150 or max(r, g, b) - min(r, g, b) > 90:
            count += 1
    ink.append(count >= 2)
icons, start, gap = [], None, 0
for i, on in enumerate(ink + [False] * 8):
    if on:
        if start is None:
            start = i
        gap, end = 0, i
    elif start is not None:
        gap += 1
        if gap > 6:  # an icon's own gaps are narrower than the spacing between icons
            if end - start >= 6:
                icons.append(LEFT + (start + end) // 2)
            start = None
print(" ".join(str(x) for x in icons))
