#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The sidebar's rows move by dragging: Music, dragged onto the top half of Recents, becomes the first favorite, and
# File Explorer shows the same order after a restart. Captures shots/sidebar-order-{before,after,restart}.png and
# writes the saved order and PASS or FAIL lines to logs/sidebar-order.txt. The row positions are those of File Explorer's first window at
# 2560x1440 with the sandbox's places. The saved order is removed again at the end.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/sidebar-order.txt"; : > "$OUT"
kwriteconfig6 --file file-explorerrc --group "Sidebar Order" --key Favorites --delete
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$HOME" > "$SBX/logs/fe.log" 2>&1 &
FE=$!
waitfor 8
shot sidebar-order-before
# Music's row is centered at y 512 and Recents' at 372; the drop goes into the top half of Recents.
MUSIC_X=706 MUSIC_Y=512 RECENTS_Y=366
"$FI" move "$MUSIC_X" "$MUSIC_Y" wait 300 down wait 200 glide "$MUSIC_X" "$((MUSIC_Y - 30))" 6 wait 100 \
      glide "$MUSIC_X" "$RECENTS_Y" 24 wait 400 up wait 600 > /dev/null 2>&1
waitfor 1
shot sidebar-order-after
ORDER=$(kreadconfig6 --file file-explorerrc --group "Sidebar Order" --key Favorites)
echo "order: $ORDER" >> "$OUT"
case "$ORDER" in
    "file://$HOME/Music,"*) echo "PASS music-becomes-the-first-favorite" >> "$OUT" ;;
    *) echo "FAIL music-becomes-the-first-favorite" >> "$OUT" ;;
esac
kill "$FE"
waitfor 2
"$SB/build/file-explorer/file-explorer" "$HOME" > "$SBX/logs/fe-restart.log" 2>&1 &
waitfor 7
shot sidebar-order-restart
# The first row read Recents before and reads Music after the restart.
if python3 -I "$SB/tests/helpers/region_changed.py" "$SBX/shots/sidebar-order-before.png" "$SBX/shots/sidebar-order-restart.png" \
    590 360 790 386 > /dev/null 2>&1; then
    echo "PASS the-order-survives-a-restart" >> "$OUT"
else
    echo "FAIL the-order-survives-a-restart" >> "$OUT"
fi
kwriteconfig6 --file file-explorerrc --group "Sidebar Order" --key Favorites --delete
