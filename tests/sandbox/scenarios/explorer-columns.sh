#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's columns view: chosen from the command palette, a column per folder from home leads to the folder on
# screen, which Dolphin's list shows as the last column; a click in a column opens that folder. Captures
# shots/columns-{before,on,clicked}.png and writes PASS or FAIL lines to logs/columns.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/columns.txt"; : > "$OUT"
SHOTS="$SBX/shots"
TOP="$HOME/Columns"
rm -rf "$TOP" && mkdir -p "$TOP/Projects/Aurora/docs" "$TOP/Projects/Orion" "$TOP/Music"
for name in index.md install.md search.md; do printf 'x\n' > "$TOP/Projects/Aurora/docs/$name"; done
printf 'x\n' > "$TOP/Projects/Aurora/README.md"
kwriteconfig6 --file file-explorerrc --group View --key Mode ""
kwriteconfig6 --file file-explorerrc --group Inspector --key Visible false
input() { "$FI" "$@" > /dev/null 2>&1; }
type_word() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44)
    local word=$1 i
    for ((i = 0; i < ${#word}; i++)); do input keydown "${code[${word:i:1}]}" keyup "${code[${word:i:1}]}" wait 60; done
}
check() {
    if python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1; then
        echo "PASS $1" >> "$OUT"
    else
        echo "FAIL $1" >> "$OUT"
    fi
}

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$TOP/Projects/Aurora/docs" > "$SBX/logs/fe-columns.log" 2>&1 &
waitfor 8
shot columns-before
# Ctrl+Shift+P, "columns", Enter.
input move 1500 900 wait 200 down wait 60 up wait 400
input keydown 29 keydown 42 keydown 25 keyup 25 keyup 42 keyup 29 wait 600
type_word columns
input wait 300 keydown 28 keyup 28 wait 2500
shot columns-on
# The first rows of the columns, where the icons view showed its first icons.
check the-columns-show-the-way-from-home columns-before columns-on 820 390 1220 450
# A click on Orion in the column of Projects.
input move 1063 426 wait 200 down wait 60 up wait 2000
shot columns-clicked
check a-click-in-a-column-opens-that-folder columns-on columns-clicked 1230 390 1640 450
