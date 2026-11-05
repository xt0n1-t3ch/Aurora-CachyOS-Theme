#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The reopen flow: open the Control Center, click outside (it closes), open it again, click the Bluetooth label.
# Captures shots/ccb-<TAG>-{bar,closed,reopened,1}.png.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 14
shot "ccb-$TAG-bar"
read -r -a X <<< "$(python3 "$SB/tests/helpers/tray_icons.py" "$SBX/shots/ccb-$TAG-bar.png")"
# The Control Center is the last icon before the clock.
N=${#X[@]}
CCX="${X[N - 1]}"
# open_cc: clicks the Control Center icon in the menu bar and waits for it to open.
open_cc() { "$FI" move "$CCX" 14 wait 300 down wait 60 up wait 300 > /dev/null 2>&1; waitfor 1.5; }
open_cc
"$FI" move 1500 700 wait 200 down wait 60 up wait 300 > /dev/null 2>&1; waitfor 1
shot "ccb-$TAG-closed"
open_cc
shot "ccb-$TAG-reopened"
# The Bluetooth pill's label, at the same place as in control-center-panels.sh.
"$FI" move 2366 143 wait 400 down wait 80 up wait 300 > /dev/null 2>&1; waitfor 1.5
shot "ccb-$TAG-1"
