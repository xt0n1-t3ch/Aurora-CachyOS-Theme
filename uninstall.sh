#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Aurora: puts the desktop back the way it was before ./install.sh, from the backup it made.
set -euo pipefail
case "${1:-}" in
    -h|--help)
        cat <<'EOF'
Usage: ./uninstall.sh

Removes everything the theme installed and puts back the settings ./install.sh saved before it changed anything:
colors, icons, fonts, panels, shortcuts, window buttons, Quick Menu, File Explorer as the default file manager, and
the optional performance tuning. sudo asks for your password to remove the links in the system plugin folder.
The backup itself stays in ~/.local/state/aurora/backups.
EOF
        exit 0 ;;
    "") ;;
    *) echo "unknown option: $1 (see ./uninstall.sh --help)" >&2; exit 2 ;;
esac
# install.py writes the path of the backup folder here; that folder's restore.sh undoes the install.
latest="$HOME/.local/state/aurora/latest-backup"
if [ ! -f "$latest" ]; then
    echo "No backup found at $latest: the theme was not installed with ./install.sh on this account." >&2
    exit 1
fi
exec "$(cat "$latest")/restore.sh"
