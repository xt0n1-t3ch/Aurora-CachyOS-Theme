#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's filter row: Ctrl+Alt+F opens it, each key typed hides the items whose names lack the text, Escape
# shows them all again. Captures shots/filter-{before,typed,none,closed}.png and writes PASS or FAIL lines to
# logs/filter.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/filter.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DIR="$HOME/Filter"
rm -rf "$DIR" && mkdir -p "$DIR/Projects" "$DIR/Music" "$DIR/Reports archive"
for name in "Report 2026.pdf" "report-draft.odt" "beach.jpg" "holiday.png" "notes.txt" "main.c" "budget.ods" \
            "quarterly report.xlsx"; do
    printf 'x\n' > "$DIR/$name"
done
# On the sandbox's 2560x1440 screen: the last rows of the unfiltered list, and the rows the filter keeps.
LAST_ROWS=(820 900 1400 1010)
FIRST_ROWS=(820 460 1400 700)
VIEW=(810 420 1990 1130)

input() { "$FI" "$@" > /dev/null 2>&1; }
type_word() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44)
    local word=$1 i
    for ((i = 0; i < ${#word}; i++)); do input keydown "${code[${word:i:1}]}" keyup "${code[${word:i:1}]}" wait 60; done
}
# text SHOT X0 Y0 X1 Y1: prints how many pixels of the region are bright enough to be item text.
text() {
    python3 -I -c 'import sys; from PIL import Image
im = Image.open(sys.argv[1]).convert("L").crop(tuple(int(v) for v in sys.argv[2:6]))
print(sum(1 for p in im.getdata() if p > 170))' "$SHOTS/$1.png" "$2" "$3" "$4" "$5"
}
# emptied NAME BEFORE AFTER X0 Y0 X1 Y1: PASS when the region held text in BEFORE and holds none in AFTER.
emptied() {
    local had now
    had=$(text "$2" "$4" "$5" "$6" "$7"); now=$(text "$3" "$4" "$5" "$6" "$7")
    [ "$had" -gt 200 ] && [ "$now" -lt 20 ] && echo "PASS $1" >> "$OUT" || echo "FAIL $1 ($had then $now)" >> "$OUT"
}
# check NAME BEFORE AFTER X0 Y0 X1 Y1 [same]: PASS when the region changed (or, with "same", when it did not).
check() {
    local changed=1
    python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1 && changed=0
    if [ "${8:-}" = same ]; then changed=$((1 - changed)); fi
    [ "$changed" = 0 ] && echo "PASS $1" >> "$OUT" || echo "FAIL $1" >> "$OUT"
}

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DIR" > "$SBX/logs/fe-filter.log" 2>&1 &
waitfor 8
shot filter-before
# Ctrl+Alt+F (29 Ctrl, 56 Alt, 33 F), then "report".
input keydown 29 keydown 56 keydown 33 keyup 33 keyup 56 keyup 29 wait 500
type_word report
input wait 600
shot filter-typed
emptied filter-hides-what-does-not-match filter-before filter-typed "${LAST_ROWS[@]}"
type_word zz
input wait 600
shot filter-none
emptied no-match-empties-the-view filter-typed filter-none "${FIRST_ROWS[@]}"
# Escape closes the row and every item comes back.
input keydown 1 keyup 1 wait 800
shot filter-closed
check escape-shows-everything-again filter-before filter-closed "${VIEW[@]}" same
