#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's Inspector: Ctrl+Alt+I opens it on the folder (its whole size counted), a click on a picture shows
# the picture, its dates and permissions, Compute gives its SHA-256 and a pasted hash is compared. Captures
# shots/inspector-{before,folder,picture,hash}.png and writes PASS or FAIL lines to logs/inspector.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/inspector.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DIR="$HOME/Inspector"
rm -rf "$DIR" && mkdir -p "$DIR/Projects/Aurora"
cp "$(find /usr/share/wallpapers -name '*.png' -size +200k | sort | head -1)" "$DIR/Wallpaper.png"
head -c 3000000 /dev/zero > "$DIR/Projects/Aurora/build.bin"
printf 'notes\n' > "$DIR/notes.txt"
# The Inspector starts closed, in the icons view.
kwriteconfig6 --file file-explorerrc --group Inspector --key Visible false
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
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-inspector.log" 2>&1 &
waitfor 8
shot inspector-before
# A click on the empty part of the view gives it the keyboard; Ctrl+Alt+I (29, 56, 23).
input move 1500 900 wait 200 down wait 80 up wait 300
input keydown 29 keydown 56 keydown 23 keyup 23 keyup 56 keyup 29 wait 2500
shot inspector-folder
check ctrl-alt-i-opens-the-inspector inspector-before inspector-folder 1660 420 1980 1100
# A click on the picture.
input move 1160 440 wait 200 down wait 80 up wait 2500
shot inspector-picture
check a-picture-shows-in-the-inspector inspector-folder inspector-picture 1700 400 1980 560
# Compute, then the file's own SHA-256 typed into the compare field (evdev codes of 0-9 and a-f).
input move 1934 807 wait 200 down wait 80 up wait 2000
input move 1840 905 wait 200 down wait 80 up wait 300
SUM=$(sha256sum "$DIR/Wallpaper.png" | cut -d' ' -f1)
declare -A hexkey=([0]=11 [1]=2 [2]=3 [3]=4 [4]=5 [5]=6 [6]=7 [7]=8 [8]=9 [9]=10 [a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33)
for ((i = 0; i < ${#SUM}; i++)); do input keydown "${hexkey[${SUM:i:1}]}" keyup "${hexkey[${SUM:i:1}]}" wait 15; done
input wait 800
shot inspector-hash
check compute-shows-the-hash-and-the-match inspector-picture inspector-hash 1700 840 1990 960
