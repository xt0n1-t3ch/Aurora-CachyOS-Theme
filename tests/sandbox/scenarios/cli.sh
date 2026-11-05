#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The aurora command in Konsole with Aurora's profile, on Dawn Bird: the menu it shows on a desktop where Aurora is
# installed (the banner with its credit above it), then `aurora status`. Captures shots/cli-menu.png and
# shots/cli-status.png. Nothing is installed: the menu waits for a choice that never comes.
source "$SBX/prelude.sh"
source "$SB/tests/helpers/showcase_wallpaper.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
showcase_wallpaper
# aurora shows its menu once Aurora is installed, which it tells by this link to the copy it was installed from.
mkdir -p "$HOME/.local/share/aurora"
made_link=false
if [ ! -e "$HOME/.local/share/aurora/source" ]; then
    ln -s "$SB" "$HOME/.local/share/aurora/source"
    made_link=true
fi

# place: puts the Konsole window in the middle of the screen, large enough for the banner.
cat > "$SBX/run/place_cli.js" <<'JS'
for (const w of workspace.windowList()) {
  if (w.normalWindow && w.resourceClass.includes("konsole")) {
    w.frameGeometry = {x: 620, y: 230, width: 1320, height: 880};
    workspace.activeWindow = w;
  }
}
JS
place() {
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SBX/run/place_cli.js" cli_place)
    qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null
    waitfor 2
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript cli_place > /dev/null
}
export PATH="$SB/build/cli:$PATH"

konsole --profile Aurora --hold -e aurora > "$SBX/logs/cli-menu.log" 2>&1 &
menu=$!
waitfor 5
place
"$SB/tests/helpers/fakeinput/fakeinput" move 2500 1300 wait 200 > /dev/null 2>&1
waitfor 1
shot cli-menu
kill "$menu"
waitfor 2

konsole --profile Aurora --hold -e aurora status > "$SBX/logs/cli-status.log" 2>&1 &
status=$!
waitfor 6
place
waitfor 1
shot cli-status
kill "$status"
waitfor 1

$made_link && rm "$HOME/.local/share/aurora/source"
qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1
waitfor 3
