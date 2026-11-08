#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The README's pictures, on Dawn Bird, the wallpaper of every Aurora picture (tests/helpers/showcase_wallpaper.sh): the
# desktop with File Explorer, then Quick Menu, the Control Center, the Kali menu, notification banners and the
# notifications popover. Captures go
# to shots/readme-*.png; run it with SANDBOX_SCALE=2 for the README's sharp pictures (assets/screenshots/make.py crops
# them).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
# KWin keeps the sandbox screen's saved scale of 1; a sharper run sets its own and puts 1 back at the end.
SCALE=${SANDBOX_SCALE:-1}
[ "$SCALE" != 1 ] && kscreen-doctor "output.Virtual-0.scale.$SCALE" > "$SBX/logs/readme-scale.log" 2>&1
source "$SB/tests/helpers/orion_not_claude.sh"
source "$SB/tests/helpers/showcase_wallpaper.sh"
# Orion first under Developer Tools, in the order that dragging it there saves.
mkdir -p "$HOME/.config/aurora"
printf '[Applications]\norder="{\\"Developer Tools\\":[\\"applications:orion-app.desktop\\"]}"\n' > "$HOME/.config/aurora/quick-menu.conf"
kwriteconfig6 --file "$HOME/.config/plasmanotifyrc" --group Notifications --key PopupPosition BottomRight
# A global shortcut for the notifications applet inside the tray, so the popover opens without a click having to find
# the bell (as in notifications-history.sh).
APPSRC="$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc"
group=$(grep -o '^\[Containments\]\[[0-9]*\]\[Applets\]\[[0-9]*\]\[Applets\]\[[0-9]*\]' \
    <(grep -B2 '^plugin=org.kde.plasma.notifications$' "$APPSRC") | tail -1)
notifications_id=$(echo "$group" | grep -o '[0-9]*' | tail -1)
grep -qF "${group}[Shortcuts]" "$APPSRC" || printf '\n%s[Shortcuts]\nglobal=Meta+Alt+N\n' "$group" >> "$APPSRC"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
showcase_wallpaper
source "$SB/tests/helpers/tray_apps.sh"
python3 "$SB/tests/helpers/fake_mpris.py" > "$SBX/logs/fake-mpris.log" 2>&1 &
QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon > "$SBX/logs/readme-krunner.log" 2>&1 &
"$SB/build/file-explorer/file-explorer" "$HOME" > "$SBX/logs/readme-fe.log" 2>&1 &
waitfor 8

# File Explorer on the left, clear of the Dock so the Dock stays up, leaving the Dawn Bird and its light trail in view.
cat > "$SBX/run/place_readme.js" <<'JS'
for (const w of workspace.windowList()) {
  if (w.normalWindow && w.resourceClass.includes("file-explorer")) {
    w.frameGeometry = {x: 110, y: 150, width: 1320, height: 980};
    workspace.activeWindow = w;
  }
}
JS
id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SBX/run/place_readme.js" readme_place)
qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null
waitfor 2
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript readme_place > /dev/null
"$FI" move 2500 700 wait 200 > /dev/null 2>&1
waitfor 2
shot readme-desktop

# Quick Menu over the bare wallpaper: File Explorer is minimized for this picture and comes back after it, active
# again, since the menu bar picture shows its name.
explorer() {   # explorer true|false: minimizes or restores File Explorer
    cat > "$SBX/run/readme_explorer.js" <<JS
for (const w of workspace.windowList()) {
  if (w.normalWindow && w.resourceClass.includes("file-explorer")) {
    w.minimized = $1;
    if (!$1) workspace.activeWindow = w;
  }
}
JS
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SBX/run/readme_explorer.js" readme_explorer)
    qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null
    waitfor 2
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript readme_explorer > /dev/null
}
explorer true
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay
waitfor 3
shot readme-quick-menu
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay
waitfor 2
explorer false

# The Control Center is the last icon before the clock.
# tray_icons.py reads a capture at the screen's own 2560x1440, so a sharper capture is scaled back first.
python3 -I -c "from PIL import Image; i = Image.open('$SBX/shots/readme-desktop.png'); i.resize((2560, 1440)).save('$SBX/run/readme-bar.png')"
read -r -a X <<< "$(python3 "$SB/tests/helpers/tray_icons.py" "$SBX/run/readme-bar.png")"
"$FI" move "${X[${#X[@]} - 1]}" 14 wait 400 down wait 60 up wait 300 > /dev/null 2>&1
waitfor 3
shot readme-control-center
# Escape closes the Control Center.
"$FI" keydown 1 keyup 1 wait 300 > /dev/null 2>&1
waitfor 2

"$FI" move 18 14 wait 400 down wait 60 up wait 300 > /dev/null 2>&1
waitfor 2
shot readme-menu
# Escape closes the menu; the pointer moves off the bar.
"$FI" keydown 1 keyup 1 wait 300 move 2500 700 > /dev/null 2>&1
waitfor 2

notify-send -t 30000 -a "Discord" -i discord -h string:desktop-entry:discord "Steven" "Ranked at 8? I'll set up the lobby."
waitfor 1
notify-send -t 30000 -a "System Settings" -i preferences-system "Updates ready" "Plasma 6.8.3 is ready to install." -A "later=Later" -A "now=Install" &
waitfor 3
shot readme-notifications

# The popover lists what went to the history once the banners expired; Plasma keeps a notification there only when it
# names its app's desktop entry.
waitfor 30
notify-send -t 3000 -a "Microsoft Edge" -h string:desktop-entry:microsoft-edge "Download complete" "aurora-1.3.0.tar.gz"
notify-send -t 3000 -a "File Explorer" -h string:desktop-entry:file-explorer "Moved to Trash" "Quarterly report.pdf"
notify-send -t 3000 -a "ZapFast" -h string:desktop-entry:zapfast -h "string:image-path:$SB/tests/helpers/fake-album.png" "Alex" "On my way, see you at 8."
waitfor 8
qdbus6 org.kde.kglobalaccel /component/plasmashell org.kde.kglobalaccel.Component.invokeShortcut \
    "activate widget $notifications_id"
waitfor 3
shot readme-notification-center
[ "$SCALE" != 1 ] && kscreen-doctor output.Virtual-0.scale.1 >> "$SBX/logs/readme-scale.log" 2>&1
qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1
waitfor 3
orion_not_claude_end
