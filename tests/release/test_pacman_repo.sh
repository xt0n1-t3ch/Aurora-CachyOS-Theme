#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/pacman-repo.sh builds Aurora's pacman repository: a signed package, a signed database named aurora whose
# files are real files (GitHub release assets cannot be links) and the signing key's public half. The last checks
# install the package with pacman from that folder, the way a user's pacman.conf section reads the release, under
# fakeroot in a throwaway root. Publishing goes through a stand-in gh that records its calls: the database goes up only
# after the package it lists, and older files go last. Uses a throwaway signing key and a small package, so it needs
# no network.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
work=$(mktemp -d)
# Stops the gpg-agents of the two throwaway keyrings, then removes the folder.
trap 'gpgconf --homedir "$work/gnupg" --kill all 2> /dev/null; gpgconf --homedir "$work/sys/keyring" --kill all 2> /dev/null
      rm -rf "$work"' EXIT
failures=0
# check NAME GOT WANT: prints PASS when GOT equals WANT and FAIL otherwise; a FAIL counts as a failure.
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}

export GNUPGHOME="$work/gnupg"
mkdir -m 700 "$GNUPGHOME"
gpg --batch --quiet --passphrase '' --quick-gen-key "Test packages <test@example.org>" ed25519 sign never 2> /dev/null
key=$(gpg --with-colons --list-secret-keys 2> /dev/null | awk -F: '$1 == "fpr" { print $10; exit }')
export AURORA_SIGNING_KEY=$key

mkdir "$work/pkg"
cat > "$work/pkg/PKGBUILD" <<'EOF'
pkgname=aurora-test
pkgver=1.0.0
pkgrel=1
pkgdesc='A package for the repository test'
arch=('any')
license=('GPL-3.0-only')
package() {
    install -d "$pkgdir/usr/share/aurora-test"
    echo "installed" > "$pkgdir/usr/share/aurora-test/ok"
}
EOF

out="$work/repo"
"$root/scripts/pacman-repo.sh" "$work/pkg" "$out" > "$work/build.log" 2>&1; code=$?
check "the repository builds" "$code" 0
[ "$code" -eq 0 ] || cat "$work/build.log"
package="$out/aurora-test-1.0.0-1-any.pkg.tar.zst"
for file in aurora.db aurora.files; do
    check "$file is a real file" "$([ -f "$out/$file" ] && [ ! -L "$out/$file" ] && echo file)" file
    check "$file's signature verifies" \
        "$(gpg --batch --verify "$out/$file.sig" "$out/$file" > /dev/null 2>&1 && echo ok)" ok
done
check "the package's signature verifies" "$(gpg --batch --verify "$package.sig" "$package" > /dev/null 2>&1 && echo ok)" ok
check "the database lists the package" "$(bsdtar -tf "$out/aurora.db" 2> /dev/null | grep -c '^aurora-test-1.0.0-1/desc$')" 1
check "aurora.asc is the signing key" \
    "$(gpg --show-keys --with-colons "$out/aurora.asc" 2> /dev/null | awk -F: '$1 == "fpr" { print $10; exit }')" "$key"
check "no links are left for the release" "$(find "$out" -type l | wc -l)" 0

AURORA_SIGNING_KEY=0000000000000000000000000000000000000000 \
    "$root/scripts/pacman-repo.sh" "$work/pkg" "$work/unsigned" > "$work/nokey.log" 2>&1; code=$?
check "a key without its secret half is refused" "$([ "$code" -ne 0 ] && echo refused)" refused
check "nothing is built without the key" "$(find "$work/unsigned" -name '*.pkg.tar.*' 2> /dev/null | wc -l)" 0

