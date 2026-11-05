#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu's apps can be dragged within their section: ProtonPlus, the fifth app of Games, dragged onto Faugus, the
# first, becomes the first; the order is saved and still holds after KRunner restarts. Writes PASS or FAIL to
# logs/quick-menu-drag.txt and captures shots/quick-menu-drag-{before,after,restart}.png. The tile positions are those
# of the sandbox's catalog at scale 1 (Quick Menu's panel at 956,420; Games' row at y 1160).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/quick-menu-drag.txt"; : > "$OUT"
ORDER="$HOME/.config/aurora/quick-menu.conf"
rm -f "$ORDER"
source "$SB/tests/helpers/orion_not_claude.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
# start_quick_menu: starts the Quick Menu KRunner daemon, logging to logs/quick-menu-drag-krunner.log, and sets
# QUICK_MENU_PID.
start_quick_menu() {
    QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon >> "$SBX/logs/quick-menu-drag-krunner.log" 2>&1 &
    QUICK_MENU_PID=$!
}
: > "$SBX/logs/quick-menu-drag-krunner.log"
start_quick_menu
waitfor 4
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay; waitfor 2
shot quick-menu-drag-before
FIRST_X=1036 FIFTH_X=1524 ROW_Y=1160
"$FI" move "$FIFTH_X" "$ROW_Y" wait 300 down wait 200 glide "$((FIFTH_X - 40))" "$ROW_Y" 6 wait 100 \
      glide "$FIRST_X" "$ROW_Y" 24 wait 400 up wait 400 > /dev/null 2>&1
waitfor 2
shot quick-menu-drag-after
if grep -q 'Games.*ProtonPlus.*faugus' "$ORDER" 2>/dev/null; then
    echo "PASS the dragged order is saved: ProtonPlus before Faugus" >> "$OUT"
else
    echo "FAIL the dragged order is not saved: $(cat "$ORDER" 2>/dev/null)" >> "$OUT"
fi
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay; waitfor 1
kill "$QUICK_MENU_PID"; waitfor 2
start_quick_menu
waitfor 4
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay; waitfor 2
shot quick-menu-drag-restart
qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1
waitfor 3
orion_not_claude_end
