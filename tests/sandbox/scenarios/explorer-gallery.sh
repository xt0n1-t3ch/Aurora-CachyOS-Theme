#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's gallery view: chosen from the command palette, the selected picture shows large above Dolphin's icons,
# and Right moves to the next one. Captures shots/gallery-{empty,first,next}.png and writes PASS or FAIL lines to
# logs/gallery.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/gallery.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DIR="$HOME/Gallery"
rm -rf "$DIR" && mkdir -p "$DIR"
i=0
while read -r picture; do
    i=$((i + 1)); cp "$picture" "$DIR/picture-$i.${picture##*.}"
done < <(find /usr/share/wallpapers -name '*.png' -size +300k | sort | head -4)
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
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-gallery.log" 2>&1 &
waitfor 8
# Ctrl+Shift+P, "gallery", Enter.
input move 1500 1000 wait 200 down wait 60 up wait 400
input keydown 29 keydown 42 keydown 25 keyup 25 keyup 42 keyup 29 wait 600
type_word gallery
input wait 300 keydown 28 keyup 28 wait 2000
shot gallery-empty
# Ctrl+A (29, 30) selects them all; the stage shows the first.
input keydown 29 keydown 30 keyup 30 keyup 29 wait 2500
shot gallery-first
check the-selected-picture-shows-large gallery-empty gallery-first 1000 450 1800 800
# Right (106) to the next picture.
input keydown 106 keyup 106 wait 2500
shot gallery-next
# The caption under the picture names the next one.
check right-shows-the-next-picture gallery-first gallery-next 1300 875 1500 905
