#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The menu bar as the installer leaves it: plasmashell with the tray's apps and a player, then shots/bar-<TAG>.png.
source "$SBX/prelude.sh"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
python3 "$SB/tests/helpers/fake_mpris.py" > "$SBX/logs/fake-mpris.log" 2>&1 &
waitfor 8
shot "bar-$TAG"
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript 'panels().forEach(function(p){ print(p.location + " hiding=" + p.hiding + " opacity=" + p.opacity + ": " + p.widgets().map(function(w){ return w.type.replace("org.kde.plasma.", ""); }).join(", ")); });' > "$SBX/logs/bar-$TAG.txt" 2>&1
