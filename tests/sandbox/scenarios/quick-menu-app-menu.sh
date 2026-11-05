#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu's app menu, end to end. A right-click on Discord opens its menu and "Pin to Top" moves Discord into the
# Pinned section; the Menu key opens the same menu from the keyboard (and not the search field's text menu) and
# "Unpin from Dock" takes Discord off the Dock; Gwenview opened from its menu shows under Suggestions, and "Remove
# from Suggestions" takes it out again. Writes PASS or FAIL lines to logs/quick-menu-menu.txt and captures
# shots/quick-menu-menu-*.png. Labels are found on the captures with tesseract (tests/helpers/find_text.py).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/quick-menu-menu.txt"; : > "$OUT"
LOG="$SBX/logs/quick-menu-menu-krunner.log"; : > "$LOG"
ORDER="$HOME/.config/aurora/quick-menu.conf"
rm -f "$ORDER"
# Also keeps the Dock's launchers, which orion_not_claude_end puts back.
source "$SB/tests/helpers/orion_not_claude.sh"
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
# The sandbox's activity manager can start without a current activity, and then it records no app use; a real
# session always has one. Suggestions need those records, so the first activity is made current.
# activities METHOD [ARGUMENT]: calls the activity manager.
activities() { qdbus6 org.kde.ActivityManager /ActivityManager/Activities "org.kde.ActivityManager.Activities.$1" "${@:2}"; }
if [ -z "$(activities CurrentActivity)" ]; then
    activities SetCurrentActivity "$(activities ListActivities | head -1)" > /dev/null
fi
QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon >> "$LOG" 2>&1 &
waitfor 4
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay; waitfor 3

# evdev key codes (linux/input-event-codes.h).
DOWN=108 ENTER=28 MENU=127 ESC=1 CTRL=29 Q=16
key() { "$FI" keydown "$1" wait 40 keyup "$1" wait 250 > /dev/null 2>&1; }
check() { if [ "$2" = yes ]; then echo "PASS $1" >> "$OUT"; else echo "FAIL $1" >> "$OUT"; fi; }
# where SHOT PHRASE [NTH|all]: the centre "x y" of PHRASE on shots/SHOT.png, or nothing.
where() { python3 -I "$SB/tests/helpers/find_text.py" "$SBX/shots/$1.png" "$2" "${3:-1}"; }
# press BUTTON SHOT PHRASE [NTH]: clicks PHRASE as SHOT shows it, with the left or right button.
press() {
    local x y
    read -r x y <<< "$(where "$2" "$3" "${4:-1}")"
    [ -n "$x" ] || { echo "FAIL \"$3\" is not on $2.png" >> "$OUT"; return 1; }
    if [ "$1" = right ]; then
        "$FI" move "$x" "$y" wait 300 rdown wait 60 rup wait 600 > /dev/null 2>&1
    else
        "$FI" move "$x" "$y" wait 300 down wait 60 up wait 600 > /dev/null 2>&1
    fi
}
dock() {
    qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript \
        "var out = []; panels().forEach(function (p) {
        p.widgets('org.kde.plasma.icontasks').forEach(function (w) { w.currentConfigGroup = ['General'];
        out.push(String(w.readConfig('launchers', ''))); }); }); print(out.join('|'));"
}
# suggested SHOT: whether Gwenview shows in Suggestions on SHOT: inside the panel (below the search field) and above
# the Creativity section, where it is listed anyway.
suggested() {
    local field creativity
    field=$(where "$1" "Quick Menu Search" | cut -d' ' -f2)
    creativity=$(where "$1" "Creativity" | cut -d' ' -f2)
    where "$1" "Gwenview" all | awk -v top="${field:-0}" -v bottom="${creativity:-99999}" \
        '$2 > top && $2 < bottom { found = 1 } END { exit !found }'
}

# 1. Right-click, then Pin to Top: Down highlights Open, the second Down skips the separator.
shot quick-menu-menu-start
press right quick-menu-menu-start "Discord"
waitfor 1
shot quick-menu-menu-open
key $DOWN; key $DOWN; key $ENTER
waitfor 2
shot quick-menu-menu-pinned
grep -q 'pinned=.*discord.desktop' "$ORDER" 2>/dev/null && pinned=yes || pinned=no
check "Pin to Top saves Discord as pinned" "$pinned"
[ -n "$(where quick-menu-menu-pinned "Pinned")" ] && section=yes || section=no
check "the Pinned section shows above the categories" "$section"

# 2. From the search field, Down selects the first app (Discord, now pinned) and the Menu key opens its menu with
# Open highlighted. The field's style has its own menu for that key (Undo, Cut, Copy, Select All): it must not open.
"$FI" move 2400 200 wait 200 > /dev/null 2>&1
key $DOWN; key $MENU
waitfor 1
shot quick-menu-menu-key
[ -n "$(where quick-menu-menu-key "pin from Top")" ] && [ -z "$(where quick-menu-menu-key "Select All")" ] \
    && only=yes || only=no
check "the Menu key opens the app's menu and not the search field's" "$only"
# Open is highlighted; Down goes to Unpin from Top, Down again to Unpin from Dock.
key $DOWN; key $DOWN; key $ENTER
waitfor 3
dock | grep -q 'discord.desktop' && still=yes || still=no
check "Unpin from Dock takes Discord off the Dock" "$([ "$still" = no ] && echo yes || echo no)"
grep -q 'applications:' <(dock) && kept=yes || kept=no
check "the Dock keeps its other apps" "$kept"
shot quick-menu-menu-undocked

# 3. Gwenview opened from its menu counts as used, so Quick Menu suggests it next time; then it is removed from there.
press right quick-menu-menu-undocked "Gwenview"
waitfor 1
key $DOWN; key $ENTER
waitfor 5
# Gwenview has the focus: Ctrl+Q quits it, so its window does not sit behind Quick Menu.
"$FI" keydown $CTRL wait 40 keydown $Q wait 40 keyup $Q wait 40 keyup $CTRL > /dev/null 2>&1
waitfor 2
qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay; waitfor 3
shot quick-menu-menu-reopened
# Suggestions show one row until Show More; Gwenview, used once, can rank below the first row.
press left quick-menu-menu-reopened "Show More"
waitfor 1
shot quick-menu-menu-suggested
suggested quick-menu-menu-suggested && listed=yes || listed=no
check "an app opened from Quick Menu shows under Suggestions" "$listed"
press right quick-menu-menu-suggested "Gwenview"
waitfor 1
shot quick-menu-menu-suggestion
press left quick-menu-menu-suggestion "Remove from Suggestions"
waitfor 2
shot quick-menu-menu-forgotten
[ "$listed" = yes ] && ! suggested quick-menu-menu-forgotten && gone=yes || gone=no
check "Remove from Suggestions takes the app out of Suggestions" "$gone"
key $ESC

grep -E "QuickMenu(Apps|Popup|Tile)\.qml|quickmenu\.js" "$LOG" > "$SBX/logs/quick-menu-menu-qml.txt"
check "Quick Menu's QML logged no warnings" "$([ -s "$SBX/logs/quick-menu-menu-qml.txt" ] && echo no || echo yes)"

qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1
waitfor 3
orion_not_claude_end
rm -f "$ORDER"
