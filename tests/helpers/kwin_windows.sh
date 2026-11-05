#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Prints every KWin window (caption, class, role flags, geometry) through a one-shot KWin script; the lines
# go to KWin's own output with the prefix GGWIN.
f=$(mktemp --suffix=.js)
cat > "$f" <<'JS'
for (const w of workspace.windowList()) {
  print("GGWIN " + w.caption.substring(0, 50) + " | " + w.resourceClass + " | popup=" + w.popupWindow + " applet=" + w.appletPopup
        + " dock=" + w.dock + " hidden=" + w.hidden + " min=" + w.minimized + " opacity=" + w.opacity + " | " + JSON.stringify(w.frameGeometry));
}
JS
id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$f" gg_windows_$$)
qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run > /dev/null
sleep 0.5
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript gg_windows_$$ > /dev/null
rm -f "$f"
