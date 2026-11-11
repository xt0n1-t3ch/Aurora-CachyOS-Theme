#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds every component into build/<part>, for the Plasma, KWin and Qt installed on this machine.
#
#   installer/build.sh [PART...]    build all parts, or the ones named
#   installer/build.sh --plan       print where each Plasma part's sources come from, build nothing
#   installer/build.sh --current    exit 0 when every part in BUILD_DIR was built for this system (the package's
#                                   prebuilt parts), else name the ones built for another; build nothing
#
# The system tray, the notifications and Quick Menu are Plasma's own code with a patch. They build from vendor/
# when vendor/ holds the installed plasma-workspace version; otherwise from that version's sources (fetched from
# KDE with git) with the same patch. A part whose patch no longer applies is skipped, and Plasma keeps its stock
# one. Each built part gets build/<part>/built-for, the versions it was built against, which the installer copies
# next to it; the login script and Quick Menu's launcher compare it with the installed packages.
set -euo pipefail
ROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
BUILD=${BUILD_DIR:-$ROOT/build}
PLASMA_GIT=${PLASMA_GIT:-https://invent.kde.org/plasma/plasma-workspace.git}
# Parts built from Aurora's own sources, and parts built from Plasma's code plus a patch.
OWN_PARTS="file-explorer window-buttons glass energy-page pointing-hand"
PLASMA_PARTS="system-tray notifications quick-menu"

# Versions without the package release ("6.8.2-1.1" is 6.8.2), the way the login script compares them.
version_of() {
    pacman -Q "$1" 2>/dev/null | awk '{print $2}' | sed 's/-[^-]*$//'
}
# Prints the plasma-workspace version that components/PART/vendor/ holds, read from the part's CMakeLists.txt.
vendored_version() {
    sed -n 's/^project([a-z-]* VERSION \([0-9.]*\).*/\1/p' "$ROOT/components/$1/CMakeLists.txt"
}
# Prints where PART's sources come from: "vendor VERSION" when vendor/ holds the installed plasma-workspace,
# "fetch VERSION" otherwise.
source_of() {
    local installed
    installed=$(version_of plasma-workspace)
    if [ "$installed" = "$(vendored_version "$1")" ]; then echo "vendor $installed"; else echo "fetch $installed"; fi
}

if [ "${1:-}" = "--plan" ]; then
    for part in $PLASMA_PARTS; do echo "$part $(source_of "$part")"; done
    exit 0
fi

# Prints the plasma-workspace paths PART needs, for the sparse checkout of fetch_sources.
sparse_paths() {
    case "$1" in
        system-tray) echo "/applets/systemtray/ /libdbusmenuqt/ /statusnotifierwatcher/systemtraytypedefs.h /libkmpris/dbus/org.freedesktop.DBus.Properties.xml" ;;
        notifications) echo "/applets/notifications/" ;;
        quick-menu) echo "/krunner/" ;;
    esac
}
# Prints the plasma-workspace folder that PART's patch applies to.
patch_dir() {
    case "$1" in
        system-tray) echo applets/systemtray ;;
        notifications) echo applets/notifications ;;
        quick-menu) echo krunner ;;
    esac
}
# Prints the folder that holds the plasma-workspace layout inside PART's vendor/. The binary shaders sit there.
vendor_root() {
    case "$1" in
        quick-menu) echo "$ROOT/components/quick-menu/vendor" ;;
        *) echo "$ROOT/components/$1/vendor/plasma-workspace" ;;
    esac
}

