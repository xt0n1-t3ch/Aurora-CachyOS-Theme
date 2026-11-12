#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's Copy Path with several items: Ctrl+A, then Ctrl+Alt+C, puts every path on the clipboard, one per
# line; pasted into KWrite they show. Captures shots/copypaths-{selected,pasted}.png and writes PASS or FAIL lines to
# logs/copypaths.txt (the clipboard is read through Klipper, which runs in plasmashell).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/copypaths.txt"; : > "$OUT"
DIR="$HOME/CopyPaths"
rm -rf "$DIR" && mkdir -p "$DIR/Folder one"
printf 'x\n' > "$DIR/a.txt"
printf 'x\n' > "$DIR/b c.pdf"
input() { "$FI" "$@" > /dev/null 2>&1; }

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-copypaths.log" 2>&1 &
waitfor 8
# A click on the first row gives the view the keyboard; Ctrl+A (29, 30); Ctrl+Alt+C (29, 56, 46).
input move 900 454 wait 200 down wait 80 up wait 300
input keydown 29 keydown 30 keyup 30 keyup 29 wait 400
shot copypaths-selected
input keydown 29 keydown 56 keydown 46 keyup 46 keyup 56 keyup 29 wait 800
GOT=$(qdbus6 org.kde.klipper /klipper getClipboardContents 2> /dev/null)
WANT=$(printf '%s\n%s\n%s' "$DIR/Folder one" "$DIR/a.txt" "$DIR/b c.pdf")
if [ "$(sort <<< "$GOT")" = "$(sort <<< "$WANT")" ]; then
    echo "PASS ctrl-alt-c-copies-every-selected-path" >> "$OUT"
else
    echo "FAIL ctrl-alt-c-copies-every-selected-path: $GOT" >> "$OUT"
fi
# A new file, so KWrite opens on an empty document and not on its welcome page.
rm -f "$SBX/run/pasted.txt"
kwrite "$SBX/run/pasted.txt" > "$SBX/logs/kwrite.log" 2>&1 &
KW=$!
waitfor 5
# Ctrl+V (29, 47).
input keydown 29 keydown 47 keyup 47 keyup 29 wait 800
shot copypaths-pasted
kill "$KW" 2> /dev/null
