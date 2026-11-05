#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Opens the Control Center from the menu bar and captures it: shots/cc-<TAG>-bar.png and shots/cc-<TAG>-open.png.
# TAG comes from SCEN_TAG (default "now"). Control Center is the last icon before the clock
# (tests/helpers/tray_icons.py); CC_X replaces the x that tray_icons.py finds.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
[ -x "$SB/tests/helpers/fake_mpris.py" ] && python3 "$SB/tests/helpers/fake_mpris.py" > "$SBX/logs/fake-mpris.log" 2>&1 &
waitfor 8
shot "cc-$TAG-bar"
read -r -a X <<< "$(python3 "$SB/tests/helpers/tray_icons.py" "$SBX/shots/cc-$TAG-bar.png")"
N=${#X[@]}
CCX="${CC_X:-${X[N - 1]}}"
echo "icons: ${X[*]} cc=$CCX" > "$SBX/logs/cc-$TAG.txt"
"$FI" move "$CCX" 14 wait 400 down wait 60 up wait 300 > /dev/null 2>&1
waitfor 2
shot "cc-$TAG-open"
# With CC_DEBUG or SCEN_TAG set, dumps KWin's window list (GGWIN lines in KWin's own output).
[ -n "$CC_DEBUG$SCEN_TAG" ] && "$SB/tests/helpers/kwin_windows.sh"
waitfor 1
