#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# packaging/aur/PKGBUILD stays in step with the rest of the repository: it depends on exactly the packages in
# installer/dependencies.txt plus what the aurora command needs itself (the two libraries it links, glibc and libgcc,
# and the two programs it runs, curl and tar), builds with installer/build-dependencies.txt plus cargo, and .SRCINFO
# is what makepkg writes for it. Reads the PKGBUILD in a subshell; builds nothing.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
failures=0
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}

listed=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$root/installer/dependencies.txt" | awk '{print $1}')
want=$({ echo "$listed"; printf '%s\n' glibc libgcc curl tar; } | sort -u | tr '\n' ' ')
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

if command -v makepkg > /dev/null; then
    fresh=$(cd "$root/packaging/aur" && makepkg --printsrcinfo 2>/dev/null)
    check ".SRCINFO is what makepkg writes for the PKGBUILD" "$(diff <(echo "$fresh") "$root/packaging/aur/.SRCINFO" > /dev/null && echo same)" same
fi
exit "$failures"
