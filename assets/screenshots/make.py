# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Makes the README's pictures from the sandbox captures of tests/sandbox/scenarios/readme.sh and readme-explorer.sh.

Run readme.sh with SANDBOX_SCALE=2 (5120x2880 captures of the 2560x1440 screen), readme-explorer.sh at the screen's
own scale, and Mission Center's own sandbox for its two pictures, then this script:

    SANDBOX_SCALE=2 tests/sandbox/run.sh "$PWD/tests/sandbox/scenarios/readme.sh" 240
    tests/sandbox/run.sh "$PWD/tests/sandbox/scenarios/readme-explorer.sh" 300
    PAGES="cpu,performance-page,cpu,, apps,apps-page,cpu,," \
        components/mission-center/tools/sandbox/run.sh "$PWD/components/mission-center/tools/sandbox/capture.sh" 120
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
    # The notifications popover under the bell, with the tray.
    "notification-center": ("readme-notification-center", (1940, 0, 2560, 530)),
    # File Explorer's window sits at 560,270 (1440x890) and keeps its shadow; the Git picture reaches down and right to
    # take in the Git submenu.
    "file-explorer-git": ("readme-fe-git", (520, 230, 2110, 1330)),
    "file-explorer-columns": ("readme-fe-columns", (520, 230, 2040, 1200)),
    "file-explorer-gallery": ("readme-fe-gallery", (520, 230, 2040, 1200)),
    "file-explorer-inspector": ("readme-fe-inspector", (520, 230, 2040, 1200)),
    "file-explorer-palette": ("readme-fe-palette", (520, 230, 2040, 1200)),
    "file-explorer-rename": ("readme-fe-rename", (520, 230, 2040, 1200)),
    # fe-shots' window, 1280x800 in the middle of the screen, under the Commit sheet.
    "file-explorer-commit": ("readme-fe-commit", (600, 280, 1960, 1160)),
}
# Mission Center's captures (components/mission-center/tools/sandbox/capture.sh): the window placed at 100,100 of the
# screen at its own scale of 1, with its shadow and the wallpaper around it; each picture keeps the window and the
# shadow.
MISSION = os.path.join(ROOT, "components/mission-center/tools/sandbox/shots")
MISSION_CROPS = {
    "mission-center": ("-cpu", (10, 10, 1330, 910)),
    "mission-center-apps": ("-apps", (10, 10, 1330, 910)),
}


def save(image, name):
    """Writes `image` next to this script as NAME.jpg, a progressive JPEG at quality 90."""
    image.convert("RGB").save(os.path.join(OUT, f"{name}.jpg"), quality=90, optimize=True, progressive=True)


# The desktop picture is the whole capture scaled to the screen size; the others are crops. Each capture's size over the
# screen size (2 with SANDBOX_SCALE=2, else 1) turns its box into the capture's pixels.
desktop = Image.open(os.path.join(SHOTS, "readme-desktop.png"))
save(desktop.resize((2560, 1440), Image.LANCZOS), "desktop")
for name, (capture, box) in CROPS.items():
    image = Image.open(os.path.join(SHOTS, f"{capture}.png"))
    scale = image.width // 2560
    cropped = image.crop(tuple(v * scale for v in box))
    save(cropped, name)
    print(name, cropped.size)
for name, (capture, box) in MISSION_CROPS.items():
    image = Image.open(os.path.join(MISSION, f"{capture}.png")).crop(box)
    save(image, name)
    print(name, image.size)
