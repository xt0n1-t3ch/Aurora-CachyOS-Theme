#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Notification banners: popups at the bottom right, one capture per kind of notification, and each banner window's
# geometry from KWin (GGNOTIF lines in KWin's own output) to check the corner and spacing.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
kwriteconfig6 --file "$HOME/.config/plasmanotifyrc" --group Notifications --key PopupPosition BottomRight
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
OUT="$SBX/logs/notif.txt"; : > "$OUT"
# geometry N: runs a one-shot KWin script that prints the geometry of every notification window; N keeps the
# script names apart.
geometry() {
    cat > "$SBX/run/kw_notif.js" <<'JS'
for (const w of workspace.windowList()) {
  if (w.notification || w.criticalNotification) print("GGNOTIF " + w.frameGeometry.x + " " + w.frameGeometry.y + " " + w.frameGeometry.width + " " + w.frameGeometry.height);
}
JS
    local id; id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$SBX/run/kw_notif.js" "ggn$1")
    qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null; sleep 0.5
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "ggn$1" > /dev/null
}
notify-send -t 20000 -a "Messages" -i internet-chat "Ana García" "¿Nos vemos a las 8 en el café de siempre?"
waitfor 2; shot notif-1-message; geometry 1
notify-send -t 20000 -a "Aurora" "" "Una notificación sin título muestra el nombre de la app como título, igual que macOS."
waitfor 2; shot notif-2-stack; geometry 2
notify-send -t 20000 -a "System Settings" -i preferences-system "Actualización lista" "Plasma 6.8.3 está listo para instalarse." -A "later=Más tarde" -A "now=Instalar" &
waitfor 2; shot notif-2b-actions
notify-send -t 20000 -a "Mail" -i mail-message "Pedido enviado" "Tu pedido #48213 ya salió del almacén. Llega mañana entre las 9:00 y las 13:00. Puedes seguirlo desde la app o desde el correo que te enviamos con todos los detalles del envío y la factura."
waitfor 2; shot notif-3-long; geometry 3
notify-send -t 20000 -a "org.kde.konsole" -i utilities-terminal "Compilación terminada" "cargo build terminó en 42 s."
waitfor 1
"$FI" move 2350 1300 wait 200 move 2360 1310 wait 700 > /dev/null 2>&1
shot notif-4-hover
geometry 4
