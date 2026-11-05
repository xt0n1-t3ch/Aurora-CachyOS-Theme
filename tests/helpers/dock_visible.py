# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""dock_visible.py SHOT: exit 0 when the Dock shows in the bottom strip of a 2560 x 1440 sandbox capture. The
Dock's icons are bright and colourful; the dodged desktop under a maximized dark window is not."""
import sys

from PIL import Image

strip = Image.open(sys.argv[1]).convert("RGB").crop((700, 1370, 1860, 1430))
bright = sum(1 for r, g, b in strip.getdata() if max(r, g, b) > 170 and max(r, g, b) - min(r, g, b) > 60)
sys.exit(0 if bright > 400 else 1)
