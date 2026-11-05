#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's main window on the Downloads folder, captured as shots/fe-main.png.
source "$SBX/prelude.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
"$SB/build/file-explorer/file-explorer" "$HOME/Downloads" > "$SBX/logs/fe.log" 2>&1 &
waitfor 9
shot fe-main
