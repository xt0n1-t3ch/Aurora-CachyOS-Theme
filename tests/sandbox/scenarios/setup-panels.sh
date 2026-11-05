#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds the panels, then quits plasmashell so its configuration is flushed to disk.
source "$SBX/prelude.sh"
source "$SB/tests/helpers/showcase_wallpaper.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
showcase_wallpaper
qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "$("$SB/themes/panels/render_layout.sh" "$HOME")" > "$SBX/logs/layout.log" 2>&1
waitfor 6
qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1 || kquitapp6 plasmashell > /dev/null 2>&1
waitfor 4
