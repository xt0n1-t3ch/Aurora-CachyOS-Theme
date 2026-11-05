#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# installer/build.sh builds Plasma's own parts (system tray, notifications, Quick Menu) from the copy in vendor/ when
# that copy is the installed plasma-workspace's version, and from that version's own sources otherwise, so a
# machine on another Plasma gets parts built for its own. A fake pacman picks the installed version.
set -u
here=$(dirname "$(readlink -f "$0")")
build="$here/../../installer/build.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/bin"
failures=0
# fake_plasma VERSION: puts a pacman on PATH that reports VERSION as the installed plasma-workspace.
fake_plasma() {
    # shellcheck disable=SC2016 # the fake pacman expands its own $2
    printf '#!/bin/sh\n[ "$2" = plasma-workspace ] && echo "plasma-workspace %s"\n' "$1" > "$work/bin/pacman"
    chmod +x "$work/bin/pacman"
}
# expect NAME WANT: prints PASS or FAIL for whether "build.sh --plan" prints WANT; a FAIL counts as a failure.
expect() {
    local got
    got=$(PATH="$work/bin:$PATH" "$build" --plan)
    if [ "$got" = "$2" ]; then echo "PASS $1"; else echo "FAIL $1: got '$got', want '$2'"; failures=$((failures + 1)); fi
}

fake_plasma 6.8.2-1.1
expect "the vendored version is installed: every part builds from vendor/" \
"system-tray vendor 6.8.2
notifications vendor 6.8.2
quick-menu vendor 6.8.2"

fake_plasma 6.9.0-1
expect "a newer Plasma: every part builds from that version's sources" \
"system-tray fetch 6.9.0
notifications fetch 6.9.0
quick-menu fetch 6.9.0"

fake_plasma 6.8.2-3
expect "only the package release differs: vendor/ still matches" \
"system-tray vendor 6.8.2
notifications vendor 6.8.2
quick-menu vendor 6.8.2"

# The aurora command builds with the parts when Rust is installed, and says so when it is not.
root=$(readlink -f "$here/../..")
if command -v cargo > /dev/null; then
    BUILD_DIR="$work/build" "$build" cli > "$work/cli.log" 2>&1
    got=$("$work/build/cli/aurora" --version 2>&1)
    want="aurora $(cat "$root/VERSION")"
    if [ "$got" = "$want" ]; then echo "PASS the aurora command is built into build/cli"; else echo "FAIL the aurora command: got '$got', want '$want' ($(tail -3 "$work/cli.log"))"; failures=$((failures + 1)); fi
fi
# A PATH that holds only the tools build.sh needs, so cargo is not found.
# A copy without cli/ (the aurora-theme package's, which ships its own /usr/bin/aurora) skips the part, wherever it lives.
mkdir -p "$work/packaged/installer"
cp "$root/installer/build.sh" "$work/packaged/installer/"
got=$(BUILD_DIR="$work/build3" "$work/packaged/installer/build.sh" cli 2>&1); code=$?
if [ "$code" = 0 ] && [ "$(head -1 <<< "$got")" = "cli: comes with the package" ]; then
    echo "PASS a copy without cli/ leaves the aurora command to the package"
else
    echo "FAIL a copy without cli/: exit $code, '$got'"; failures=$((failures + 1))
fi
mkdir -p "$work/nocargo"
for tool in bash sh cat dirname readlink rm mkdir echo; do ln -sf "$(command -v "$tool")" "$work/nocargo/$tool"; done
got=$(PATH="$work/nocargo" BUILD_DIR="$work/build2" "$build" cli 2>&1 | head -1)
if [ "$got" = "cli: skipped, cargo is not installed (install.sh works without the aurora command)" ]; then
    echo "PASS without Rust the aurora command is skipped and the build goes on"
else
    echo "FAIL without Rust: got '$got'"; failures=$((failures + 1))
fi

# --current tells whether the parts in BUILD_DIR (the package's prebuilt ones) were built for this system: every part
# has a stamp, and each stamp is what a build here would write.
# fake_system PLASMA QT KDECORATION KWIN: a pacman that reports those versions.
fake_system() {
    cat > "$work/bin/pacman" <<FAKE
#!/bin/sh
case "\$2" in
    plasma-workspace) echo "plasma-workspace $1" ;;
    qt6-base) echo "qt6-base $2" ;;
    kdecoration) echo "kdecoration $3" ;;
    kwin) echo "kwin $4" ;;
esac
FAKE
    chmod +x "$work/bin/pacman"
}
prebuilt="$work/prebuilt"
for part in system-tray notifications quick-menu; do mkdir -p "$prebuilt/$part"; echo "6.8.2 6.12.0" > "$prebuilt/$part/built-for"; done
for part in file-explorer glass energy-page; do mkdir -p "$prebuilt/$part"; echo "6.12.0" > "$prebuilt/$part/built-for"; done
mkdir -p "$prebuilt/window-buttons" "$prebuilt/pointing-hand"
echo "6.8" > "$prebuilt/window-buttons/built-for"
echo "6.8.2" > "$prebuilt/pointing-hand/built-for"
# current NAME WANT_CODE WANT_LAST_LINE: runs build.sh --current on the prebuilt parts.
current() {
    local got code
    got=$(PATH="$work/bin:$PATH" BUILD_DIR="$prebuilt" "$build" --current 2>&1); code=$?
    if [ "$code" = "$2" ] && [ "$(tail -1 <<< "$got")" = "$3" ]; then echo "PASS $1"
    else echo "FAIL $1: exit $code, '$got'"; failures=$((failures + 1)); fi
}
fake_system 6.8.2-1.1 6.12.0-2 6.8.2-1 6.8.2-1
current "parts built for the installed Plasma, KWin and Qt are current" 0 "every part is built for this system"
fake_system 6.8.2-1.1 6.12.0-2 6.8.2-1 6.8.3-1
current "a newer KWin leaves only the part built against KWin behind" 1 "built for another system: pointing-hand"
fake_system 6.9.0-1 6.12.0-2 6.9.0-1 6.9.0-1
current "a newer Plasma leaves Plasma's parts, the window buttons and the pointing hand behind" 1 \
    "built for another system: window-buttons pointing-hand system-tray notifications quick-menu"
rm "$prebuilt/glass/built-for"
fake_system 6.8.2-1.1 6.12.0-2 6.8.2-1 6.8.2-1
current "a part without its stamp is not current" 1 "built for another system: glass"

exit "$failures"
