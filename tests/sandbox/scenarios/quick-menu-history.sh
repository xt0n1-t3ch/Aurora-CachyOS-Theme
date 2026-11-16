#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu's past searches under the arrow keys: Up in the empty field lists them with the newest current, Down
# walks to the last one, Down on the last one goes back to the field with the first one current, and Up from the
# field stands on the last one. Makes three calculator searches first, so the list has three entries. Writes PASS or
# FAIL per check to logs/quick-menu-history.txt.
source "$SBX/prelude.sh"
export XDG_MENU_PREFIX=plasma-
kbuildsycoca6 --noincremental > "$SBX/logs/kbuildsycoca.log" 2>&1
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/quick-menu-history.txt"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
python3 "$SB/components/windows-key/windows_key.py" quick-menu > "$OUT" 2>&1
QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon > "$SBX/logs/quick-menu-krunner.log" 2>&1 &
waitfor 4

# tap CODE: presses and releases the evdev key CODE.
tap() { "$FI" keydown "$1" wait 40 keyup "$1" wait 150 > /dev/null 2>&1; }
# evdev codes: left Windows key, Down, Up, Escape, Return.
WIN=125; DOWN=108; UP=103; ESC=1; RET=28
# row SHOT: prints the top of the highlighted row in SHOT, in pixels from the top of the screen, or 0 without one.
# The highlight is the only saturated blue on the screen's vertical centre line.
row() {
    python3 -I - "$SBX/shots/$1.png" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
x = im.width // 2
print(next((y for y in range(im.height) if im.getpixel((x, y))[2] > 200 and im.getpixel((x, y))[0] < 90), 0))
PY
}
# check NAME CONDITION...: PASS when the test command CONDITION holds.
check() {
    local name=$1
    shift
    if [ "$@" ]; then echo "PASS $name" >> "$OUT"; else echo "FAIL $name ($*)" >> "$OUT"; fi
}

# 9-4, 8-2 and 7-1: digits and the minus key (evdev 12).
for query in "10 12 5" "9 12 3" "8 12 2"; do
    tap $WIN; waitfor 2
    for code in $query; do tap "$code"; done
    waitfor 4
    tap $RET; waitfor 2
    tap $ESC; waitfor 1
done

tap $WIN; waitfor 2
tap $UP; waitfor 1; shot quick-menu-history-1; first=$(row quick-menu-history-1)
tap $DOWN; waitfor 1; shot quick-menu-history-2; second=$(row quick-menu-history-2)
tap $DOWN; waitfor 1; shot quick-menu-history-3; last=$(row quick-menu-history-3)
tap $DOWN; waitfor 1; shot quick-menu-history-4; wrapped=$(row quick-menu-history-4)
tap $UP; waitfor 1; shot quick-menu-history-5; back=$(row quick-menu-history-5)

check "Up in the empty field lists the past searches" "$first" -gt 0
check "Down moves to the next entry" "$second" -gt "$first"
check "Down again reaches the last entry" "$last" -gt "$second"
check "Down on the last entry returns to the field with the first entry current" "$wrapped" -eq "$first"
check "Up from the field stands on the last entry" "$back" -eq "$last"
