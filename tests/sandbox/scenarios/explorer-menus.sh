#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's menus: fe-shots in menus mode on the Downloads folder; captures go to shots/, the log to
# logs/fe-shots-menus.log.
source "$SBX/prelude.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
# Menus from an X11 client: Wayland dismisses popups that no real input event opened.
QT_QPA_PLATFORM=xcb "$SB/build/file-explorer/fe-shots" "$SBX/shots" "$HOME/Downloads" menus > "$SBX/logs/fe-shots-menus.log" 2>&1
