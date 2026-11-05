#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Dock reveal: the dock dodges a maximized window, then the pointer touches the bottom edge at the dock's left end,
# centre and right end, and far from it. Shots: dock-<TAG>-{hidden,left,centre,right,far}.png.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'panels().forEach(function(p){ if (p.location=="bottom") { p.hiding = "dodgewindows"; print("dock " + p.id + " hiding=" + p.hiding + " length=" + p.length + " offset=" + p.offset); } });' > "$SBX/logs/dock-$TAG.txt" 2>&1
QT_QPA_PLATFORM=xcb konsole --hold -e true > /dev/null 2>&1 &
waitfor 4
# Maximizes the Konsole so the dock has a window to dodge.
id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SB/tests/helpers/kwin_maximize_active.js" gg_max)
qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null
waitfor 3
"$FI" move 1280 700 wait 300 > /dev/null 2>&1
waitfor 2
shot "dock-$TAG-hidden"
# probe NAME X: the pointer glides down to the bottom edge at X and keeps pushing against it, as a hand does, because
# KWin pushes the pointer back until the push has lasted long enough; then captures dock-<TAG>-NAME.png and moves away.
probe() {
    "$FI" move "$2" 1300 wait 200 glide "$2" 1439 10 wait 60 move "$2" 1439 wait 60 move "$2" 1440 wait 60 \
        move "$2" 1439 wait 60 move "$2" 1440 wait 60 move "$2" 1439 wait 60 move "$2" 1440 wait 60 move "$2" 1439 \
        wait 60 move "$2" 1440 wait 60 move "$2" 1439 wait 900 > /dev/null 2>&1
    shot "dock-$TAG-$1"
    "$FI" move 1280 700 wait 200 > /dev/null 2>&1
    waitfor 2.5
}
probe left 940
probe centre 1280
probe right 1620
probe far 300
