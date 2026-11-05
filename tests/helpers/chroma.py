# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""chroma.py SHOT X0 Y0 X1 Y1: the largest colour fringe (max - min channel) in a region, and how many pixels
have a strong one. Grayscale antialiased text has none; subpixel (ClearType-like) text has many."""
import sys

from PIL import Image

x0, y0, x1, y1 = (int(v) for v in sys.argv[2:6])
image = Image.open(sys.argv[1]).convert("RGB").crop((x0, y0, x1, y1))
chroma = [max(p) - min(p) for p in image.getdata()]
print(f"max {max(chroma)} strong {sum(c > 40 for c in chroma)}")
