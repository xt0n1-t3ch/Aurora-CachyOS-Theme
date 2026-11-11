#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Archives as folders in File Explorer: a double click on a zip shows what is inside, as in Windows' File Explorer;
# Alt+Up leaves it for the folder it is in. Captures shots/archive-{before,inside,up}.png and writes PASS or FAIL lines
# to logs/archive.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/archive.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DIR="$HOME/Archive"
WORK="$SBX/run/archive-src"
rm -rf "$DIR" "$WORK" && mkdir -p "$DIR" "$WORK/Photos" "$WORK/Docs"
printf 'hello\n' > "$WORK/Docs/readme.txt"
printf 'x\n' > "$WORK/Photos/beach.jpg"
printf 'x\n' > "$WORK/notes.md"
(cd "$WORK" && bsdtar -a -cf "$DIR/Holiday.zip" Photos Docs notes.md)
kwriteconfig6 --file file-explorerrc --group General --key ArchivesAsFolders true
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
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-archive.log" 2>&1 &
waitfor 8
shot archive-before
# A click on the empty view first, so the window is active and the double click is not taken as its first click;
# then a double click on the zip, the first item of the view.
input move 1500 900 wait 200 down wait 60 up wait 600
input move 885 440 wait 200 down wait 60 up wait 80 down wait 60 up wait 2500
shot archive-inside
# The address field names the zip once it is open.
check a-double-click-opens-the-zip-as-a-folder archive-before archive-inside 1080 290 1200 325
# Alt+Up (56, 103).
input keydown 56 keydown 103 keyup 103 keyup 56 wait 2000
shot archive-up
check alt-up-leaves-the-archive archive-inside archive-up 1080 290 1200 325
