#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# What PointerHints costs per pointer move over the bar and the Dock: logs GGHINT lines with microseconds.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
POINTER_DEBUG=1 QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
waitfor 6
"$FI" move 1500 16 wait 100 glide 2500 16 120 wait 200 move 700 1400 wait 100 glide 1800 1400 120 wait 300 > /dev/null 2>&1
