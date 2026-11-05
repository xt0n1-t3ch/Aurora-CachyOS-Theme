#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The widget's global shortcut opens the Control Center window (and nothing else), and a second press closes it.
# Captures shots/ccs-<TAG>-{open,closed,again}.png; the widget id and kglobalaccel's answers go to logs/ccs-<TAG>.txt.
source "$SBX/prelude.sh"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 14
# Gives every Control Center widget a global shortcut and prints their ids; kglobalaccel invokes it by that id.
ID=$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'var ids=[]; panels().forEach(function(p){ p.widgets().forEach(function(w){ if (w.type=="org.aurora.controlcenter") { w.globalShortcut="Meta+Ctrl+Alt+F12"; ids.push(w.id); } }); }); print(ids.join(","));')
echo "widget $ID" > "$SBX/logs/ccs-$TAG.txt"
waitfor 1
qdbus6 org.kde.kglobalaccel /component/plasmashell org.kde.kglobalaccel.Component.invokeShortcut "activate widget $ID" >> "$SBX/logs/ccs-$TAG.txt" 2>&1
waitfor 1.5
shot "ccs-$TAG-open"
qdbus6 org.kde.kglobalaccel /component/plasmashell org.kde.kglobalaccel.Component.invokeShortcut "activate widget $ID" >> "$SBX/logs/ccs-$TAG.txt" 2>&1
waitfor 1.5
shot "ccs-$TAG-closed"
qdbus6 org.kde.kglobalaccel /component/plasmashell org.kde.kglobalaccel.Component.invokeShortcut "activate widget $ID" >> "$SBX/logs/ccs-$TAG.txt" 2>&1
waitfor 1.5
shot "ccs-$TAG-again"
