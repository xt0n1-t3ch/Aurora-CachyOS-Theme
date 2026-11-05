#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The login script (installer/session/aurora.sh) keeps a plugin only while it was built for the installed
# packages; a plugin built for other versions is renamed *.paused so Plasma and KWin load their stock parts.
# A fake pacman and fake plugin files in a temp home.
set -u
here=$(dirname "$(readlink -f "$0")")
script="$here/../../installer/session/aurora.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/bin"
cat > "$work/bin/pacman" <<'EOF'
#!/bin/sh
case "$2" in
    plasma-workspace) echo "plasma-workspace 6.8.2-1.1" ;;
    qt6-base) echo "qt6-base 6.12.0-3.1" ;;
    kdecoration) echo "kdecoration 6.8.2-1.1" ;;
esac
EOF
chmod +x "$work/bin/pacman"
failures=0
plugins="$work/home/.local/lib/qt6/plugins"
tray="$plugins/plasma/applets/org.kde.plasma.systemtray.so"
banners="$plugins/plasma/applets/org.kde.plasma.notifications.so"
buttons="$plugins/org.kde.kdecoration3/org.aurora.decoration.so"
# login TRAY_STAMP BANNERS_STAMP BUTTONS_STAMP: makes fresh plugins and stamps (an empty stamp is not written),
# then runs the login script and saves the QT_PLUGIN_PATH it leaves in $work/env.txt.
login() {
    rm -rf "${work:?}/home"
    mkdir -p "$(dirname "$tray")" "$(dirname "$buttons")" "$work/home/.local/share/aurora"
    touch "$tray" "$banners" "$buttons"
    [ -n "$1" ] && echo "$1" > "$work/home/.local/share/aurora/system-tray-built-for"
    [ -n "$2" ] && echo "$2" > "$work/home/.local/share/aurora/notifications-built-for"
    [ -n "$3" ] && echo "$3" > "$work/home/.local/share/aurora/window-buttons-built-for"
    HOME="$work/home" PATH="$work/bin:$PATH" bash -c ". '$script'; echo \"\$QT_PLUGIN_PATH\"" > "$work/env.txt"
}
# state: prints one character per plugin (tray, banners, buttons): + while it is in place, - once it is gone.
state() {
    local out=""
    for f in "$tray" "$banners" "$buttons"; do [ -e "$f" ] && out="$out+" || out="$out-"; done
    echo "$out"
}
# check NAME GOT WANT: prints PASS when GOT equals WANT and FAIL otherwise; a FAIL counts as a failure.
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}

login "6.8.2 6.12.0" "6.8.2 6.12.0" "6.8"
check "every plugin built for the installed versions loads" "$(state)" "+++"
check "the user's plugin folder comes first" "$(cut -d: -f1 "$work/env.txt")" "$plugins"

login "6.8.1 6.12.0" "6.8.2 6.12.0" "6.8"
check "a tray built for an older Plasma steps aside, the rest stays" "$(state)" "-++"
check "the paused tray is kept for the rebuild" "$([ -e "$tray.paused" ] && echo kept)" kept

login "6.8.2 6.11.0" "6.8.2 6.11.0" "6.8"
check "a Qt update pauses both Plasma applets" "$(state)" "--+"

login "6.8.2 6.12.0" "6.8.2 6.12.0" "6.7"
check "window buttons built for another KDecoration step aside" "$(state)" "++-"

login "" "" ""
check "plugins never stamped do not load" "$(state)" "---"

exit "$failures"
