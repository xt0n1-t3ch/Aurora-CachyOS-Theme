#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Each Plasma part's patch, applied to a fresh copy of upstream/ (plus the binary shader it cannot carry), gives
# vendor/ exactly: so building a newer Plasma from its own sources with the same patch gives the same part.
set -u
root=$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
for part in system-tray notifications quick-menu; do
    case $part in
        system-tray) up=upstream; ven=vendor/plasma-workspace/applets/systemtray ;;
        notifications) up=upstream; ven=vendor/plasma-workspace/applets/notifications ;;
        quick-menu) up=upstream/krunner; ven=vendor/krunner ;;
    esac
    dir="$root/components/$part"
    cp -a "$dir/$up" "$work/$part"
    if ! patch -d "$work/$part" -p1 --quiet < "$dir/$part.patch"; then
        echo "FAIL $part: the patch does not apply to upstream/"; failures=$((failures + 1)); continue
    fi
    (cd "$dir/$ven" && find . -name '*.qsb') | while read -r shader; do install -Dm644 "$dir/$ven/$shader" "$work/$part/$shader"; done
    # Files only vendor/ has (the part's unpatched sources outside upstream/) are not the patch's business.
    differences=$(diff -r "$work/$part" "$dir/$ven" | grep -v "^Only in $dir/$ven")
    if [ -z "$differences" ]; then
        echo "PASS $part: the patch on upstream/ gives vendor/"
    else
        echo "FAIL $part:"; echo "$differences" | head -10; failures=$((failures + 1))
    fi
done
exit "$failures"
