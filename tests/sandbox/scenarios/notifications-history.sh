#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The notifications popover in the menu bar with three notifications that went to the history, then with the pointer
# over the first one. SCEN_TAG picks another case: empty, the empty popover; group, a group of three; maximized, the
# popover over a maximized window with the menu bar adaptive and then translucent. The popover opens through the
# applet's global shortcut, so no click has to find the bell. Captures notif-history-*.png; writes PASS or FAIL per
# check to logs/notif-history.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/notif-history.txt"; : > "$OUT"
APPSRC="$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc"
# The notifications applet inside the tray, as its section header in the layout names it.
group=$(grep -o '^\[Containments\]\[[0-9]*\]\[Applets\]\[[0-9]*\]\[Applets\]\[[0-9]*\]' \
    <(grep -B2 '^plugin=org.kde.plasma.notifications$' "$APPSRC") | tail -1)
id=$(echo "$group" | grep -o '[0-9]*' | tail -1)
echo "applet $id" >> "$OUT"
python3 -I - "$APPSRC" "$group" <<'EOF'
import sys
path, group = sys.argv[1], sys.argv[2]
text = open(path).read()
section = group + "[Shortcuts]"
if section not in text:
    text += "\n" + section + "\nglobal=Meta+Alt+N\n"
open(path, "w").write(text)
EOF
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12

# check COMMAND...: appends PASS or FAIL for the current NAME to the log, by whether COMMAND succeeds.
check() {
    if "$@" > /dev/null; then echo "PASS $NAME" >> "$OUT"; else echo "FAIL $NAME" >> "$OUT"; fi
}
# open_popover: opens the popover through the applet's global shortcut.
open_popover() {
    qdbus6 org.kde.kglobalaccel /component/plasmashell org.kde.kglobalaccel.Component.invokeShortcut "activate widget $id"
    waitfor 2
}
waitfor 3
shot notif-history-0-closed
if [ "${SCEN_TAG:-}" = empty ]; then
    NAME="the shortcut opens the empty popover"
    open_popover
    shot notif-history-1-empty
    check python3 "$CHANGED" "$SBX/shots/notif-history-0-closed.png" "$SBX/shots/notif-history-1-empty.png" 2000 60 2480 500
    exit 0
fi

if [ "${SCEN_TAG:-}" = maximized ]; then
    # A maximized window under the bar, with the menu bar first adaptive (Plasma's default, opaque over a maximized
    # window) and then translucent: the popover should stay glass in the second.
    notify-send -t 4000 -a "Microsoft Edge" -h string:desktop-entry:microsoft-edge "Download complete" "aurora-1.3.0.tar.gz"
    notify-send -t 4000 -a "ZapFast" -h string:desktop-entry:zapfast -h "string:image-path:$SB/tests/helpers/fake-album.png" "Alex" "On my way, see you at 8."
    # A colourful window, so what shows through the popover is plain to see.
    gwenview "$SB/themes/wallpapers/Aurora/contents/images/3840x2160.jpg" > /dev/null 2>&1 &
    waitfor 6
    mid=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SB/tests/helpers/kwin_maximize_active.js" gg_max)
    qdbus6 org.kde.KWin "/Scripting/Script$mid" org.kde.kwin.Script.run > /dev/null
    waitfor 4
    for opacity in adaptive translucent; do
        qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript \
            "panels().forEach(function(p){ if (p.location == 'top') p.opacity = '$opacity'; });" > /dev/null
        waitfor 2
        open_popover
        waitfor 3
        shot "notif-history-5-maximized-$opacity"
        "$FI" move 1280 900 wait 200 down wait 60 up wait 300 > /dev/null 2>&1
        waitfor 2
    done
    echo "maximized: see the captures" >> "$OUT"
    exit 0
fi

if [ "${SCEN_TAG:-}" = group ]; then
    # Three messages from one app make a group: its heading row, then its platters.
    for text in "On my way" "Running ten minutes late" "Grab some bread?"; do
        notify-send -t 4000 -a "ZapFast" -h string:desktop-entry:zapfast "Alex" "$text"
    done
    notify-send -t 4000 -a "Microsoft Edge" -h string:desktop-entry:microsoft-edge "Download complete" "aurora-1.3.0.tar.gz"
    waitfor 8
    NAME="the popover lists a group"
    open_popover
    shot notif-history-4-group
    check python3 "$CHANGED" "$SBX/shots/notif-history-0-closed.png" "$SBX/shots/notif-history-4-group.png" 2000 60 2480 500
    exit 0
fi

# The banners time out after 4 s. Plasma keeps an expired notification in the history only when it names its app's
# desktop entry, so each one does.
notify-send -t 4000 -a "File Explorer" -h string:desktop-entry:file-explorer "Moved to Trash" "Quarterly report.pdf"
notify-send -t 4000 -a "Discord" -h string:desktop-entry:discord "Game Night" "Tonight's lobby opens at 8. Bring your best loadout and your worst jokes. We will run three rounds of the new map, then vote on the next one, and the winners pick the music for the rest of the night. Late arrivals join the second round, so do not worry if work runs over."
notify-send -t 4000 -a "Microsoft Edge" -h string:desktop-entry:microsoft-edge "Download complete" "aurora-1.3.0.tar.gz"
notify-send -t 4000 -a "ZapFast" -h string:desktop-entry:zapfast -h "string:image-path:$SB/tests/helpers/fake-album.png" "Alex" "On my way, see you at 8."
waitfor 8

NAME="the popover lists the history"
open_popover
shot notif-history-2-list
check python3 "$CHANGED" "$SBX/shots/notif-history-0-closed.png" "$SBX/shots/notif-history-2-list.png" 2000 60 2480 500

# The pointer over the first notification shows its hover controls. The popover sits under the tray, so its first
# row is found where the list capture differs from the closed one.
read -r x y <<< "$(python3 -I - "$SBX/shots/notif-history-0-closed.png" "$SBX/shots/notif-history-2-list.png" <<'EOF'
import sys
from PIL import Image, ImageChops
a, b = (Image.open(p).convert("L") for p in sys.argv[1:3])
box = ImageChops.difference(a, b).point(lambda v: 255 if v > 20 else 0).getbbox()
left, top, right, bottom = box if box else (1800, 40, 2300, 700)
print((left + right) // 2, top + 160)
EOF
)"
"$FI" move "$x" "$y" wait 200 move $((x + 4)) $((y + 2)) wait 700 > /dev/null 2>&1
waitfor 1
shot notif-history-3-hover
