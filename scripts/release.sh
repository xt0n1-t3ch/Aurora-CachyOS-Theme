#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Cuts a release: scripts/release.sh VERSION [--publish]
#
#  1. checks: VERSION is SemVer and above the last release, the tree is clean, CHANGELOG.md is in the Keep a Changelog
#     format with changes under [Unreleased], and every test passes (scripts/check.sh);
#  2. dates [Unreleased] as the new version in CHANGELOG.md, writes VERSION and CITATION.cff's version and date,
#     commits "chore(release): VERSION" and tags vVERSION;
#  3. builds the release files in build/release/VERSION: aurora-VERSION.tar.gz (the sources and themes, so it installs
#     without git), release-notes.md (the version's changelog section) and SHA256SUMS;
#  4. writes the AUR package of the release in build/release/VERSION/aur: packaging/aur with pkgver set to VERSION,
#     pkgrel to 1 and the archive's SHA-256, and its .SRCINFO (docs/releasing.md says how to push it to the AUR);
#  5. builds the package of step 4 into the pacman repository in build/release/VERSION/repo (scripts/pacman-repo.sh):
#     in the clean chroot, started on an older processor and checked against what `aurora update` installs; any
#     failure stops the release here, before anything leaves this PC;
#  6. with --publish only: pushes the branch and the tag, creates the GitHub release with those files, publishes the
#     repository built in step 5 and checks it as a user's pacman sees it (pacman-repo.sh --publish-built).
#
# RELEASE_REPO_URL overrides the web address the changelog links to (default: the origin remote).
# RELEASE_SKIP_CHECKS=1 skips step 1's test run (the release tests use it; never for a real release).
set -euo pipefail
ROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
cd "$ROOT"
# Prints a step heading in bold. Arguments: the heading words.
say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
# Runs git with the release identity (GIT_AUTHOR_NAME and GIT_AUTHOR_EMAIL override it), because no global
# identity is set. Arguments: the git arguments.
g() { git -c user.name="${GIT_AUTHOR_NAME:-xt0n1}" -c user.email="${GIT_AUTHOR_EMAIL:-xt0n1@black-watch.net}" "$@"; }

version=${1:-}
publish=false
[ "${2:-}" = "--publish" ] && publish=true
[ -n "$version" ] || fail "usage: scripts/release.sh VERSION [--publish]"

# Prints the repository's web address from the origin remote (SSH or HTTPS form), or the GitHub project's address
# when origin is something else.
web_url() {
    local remote
    remote=$(git remote get-url origin 2>/dev/null || true)
    remote=${remote%.git}
    case "$remote" in
        git@github.com:*) echo "https://github.com/${remote#git@github.com:}" ;;
        https://*) echo "$remote" ;;
        *) echo "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme" ;;
    esac
}
url=${RELEASE_REPO_URL:-$(web_url)}

say "Checking $version"
[ -z "$(git status --porcelain)" ] || fail "the tree has uncommitted changes: commit or stash them first"
python3 -I scripts/changelog.py check || fail "CHANGELOG.md is not in the Keep a Changelog format (see above)"
today=$(date +%F)
# The changelog is released in a copy first, so a failed check leaves CHANGELOG.md alone.
staged=$(mktemp)
trap 'rm -f "$staged"' EXIT
cp CHANGELOG.md "$staged"
python3 -I scripts/changelog.py release "$version" "$today" "$url" "$staged" || fail "the changelog cannot release $version"
if [ "${RELEASE_SKIP_CHECKS:-0}" != 1 ]; then
    scripts/check.sh || fail "a check failed; nothing was released"
fi

say "Releasing $version"
cp "$staged" CHANGELOG.md
echo "$version" > VERSION
g add CHANGELOG.md VERSION
# CITATION.cff, which GitHub shows as "Cite this repository", names the release and its date.
if [ -f CITATION.cff ]; then
    sed -i -e "s/^version: .*/version: \"$version\"/" -e '/^date-released: /d' CITATION.cff
    sed -i "/^version: /a date-released: \"$today\"" CITATION.cff
    g add CITATION.cff
fi
g commit -q -m "chore(release): $version"
notes=$(python3 -I scripts/changelog.py notes "$version")
g tag -a "v$version" -m "Aurora $version" -m "$notes"
echo "commit $(git rev-parse --short HEAD), tag v$version"

say "Building the release files"
out="build/release/$version"
rm -rf "$out"
mkdir -p "$out"
name="aurora-$version"
archive="$out/$name.tar"
git archive --format=tar --prefix="$name/" -o "$archive" "v$version"
gzip -9 -n "$archive"
printf '%s\n' "$notes" > "$out/release-notes.md"
(cd "$out" && sha256sum "$name.tar.gz" release-notes.md > SHA256SUMS)
ls -l "$out"

say "Writing the AUR package"
# The archive's checksum exists only once the tagged commit is built, so the package's files for this release are
# written here instead of being committed with it.
mkdir -p "$out/aur"
cp packaging/aur/PKGBUILD packaging/aur/aurora-theme.install "$out/aur/"
sum=$(sha256sum "$out/$name.tar.gz" | cut -d' ' -f1)
sed -i -e "s/^pkgver=.*/pkgver=$version/" -e "s/^pkgrel=.*/pkgrel=1/" -e "s/^sha256sums=.*/sha256sums=('$sum')/" \
    "$out/aur/PKGBUILD"
if command -v makepkg > /dev/null; then
    (cd "$out/aur" && makepkg --printsrcinfo > .SRCINFO)
else
    echo "makepkg is not installed: write .SRCINFO with makepkg --printsrcinfo before pushing to the AUR"
fi
ls -l "$out/aur"

say "Building and checking the package"
# makepkg finds the archive in the release folder, so it does not download what is not published yet.
SRCDEST="$ROOT/$out" scripts/pacman-repo.sh "$out/aur" "$out/repo"

if $publish; then
    say "Publishing $version"
    git push origin HEAD --follow-tags
    gh release create "v$version" "$out/$name.tar.gz" "$out/SHA256SUMS" --title "Aurora $version" \
        --notes-file "$out/release-notes.md" --verify-tag
    SRCDEST="$ROOT/$out" scripts/pacman-repo.sh "$out/aur" "$out/repo" --publish-built
else
    say "Done; not published"
    echo "Publish with: git push origin HEAD --follow-tags && gh release create v$version $out/$name.tar.gz $out/SHA256SUMS --title \"Aurora $version\" --notes-file $out/release-notes.md --verify-tag && SRCDEST=$ROOT/$out scripts/pacman-repo.sh $out/aur $out/repo --publish-built"
fi
