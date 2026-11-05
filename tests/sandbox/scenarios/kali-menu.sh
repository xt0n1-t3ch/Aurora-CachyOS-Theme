#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The Apple menu with PC labels, as the live bar has it (layout.js shortcutLabelStyle 1):
# one click on its icon, one capture of the open menu in shots/applemenu-pc-labels.png.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript '
var ps = panels();
for (var i = 0; i < ps.length; i++) { var ws = ps[i].widgets("org.aurora.logomenu");
  for (var j = 0; j < ws.length; j++) { ws[j].currentConfigGroup = ["Advanced"]; ws[j].writeConfig("shortcutLabelStyle", 1); } }' \
    > "$SBX/logs/applemenu.txt" 2>&1
waitfor 2
"$FI" move 16 14 wait 400 down wait 60 up wait 300 > /dev/null 2>&1
waitfor 2
shot applemenu-pc-labels