# A stand-in gh: the release's files are the lines of $GH_ASSETS, and every call goes to $GH_LOG.
mkdir "$work/bin"
cat > "$work/bin/gh" <<'EOF'
#!/bin/bash
echo "$*" >> "$GH_LOG"
[ "$1 $2 ${4:-}" = "release view --json" ] && cat "$GH_ASSETS"
exit 0
EOF
chmod +x "$work/bin/gh"
# publish ASSETS: publishes the test package to a release that holds ASSETS. Sets code to the exit status and calls to
# the gh calls that change the release.
# The stand-in gh uploads nothing, so the check after the upload reads the built folder itself as the server, unless
# SERVED names another one.
publish() {
    printf '%s' "$1" > "$work/assets"
    : > "$work/gh.log"
    PATH="$work/bin:$PATH" GH_LOG="$work/gh.log" GH_ASSETS="$work/assets" \
        AURORA_REPO_SERVER="file://${SERVED:-$work/published}" \
        "$root/scripts/pacman-repo.sh" "$work/pkg" "$work/published" "${MODE:---publish}" > "$work/publish.log" 2>&1
    code=$?
    calls=$(grep -E '^release (upload|delete-asset)' "$work/gh.log" | sed "s|$work/published/||g")
}
publish "aurora-test-0.9.0-1-any.pkg.tar.zst
aurora.db
"
check "a new version publishes" "$code" 0
check "the package goes up before the database that lists it, older files last" "$calls" \
"release upload repo aurora-test-1.0.0-1-any.pkg.tar.zst aurora-test-1.0.0-1-any.pkg.tar.zst.sig
release upload repo aurora.asc aurora.db aurora.db.sig aurora.files aurora.files.sig --clobber
release delete-asset repo aurora-test-0.9.0-1-any.pkg.tar.zst --yes"
publish "aurora-test-1.0.0-1-any.pkg.tar.zst
"
check "a build under a published name is refused" "$([ "$code" -ne 0 ] && echo refused)" refused
check "a refused publish changes nothing" "$calls" ""
check "the refusal says to raise pkgrel" "$(grep -c 'raise pkgrel' "$work/publish.log")" 1
if command -v fakeroot > /dev/null; then
    publish ""
    check "after the upload, pacman gets the package from the server, signed" \
        "$(grep -c '^pacman gets aurora-test 1.0.0-1 from file://.*, signed and the same file$' "$work/publish.log")" 1
    # A server that still offers another build, as when an upload did not land.
    printf 'pkgname=aurora-test\npkgver=0.9.0\npkgrel=1\narch=(any)\npackage() { :; }\n' > "$work/old.PKGBUILD"
    mkdir "$work/old" && cp "$work/old.PKGBUILD" "$work/old/PKGBUILD"
    "$root/scripts/pacman-repo.sh" "$work/old" "$work/stale" > /dev/null 2>&1
    SERVED="$work/stale" publish ""
    check "a server that offers another version fails the publish" "$([ "$code" -ne 0 ] && echo refused)" refused
    check "the failure names the version pacman sees" "$(grep -c 'pacman sees aurora-test 0.9.0-1' "$work/publish.log")" 1
else
    echo "SKIP the check after the upload: fakeroot is not installed"
fi
# --publish-built publishes the folder an earlier run built, without building again.
before=$(sha256sum < "$work/published/aurora-test-1.0.0-1-any.pkg.tar.zst")
MODE=--publish-built publish ""
check "--publish-built publishes the built folder" "$code" 0
check "--publish-built does not build again" "$(sha256sum < "$work/published/aurora-test-1.0.0-1-any.pkg.tar.zst")" "$before"
check "--publish-built uploads the package" "$(grep -c 'release upload repo .*aurora-test-1.0.0-1-any.pkg.tar.zst' "$work/gh.log")" 1
rm -rf "$work/empty" && mkdir "$work/empty"
PATH="$work/bin:$PATH" GH_LOG="$work/gh.log" GH_ASSETS="$work/assets" \
    "$root/scripts/pacman-repo.sh" "$work/pkg" "$work/empty" --publish-built > "$work/empty.log" 2>&1; code=$?
check "--publish-built refuses a folder with nothing built" "$([ "$code" -ne 0 ] && echo refused)" refused

