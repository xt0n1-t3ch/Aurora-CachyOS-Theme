#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Opens the Control Center, then each detail panel in turn (Network, Bluetooth, Sound outputs), capturing each:
# shots/ccp-<TAG>-{grid,network,bluetooth,sound}.png. Module positions follow js/cc.js's grid from the right edge.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
python3 "$SB/tests/helpers/fake_mpris.py" > "$SBX/logs/fake-mpris.log" 2>&1 &
waitfor 8
shot "ccp-$TAG-bar"
read -r -a X <<< "$(python3 "$SB/tests/helpers/tray_icons.py" "$SBX/shots/ccp-$TAG-bar.png")"
# The Control Center is the last icon before the clock.
N=${#X[@]}
CCX="${X[N - 1]}"
# open_cc: clicks the Control Center icon in the menu bar and waits for it to open.
open_cc() { "$FI" move "$CCX" 14 wait 300 down wait 60 up wait 300 > /dev/null 2>&1; waitfor 1.5; }
# click X Y: clicks at X,Y and waits for the result.
click() { "$FI" move "$1" "$2" wait 300 down wait 60 up wait 300 > /dev/null 2>&1; waitfor 1.2; }
# Modules: the grid's left edge is 2560 - 10 - 284 = 2266, its top 32 + 6 = 38.
L=2266; T=38
open_cc
shot "ccp-$TAG-grid"
click $((L + 100)) $((T + 31))                 # the Network pill's label
shot "ccp-$TAG-network"
"$FI" move 1500 700 wait 200 down wait 60 up wait 300 > /dev/null 2>&1; waitfor 1   # outside: closes
open_cc
click $((L + 100)) $((T + 74 + 31))            # the Bluetooth pill's label
shot "ccp-$TAG-bluetooth"
"$FI" move 1500 700 wait 200 down wait 60 up wait 300 > /dev/null 2>&1; waitfor 1
open_cc
# Sound is the last row; with Display hidden (no brightness in the sandbox) it sits at row 4.
click $((L + 284 - 9 - 14)) $((T + 296 + 62 - 6 - 14))
shot "ccp-$TAG-sound"
