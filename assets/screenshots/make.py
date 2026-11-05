# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Makes the README's pictures from the sandbox captures of tests/sandbox/scenarios/readme.sh.

Run the scenario with SANDBOX_SCALE=2 (5120x2880 captures of the 2560x1440 screen), then this script:

    SANDBOX_SCALE=2 tests/sandbox/run.sh "$PWD/tests/sandbox/scenarios/readme.sh" 160
    python3 -I assets/screenshots/make.py
"""
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SHOTS = os.path.join(ROOT, "tests/sandbox/shots")
OUT = os.path.dirname(os.path.abspath(__file__))
# Each picture: the capture it comes from and the part of the screen it shows, in screen points (2560x1440).
CROPS = {
    # Quick Menu with the wallpaper's crystals to its right.
    "quick-menu": ("readme-quick-menu", (768, 144, 1792, 1368)),
    "control-center": ("readme-control-center", (2200, 0, 2560, 470)),
    "kali-menu": ("readme-menu", (0, 0, 420, 300)),
    "notifications": ("readme-notifications", (2166, 1168, 2560, 1376)),
}


def save(image, name):
    """Writes `image` next to this script as NAME.jpg, a progressive JPEG at quality 90."""
    image.convert("RGB").save(os.path.join(OUT, f"{name}.jpg"), quality=90, optimize=True, progressive=True)


# The capture size over the screen size: 2 with SANDBOX_SCALE=2. The desktop picture is the whole capture scaled to the
# screen size; the others are crops.
desktop = Image.open(os.path.join(SHOTS, "readme-desktop.png"))
scale = desktop.width // 2560
save(desktop.resize((2560, 1440), Image.LANCZOS), "desktop")
for name, (capture, box) in CROPS.items():
    image = Image.open(os.path.join(SHOTS, f"{capture}.png"))
    save(image.crop(tuple(v * scale for v in box)), name)
    print(name, image.crop(tuple(v * scale for v in box)).size)
