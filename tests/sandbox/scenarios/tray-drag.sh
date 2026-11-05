#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Tray drag and drop: Discord is dragged between Steam and ZapFast, then Steam is dropped on the arrow that hides icons.
# Captures shots/tray-dnd-*.png; the icon positions and the saved item lists go to logs/tray-steps.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
ICONS="$SB/tests/helpers/tray_icons.py"
# The tray's own [General] group; its panel and applet ids change whenever the layout is rebuilt.
APPLETSRC="$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc"
read -r PANEL TRAY <<< "$(python3 -c '
import re, sys
current = None
for line in open(sys.argv[1]).read().splitlines():
    header = re.fullmatch(r"\[Containments\]\[(\d+)\]\[Applets\]\[(\d+)\]", line.strip())
    if header:
        current = header.groups()
    elif line.startswith("["):
        current = None
    elif current and line.strip() == "plugin=org.kde.plasma.systemtray":
        print(*current)
        break
' "$APPLETSRC")"
CFG=(--file "$APPLETSRC" --group Containments --group "$PANEL" --group Applets --group "$TRAY" --group General)
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
waitfor 8
shot tray-dnd-0
read -r -a X <<< "$(python3 "$ICONS" "$SBX/shots/tray-dnd-0.png")"
echo "icons: ${X[*]}" > "$SBX/logs/tray-steps.txt"
# 1. Discord (second) dragged into the gap between Steam (seventh) and ZapFast (eighth), with a shot while held.
TARGET=$(( (X[6] + X[7]) / 2 ))
"$FI" move "${X[1]}" 16 wait 300 down wait 150 glide $((X[1] + 20)) 16 6 glide $TARGET 16 25 wait 2600 up wait 400 > "$SBX/logs/fi1.log" 2>&1 &
FIPID=$!
waitfor 2
shot tray-dnd-1-dragging
wait $FIPID
waitfor 2
shot tray-dnd-2-reordered
echo "order: $(kreadconfig6 "${CFG[@]}" --key itemOrder)" >> "$SBX/logs/tray-steps.txt"
# 2. Steam, now sixth, dropped on the arrow. The arrow is too faint for tray_icons.py; it sits halfway between
#    the last tray icon and Quick Menu, which come before Control Center at the end of the list.
read -r -a X <<< "$(python3 "$ICONS" "$SBX/shots/tray-dnd-2-reordered.png")"
N=${#X[@]}
ARROW=$(( (X[N - 3] + X[N - 2]) / 2 ))
echo "hidden before: $(kreadconfig6 "${CFG[@]}" --key hiddenItems)" >> "$SBX/logs/tray-steps.txt"
echo "drag ${X[5]} -> arrow $ARROW" >> "$SBX/logs/tray-steps.txt"
"$FI" move "${X[5]}" 16 wait 300 down wait 150 glide $((X[5] + 20)) 16 6 glide $ARROW 16 25 wait 1500 up wait 400 > "$SBX/logs/fi2.log" 2>&1 &
FIPID=$!
waitfor 1.8
shot tray-dnd-3-over-arrow
wait $FIPID
waitfor 2
shot tray-dnd-4-hidden
echo "hidden: $(kreadconfig6 "${CFG[@]}" --key hiddenItems)" >> "$SBX/logs/tray-steps.txt"
echo "shown: $(kreadconfig6 "${CFG[@]}" --key shownItems)" >> "$SBX/logs/tray-steps.txt"
