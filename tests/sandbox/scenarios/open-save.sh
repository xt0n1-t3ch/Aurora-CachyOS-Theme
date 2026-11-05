#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The FileChooser backend: an OpenFile call shows a File Explorer panel; double-clicking the first file answers
# its URI. A SaveFile call answers the typed name in the folder when Save is pressed (Return in the name field).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
TAG="${SCEN_TAG:-now}"
OUT="$SBX/logs/portal-$TAG.txt"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 10
( QT_FORCE_STDERR_LOGGING=1 "$SB/build/file-explorer/file-explorer" --portal > "$SBX/logs/portal-backend-$TAG.log" 2>&1; echo "backend exit $?" >> "$SBX/logs/portal-backend-$TAG.log" ) &
waitfor 2
# The folder as the NUL-terminated byte array that the portal's current_folder option takes.
folder=$(printf '%s' "$HOME/pick-test" | od -An -tx1 | tr -d ' \n' | sed 's/\(..\)/0x\1, /g')
gdbus call --session --dest org.freedesktop.impl.portal.desktop.aurora --object-path /org/freedesktop/portal/desktop \
    --method org.freedesktop.impl.portal.FileChooser.OpenFile /org/freedesktop/portal/desktop/request/1_1/open1 "test.app" "" "Open Image" \
    "{'filters': <[('Images', [(uint32 0, '*.png')]), ('All files', [(uint32 0, '*')])]>, 'current_folder': <[byte ${folder}0x00]>}" > "$OUT.open" 2>&1 &
waitfor 4
shot "portal-$TAG-open"
read -r X Y W H <<< "$("$SB/tests/helpers/window_rect.sh" "Open Image")"
echo "window $X $Y $W $H" > "$OUT"
qdbus6 org.kde.KWin /WindowsRunner org.kde.krunner1.Match "Open Image" >> "$OUT" 2>&1
# Double-clicks the first file in the panel.
"$FI" move $((X + 340)) $((Y + 98)) wait 300 down wait 40 up wait 80 down wait 40 up wait 500 > /dev/null 2>&1
waitfor 2
echo "open answer: $(cat "$OUT.open")" >> "$OUT"
gdbus call --session --dest org.freedesktop.impl.portal.desktop.aurora --object-path /org/freedesktop/portal/desktop \
    --method org.freedesktop.impl.portal.FileChooser.SaveFile /org/freedesktop/portal/desktop/request/1_1/save1 "test.app" "" "Save Screenshot" \
    "{'filters': <[('PNG image', [(uint32 0, '*.png')])]>, 'current_name': <'Screenshot'>, 'current_folder': <[byte ${folder}0x00]>}" > "$OUT.save" 2>&1 &
waitfor 4
shot "portal-$TAG-save"
read -r X Y W H <<< "$("$SB/tests/helpers/window_rect.sh" "Save Screenshot")"
echo "save window $X $Y $W $H" >> "$OUT"
"$FI" move $((X + 1222)) $((Y + 770)) wait 300 down wait 60 up wait 500 > /dev/null 2>&1
waitfor 2
echo "save answer: $(cat "$OUT.save")" >> "$OUT"
