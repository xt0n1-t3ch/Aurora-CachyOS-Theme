#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's command palette: Ctrl+Shift+P opens it, "show hidden" finds Show Hidden Files, Enter runs it and the
# hidden file appears. Captures shots/palette-{before,open,typed,ran}.png and writes PASS or FAIL lines to
# logs/palette.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/palette.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DIR="$HOME/Palette"
rm -rf "$DIR" && mkdir -p "$DIR"
printf 'x\n' > "$DIR/visible.txt"
printf 'x\n' > "$DIR/.hidden-note"
# The scenario starts with hidden files off.
kwriteconfig6 --file "$HOME/.local/share/file-explorer/view_properties/global/.directory" --group Settings --key HiddenFilesShown false

input() { "$FI" "$@" > /dev/null 2>&1; }
type_word() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44 [' ']=57)
    local word=$1 i
    for ((i = 0; i < ${#word}; i++)); do input keydown "${code[${word:i:1}]}" keyup "${code[${word:i:1}]}" wait 60; done
}
# check NAME BEFORE AFTER X0 Y0 X1 Y1: PASS when the region changed.
check() {
    if python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1; then
        echo "PASS $1" >> "$OUT"
    else
        echo "FAIL $1" >> "$OUT"
    fi
}

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-palette.log" 2>&1 &
waitfor 8
shot palette-before
# Ctrl+Shift+P (29, 42, 25).
input keydown 29 keydown 42 keydown 25 keyup 25 keyup 42 keyup 29 wait 700
shot palette-open
# The panel over the top of the window, centered on it.
check ctrl-shift-p-opens-the-palette palette-before palette-open 1000 390 1560 520
# "hidden" alone ranks Sort By's "Hidden Files Last" first (it starts with the word); "show hidden" finds the command.
type_word 'show hidden'
input wait 500
shot palette-typed
input keydown 28 keyup 28 wait 1500
shot palette-ran
# The first items of the view: the hidden file now comes before visible.txt.
check enter-runs-show-hidden-files palette-before palette-ran 820 400 1400 520
check the-palette-closes-after-running palette-open palette-ran 1000 390 1560 520
