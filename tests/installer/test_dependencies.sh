#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# installer/dependencies.txt and build-dependencies.txt name every package the build and the install use: each CMake package a configured
# build found (build/*/CMakeCache.txt) and each command the installer runs belongs to a listed package or to one
# that a listed package pulls in, so install.sh on a fresh CachyOS installs all of them. Run after installer/build.sh.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
listed=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$root/installer/dependencies.txt" "$root/installer/build-dependencies.txt" \
    | awk '{print $1}' | sort -u)
# What pacman installs for the list: the listed packages and everything they depend on, level by level.
installed=$listed
level=$listed
while [ -n "$level" ]; do
    # shellcheck disable=SC2086
    deps=$(LC_ALL=C pacman -Qi $level 2>/dev/null | sed -n 's/^Depends On *: //p' | tr ' ' '\n' |
           sed -e 's/[<>=].*//' -e '/^None$/d' -e '/^$/d' | sort -u)
    # Names pacman knows only as provides (sh, libgl) are left out: a real package of the closure owns their files.
    # shellcheck disable=SC2086
    level=$(comm -13 <(echo "$installed") <(pacman -Qq $deps 2>/dev/null | sort -u))
    installed=$(printf '%s\n%s\n' "$installed" "$level" | sed '/^$/d' | sort -u)
done
failures=0
# need WHAT PATH: prints FAIL unless one of the packages that own PATH is installed by the list.
need() {
    local owners
    owners=$(pacman -Qqo "$2" 2>/dev/null)
    if [ -z "$owners" ]; then
        echo "FAIL $1: no package owns $2"; failures=$((failures + 1))
    elif [ -z "$(comm -12 <(echo "$installed") <(echo "$owners" | sort -u))" ]; then
        echo "FAIL $1 comes from $(paste -sd' ' <<< "$owners"), which installer/dependencies.txt and build-dependencies.txt do not install"; failures=$((failures + 1))
    fi
}

caches=("$root"/build/*/CMakeCache.txt)
if [ ! -e "${caches[0]}" ]; then
    echo "FAIL no configured build in build/: run installer/build.sh first"
    exit 1
fi
# CMake config folders under /usr (the build's own trees and the vendored sources are not packages).
dirs=$(grep -hE '^[A-Za-z0-9_]*_DIR:PATH=/usr/.*/cmake(/.*)?$' "${caches[@]}" | sed 's/^[^=]*=//' | sort -u)
for dir in $dirs; do
    [ -d "$dir" ] && need "CMake package $(basename "$dir")" "$dir"
done
echo "checked $(wc -w <<< "$dirs") CMake packages"

commands="cmake ninja gcc git patch python3 kwriteconfig6 kreadconfig6 kbuildsycoca6 qdbus6 busctl systemctl dconf
fc-cache plasma-apply-colorscheme plasma-apply-desktoptheme plasma-apply-cursortheme dbus-send"
for command in $commands; do
    path=$(command -v "$command") || { echo "FAIL command $command is not installed"; failures=$((failures + 1)); continue; }
    need "command $command" "$(readlink -f "$path")"
done
for path in /usr/lib/plasma-changeicons /usr/lib/qt6/bin/qmltestrunner; do
    need "$path" "$path"
done
python3 -I -c 'import PIL' 2>/dev/null || { echo "FAIL python module PIL"; failures=$((failures + 1)); }
need "python module PIL" "$(python3 -I -c 'import PIL, os; print(os.path.dirname(PIL.__file__))')/__init__.py"
need "Kvantum" "/usr/bin/kvantummanager"
echo "checked $(wc -w <<< "$commands") commands"

# QML modules the installed QML imports at run time (the widgets, Plasma's parts in vendor/, the energy page). A module found in the system's QML folder must come from a package the list names itself,
# not one that a listed package happens to pull in today.
modules=$(python3 -I - "$root" <<'PY'
import glob, os, re, sys
root = sys.argv[1]
patterns = ["components/*/package/**/*.qml", "components/*/vendor/**/*.qml", "components/*/ui/**/*.qml",
            "components/*/qml/**/*.qml"]
found = set()
for pattern in patterns:
    for path in glob.glob(os.path.join(root, pattern), recursive=True):
        with open(path, errors="replace") as f:
            for line in f:
                match = re.match(r"\s*import\s+([A-Za-z][\w.]*)", line)
                if match:
                    found.add(match.group(1))
for uri in sorted(found):
    qmldir = os.path.join("/usr/lib/qt6/qml", *uri.split("."), "qmldir")
    if os.path.exists(qmldir):
        print(uri, qmldir)
PY
)
direct=$listed
while read -r uri qmldir; do
    [ -n "$uri" ] || continue
    owners=$(pacman -Qqo "$qmldir" 2>/dev/null | sort -u)
    if [ -z "$(comm -12 <(echo "$direct") <(echo "$owners"))" ]; then
        echo "FAIL QML module $uri comes from $(paste -sd' ' <<< "$owners"), which installer/dependencies.txt does not name"
        failures=$((failures + 1))
    fi
done <<< "$modules"
echo "checked $(grep -c . <<< "$modules") QML modules"
[ "$failures" -eq 0 ] && echo "PASS every package the build and the install use is listed"
exit "$failures"
