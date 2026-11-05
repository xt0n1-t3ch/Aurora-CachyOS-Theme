#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# One touch of the bottom edge reveals the dodging dock, as on macOS: the pointer glides down onto the edge once,
# away from the dock first (it must stay hidden), then under the dock's centre. Writes PASS/FAIL to
# logs/dock-touch-<TAG>.txt, judged by the dock's pixels in the bottom strip.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
TAG="${SCEN_TAG:-now}"
OUT="$SBX/logs/dock-touch-$TAG.txt"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'panels().forEach(function(p){ if (p.location=="bottom") p.hiding = "dodgewindows"; });' > /dev/null 2>&1
QT_QPA_PLATFORM=xcb konsole --hold -e true > /dev/null 2>&1 &
waitfor 4
# Maximizes the Konsole so the dock has a window to dodge.
id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SB/tests/helpers/kwin_maximize_active.js" gg_max)
qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null
"$FI" move 1280 700 wait 300 > /dev/null 2>&1
waitfor 3
shot "docktouch-$TAG-hidden"
# touch_at X: the pointer glides down onto the bottom edge at X and rests there.
touch_at() {
    "$FI" move "$1" 1300 wait 200 glide "$1" 1439 12 wait 900 > /dev/null 2>&1
}
# shown NAME: succeeds when the dock is visible in shots/NAME.png.
shown() { python3 "$SB/tests/helpers/dock_visible.py" "$SBX/shots/$1.png"; }
: > "$OUT"
if shown "docktouch-$TAG-hidden"; then echo "FAIL the dock dodges the maximized window" >> "$OUT"; else echo "PASS the dock dodges the maximized window" >> "$OUT"; fi
touch_at 300
shot "docktouch-$TAG-far"
if shown "docktouch-$TAG-far"; then echo "FAIL a touch far from the dock leaves it hidden" >> "$OUT"; else echo "PASS a touch far from the dock leaves it hidden" >> "$OUT"; fi
"$FI" move 1280 700 wait 300 > /dev/null 2>&1
waitfor 2
touch_at 1280
shot "docktouch-$TAG-centre"
if shown "docktouch-$TAG-centre"; then echo "PASS one touch under the dock reveals it" >> "$OUT"; else echo "FAIL one touch under the dock reveals it" >> "$OUT"; fi
