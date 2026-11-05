#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The menu bar logo: each of Aurora's logos in the menu bar (shots/menu-logo-<icon>.png), then the menu's settings
# page with the logo picker (shots/menu-logos-settings.png). CONFIG_AT is where "Configure Kpple Menu…" sits in the
# menu's right-click menu at scale 1 (read off shots/menu-logos-context.png, which the scenario also captures).
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
# set_logo ICON: writes ICON as the icon setting of every menu widget, through plasmashell's scripting.
set_logo() {
    qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "
var ps = panels();
for (var i = 0; i < ps.length; i++) { var ws = ps[i].widgets('org.aurora.logomenu');
  for (var j = 0; j < ws.length; j++) { ws[j].currentConfigGroup = ['Advanced']; ws[j].writeConfig('icon', '$1'); } }" \
        >> "$SBX/logs/menu-logos.txt" 2>&1
}
for icon in aurora-logo-cachyos-symbolic aurora-logo-cachyos aurora-logo-arch aurora-logo-arch-symbolic \
            aurora-logo-apple-symbolic aurora-logo-kali-symbolic; do
    set_logo "$icon"
    waitfor 2
    shot "menu-logo-$icon"
done
set_logo aurora-logo-cachyos-symbolic
waitfor 2
"$FI" move 16 14 wait 300 rdown wait 60 rup wait 600 > /dev/null 2>&1
waitfor 2
shot menu-logos-context
CONFIG_AT="130 57"
read -r cx cy <<< "$CONFIG_AT"
"$FI" move "$cx" "$cy" wait 300 down wait 60 up > /dev/null 2>&1
waitfor 5
shot menu-logos-settings
