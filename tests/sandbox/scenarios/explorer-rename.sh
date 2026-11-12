#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's Rename sheet: Ctrl+A, F2, a text to find and its replacement, Enter; then Ctrl+Z takes the whole
# batch back. Captures shots/rename-{before,sheet,done,undone}.png and writes PASS or FAIL lines to logs/rename.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/rename.txt"; : > "$OUT"
DIR="$HOME/Rename"
rm -rf "$DIR" && mkdir -p "$DIR"
for n in 0001 0002 0003 0004 0005; do printf 'x\n' > "$DIR/img-$n.jpg"; done
printf 'x\n' > "$DIR/notes.txt"

input() { "$FI" "$@" > /dev/null 2>&1; }
type_word() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44 [-]=12 [' ']=57)
    local word=$1 i
    for ((i = 0; i < ${#word}; i++)); do input keydown "${code[${word:i:1}]}" keyup "${code[${word:i:1}]}" wait 60; done
}
# count PATTERN: how many names in the folder match the shell pattern.
count() { local n=0 f; for f in "$DIR"/$1; do [ -e "$f" ] && n=$((n + 1)); done; echo "$n"; }
verdict() { [ "$2" = "$3" ] && echo "PASS $1" >> "$OUT" || echo "FAIL $1 (expected $3, got $2)" >> "$OUT"; }

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-rename.log" 2>&1 &
waitfor 8
shot rename-before
# A click on the first row puts the keyboard in the view; Ctrl+A (29, 30) selects all; F2 (60) renames.
input move 900 454 wait 200 down wait 80 up wait 300
input keydown 29 keydown 30 keyup 30 keyup 29 wait 300 keydown 60 keyup 60 wait 1500
type_word img-
# Tab (15) to Replace with.
input keydown 15 keyup 15 wait 200
type_word trip-
input wait 600
shot rename-sheet
# Enter (28) presses Rename, the sheet's default button.
input keydown 28 keyup 28 wait 2500
shot rename-done
verdict renames-the-five-photos "$(count 'trip-*.jpg')" 5
verdict leaves-the-other-file "$(count notes.txt)" 1
# Ctrl+Z (29, 44): one Undo for the whole batch.
input keydown 29 keydown 44 keyup 44 keyup 29 wait 2500
shot rename-undone
verdict one-undo-takes-the-batch-back "$(count 'img-*.jpg')" 5
