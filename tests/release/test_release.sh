#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/release.sh turns [Unreleased] into a version: it refuses a dirty tree or an empty [Unreleased], dates the
# changelog, sets VERSION, commits, tags, and builds the release files (a source archive of the repository with its
# themes, the release notes, SHA256SUMS), then builds and checks the package (scripts/pacman-repo.sh). It publishes
# nothing without --publish, and with --publish nothing leaves the PC before the package is built and checked: a
# failed build pushes no tag and creates no release. Runs in a throwaway repository with stand-ins for pacman-repo.sh
# and gh and a local remote, so it needs no network and touches nothing else.
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
# Stand-ins: pacman-repo.sh and gh record their calls in one log, in order; pacman-repo.sh exits with $REPO_EXIT.
cat > scripts/pacman-repo.sh <<'EOF'
#!/bin/bash
echo "repo $*" >> "$CALLS"
exit "${REPO_EXIT:-0}"
EOF
chmod +x scripts/pacman-repo.sh
mkdir -p "$work/bin"
cat > "$work/bin/gh" <<'EOF'
#!/bin/bash
echo "gh $*" >> "$CALLS"
EOF
chmod +x "$work/bin/gh"
export CALLS="$work/calls.log"
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
check "nothing was published" "$(grep -c '==> Publishing' "$work/release.log")" 0
check "the package is built and checked without publishing" "$(cat "$CALLS")" \
    "repo build/release/1.0.0/aur build/release/1.0.0/repo"

scripts/release.sh 1.0.1 > "$work/empty.log" 2>&1; code=$?
check "a release with an empty [Unreleased] is refused" "$([ "$code" -ne 0 ] && echo refused)" refused

# next VERSION: a change under [Unreleased], committed, so VERSION can be released.
next() {
    sed -i "s/^## \[Unreleased\]$/## [Unreleased]\n\n### Fixed\n\n- Something for $1./" CHANGELOG.md
    g commit -qam "fix: something for $1"
}
g init -q --bare "$work/remote.git"
g remote add origin "$work/remote.git"
g push -q origin main
next 1.1.0
: > "$CALLS"
PATH="$work/bin:$PATH" REPO_EXIT=1 scripts/release.sh 1.1.0 --publish > "$work/failed.log" 2>&1; code=$?
check "a release whose package fails its checks fails" "$([ "$code" -ne 0 ] && echo refused)" refused
check "a failed package pushes no tag" "$(git --git-dir="$work/remote.git" tag -l v1.1.0)" ""
check "a failed package creates no GitHub release" "$(grep -c '^gh ' "$CALLS")" 0
next 1.2.0
: > "$CALLS"
PATH="$work/bin:$PATH" scripts/release.sh 1.2.0 --publish > "$work/published.log" 2>&1; code=$?
check "a release whose package passes publishes" "$code" 0
check "the tag goes out after the package passed" "$(git --git-dir="$work/remote.git" tag -l v1.2.0)" v1.2.0
check "build and checks first, then the GitHub release, then the repository" \
    "$(sed -E 's/^(repo|gh) ([a-z-]+ [a-z-]+).*?( --publish-built)?$/\1 \2\3/; s/ build\/release\/1.2.0\/(aur|repo)//g' "$CALLS" | cut -d' ' -f1-3)" \
    "$(printf 'repo\ngh release create\nrepo --publish-built')"

exit "$failures"
