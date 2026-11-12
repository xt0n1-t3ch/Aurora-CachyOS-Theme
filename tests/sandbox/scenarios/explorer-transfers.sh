#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's transfer center: a copy of 30,000 small files shows a ring and its percent in the action row; a click
# opens the panel with the copy, its bar and its buttons; the button leaves when the copy ends. Captures
# shots/transfers-{running,panel,done}.png and writes PASS or FAIL lines to logs/transfers.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/transfers.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DIR="$HOME/Transfers"
rm -rf "$DIR" && mkdir -p "$DIR/A Source" "$DIR/B Target"
input() { "$FI" "$@" > /dev/null 2>&1; }
# Many small files: KIO copies them one by one, slowly enough to watch, and they take almost no disk.
python3 -I -c 'import sys
for i in range(1, 30001):
    with open(f"{sys.argv[1]}/file-{i}.txt", "w") as f:
        f.write(f"file {i}\n")' "$DIR/A Source"
type_word() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44)
    local word=$1 i
    for ((i = 0; i < ${#word}; i++)); do input keydown "${code[${word:i:1}]}" keyup "${code[${word:i:1}]}" wait 60; done
}
kwriteconfig6 --file file-explorerrc --group View --key Mode ""
kwriteconfig6 --file file-explorerrc --group Inspector --key Visible false
check() {
    if python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1; then
        echo "PASS $1" >> "$OUT"
    else
        echo "FAIL $1" >> "$OUT"
    fi
}

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
QT_QPA_PLATFORM=xcb "$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-transfers.log" 2>&1 &
waitfor 8
# The icons view, from the command palette, so the two folders sit side by side.
input move 1500 1000 wait 200 down wait 60 up wait 300
input keydown 29 keydown 42 keydown 25 keyup 25 keyup 42 keyup 29 wait 600
type_word icons
input wait 300 keydown 28 keyup 28 wait 1500
shot transfers-before
# A click on "A Source", Ctrl+C (29, 46); a double click on "B Target", Ctrl+V (29, 47).
input move 885 440 wait 200 down wait 60 up wait 300 keydown 29 keydown 46 keyup 46 keyup 29 wait 300
input move 1021 440 wait 200 down wait 60 up wait 80 down wait 60 up wait 1500
input keydown 29 keydown 47 keyup 47 keyup 29 wait 1200
shot transfers-running
# The transfer button sits in the open middle of the action row.
check the-copy-shows-in-the-action-row transfers-before transfers-running 1390 340 1520 375
input move 1450 355 wait 200 down wait 60 up wait 800
shot transfers-panel
check a-click-opens-the-panel transfers-running transfers-panel 1110 395 1500 480
for _ in $(seq 1 120); do [ "$(find "$DIR/B Target" -type f | wc -l)" -ge 30000 ] && break; sleep 1; done
input wait 2000
shot transfers-done
check the-button-leaves-when-the-copy-ends transfers-running transfers-done 1390 340 1520 375
[ "$(find "$DIR/B Target" -type f | wc -l)" = 30000 ] && echo "PASS every-file-is-copied" >> "$OUT" || echo "FAIL every-file-is-copied" >> "$OUT"
