#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# window_rect.sh TEXT: "x y width height" of the first KWin window whose title contains TEXT. KWin's window runner
# hands out window ids ("0_{uuid}"); getWindowInfo takes the uuid.
id=$(qdbus6 --literal org.kde.KWin /WindowsRunner org.kde.krunner1.Match "$1" 2>/dev/null | grep -o '"[0-9]*_{[^}]*}"' | head -1 | tr -d '"')
[ -z "$id" ] && exit 1
qdbus6 org.kde.KWin /KWin org.kde.KWin.getWindowInfo "${id#*_}" 2>/dev/null \
    | awk -F': ' '/^x:/{x=$2} /^y:/{y=$2} /^width:/{w=$2} /^height:/{h=$2} END{printf "%d %d %d %d\n", x, y, w, h}'
