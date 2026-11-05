#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Steam is hidden (from tray-drag.sh): open the popup, find Steam in it, drag it back into the bar.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
ICONS="$SB/tests/helpers/tray_icons.py"
# The panel and tray applet ids of the sandbox's saved layout (tray-drag.sh reads them at run time).
CFG=(--file "$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc" --group Containments --group 82 --group Applets --group 88 --group General)
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
waitfor 8
shot tray-show-0
read -r -a X <<< "$(python3 "$ICONS" "$SBX/shots/tray-show-0.png")"
N=${#X[@]}
# The arrow sits 26 px left of Quick Menu's icon.
ARROW=$(( X[N - 2] - 26 ))
echo "icons: ${X[*]} arrow $ARROW" > "$SBX/logs/tray-show.txt"
"$FI" move $ARROW 16 wait 200 down wait 80 up wait 300 > "$SBX/logs/fi3.log" 2>&1
waitfor 1.5
shot tray-show-1-popup
# Steam's picture from the first capture of tray-drag.sh, where it was the seventh icon.
read -r SX SY SCORE <<< "$(python3 "$SB/tests/helpers/find_icon.py" "$SBX/shots/tray-show-1-popup.png" "$SBX/shots/tray-dnd-0.png" 1993 16 22)"
echo "steam in popup: $SX $SY score $SCORE" >> "$SBX/logs/tray-show.txt"
TARGET=$(( (X[0] + X[1]) / 2 ))
"$FI" move "$SX" "$SY" wait 300 down wait 150 glide $((SX + 20)) $((SY - 10)) 6 glide $TARGET 16 30 wait 1500 up wait 400 >> "$SBX/logs/fi3.log" 2>&1 &
FIPID=$!
waitfor 2.2
shot tray-show-2-dragging
wait $FIPID
waitfor 2
shot tray-show-3-shown
{
    echo "order: $(kreadconfig6 "${CFG[@]}" --key itemOrder)"
    echo "hidden: $(kreadconfig6 "${CFG[@]}" --key hiddenItems)"
    echo "shown: $(kreadconfig6 "${CFG[@]}" --key shownItems)"
} >> "$SBX/logs/tray-show.txt"
