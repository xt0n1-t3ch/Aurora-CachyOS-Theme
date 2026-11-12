#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Git in File Explorer: in a repository, each file shows its Git status (Dolphin's Git plugin from dolphin-plugins)
# and the context menu has a Git submenu. Needs dolphin-plugins installed, or AURORA_DOLPHIN_PLUGINS naming the root
# of its unpacked package. Captures shots/git-{status,menu,submenu}.png and writes PASS or FAIL lines to
# logs/git.txt. Runs File Explorer as an X11 client, so its menus open from scripted input (see docs/sandbox.md).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/git.txt"; : > "$OUT"
SHOTS="$SBX/shots"
PLUGINS="${AURORA_DOLPHIN_PLUGINS:-}/usr/lib/qt6/plugins/dolphin/vcs"
if [ ! -e "$PLUGINS/fileviewgitplugin.so" ]; then
    echo "FAIL no Git plugin in $PLUGINS (install dolphin-plugins or set AURORA_DOLPHIN_PLUGINS)" >> "$OUT"; exit 0
fi
if [ -n "${AURORA_DOLPHIN_PLUGINS:-}" ]; then
    mkdir -p "$HOME/.local/lib/qt6/plugins/dolphin/vcs"
    cp "$PLUGINS"/fileviewgitplugin.so "$HOME/.local/lib/qt6/plugins/dolphin/vcs/"
fi
kwriteconfig6 --file dolphinrc --group VersionControl --key enabledPlugins Git
# Hidden files off, so changed.txt is the first item, and the Inspector closed.
kwriteconfig6 --file "$HOME/.local/share/file-explorer/view_properties/global/.directory" --group Settings --key HiddenFilesShown false
kwriteconfig6 --file file-explorerrc --group Inspector --key Visible false
REPO="$HOME/Repo"
rm -rf "$REPO" && mkdir -p "$REPO"
(
    cd "$REPO" && git init -q && git config user.name Sandbox && git config user.email sandbox@example.org
    printf 'one\n' > committed.txt && printf 'two\n' > changed.txt && git add . && git commit -qm first
    printf 'two, edited\n' > changed.txt
    printf 'new\n' > untracked.txt
)
input() { "$FI" "$@" > /dev/null 2>&1; }
check() {
    if python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1; then
        echo "PASS $1" >> "$OUT"
    else
        echo "FAIL $1" >> "$OUT"
    fi
}

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
QT_QPA_PLATFORM=xcb "$SB/build/file-explorer/file-explorer" "$REPO" > "$SBX/logs/fe-git.log" 2>&1 &
waitfor 9
shot git-status
# A right-click on changed.txt, the first item.
input move 885 440 wait 200 rdown wait 80 rup wait 2500
shot git-menu
check the-context-menu-opens git-status git-menu 900 450 1300 1100
# The Git submenu, the menu's last entry.
input glide 960 924 10 wait 2000
shot git-submenu
check the-git-submenu-opens git-menu git-submenu 1160 880 1500 1100
