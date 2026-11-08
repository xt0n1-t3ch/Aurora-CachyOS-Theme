#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds Aurora's pacman repository: scripts/pacman-repo.sh PKGBUILD_DIR OUT_DIR [--publish | --publish-built]
#
#  1. builds the package of PKGBUILD_DIR and signs it: in the clean chroot when it holds machine code
#     (scripts/clean-build.sh, which also checks it on an older processor and against `aurora update`), with makepkg
#     here otherwise;
#  2. writes a fresh database named aurora in OUT_DIR, signed too, with its files as real files (GitHub release
#     assets cannot be links), and the signing key's public half as aurora.asc;
#  3. with --publish only: makes the files of OUT_DIR the files of the GitHub release tagged `repo`, the address the
#     [aurora] section of a user's pacman.conf reads (get.sh, SERVER). The package goes up first, the database that
#     lists it next, and the files of an older build last, so the database online never names a package that is not
#     there. A package whose name is already published is refused. Then scripts/verify-repo.sh syncs it the way a
#     user's pacman does, from the database that was online before, and fails unless pacman gets this very package.
#
# --publish-built skips steps 1 and 2 and publishes what OUT_DIR holds from an earlier run (release.sh builds first
# and publishes only once every check passed). AURORA_REPO_SERVER names another address for the check (the tests).
#
# The files are signed with the key whose public half is packaging/repo/aurora.asc; AURORA_SIGNING_KEY names another
# key (the tests use it). SRCDEST, when set, is where makepkg looks for the sources first (release.sh points it at the
# release's archive before the archive is published).
set -euo pipefail
ROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
# Prints a step heading in bold. Arguments: the heading words.
say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

src=${1:-}
out=${2:-}
publish=false
built=false
case "${3:-}" in
    --publish) publish=true ;;
    --publish-built) publish=true; built=true ;;
    '') ;;
    *) fail "unknown option ${3:-}" ;;
esac
[ -n "$src" ] && [ -n "$out" ] || fail "usage: scripts/pacman-repo.sh PKGBUILD_DIR OUT_DIR [--publish | --publish-built]"
[ -f "$src/PKGBUILD" ] || fail "$src has no PKGBUILD"
src=$(readlink -f "$src")

key=${AURORA_SIGNING_KEY:-$(gpg --show-keys --with-colons "$ROOT/packaging/repo/aurora.asc" 2> /dev/null |
    awk -F: '$1 == "fpr" { print $10; exit }')}
[ -n "$key" ] || fail "no signing key: packaging/repo/aurora.asc is missing and AURORA_SIGNING_KEY is not set"
gpg --batch --list-secret-keys "$key" > /dev/null 2>&1 || fail "the secret half of key $key is not in this keyring"

build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT

if $built; then
    out=$(readlink -f "$out")
    mapfile -t packages < <(cd "$src" && PKGDEST="$out" BUILDDIR="$build" makepkg --packagelist)
    for file in "${packages[@]/%/.sig}" "${packages[@]}" "$out/aurora.db" "$out/aurora.db.sig" "$out/aurora.asc"; do
        [ -f "$file" ] || fail "$file is missing: build the repository first (without --publish-built)"
    done
else
    # A fresh folder each time, so the database lists only this build.
    rm -rf "$out"
    mkdir -p "$out"
    out=$(readlink -f "$out")

    # A package with machine code runs on other people's processors, so it is built only in the clean chroot, which
    # also starts it on an older processor (scripts/clean-build.sh); AURORA_CLEAN_BUILD names a stand-in for the
    # tests. A package for any architecture holds no machine code and is built here.
    arches=$(cd "$src" && makepkg --printsrcinfo | sed -n 's/^\tarch = //p' | sort -u)
    if [ "$arches" = any ]; then
        say "Building the package"
        # PKGDEST and BUILDDIR are set here because a user's makepkg.conf may send them elsewhere.
        (cd "$src" && PKGDEST="$out" BUILDDIR="$build" makepkg --force --nodeps --clean --noconfirm)
    else
        "${AURORA_CLEAN_BUILD:-$ROOT/scripts/clean-build.sh}" "$src" "$out"
    fi
    mapfile -t packages < <(cd "$src" && PKGDEST="$out" BUILDDIR="$build" makepkg --packagelist)
    for package in "${packages[@]}"; do
        [ -f "$package" ] || fail "the build did not write $package"
        # makepkg --sign signs the same way (scripts/libmakepkg/integrity/generate_signature.sh).
        gpg --batch --yes --detach-sign --use-agent --no-armor --local-user "$key" --output "$package.sig" "$package"
    done

    say "Writing the database"
    repo-add --sign --key "$key" --verify "$out/aurora.db.tar.zst" "${packages[@]}"
    # repo-add names its database aurora.db.tar.zst and links aurora.db to it; pacman asks for aurora.db.
    for name in aurora.db aurora.db.sig aurora.files aurora.files.sig; do
        cp --remove-destination "$(readlink -f "$out/$name")" "$out/$name"
    done
    rm -f "$out"/aurora.db.tar.zst* "$out"/aurora.files.tar.zst*
    gpg --batch --armor --export "$key" > "$out/aurora.asc"
    ls -l "$out"
fi

if $publish; then
    say "Publishing the repository"
    cd "$ROOT"
    server=${AURORA_REPO_SERVER:-$(sed -n 's/^REPO=//p' "$ROOT/get.sh")/releases/download/repo}
    # The database users have now, with the server's time as its file time, for the check after the upload.
    seed="$build/before.db"
    curl -fsSL -R -o "$seed" "$server/aurora.db" 2> /dev/null || rm -f "$seed"
    if ! gh release view repo > /dev/null 2>&1; then
        gh release create repo --title "Aurora pacman repository" --prerelease --latest=false \
            --notes "The signed pacman repository of Aurora. README.md, \"Installation\", says how to add it."
    fi
    published=$(gh release view repo --json assets --jq '.assets[].name')
    # pacman keeps a package by its file name and checks it against the database it read, and makepkg never builds
    # the same bytes twice: a new build under a name already out fails for everyone who has the older database or
    # the older file in their cache.
    for package in "${packages[@]}"; do
        name=$(basename "$package")
        ! grep -qxF "$name" <<< "$published" ||
            fail "$name is already published; raise pkgrel in $src/PKGBUILD and run this again"
        gh release upload repo "$package" "$package.sig"
    done
    # The database and its signature go up in one call, so the moment a sync could read one without the other is
    # as short as it can be; pacman downloads a database whose signature does not match again on its next sync.
    gh release upload repo "$out/aurora.asc" "$out/aurora.db" "$out/aurora.db.sig" "$out/aurora.files" \
        "$out/aurora.files.sig" --clobber
    # Files of an older build go last, once no database online names them, so the release holds one version.
    while read -r asset; do
        [ -e "$out/$asset" ] || gh release delete-asset repo "$asset" --yes
    done <<< "$published"

    say "Checking the repository as a user's pacman sees it"
    "$ROOT/scripts/verify-repo.sh" "$server" "$out" "$seed"
fi
