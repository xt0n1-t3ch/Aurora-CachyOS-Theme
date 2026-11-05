# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Sourced inside a scenario: the background apps of a real desktop as tray icons with their real icons, in the order
# of that tray. Discord has a red dot and ZapFast a "9+" badge, as on the real tray.
# shellcheck shell=bash
export QT_QPA_PLATFORMTHEME=kde
# Each spec is NAME|ICON|BADGE, as fake-tray takes them; BADGE is optional.
for spec in "Discord|/usr/share/icons/hicolor/256x256/apps/discord.png|•" \
            "Easy Effects|com.github.wwmm.easyeffects-symbolic|" \
            "Limusic|/usr/share/icons/hicolor/64x64/apps/limusic-app.png|" \
            "Steam|/usr/share/pixmaps/steam_tray_mono.png|" \
            "Orion|$REALHOME/.local/share/icons/hicolor/scalable/apps/orion-app.svg|" \
            "Orion Helper|$REALHOME/.local/share/icons/hicolor/scalable/apps/orion-app.svg|" \
            "ZapFast|/usr/share/icons/hicolor/scalable/apps/zapfast.svg|9+" \
            "Cachy-Update|/usr/share/icons/hicolor/scalable/apps/cachy-update-blue.svg|"; do
  IFS='|' read -r name icon badge <<< "$spec"
  # An empty badge passes no argument.
  "$SB/tests/helpers/fake-tray/build/fake-tray" "$name" "$icon" ${badge:+"$badge"} > "$SBX/logs/fake-tray-${name// /_}.log" 2>&1 &
done
