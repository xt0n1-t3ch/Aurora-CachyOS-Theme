#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Git in File Explorer. In Projects, a folder that holds a repository (app) and a plain one (notes): the repository's
# folder has a Git menu of its own seen from outside, the plain folder has none, and the background of Projects offers
# Clone and Create. Inside app, files show their status and a file's Git menu acts on it. Then fe-shots opens the
# Commit, Changes, History, Branches and Clone sheets. Needs dolphin-plugins installed (for the status in the view), or
# AURORA_DOLPHIN_PLUGINS naming the root of its unpacked package. Captures shots/git-*.png and fe-git-*.png and writes
# PASS or FAIL lines to logs/git.txt. Runs File Explorer as an X11 client, so its menus open from scripted input (see
# docs/sandbox.md).
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
kwriteconfig6 --file "$HOME/.local/share/file-explorer/view_properties/global/.directory" --group Settings --key HiddenFilesShown false
# The sandbox's home sits inside Aurora's own checkout; git must not look above it.
export GIT_CEILING_DIRECTORIES="$HOME"
PROJECTS="$HOME/Projects"
APP="$PROJECTS/app"
REMOTE="$HOME/.remotes/app.git"
rm -rf "$PROJECTS" "$HOME/.remotes" && mkdir -p "$APP" "$PROJECTS/notes"
printf 'Buy milk\n' > "$PROJECTS/notes/todo.txt"
git init -q --bare -b main "$REMOTE"
(
    cd "$APP" && git init -q -b main && git config user.name Sandbox && git config user.email sandbox@example.org
    mkdir -p src docs
    printf '# app\n' > README.md; printf 'int main(void) { return 0; }\n' > src/main.c
    printf 'int add(int a, int b) { return a + b; }\n' > src/util.c; printf 'How to use app.\n' > docs/guide.md
    git add . && git commit -qm "Start the app"
    printf 'int main(void) { return run(); }\n' > src/main.c && git commit -qam "Run from main"
    printf '# app\n\nA small app.\n' > README.md && git commit -qam "Describe the app"
    git remote add origin "$REMOTE" && git push -q -u origin main
    git remote add github git@github.com:xt0n1-t3ch/app.git
    git branch feature/login
    printf 'int main(void) { return run(load()); }\n' > src/main.c
    printf 'int load(void) { return 1; }\n' > src/config.c && git add src/config.c
    git rm -q docs/guide.md
    git mv src/util.c src/helpers.c
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
QT_QPA_PLATFORM=xcb "$SB/build/file-explorer/file-explorer" "$PROJECTS" > "$SBX/logs/fe-git.log" 2>&1 &
FE=$!
waitfor 9
shot git-projects
# A right-click on app, the first item: its menu ends with Git.
input move 885 440 wait 200 rdown wait 80 rup wait 2000
shot git-folder-menu
check the-folder-menu-opens git-projects git-folder-menu 900 450 1300 1100
# Git, the menu's last entry, opens app's own commands: never Clone for a repository's folder.
input glide 930 972 10 wait 2500
shot git-folder-submenu
check the-repository-folder-has-its-git-menu git-folder-menu git-folder-submenu 1110 760 1560 1200
input keydown 1 keyup 1 wait 300 keydown 1 keyup 1 wait 600
# notes, the second item: no Git entry.
input move 1031 440 wait 200 rdown wait 80 rup wait 2000
shot git-plain-menu
check a-plain-folder-has-no-git-entry git-folder-menu git-plain-menu 1040 960 1160 985
input keydown 1 keyup 1 wait 600
# The background of Projects: Clone and Create.
input move 1500 1000 wait 200 rdown wait 80 rup wait 2000
shot git-background-menu
input glide 1565 1312 10 wait 2500
shot git-background-submenu
check the-background-offers-clone-and-create git-background-menu git-background-submenu 1840 1200 2300 1360
input keydown 1 keyup 1 wait 300 keydown 1 keyup 1 wait 600
# Into app: a file's menu acts on the file.
input move 885 440 wait 200 down wait 60 up wait 80 down wait 60 up wait 2500
shot git-app
input move 1031 440 wait 200 rdown wait 80 rup wait 2000
shot git-file-menu
input glide 1076 924 10 wait 2500
shot git-file-submenu
check a-file-has-its-git-menu git-file-menu git-file-submenu 1250 700 1700 1200
input keydown 1 keyup 1 wait 300 keydown 1 keyup 1 wait 600
kill "$FE"
waitfor 2
# The sheets, opened by fe-shots over app.
"$SB/build/file-explorer/fe-shots" "$SHOTS" "$APP" git > "$SBX/logs/fe-shots-git.log" 2>&1
echo "done" >> "$OUT"
