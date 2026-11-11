#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Undo in File Explorer: a file moved to the Trash with Delete comes back with Ctrl+Z. Writes PASS or FAIL lines to
# logs/undo.txt and captures shots/undo-{trashed,back}.png.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/undo.txt"; : > "$OUT"
DIR="$HOME/Undo"
rm -rf "$DIR" && mkdir -p "$DIR" && printf 'x\n' > "$DIR/keep-me.txt"
input() { "$FI" "$@" > /dev/null 2>&1; }
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-undo.log" 2>&1 &
waitfor 8
# The only file is the first row of the view; a click selects it, Delete trashes it.
input move 900 454 wait 200 down wait 80 up wait 400 keydown 111 keyup 111 wait 1500
shot undo-trashed
[ ! -e "$DIR/keep-me.txt" ] && echo "PASS delete-moves-the-file-to-the-trash" >> "$OUT" || echo "FAIL delete-moves-the-file-to-the-trash" >> "$OUT"
# Ctrl+Z (29 Ctrl, 44 Z).
input keydown 29 keydown 44 keyup 44 keyup 29 wait 2000
shot undo-back
[ -e "$DIR/keep-me.txt" ] && echo "PASS ctrl-z-brings-it-back" >> "$OUT" || echo "FAIL ctrl-z-brings-it-back" >> "$OUT"
