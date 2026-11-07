#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# packaging/aur/PKGBUILD stays in step with the rest of the repository: it depends on exactly the packages in
# installer/dependencies.txt plus what the aurora command needs itself (the two libraries it links, glibc and libgcc,
# the two programs it downloads with, curl and tar, and what its update notices use: checkupdates from pacman-contrib,
# notify-send from libnotify and Konsole to run the update in), builds with installer/build-dependencies.txt plus cargo, and .SRCINFO
# is what makepkg writes for it. Reads the PKGBUILD in a subshell; builds nothing.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
failures=0
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}

listed=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$root/installer/dependencies.txt" | awk '{print $1}')
want=$({ echo "$listed"; printf '%s\n' glibc libgcc curl tar pacman-contrib libnotify konsole; } | sort -u | tr '\n' ' ')
# shellcheck disable=SC1091 # the PKGBUILD is read on purpose, in a subshell
got=$(cd "$root/packaging/aur" && bash -c 'source ./PKGBUILD && printf "%s\n" "${depends[@]}"' | sort -u | tr '\n' ' ')
check "the package depends on installer/dependencies.txt and what the aurora command needs" "$got" "$want"
build_listed=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$root/installer/build-dependencies.txt" | awk '{print $1}')
want=$({ echo "$build_listed"; echo cargo; } | sort -u | tr '\n' ' ')
# shellcheck disable=SC1091
got=$(cd "$root/packaging/aur" && bash -c 'source ./PKGBUILD && printf "%s\n" "${makedepends[@]}"' | sort -u | tr '\n' ' ')
check "the package builds with installer/build-dependencies.txt and cargo" "$got" "$want"

# makepkg writes .PKGINFO from the PKGBUILD's variables after package() runs, so a function that assigns one of them
# (a loop over `license`, say) changes what the package declares.
names='pkgname|pkgver|pkgrel|pkgdesc|arch|url|license|depends|makedepends|optdepends|source|sha256sums|install|options'
# shellcheck disable=SC1091
bodies=$(cd "$root/packaging/aur" && bash -c 'source ./PKGBUILD && declare -f prepare build check package' 2>/dev/null)
clobbered=$(grep -E "(^|[;[:space:]])(for|local|declare)?[[:space:]]*($names)(=|[[:space:]]+in[[:space:]])" <<< "$bodies" \
    | grep -v -E "local[[:space:]]+[a-z_]+=" | head -3)
check "the package functions leave makepkg's variables alone" "$clobbered" ""

# makepkg hands build() the build machine's flags, and CachyOS's makepkg.conf asks for that machine's CPU; the
# package still has to run on every x86_64 PC. The compilers themselves say what the flags build() sets target.
# shellcheck disable=SC1091,SC2016 # the PKGBUILD is read on purpose; $CFLAGS expands in the subshell
mapfile -t flags < <(cd "$root/packaging/aur" &&
    CFLAGS="-march=native -O3 -pipe" CXXFLAGS="-march=native -O3 -pipe" RUSTFLAGS="-C opt-level=3 -C target-cpu=native" \
    bash -c 'source ./PKGBUILD; _baseline; printf "%s\n" "$CFLAGS" "$CXXFLAGS" "$RUSTFLAGS"' 2>/dev/null)
check "the flags reach the compilers" "${#flags[@]}" "3"
# The baseline is SSE2; each name below is a newer instruction set the compiler would announce it may use.
above='SSE3|SSSE3|SSE4_1|SSE4_2|POPCNT|AVX|AVX2|BMI|BMI2|FMA|LZCNT|MOVBE|AVX512F'
# shellcheck disable=SC2086 # one argument per flag
newer=$(gcc ${flags[0]:-} -dM -E -x c /dev/null 2>/dev/null | grep -cE "#define __($above)__ ")
check "C code in the package needs nothing newer than the x86-64 baseline" "$newer" "0"
# shellcheck disable=SC2086
newer=$(g++ ${flags[1]:-} -dM -E -x c++ /dev/null 2>/dev/null | grep -cE "#define __($above)__ ")
check "C++ code in the package needs nothing newer than the x86-64 baseline" "$newer" "0"
if command -v rustc > /dev/null; then
    # shellcheck disable=SC2086
    newer=$(rustc ${flags[2]:-} --print cfg 2>/dev/null | tr 'a-z.' 'A-Z_' | grep -cE "TARGET_FEATURE=\"($above|BMI1)\"")
    check "Rust code in the package needs nothing newer than the x86-64 baseline" "$newer" "0"
fi
# shellcheck disable=SC1091
check "build() builds for the x86-64 baseline" \
    "$(cd "$root/packaging/aur" && bash -c 'source ./PKGBUILD && declare -f build' | grep -c '^ *_baseline;\?$')" "1"
# shellcheck disable=SC1091
check "check() refuses machine code that needs more than the baseline" \
    "$(cd "$root/packaging/aur" && bash -c 'source ./PKGBUILD && declare -f check' | grep -c 'scripts/cpu_baseline.py')" "1"

if command -v makepkg > /dev/null; then
    fresh=$(cd "$root/packaging/aur" && makepkg --printsrcinfo 2>/dev/null)
    check ".SRCINFO is what makepkg writes for the PKGBUILD" "$(diff <(echo "$fresh") "$root/packaging/aur/.SRCINFO" > /dev/null && echo same)" same
fi
exit "$failures"
