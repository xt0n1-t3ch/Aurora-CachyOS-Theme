#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/release.sh turns [Unreleased] into a version: it refuses a dirty tree or an empty [Unreleased], dates the
# changelog, sets VERSION, commits, tags, and builds the release files (a source archive of the repository with its
# themes, the release notes, SHA256SUMS). It publishes nothing without --publish. Runs in a throwaway repository, so it
# needs no network and touches nothing else.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
# check NAME GOT WANT: prints PASS when GOT equals WANT and FAIL otherwise; a FAIL counts as a failure.
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}
# g ARGS...: git with a throwaway identity, for the temporary repository.
g() { git -c user.name=test -c user.email=test@example.org "$@"; }

# The theme's repository, with a theme file of its own.
mkdir -p "$work/theme/scripts" "$work/theme/themes"
cd "$work/theme" || exit 1
echo "theme files" > themes/theme.txt
g init -q -b main
cp "$root/scripts/release.sh" "$root/scripts/changelog.py" scripts/
mkdir -p packaging/aur
cp "$root/packaging/aur/PKGBUILD" "$root/packaging/aur/aurora-theme.install" packaging/aur/
cp "$root/CITATION.cff" .
echo "0.0.0" > VERSION
printf '/build/\n' > .gitignore
cat > CHANGELOG.md <<'EOF'
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Everything.

[Unreleased]: https://example.org/theme/commits/HEAD
EOF
g add . && g commit -qm "feat: the theme"
export RELEASE_REPO_URL=https://example.org/theme RELEASE_SKIP_CHECKS=1

echo "dirty" > stray.txt
scripts/release.sh 1.0.0 > "$work/dirty.log" 2>&1; code=$?
check "a tree with uncommitted changes is refused" "$([ "$code" -ne 0 ] && echo refused)" refused
rm stray.txt

scripts/release.sh 1.0 > "$work/semver.log" 2>&1; code=$?
check "a version that is not SemVer is refused" "$([ "$code" -ne 0 ] && echo refused)" refused

scripts/release.sh 1.0.0 > "$work/release.log" 2>&1; code=$?
check "the release succeeds" "$code" 0
check "VERSION names the release" "$(cat VERSION)" 1.0.0
check "CITATION.cff names the release" "$(sed -n 's/^version: "\(.*\)"$/\1/p' CITATION.cff)" 1.0.0
check "CITATION.cff dates the release" "$(sed -n 's/^date-released: "\(.*\)"$/\1/p' CITATION.cff)" "$(date +%F)"
check "the changelog dates the version" "$(grep -c "^## \[1.0.0\] - $(date +%F)$" CHANGELOG.md)" 1
check "a fresh [Unreleased] stays on top" "$(grep -m1 '^## ' CHANGELOG.md)" "## [Unreleased]"
check "the release is committed" "$(git log -1 --format=%s)" "chore(release): 1.0.0"
check "the tag points at the release commit" "$(git rev-parse "v1.0.0^{commit}")" "$(git rev-parse HEAD)"
check "the tree is clean afterwards" "$(git status --porcelain | wc -l)" 0
out=build/release/1.0.0
check "the source archive includes the themes" \
    "$(tar -xzOf "$out/aurora-1.0.0.tar.gz" aurora-1.0.0/themes/theme.txt 2>/dev/null)" "theme files"
check "the archive carries the version" "$(tar -xzOf "$out/aurora-1.0.0.tar.gz" aurora-1.0.0/VERSION 2>/dev/null)" 1.0.0
check "the release notes are the version's section" "$(cat "$out/release-notes.md")" "$(printf '### Added\n\n- Everything.')"
check "the checksums verify" "$(cd "$out" && sha256sum -c --quiet SHA256SUMS && echo ok)" ok
# The AUR files of the release: the package's version and the archive's checksum, and .SRCINFO to match.
check "the AUR package is the release's version" "$(sed -n 's/^pkgver=//p' "$out/aur/PKGBUILD")" 1.0.0
check "the AUR package starts at release 1" "$(sed -n 's/^pkgrel=//p' "$out/aur/PKGBUILD")" 1
check "the AUR package checks the archive's SHA-256" "$(sed -n "s/^sha256sums=('\(.*\)')$/\1/p" "$out/aur/PKGBUILD")" \
    "$(sha256sum "$out/aurora-1.0.0.tar.gz" | cut -d' ' -f1)"
check "the AUR package carries its install script" "$([ -f "$out/aur/aurora-theme.install" ] && echo yes)" yes
if command -v makepkg > /dev/null; then
    check ".SRCINFO matches the PKGBUILD" "$(grep -c -E '^\s+pkgver = 1.0.0$' "$out/aur/.SRCINFO" 2>/dev/null)" 1
fi
check "nothing was published" "$(grep -c 'publish' "$work/release.log")" "$(grep -c 'not published' "$work/release.log")"

scripts/release.sh 1.0.1 > "$work/empty.log" 2>&1; code=$?
check "a release with an empty [Unreleased] is refused" "$([ "$code" -ne 0 ] && echo refused)" refused

exit "$failures"
