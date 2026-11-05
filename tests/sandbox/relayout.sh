#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Rebuilds the sandbox panels from themes/panels/layout.js, then copies the tray settings scripting cannot write
# from the real desktop's panel file.
SBX=$(dirname "$(readlink -f "$0")")
SB=$(readlink -f "$SBX/../..")
"$SBX/run.sh" "$SBX/scenarios/setup-panels.sh" 60 > "$SBX/logs/setup-panels.log" 2>&1
python3 "$SB/themes/panels/patch_tray.py" "$SBX/home/.config/plasma-org.kde.plasma.desktop-appletsrc" \
    "$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc" --screenshot-shortcut
