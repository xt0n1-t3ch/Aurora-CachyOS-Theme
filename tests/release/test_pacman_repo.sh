#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/pacman-repo.sh builds Aurora's pacman repository: a signed package, a signed database named aurora whose
# files are real files (GitHub release assets cannot be links) and the signing key's public half. The last checks
# install the package with pacman from that folder, the way a user's pacman.conf section reads the release, under
# fakeroot in a throwaway root. Uses a throwaway signing key and a small package, so it needs no network.
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

exit "$failures"