# pacman installs the package from the repository, trusting only the key in aurora.asc. pacman-key and pacman need
# root, which fakeroot gives them inside the throwaway root.
if command -v fakeroot > /dev/null; then
    sys="$work/sys"
    mkdir -p "$sys/root" "$sys/db" "$sys/cache" "$sys/hooks" "$sys/keyring"
    cat > "$sys/pacman.conf" <<EOF
[options]
Architecture = auto
SigLevel = Required DatabaseRequired

[aurora]
Server = file://$out
EOF
    pm() { fakeroot pacman --config "$sys/pacman.conf" --root "$sys/root" --dbpath "$sys/db" --cachedir "$sys/cache" \
        --hookdir "$sys/hooks" --gpgdir "$sys/keyring" --logfile "$sys/pacman.log" --noconfirm "$@"; }
    fakeroot sh -c "pacman-key --gpgdir '$sys/keyring' --init && pacman-key --gpgdir '$sys/keyring' --add '$out/aurora.asc' \
        && pacman-key --gpgdir '$sys/keyring' --lsign-key '$key'" > "$work/keyring.log" 2>&1
    pm -Sy aurora-test > "$work/install.log" 2>&1; code=$?
    check "pacman installs the package from the repository" "$code" 0
    [ "$code" -eq 0 ] || cat "$work/keyring.log" "$work/install.log"
    check "the package's files are in place" "$(cat "$sys/root/usr/share/aurora-test/ok" 2> /dev/null)" installed
    # A database signed by a key pacman does not trust is refused.
    printf 'tampered' >> "$out/aurora.db"
    rm -rf "$sys/db/sync"
    pm -Sy > "$work/tampered.log" 2>&1; code=$?
    check "a tampered database is refused" "$([ "$code" -ne 0 ] && echo refused)" refused
else
    echo "SKIP pacman install: fakeroot is not installed"
fi

# A package with machine code is built only in the clean chroot (scripts/clean-build.sh): a stand-in in its place
# records the call and builds with makepkg, as the real one would in the chroot. A package for any architecture holds
# no machine code and is built here.
cat > "$work/clean-build" <<'EOF'
#!/bin/bash
echo "clean $*" >> "$CLEAN_LOG"
cd "$1" && PKGDEST="$2" makepkg --force --nodeps --noconfirm > /dev/null 2>&1
EOF
chmod +x "$work/clean-build"
mkdir "$work/native"
sed "s/^arch=('any')/arch=('x86_64')/; s/^pkgname=aurora-test/pkgname=aurora-native/" "$work/pkg/PKGBUILD" \
    > "$work/native/PKGBUILD"
: > "$work/clean.log"
CLEAN_LOG="$work/clean.log" AURORA_CLEAN_BUILD="$work/clean-build" \
    "$root/scripts/pacman-repo.sh" "$work/native" "$work/native-repo" > "$work/native.log" 2>&1; code=$?
check "a package with machine code builds" "$code" 0
[ "$code" -eq 0 ] || cat "$work/native.log"
check "it is built in the clean chroot" "$(grep -c "^clean $work/native $work/native-repo" "$work/clean.log")" 1
native="$work/native-repo/aurora-native-1.0.0-1-x86_64.pkg.tar.zst"
check "the package built in the clean chroot is signed" \
    "$(gpg --batch --verify "$native.sig" "$native" > /dev/null 2>&1 && echo ok)" ok
check "the database lists it" "$(bsdtar -tf "$work/native-repo/aurora.db" 2> /dev/null | grep -c '^aurora-native-1.0.0-1/desc$')" 1
: > "$work/clean.log"
CLEAN_LOG="$work/clean.log" AURORA_CLEAN_BUILD="$work/clean-build" \
    "$root/scripts/pacman-repo.sh" "$work/pkg" "$work/any-repo" > /dev/null 2>&1
check "a package for any architecture is built here" "$(wc -l < "$work/clean.log")" 0

exit "$failures"
