#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The window buttons on an X11 Konsole beside System Settings: normal, then the pointer on the close and maximize
# buttons (shots/deco-*.png). The Konsole's client geometry goes to logs/deco-geom.txt.
source "$SBX/prelude.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
QT_QPA_PLATFORM=xcb konsole --hold -e fastfetch > "$SBX/logs/konsole.log" 2>&1 &
waitfor 5
KWID=$(xdotool search --sync --classname konsole | head -1)
xdotool windowsize "$KWID" 980 560; xdotool windowmove "$KWID" 1380 150
systemsettings > "$SBX/logs/systemsettings.log" 2>&1 &
waitfor 7
xdotool windowactivate "$KWID"
waitfor 2
shot deco-normal
eval "$(xdotool getwindowgeometry --shell "$KWID")"
# The close button centre: 8 px margin, 3 px capsule padding, half a 38 px segment; the bar is 46 px tall.
xdotool mousemove $((X + WIDTH - 8 - 3 - 19)) $((Y - 23))
waitfor 1
spectacle -b -n -f -p -o "$SBX/shots/deco-hover-close.png" > /dev/null 2>&1
# The maximize button is one segment (38 px) to the left.
xdotool mousemove $((X + WIDTH - 8 - 3 - 38 - 19)) $((Y - 23))
waitfor 1
spectacle -b -n -f -p -o "$SBX/shots/deco-hover-max.png" > /dev/null 2>&1
echo "konsole client geometry: $X $Y $WIDTH $HEIGHT" > "$SBX/logs/deco-geom.txt"
