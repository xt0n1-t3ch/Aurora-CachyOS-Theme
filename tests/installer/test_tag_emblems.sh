#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Checks themes/icons/tag_emblems.py: it writes the 63 emblems File Explorer names, registers their folder once, and
# draws Finder's colors exactly as File Explorer paints them (src/core/tags.cpp).
#
#   tests/installer/test_tag_emblems.sh
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
pass=0
fail=0
# Prints PASS or FAIL for a check. Arguments: the check's name, then the command that must succeed.
check() {
    local name=$1
    shift
    if "$@"; then
        echo "PASS: $name"
        pass=$((pass + 1))
    else
        echo "FAIL: $name"
        fail=$((fail + 1))
    fi
}

theme=$(mktemp -d)
trap 'rm -rf "$theme"' EXIT
printf '[Icon Theme]\nName=Test\nDirectories=apps/scalable\n' > "$theme/index.theme"

python3 -I "$ROOT/themes/icons/tag_emblems.py" "$theme" > /dev/null
python3 -I "$ROOT/themes/icons/tag_emblems.py" "$theme" > /dev/null

check "it writes one emblem per set of one to three colors" test "$(find "$theme/emblems/scalable" -name "*.svg" | wc -l)" -eq 63
check "the emblem of a red and blue file is there" test -f "$theme/emblems/scalable/emblem-aurora-tags-red-blue.svg"
check "a second run lists the emblems folder once" test "$(grep -c '^\[emblems/scalable\]' "$theme/index.theme")" -eq 1
check "the folder is in the theme's Directories" grep -q '^Directories=emblems/scalable,apps/scalable$' "$theme/index.theme"

# The script's colors, as "Name r g b" lines, against the QColor table of src/core/tags.cpp.
script=$(python3 -I -c '
import sys; sys.path.insert(0, sys.argv[1])
import tag_emblems
for name, color in tag_emblems.COLORS:
    print(name, int(color[1:3], 16), int(color[3:5], 16), int(color[5:7], 16))' "$ROOT/themes/icons")
app=$(grep -oE 'QStringLiteral\("[A-Za-z]+"\), QColor\([0-9]+, [0-9]+, [0-9]+\)' "$ROOT/components/file-explorer/src/core/tags.cpp" \
    | sed -E 's/QStringLiteral\("([A-Za-z]+)"\), QColor\(([0-9]+), ([0-9]+), ([0-9]+)\)/\1 \2 \3 \4/')
check "the emblems use File Explorer's colors in its order" test "$script" = "$app"

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
