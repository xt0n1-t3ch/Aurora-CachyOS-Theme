#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The menu bar names the active app: shots/app-title-none.png on the bare desktop, then
# shots/app-title-konsole.png with Konsole in front.
source "$SBX/prelude.sh"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
source "$SB/tests/helpers/showcase_wallpaper.sh"
waitfor 3
shot "app-title-none"
konsole > "$SBX/logs/konsole.log" 2>&1 &
waitfor 6
shot "app-title-konsole"
