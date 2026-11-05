#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The File Explorer mockup's scene, live: a project folder in X/2-Dev with tagged files, its menu, a typed path,
# Quick Look, Split View and a tag's shelf. fe-shots in finder mode; captures go to shots/fe-f*.png.
source "$SBX/prelude.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
project="$HOME/X/2-Dev/Aurora-CachyOS-Theme"
rm -rf "$HOME/X" "$HOME/.local/share/file-explorer/tags"
mkdir -p "$project"
for dir in assets cli components docs installer packaging scripts tests themes; do mkdir -p "$project/$dir"; done
for name in CHANGELOG.md get.sh install.sh LICENSE README.md; do cp "$SB/$name" "$project/$name"; done
# Menus from an X11 client: Wayland dismisses popups that no real input event opened.
QT_QPA_PLATFORM=xcb "$SB/build/file-explorer/fe-shots" "$SBX/shots" "$project" finder > "$SBX/logs/fe-shots-finder.log" 2>&1
