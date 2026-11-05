#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The sidebar's drives in the Explorer: fe-shots drives mode, captures in shots/drv-*.png, log in logs/fe-drives.log.
# X11 (xcb) because a Wayland compositor closes popups that no real input opened (see components/file-explorer/tools/shots.cpp).
source "$SBX/prelude.sh"
rm -f "$HOME/.config/file-explorerrc"
QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=xcb "$SB/build/file-explorer/fe-shots" "$SBX/shots" / drives > "$SBX/logs/fe-drives.log" 2>&1
echo "config after: $(cat "$HOME/.config/file-explorerrc" 2>/dev/null | tr '\n' ' ')" >> "$SBX/logs/fe-drives.log"
