#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The hand over KWin's window buttons: a decorated Konsole, the pointer on its title, then on its close button.
# logs/kwinhand.txt gets the loaded KWin plugins and the plugin's GGKWIN lines come from KWin's own log.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
OUT="$SBX/logs/kwinhand.txt"
echo "plugins: $(qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadedPlugins 2>&1 | tr '\n' ' ')" > "$OUT"
QT_QPA_PLATFORM=wayland konsole --separate > /dev/null 2>&1 &
waitfor 5
read -r X Y W H <<< "$("$SB/tests/helpers/window_rect.sh" "Konsole")"
echo "konsole at $X $Y $W $H" >> "$OUT"
"$FI" move $((X + W / 2)) $((Y + 14)) wait 300 move $((X + W / 2 + 4)) $((Y + 14)) wait 300 > /dev/null 2>&1
echo "title: done" >> "$OUT"
"$FI" move $((X + W - 24)) $((Y + 16)) wait 300 move $((X + W - 22)) $((Y + 16)) wait 300 > /dev/null 2>&1
echo "close button: done" >> "$OUT"
"$FI" move $((X + W / 2)) $((Y + 120)) wait 300 > /dev/null 2>&1
echo "client area: done" >> "$OUT"