# Plasma's sources of VERSION for PART, patched, in OUT. Fails when the download fails or the patch does not
# apply. Every step checks its result: callers use this in a condition, where set -e does not stop.
fetch_sources() {
    local part=$1 version=$2 out=$3 shader
    rm -rf "$out"
    git clone --quiet --depth 1 --branch "v$version" --filter=blob:none --sparse "$PLASMA_GIT" "$out" || return 1
    # shellcheck disable=SC2046 # one argument per path
    git -C "$out" sparse-checkout set --no-cone $(sparse_paths "$part") || return 1
    patch -d "$out/$(patch_dir "$part")" -p1 --dry-run --quiet < "$ROOT/components/$part/$part.patch" || return 1
    patch -d "$out/$(patch_dir "$part")" -p1 --quiet < "$ROOT/components/$part/$part.patch" || return 1
    # The glass shader is a binary file, so the patch cannot carry it.
    for shader in $(cd "$(vendor_root "$part")" && find . -name '*.qsb'); do
        install -Dm644 "$(vendor_root "$part")/$shader" "$out/$shader" || return 1
    done
}

# Configures and builds PART from SOURCE_DIR into $BUILD/PART: compile PART SOURCE_DIR [CMAKE_ARGS...]. The logs go
# to $BUILD/PART.configure.log and $BUILD/PART.build.log. Returns 1 when either step fails. The build runs at nice 10
# so the desktop stays responsive.
compile() {
    local part=$1 source=$2
    shift 2
    # A compiler cache makes a build of unchanged sources a cache read: ccache, or sccache (which Rust setups often
    # have already). Paths under the copy count from its root, so another copy or build folder of the same sources (a
    # package build, a worktree) reads the same cache; the debug info then names the first folder, which the package
    # strips anyway. sccache's daemon reads SCCACHE_BASEDIRS only when it starts.
    local launcher=() cache
    for cache in ccache sccache; do
        if command -v "$cache" > /dev/null; then launcher=(-DCMAKE_CXX_COMPILER_LAUNCHER="$cache"); break; fi
    done
    export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$ROOT}" CCACHE_NOHASHDIR="${CCACHE_NOHASHDIR:-1}"
    export SCCACHE_BASEDIRS="${SCCACHE_BASEDIRS:-$ROOT}"
    mkdir -p "$BUILD"
    cmake -S "$source" -B "$BUILD/$part" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -Wno-dev "${launcher[@]}" "$@" \
        > "$BUILD/$part.configure.log" 2>&1 || { echo "  configure failed, see build/$part.configure.log"; return 1; }
    nice -n 10 cmake --build "$BUILD/$part" > "$BUILD/$part.build.log" 2>&1 \
        || { echo "  build failed, see build/$part.build.log"; return 1; }
}

# Prints the stamp a build of PART here writes: the versions of what it links against, as the login script compares
# them. KDecoration's minor version is enough for the window buttons; Plasma's parts follow plasma-workspace and Qt.
stamp_of() {
    case "$1" in
        system-tray|notifications|quick-menu) echo "$(version_of plasma-workspace) $(version_of qt6-base)" ;;
        window-buttons) version_of kdecoration | cut -d. -f1,2 ;;
        pointing-hand) version_of kwin ;;
        file-explorer|glass|energy-page) version_of qt6-base ;;
    esac
}

if [ "${1:-}" = "--current" ]; then
    stale=""
    for part in $OWN_PARTS $PLASMA_PARTS; do
        [ "$(cat "$BUILD/$part/built-for" 2>/dev/null)" = "$(stamp_of "$part")" ] || stale="$stale $part"
    done
    if [ -n "$stale" ]; then
        echo "built for another system:$stale"
        exit 1
    fi
    echo "every part is built for this system"
    exit 0
fi

