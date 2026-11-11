#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's Compare Folders: Split View with Left and Right, then Shift+F3 lists what differs; Ctrl+A and Copy to
# Right bring Right in step. Captures shots/compare-{split,sheet,synced}.png and writes PASS or FAIL lines to
# logs/compare.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/compare.txt"; : > "$OUT"
SHOTS="$SBX/shots"
TOP="$HOME/Compare"
rm -rf "$TOP" && mkdir -p "$TOP/left/Photos" "$TOP/right"
printf 'same\n' > "$TOP/left/notes.txt"; cp -p "$TOP/left/notes.txt" "$TOP/right/notes.txt"
printf 'version two, longer\n' > "$TOP/left/plan.md"; printf 'version one\n' > "$TOP/right/plan.md"
printf 'x\n' > "$TOP/left/Photos/beach.jpg"
printf 'x\n' > "$TOP/right/old-draft.odt"
kwriteconfig6 --file file-explorerrc --group View --key Mode ""
kwriteconfig6 --file file-explorerrc --group Inspector --key Visible false
input() { "$FI" "$@" > /dev/null 2>&1; }
type_text() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44 [/]=53 [.]=52)
    local text=$1 i c
    for ((i = 0; i < ${#text}; i++)); do
        c=${text:i:1}
        input keydown "${code[$c]}" keyup "${code[$c]}" wait 50
    done
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
"$SB/build/file-explorer/file-explorer" "$TOP/left" > "$SBX/logs/fe-compare.log" 2>&1 &
waitfor 8
# F3 (61) splits; Ctrl+L (29, 38) and ../right, a path the address field takes relative to the folder, in the new
# pane.
input move 1500 1000 wait 200 down wait 60 up wait 300 keydown 61 keyup 61 wait 1500
input keydown 29 keydown 38 keyup 38 keyup 29 wait 400
type_text "../right"
input keydown 28 keyup 28 wait 1500
shot compare-split
# Shift+F3 (42, 61).
input keydown 42 keydown 61 keyup 61 keyup 42 wait 2500
shot compare-sheet
check shift-f3-opens-the-comparison compare-split compare-sheet 900 450 1700 900
# A click on Photos, then Copy to Right: the folder goes over with what is in it, and the list is compared again.
input move 840 552 wait 200 down wait 60 up wait 400
shot compare-selected
input move 1642 972 wait 200 down wait 60 up wait 3000
shot compare-synced
[ -f "$TOP/right/Photos/beach.jpg" ] && echo "PASS copy-to-right-copies-the-folder" >> "$OUT" || echo "FAIL copy-to-right-copies-the-folder" >> "$OUT"
check the-list-is-compared-again compare-selected compare-synced 770 540 1400 600
