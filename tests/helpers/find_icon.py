# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""find_icon.py SHOT TEMPLATE_SHOT X Y SIZE: where the icon seen at X,Y (SIZE px) in TEMPLATE_SHOT appears in SHOT,
grown to the popup's icon size (32 px), searched below the menu bar; prints its centre x y and the match score
(a correlation: 1.00 is a perfect match)."""
import sys

import numpy as np
from PIL import Image

shot, source, x, y, size = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
half = size // 2
template = Image.open(source).convert("L").crop((x - half, y - half, x + half, y + half)).resize((32, 32), Image.LANCZOS)
t = np.asarray(template, dtype=np.float32)
t = (t - t.mean()) / (t.std() + 1e-6)
region = np.asarray(Image.open(shot).convert("L"), dtype=np.float32)[34:600, 1400:2560]
# Each 32 px window and the template are scaled to zero mean and unit variance; the score is their mean product.
best, where = -1e9, (0, 0)
for top in range(0, region.shape[0] - 32, 2):
    for left in range(0, region.shape[1] - 32, 2):
        w = region[top:top + 32, left:left + 32]
        score = float(((w - w.mean()) / (w.std() + 1e-6) * t).mean())
        if score > best:
            best, where = score, (left, top)
print(1400 + where[0] + 16, 34 + where[1] + 16, f"{best:.2f}")
