#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/built_for.py, the release check that a package was built for the Plasma, KWin, Qt and KDecoration every
# repository of this PC has, so `aurora update` installs it instead of waiting. It reads a recorded .BUILDINFO
# (aurora-theme 1.1.1) and recorded `pacman -Si` output (CachyOS and Arch, 2026-11-08): the same versions pass, a
# newer Plasma in one repository fails and names it, KDecoration may differ in its last number only, a package the
# .BUILDINFO does not name fails, and the packages and rules come from cli/src/system.rs.
set -u
root=$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}
buildinfo="$root/cli/tests/fixtures/buildinfo.txt"
si="$root/tests/release/fixtures/pacman-si.txt"
run() {
    python3 -I "$root/scripts/built_for.py" --from "$1" "$2" > "$work/out" 2>&1
    echo $?
}

check "the versions every repository has pass" "$(run "$buildinfo" "$si")" 0
check "each repository's version is listed" "$(grep -c 'cachyos-extra-v3\|extra' "$work/out")" 8
# CachyOS's repository moves to Plasma 6.8.3 while the package was built for 6.8.2.
sed '/^Repository *: cachyos-extra-v3/,/^$/s/^Version *: 6.8.2-1.1$/Version         : 6.8.3-1.1/' "$si" > "$work/newer.txt"
check "the fixture changed one version" "$(diff "$si" "$work/newer.txt" | grep -c '^>')" 2
check "a newer Plasma in one repository fails" "$(run "$buildinfo" "$work/newer.txt")" 1
check "the failure names the repository and both versions" \
    "$(grep -c 'KDE Plasma.*6\.8\.2.*cachyos-extra-v3.*6\.8\.3' "$work/out")" 1
# Only kdecoration's line moves to 6.8.3: KDecoration counts by its minor version.
awk '/^Name *: kdecoration/{k=1} /^$/{k=0} k && /^Version/{sub(/6\.8\.2/, "6.8.3")} {print}' "$si" > "$work/kdeco.txt"
check "KDecoration may differ in its last number" "$(run "$buildinfo" "$work/kdeco.txt")" 0
awk '/^Name *: kdecoration/{k=1} /^$/{k=0} k && /^Version/{sub(/6\.8\.2/, "6.9.0")} {print}' "$si" > "$work/kdeco-minor.txt"
check "KDecoration may not differ in its minor version" "$(run "$buildinfo" "$work/kdeco-minor.txt")" 1
grep -v '^installed = kwin-' "$buildinfo" > "$work/no-kwin.txt"
check "a .BUILDINFO without KWin fails" "$(run "$work/no-kwin.txt" "$si")" 1
check "the packages and rules come from cli/src/system.rs" \
    "$(python3 -I -c "import sys; sys.path.insert(0, '$root/scripts'); import built_for; print(len(built_for.links()))")" \
    "$(grep -c '^ *Link { package:' "$root/cli/src/system.rs")"
exit "$failures"
