#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Runs ShellCheck at its strictest level (style) on every shell script Aurora owns, with the settings in .shellcheckrc,
# and on the restore script installer/install.py writes into each backup. Left out: code that is not Aurora's (vendor/,
# upstream/, the adapted menu bar style's helper scripts), the sandbox's home and the saved backups. Prints
# PASS, or the findings and exits 1.
set -u
ROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
cd "$ROOT" || exit 1
command -v shellcheck > /dev/null || { echo "shellcheck is not installed (pacman -S shellcheck)"; exit 1; }

mapfile -t scripts < <({
    git ls-files '*.sh'
    # Scripts without the .sh suffix, known by their shebang.
    git ls-files | grep -v '\.' | while read -r file; do
        head -c 40 "$file" 2>/dev/null | grep -q '^#!.*\(bash\|sh\)$\|^#!.*\(bash\|sh\) ' && echo "$file"
    done
} | grep -v -e '/upstream/' -e '/vendor/' -e '^components/menu-bar-style/package/' -e '^tests/sandbox/home/' | sort -u)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
python3 -I -c 'import importlib.util, sys
spec = importlib.util.spec_from_file_location("install", "installer/install.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
open(sys.argv[1], "w").write(module.RESTORE)' "$work/restore.sh"

if shellcheck -S style "${scripts[@]}" "$work/restore.sh"; then
    echo "PASS ${#scripts[@]} scripts and the restore script have no ShellCheck findings"
else
    exit 1
fi
