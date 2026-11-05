#!/bin/sh
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Starts Quick Menu in place of KRunner (plasma-krunner.service runs this through the drop-in
# ~/.config/systemd/user/plasma-krunner.service.d/aurora.conf). Quick Menu is Plasma's own KRunner built from
# the installed plasma-workspace with the Quick Menu look, so it runs only while it was built for the installed
# plasma-workspace and Qt; after an update that changes either, the stock KRunner starts until
# ~/.local/share/aurora/rebuild.sh builds Quick Menu again.
# Usage: krunner-launch.sh [KRUNNER_ARGS...]; the arguments go to whichever KRunner starts.
# Environment (for tests/test_launch.sh): QUICK_MENU_BIN, STOCK_KRUNNER and QUICK_MENU_STAMP replace the three paths
# below. The stamp holds the plasma-workspace and qt6-base versions Quick Menu was built for, which installer/build.sh
# writes and the install copies.
quick_menu=${QUICK_MENU_BIN:-$HOME/.local/lib/aurora/quick-menu/krunner}
stock=${STOCK_KRUNNER:-/usr/bin/krunner}
stamp=${QUICK_MENU_STAMP:-$HOME/.local/share/aurora/quick-menu-built-for}

# version PACKAGE: prints the installed version of the package without its pkgrel, or nothing when it is missing.
version() {
    pacman -Q "$1" 2>/dev/null | awk '{print $2}' | sed 's/-[^-]*$//'
}

# Anything but a matching stamp and an existing binary starts the stock KRunner, so a Plasma or Qt update never leaves
# the desktop without search.
if [ -x "$quick_menu" ] && [ "$(version plasma-workspace) $(version qt6-base)" = "$(cat "$stamp" 2>/dev/null)" ]; then
    exec "$quick_menu" "$@"
fi
exec "$stock" "$@"
