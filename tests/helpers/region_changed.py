#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""region_changed.py BEFORE AFTER X0 Y0 X1 Y1: exits 0 when the region of AFTER differs from BEFORE.

Scenarios use it to tell whether a click opened something: a popup that appears under the menu bar
changes the pixels there, the static sandbox wallpaper does not. Prints the mean channel difference.
"""
import sys

from PIL import Image, ImageChops, ImageStat

# A mean channel difference (0 to 255) above this counts as changed.
THRESHOLD = 6.0


def main() -> int:
    """Print the mean channel difference of the region and return 0 when it is above THRESHOLD, else 1."""
    before, after = (Image.open(path).convert("RGB") for path in sys.argv[1:3])
    box = tuple(int(value) for value in sys.argv[3:7])
    diff = ImageChops.difference(before.crop(box), after.crop(box))
    mean = sum(ImageStat.Stat(diff).mean) / 3
    print(f"{mean:.1f}")
    return 0 if mean > THRESHOLD else 1


if __name__ == "__main__":
    sys.exit(main())
