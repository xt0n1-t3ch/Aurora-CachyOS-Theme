#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The menu bar and Dock layout (themes/panels/layout.js) keeps the apps pinned in the task manager it replaces, with
# File Explorer put first when it is missing. Pins two apps, builds the panels, reads the Dock's pins back from the
# saved panel file, then restores the sandbox's own pins. Writes PASS or FAIL to logs/dock-pins.txt.
source "$SBX/prelude.sh"
APPLETS="$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc"
OUT="$SBX/logs/dock-pins.txt"
original=$(grep -m1 '^launchers=' "$APPLETS")
sed -i "s|^launchers=.*|launchers=applications:org.kde.konsole.desktop,applications:org.kde.kate.desktop|" "$APPLETS"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$("$SB/themes/panels/render_layout.sh" "$HOME")" > "$SBX/logs/layout.log" 2>&1
waitfor 6
# Quitting plasmashell flushes its configuration to disk, where the pins are read back.
qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1
waitfor 4
got=$(grep '^launchers=' "$APPLETS" | sort -u)
want="launchers=applications:file-explorer.desktop,applications:org.kde.konsole.desktop,applications:org.kde.kate.desktop"
if [ "$got" = "$want" ]; then echo "PASS the Dock keeps the pinned apps, File Explorer first" > "$OUT"
else printf 'FAIL the Dock pins: got %s, want %s\n' "$got" "$want" > "$OUT"; fi
sed -i "s|^launchers=.*|$original|" "$APPLETS"
