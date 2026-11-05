#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# A GTK app (GNOME Text Editor) and Konsole side by side, to compare their window buttons: shots/gtk-and-konsole.png.
source "$SBX/prelude.sh"
/usr/lib/xdg-desktop-portal-kde > "$SBX/logs/portal-kde.log" 2>&1 &
/usr/lib/xdg-desktop-portal > "$SBX/logs/portal.log" 2>&1 &
plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/tray_apps.sh"
printf '# Aurora\n\nNotas de prueba: la misma cápsula de botones en todas las apps.\n' > "$HOME/Documents/Aurora.md"
gnome-text-editor "$HOME/Documents/Aurora.md" > "$SBX/logs/text-editor.log" 2>&1 &
waitfor 6
konsole --hold -e fastfetch > "$SBX/logs/konsole.log" 2>&1 &
waitfor 6
ID=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SB/tests/helpers/place_windows.js" place-windows)
qdbus6 org.kde.KWin "/Scripting/Script$ID" org.kde.kwin.Script.run > "$SBX/logs/place.log" 2>&1
waitfor 3
shot gtk-and-konsole
