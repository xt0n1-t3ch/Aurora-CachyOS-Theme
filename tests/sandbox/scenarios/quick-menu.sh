#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu (components/quick-menu) the way a user meets it: Win opens the Applications view, the arrows walk
# the grid, "Show More" unfolds a section, typing lists results, Escape closes, and the glass stays readable over a
# white window. Captures shots/quick-menu-*.png; the sandbox home carries a copy of a real desktop's activity
# statistics, so Suggestions shows its most used apps.
source "$SBX/prelude.sh"
# Plasma's menu is plasma-applications.menu; the session sets this prefix, the sandbox's clean environment does not.
export XDG_MENU_PREFIX=plasma-
kbuildsycoca6 --noincremental > "$SBX/logs/kbuildsycoca.log" 2>&1
FI="$SB/tests/helpers/fakeinput/fakeinput"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
python3 "$SB/components/windows-key/windows_key.py" quick-menu > "$SBX/logs/quick-menu.txt" 2>&1
QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon > "$SBX/logs/quick-menu-krunner.log" 2>&1 &
waitfor 4

# key ARGS...: runs fakeinput with ARGS and discards its output.
key() { "$FI" "$@" > /dev/null 2>&1; }
# tap CODE: presses and releases the evdev key CODE.
tap() { key keydown "$1" wait 40 keyup "$1" wait 120; }
# evdev codes: left Windows key, Down, Right, Escape.
WIN=125; DOWN=108; RIGHT=106; ESC=1

tap $WIN; waitfor 2
shot quick-menu-1-apps

tap $DOWN; tap $RIGHT; tap $RIGHT; waitfor 1
shot quick-menu-2-keys

# "Show More" of the first section that has one: a row of its exact text colour (120, 150, 255, the
# lightened accent), inside the centred panel; an app icon never holds that many pixels of it in one row.
read -r MX MY <<< "$(python3 -I - "$SBX/shots/quick-menu-1-apps.png" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
w, h = im.size
for y in range(h // 4, h * 15 // 16):
    xs = [x for x in range(w // 2, w // 2 + 320)
          if all(abs(c - t) <= 8 for c, t in zip(im.getpixel((x, y)), (120, 150, 255)))]
    if len(xs) >= 8:
        print((xs[0] + xs[-1]) // 2, y)
        sys.exit()
print("0 0")
PY
)"
echo "show more at $MX $MY" >> "$SBX/logs/quick-menu.txt"
key move "$MX" "$MY" wait 300 down wait 60 up wait 400
waitfor 1
shot quick-menu-3-show-more

tap $ESC; waitfor 1
tap $WIN; waitfor 1
for letter in 33 23 19 18; do tap $letter; done # f i r e
waitfor 4
shot quick-menu-4-results

tap $ESC; waitfor 1
shot quick-menu-5-closed

# Over a white window: the glass must keep its white text readable.
python3 -I -c "from PIL import Image; Image.new('RGB', (2560, 1440), 'white').save('$SBX/run/white.png')"
gwenview -f "$SBX/run/white.png" > "$SBX/logs/gwenview.log" 2>&1 &
waitfor 5
tap $WIN; waitfor 2
shot quick-menu-6-over-white
