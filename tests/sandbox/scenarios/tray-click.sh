#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The tray's icons take clicks and still drag: a click on the arrow, on a plasmoid icon and on an app's icon
# each does its job, and a drag still reorders the bar. Writes PASS or FAIL per check to logs/tray-click.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
ICONS="$SB/tests/helpers/tray_icons.py"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/tray-click.txt"
APPSRC="$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
waitfor 8
shot tray-click-0
read -r -a X <<< "$(python3 "$ICONS" "$SBX/shots/tray-click-0.png")"
N=${#X[@]}
echo "icons: ${X[*]}" > "$OUT"

# check COMMAND...: appends PASS or FAIL for the current NAME to the log, by whether COMMAND succeeds.
check() {
    if "$@" > /dev/null; then echo "PASS $NAME" >> "$OUT"; else echo "FAIL $NAME" >> "$OUT"; fi
}
# click X: clicks the menu bar at X.
click() {
    "$FI" move "$1" 16 wait 400 down wait 60 up wait 300 > /dev/null 2>&1
}
# close_popup: clicks in the middle of the screen, away from the bar, to close an open popup.
close_popup() {
    "$FI" move 1280 900 wait 200 down wait 60 up wait 300 > /dev/null 2>&1
    waitfor 1
}

# The arrow sits halfway between the last tray icon and the control centre.
NAME="a click on the arrow opens the hidden icons"
click $(( (X[N - 3] + X[N - 2]) / 2 ))
waitfor 1.5
shot tray-click-1-arrow
check python3 "$CHANGED" "$SBX/shots/tray-click-0.png" "$SBX/shots/tray-click-1-arrow.png" 1500 40 2560 500
close_popup

NAME="a click on a plasmoid icon opens its popup"
click "${X[N - 3]}"
waitfor 1.5
shot tray-click-2-plasmoid
check python3 "$CHANGED" "$SBX/shots/tray-click-0.png" "$SBX/shots/tray-click-2-plasmoid.png" 1500 40 2560 500
close_popup

NAME="a click on an app's icon reaches the app"
click "${X[1]}"
waitfor 1.5
check grep -q '^activated' "$SB"/logs/fake-tray-*.log
close_popup

NAME="a drag still reorders the bar"
before=$(grep -h '^itemOrder=' "$APPSRC")
"$FI" move "${X[1]}" 16 wait 300 down wait 150 glide $((X[1] + 20)) 16 6 glide $(( (X[6] + X[7]) / 2 )) 16 25 wait 1200 up wait 600 > /dev/null 2>&1
waitfor 2
shot tray-click-3-dragged
after=$(grep -h '^itemOrder=' "$APPSRC")
echo "order before: $before" >> "$OUT"
echo "order after: $after" >> "$OUT"
check test "$before" != "$after"
