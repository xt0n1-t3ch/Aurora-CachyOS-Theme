#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The hand over clickable shell surfaces: the pointer rests on a Dock icon, a tray icon, Quick Menu, Control Center
# and on the empty bar; each shot (with the pointer) is shots/hand-<place>.png.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
POINTER_DEBUG=1 QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
waitfor 6
# pshot PLACE: captures the screen with the pointer drawn in (spectacle -p) as shots/hand-PLACE.png.
pshot() { spectacle -b -n -f -p -o "$SBX/shots/hand-$1.png" > /dev/null 2>&1; }
# at PLACE X Y: the pointer arrives at X,Y with a real motion, rests there, and PLACE is captured.
at() { "$FI" move "$(( $2 - 6 ))" "$(( $3 - 4 ))" wait 120 move "$2" "$3" wait 500 > /dev/null 2>&1; pshot "$1"; }
at dock 830 1400
at tray 1881 16
at quick-menu 2341 16
at cc 2387 16
at bar 1000 16
at desktop 1280 700
