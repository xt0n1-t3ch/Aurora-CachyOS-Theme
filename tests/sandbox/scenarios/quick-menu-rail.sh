#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu's account and power rail, end to end: the user's picture (~/.face.icon) beside their name (captured); the power
# menu's Restart and Shut Down and the account menu's Log Out and Lock each reach Plasma's session services. Those
# services are stood in for by tests/helpers/fake_session.py, which only records the call: the real logout prompt
# confirms itself after its countdown and would restart the computer. Nothing is clicked unless the stand-in owns every
# name. Sleep, Hibernate and Switch User talk to the system bus, so they are covered by tests/tst_quickmenu.qml only.
# Writes PASS or FAIL lines to logs/quick-menu-rail.txt and captures shots/quick-menu-rail-*.png.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/quick-menu-rail.txt"; : > "$OUT"
CALLS="$SBX/logs/quick-menu-rail-calls.txt"; : > "$CALLS"
check() { if [ "$2" = yes ]; then echo "PASS $1" >> "$OUT"; else echo "FAIL $1" >> "$OUT"; fi; }
where() { python3 -I "$SB/tests/helpers/find_text.py" "$SBX/shots/$1.png" "$2" "${3:-1}"; }
click() { "$FI" move "$1" "$2" wait 300 down wait 60 up wait 700 > /dev/null 2>&1; }

python3 -I "$SB/tests/helpers/fake_session.py" "$CALLS" > "$SBX/logs/quick-menu-rail-fake.txt" 2>&1 &
FAKE=$!
waitfor 2
if ! grep -qx ready "$SBX/logs/quick-menu-rail-fake.txt"; then
    check "the stand-in session services own their names (nothing was clicked)" no
    kill "$FAKE" 2> /dev/null
    exit 0
fi

# A picture for the account, as System Settings > Users saves one; removed at the end.
cp "$SB/assets/logo.png" "$HOME/.face.icon"
source "$SB/tests/helpers/showcase_wallpaper.sh"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 10
showcase_wallpaper
QT_FORCE_STDERR_LOGGING=1 "$SB/build/quick-menu/bin/krunner" --daemon > "$SBX/logs/quick-menu-rail-krunner.log" 2>&1 &
waitfor 5

# open_rail: opens Quick Menu and finds the rail: NAME_X NAME_Y (the account) and POWER_X (the power button, which sits
# under the right end of "Show More").
open_rail() {
    qdbus6 org.kde.krunner /App org.kde.krunner.App.toggleDisplay; waitfor 2
    shot quick-menu-rail-open
    read -r NAME_X NAME_Y <<< "$(where quick-menu-rail-open "$1")"
    read -r MORE_X _ <<< "$(where quick-menu-rail-open "Show More")"
    POWER_X=$((MORE_X + 22))
}
# choose MENU_X MENU_Y ENTRY CALL: opens the menu at MENU_X MENU_Y, clicks ENTRY and checks CALL was recorded.
choose() {
    : > "$CALLS"
    click "$1" "$2"
    shot quick-menu-rail-menu
    local x y
    read -r x y <<< "$(where quick-menu-rail-menu "$3")"
    if [ -z "$x" ]; then check "$3 is in the menu" no; return; fi
    click "$x" "$y"
    waitfor 2
    check "$3 asks the session manager ($(tr '\n' ' ' < "$CALLS"))" "$(grep -q "$4" "$CALLS" && echo yes || echo no)"
}

USER_NAME=$(getent passwd "$USER" | cut -d: -f5 | cut -d, -f1)
open_rail "${USER_NAME:-$USER}"
check "the rail shows the account's name" "$([ -n "$NAME_X" ] && echo yes || echo no)"
# The picture beside the name, for the eye: shots/quick-menu-rail-avatar.png.
shot quick-menu-rail-avatar

choose "$POWER_X" "$NAME_Y" "Restart" "promptReboot\|logoutAndReboot"
open_rail "${USER_NAME:-$USER}"
choose "$POWER_X" "$NAME_Y" "Shut Down" "promptShutDown\|logoutAndShutdown"
open_rail "${USER_NAME:-$USER}"
choose "$NAME_X" "$NAME_Y" "Log Out" "promptLogout\|org.kde.Shutdown.logout"
open_rail "${USER_NAME:-$USER}"
choose "$NAME_X" "$NAME_Y" "Lock" "ScreenSaver.Lock"

rm -f "$HOME/.face.icon"
kill "$FAKE" 2> /dev/null
qdbus6 org.kde.plasmashell /MainApplication org.qtproject.Qt.QCoreApplication.quit > /dev/null 2>&1
waitfor 2
