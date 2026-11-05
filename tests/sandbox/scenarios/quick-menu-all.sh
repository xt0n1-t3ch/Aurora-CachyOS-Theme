#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu's All Applications: Win opens the Applications view, a click on "All Applications" lists every app under
# its first letter, and "Back" returns. Captures shots/quick-menu-all-*.png and logs where it clicked.
source "$SBX/prelude.sh"
export XDG_MENU_PREFIX=plasma-
kbuildsycoca6 --noincremental > "$SBX/logs/kbuildsycoca.log" 2>&1
FI="$SB/tests/helpers/fakeinput/fakeinput"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
python3 "$SB/components/windows-key/windows_key.py" quick-menu > "$SBX/logs/quick-menu-all.txt" 2>&1
QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon > "$SBX/logs/quick-menu-krunner.log" 2>&1 &
waitfor 4

# key ARGS...: runs fakeinput with ARGS and discards its output.
key() { "$FI" "$@" > /dev/null 2>&1; }
# tap CODE: presses and releases the evdev key CODE.
tap() { key keydown "$1" wait 40 keyup "$1" wait 120; }
WIN=125

tap $WIN; waitfor 2
shot quick-menu-all-1-apps

# find_switch SHOT: prints the middle of the switch: the first row of near-white label pixels in the right half of the
# centred panel, in the band just under the search field (the panel spans the middle quarter of the screen).
find_switch() {
    python3 -I - "$1" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
w, h = im.size
best = None
for y in range(h * 33 // 100, h * 40 // 100):
    xs = [x for x in range(w // 2, w * 5 // 8) if min(im.getpixel((x, y))) > 215]
    if len(xs) > 12:
        best = ((xs[0] + xs[-1]) // 2, y)
        break
print(*(best or (0, 0)))
PY
}
read -r AX AY <<< "$(find_switch "$SBX/shots/quick-menu-all-1-apps.png")"
echo "all applications at $AX $AY" >> "$SBX/logs/quick-menu-all.txt"
key move "$AX" "$AY" wait 300 down wait 60 up wait 400
waitfor 2
shot quick-menu-all-2-list
# Back is narrower than All Applications, so it is found again.
read -r AX AY <<< "$(find_switch "$SBX/shots/quick-menu-all-2-list.png")"
echo "back at $AX $AY" >> "$SBX/logs/quick-menu-all.txt"
key move "$AX" "$AY" wait 300 down wait 60 up wait 400
waitfor 2
shot quick-menu-all-3-back