# Builds one part and writes $BUILD/PART/built-for, the versions it was built against. Returns 1 when the build
# fails and 2 for an unknown part name.
build_part() {
    local part=$1 how version
    rm -f "$BUILD/$part/built-for"
    case "$part" in
        system-tray|notifications|quick-menu)
            read -r how version <<< "$(source_of "$part")"
            if [ "$how" = vendor ]; then
                echo "$part: Plasma $version, from vendor/"
                compile "$part" "$ROOT/components/$part" -DBUILD_TESTING=OFF || return 1
            else
                echo "$part: Plasma $version, from KDE's sources of that version"
                fetch_sources "$part" "$version" "$BUILD/$part-sources" \
                    || { echo "  the patch does not apply to Plasma $version: Plasma keeps its own $part"; return 1; }
                compile "$part" "$ROOT/components/$part" -DBUILD_TESTING=OFF -DPLASMA_SOURCES="$BUILD/$part-sources" || return 1
            fi ;;
        window-buttons)
            echo "$part: KDecoration $(version_of kdecoration)"
            compile "$part" "$ROOT/components/$part" || return 1 ;;
        pointing-hand)
            echo "$part: KWin $(version_of kwin)"
            compile "$part" "$ROOT/components/$part" || return 1 ;;
        cli)
            # The aurora command (cli/, Rust). Optional: install.sh works without it. The AUR package's copy has no
            # cli/, because the package ships its own /usr/bin/aurora. It has no built-for stamp because it does not
            # depend on Plasma.
            if [ ! -d "$ROOT/cli" ]; then echo "cli: comes with the package"; return 0; fi
            if ! command -v cargo > /dev/null; then
                echo "cli: skipped, cargo is not installed (install.sh works without the aurora command)"; return 0
            fi
            echo "cli: the aurora command"
            mkdir -p "$BUILD"
            local exe
            # cargo's own target folder (the user's config may share one between projects); the JSON names the binary.
            exe=$(cd "$ROOT/cli" && nice -n 10 cargo build --release --locked --message-format=json-render-diagnostics \
                      2> "$BUILD/cli.build.log" | python3 -I -c '
import json, sys
for line in sys.stdin:
    message = json.loads(line)
    if message.get("reason") == "compiler-artifact" and message.get("executable"):
        print(message["executable"])' | tail -1) || true
            [ -n "$exe" ] && [ -x "$exe" ] || { echo "  build failed, see build/cli.build.log"; return 1; }
            install -Dm755 "$exe" "$BUILD/cli/aurora"
            return 0 ;;
        search)
            # aurora-search, File Explorer's whole-disk search helper (Rust). Optional like cli: without it File
            # Explorer searches below the folder, as before. No built-for stamp: it depends on the kernel, not Plasma.
            if ! command -v cargo > /dev/null; then
                echo "search: skipped, cargo is not installed (File Explorer searches folders in place)"; return 0
            fi
            echo "search: aurora-search"
            mkdir -p "$BUILD"
            local bins
            bins=$(cd "$ROOT/components/file-explorer/search" && nice -n 10 cargo build --release --locked --bins \
                       --message-format=json-render-diagnostics 2> "$BUILD/search.build.log" | python3 -I -c '
import json, sys
for line in sys.stdin:
    message = json.loads(line)
    if message.get("reason") == "compiler-artifact" and message.get("executable"):
        print(message["executable"])') || true
            local helper query
            helper=$(grep '/aurora-search$' <<< "$bins" | tail -1)
            query=$(grep '/aurora-search-query$' <<< "$bins" | tail -1)
            [ -x "$helper" ] && [ -x "$query" ] || { echo "  build failed, see build/search.build.log"; return 1; }
            install -Dm755 "$helper" "$BUILD/search/aurora-search"
            install -Dm755 "$query" "$BUILD/search/aurora-search-query"
            return 0 ;;
        file-explorer|glass|energy-page)
            echo "$part"
            compile "$part" "$ROOT/components/$part" || return 1 ;;
        *)
            echo "unknown part: $part (parts: $OWN_PARTS $PLASMA_PARTS cli search)" >&2
            return 2 ;;
    esac
    stamp_of "$part" > "$BUILD/$part/built-for"
}

# Every part is tried even when one fails; the failures are reported together at the end.
parts=${*:-$OWN_PARTS $PLASMA_PARTS cli search}
failed=""
for part in $parts; do
    build_part "$part" || failed="$failed $part"
done
if [ -n "$failed" ]; then
    echo "not built:$failed"
    exit 1
fi
echo "all parts built in $BUILD"
