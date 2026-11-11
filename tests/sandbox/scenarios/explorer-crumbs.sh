#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The path segments of File Explorer's address field and path bar: at rest, hovered, the chevron's folder menu, the
# ellipsis menu, a file dragged onto a segment (the accent target, the segment opening while the drag rests on it,
# KIO's Move, Copy or Link menu at the pointer, on a segment and on the sidebar), a drop refused on the folder the file
# is in, Ctrl+L for typing, Ctrl+click and a middle click for new tabs, and a click on the first segment. Captures shots/crumbs-*.png and writes PASS or FAIL lines to logs/crumbs.txt. The positions are those of
# File Explorer's first window at 2560x1440 on ~/Crumbs/Projects/Aurora/docs.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/crumbs.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DOCS="$HOME/Crumbs/Projects/Aurora/docs"
mkdir -p "$DOCS" "$HOME/Crumbs/Projects/Orion" "$HOME/Crumbs/Projects/Notes"
printf 'int main(void) { return 0; }\n' > "$DOCS/main.c"
printf '#!/bin/sh\necho hola\n' > "$DOCS/hola"; chmod +x "$DOCS/hola"

# check NAME BEFORE AFTER X0 Y0 X1 Y1: PASS when the region changed between the two captures.
check() {
    if python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1; then
        echo "PASS $1" >> "$OUT"
    else
        echo "FAIL $1" >> "$OUT"
    fi
}
# drag X0 Y0 X1 Y1 HOLD_MS: presses at the first point, glides to the second, holds there and lets go, in one fakeinput
# run in the background (KWin lets go of the button when the fakeinput client ends). Arrives about a second after it
# starts; the caller captures during the hold and waits for DRAG.
drag() {
    "$FI" move "$1" "$2" wait 300 down wait 200 glide "$(($1 + 30))" "$2" 6 wait 100 glide "$3" "$4" 30 wait "$5" up \
        > /dev/null 2>&1 &
    DRAG=$!
}
input() { "$FI" "$@" > /dev/null 2>&1; }

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DOCS" > "$SBX/logs/fe.log" 2>&1 &
waitfor 8
TOP_Y=309 BOTTOM_Y=1135
MAIN_X=900 MAIN_Y=454
shot crumbs-rest

input move 1060 "$TOP_Y" wait 100 move 1075 "$TOP_Y" wait 1500
shot crumbs-hover
check hover-brightens-the-segment crumbs-rest crumbs-hover 1060 300 1095 318

input move 1103 "$TOP_Y" wait 200 down wait 80 up wait 900
shot crumbs-folder-menu
check chevron-lists-folders crumbs-rest crumbs-folder-menu 1100 325 1300 400
input keydown 1 keyup 1 wait 500

input move 1035 "$TOP_Y" wait 200 down wait 80 up wait 900
shot crumbs-folded-menu
check ellipsis-lists-folded-segments crumbs-rest crumbs-folded-menu 1035 325 1200 380
input keydown 1 keyup 1 wait 500

input move 959 "$BOTTOM_Y" wait 200 down wait 80 up wait 900
shot crumbs-bottom-menu
input keydown 1 keyup 1 wait 500

# The drag rests on Aurora: first the segment takes the drop in the accent, then Aurora opens under the drag. Esc
# cancels the drag and Alt+Left goes back to docs.
drag "$MAIN_X" "$MAIN_Y" 1075 "$TOP_Y" 2600
sleep 1.1
shot crumbs-drop-target
check drop-target-in-accent crumbs-hover crumbs-drop-target 1055 298 1098 320
sleep 0.6
shot crumbs-spring
check holding-a-drag-opens-the-segment crumbs-drop-target crumbs-spring 830 440 1400 470
input keydown 1 keyup 1
wait "$DRAG"
input wait 600 keydown 56 keydown 105 keyup 105 keyup 56 wait 1200

# A quick drop on Crumbs in the path bar: KIO asks Move, Copy or Link, at the pointer; Esc cancels.
drag "$MAIN_X" "$MAIN_Y" 927 "$BOTTOM_Y" 250
wait "$DRAG"
waitfor 1
shot crumbs-drop-menu
check drop-menu-opens-at-the-pointer crumbs-rest crumbs-drop-menu 920 1145 1150 1200
input keydown 1 keyup 1 wait 600

# The same on the sidebar's Desktop row: the menu opens at the pointer there too.
drag "$MAIN_X" "$MAIN_Y" 634 400 250
wait "$DRAG"
waitfor 1
shot crumbs-sidebar-drop-menu
check sidebar-drop-menu-opens-at-the-pointer crumbs-rest crumbs-sidebar-drop-menu 640 405 880 560
input keydown 1 keyup 1 wait 600

# main.c is in docs already: the segment stays plain and the drag shows it is refused.
drag "$MAIN_X" "$MAIN_Y" 1153 "$BOTTOM_Y" 2200
sleep 1.4
shot crumbs-refused
input keydown 1 keyup 1
wait "$DRAG"
waitfor 1

input keydown 29 keydown 38 keyup 38 keyup 29 wait 700
shot crumbs-typing
input keydown 1 keyup 1 wait 500

# Ctrl+click opens a segment in a new tab, which brings the tab row.
input move 927 "$BOTTOM_Y" wait 200 keydown 29 wait 50 down wait 80 up wait 50 keyup 29 wait 1200
shot crumbs-new-tab
check ctrl-click-opens-a-new-tab crumbs-refused crumbs-new-tab 830 370 1990 430

# A middle click opens one too: a third tab joins the row.
input move 1007 "$BOTTOM_Y" wait 200 mdown wait 80 mup wait 1200
shot crumbs-middle-tab
# The second tab's label moves left to make room for the third.
check middle-click-opens-a-new-tab crumbs-new-tab crumbs-middle-tab 1180 388 1260 404

input move 855 "$BOTTOM_Y" wait 200 down wait 80 up wait 1200
shot crumbs-home
check first-segment-opens-home crumbs-middle-tab crumbs-home 820 400 1400 560
