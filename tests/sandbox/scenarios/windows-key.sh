#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The Windows key on its own opens Quick Menu (KRunner) and the next press closes it, as Start does on Windows and
# Command-Space on a Mac; Win+E leaves Quick Menu shut and Alt+Space still opens it. Starts from Plasma's stock binding,
# which has no launcher applet to open in this theme's bar. Writes PASS or FAIL per check to logs/winkey.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/winkey.txt"
# KRunner's search field drops down at the top centre (the mean difference is 26.6 shown and 0.0 hidden).
SPOT=(1000 30 1560 80)
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12

# expect shown|hidden SHOT: compares Quick Menu's place in SHOT with the first capture, taken before any key.
expect() {
    local got=hidden
    python3 "$CHANGED" "$SBX/shots/winkey-0.png" "$SBX/shots/$2.png" "${SPOT[@]}" > /dev/null && got=shown
    if [ "$got" = "$1" ]; then echo "PASS $NAME" >> "$OUT"; else echo "FAIL $NAME (Quick Menu $got)" >> "$OUT"; fi
}
# press FAKEINPUT_ARGS...: sends the input, then waits 3 s for Quick Menu to react.
press() {
    "$FI" "$@" > /dev/null 2>&1
    waitfor 3
}
# A tap of the left Windows key (evdev 125).
WIN=(keydown 125 wait 80 keyup 125)

python3 "$SB/components/windows-key/windows_key.py" stock > "$OUT" 2>&1
shot winkey-0

NAME="stock binding: Win alone opens nothing, the bug Plasma 6.1 brought"
press "${WIN[@]}"
shot winkey-1-stock
expect hidden winkey-1-stock

python3 "$SB/components/windows-key/windows_key.py" quick-menu >> "$OUT" 2>&1

NAME="Win alone opens Quick Menu"
press "${WIN[@]}"
shot winkey-2-open
expect shown winkey-2-open

NAME="Win again closes Quick Menu"
press "${WIN[@]}"
shot winkey-3-closed
expect hidden winkey-3-closed

NAME="Win+E does not open Quick Menu"
press keydown 125 wait 60 keydown 18 wait 60 keyup 18 wait 60 keyup 125
shot winkey-4-win-e
expect hidden winkey-4-win-e

NAME="Alt+Space still opens Quick Menu"
press keydown 56 wait 60 keydown 57 wait 60 keyup 57 wait 60 keyup 56
shot winkey-5-alt-space
expect shown winkey-5-alt-space
