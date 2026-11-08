#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Checks a published pacman repository the way a user's pacman sees it: scripts/verify-repo.sh SERVER OUT_DIR [SEED_DB]
#
# A throwaway pacman, which trusts only the key in OUT_DIR/aurora.asc as get.sh sets it up, syncs [aurora] from
# SERVER, then each package of OUT_DIR must be the version the database offers, download, pass pacman's signature
# check and be the same file as in OUT_DIR. SEED_DB is a database synced before the upload, with its server time as
# its file time (curl -R): pacman then asks the server only for a newer one, as on a PC that synced the last release.
# Runs pacman under fakeroot; touches nothing of this PC's pacman.
set -euo pipefail
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

server=${1:-}
out=${2:-}
seed=${3:-}
[ -n "$server" ] && [ -d "$out" ] || fail "usage: scripts/verify-repo.sh SERVER OUT_DIR [SEED_DB]"
work=$(mktemp -d)
# Stops the throwaway keyring's gpg-agent, then removes the folder.
trap 'gpgconf --homedir "$work/keyring" --kill all 2> /dev/null; rm -rf "$work"' EXIT
mkdir -p "$work/root" "$work/db/sync" "$work/cache" "$work/hooks" "$work/keyring"
cat > "$work/pacman.conf" <<EOF
[options]
Architecture = auto
SigLevel = Required DatabaseOptional

[aurora]
Server = $server
EOF
[ -n "$seed" ] && [ -f "$seed" ] && cp -p "$seed" "$work/db/sync/aurora.db"
key=$(gpg --show-keys --with-colons "$out/aurora.asc" 2> /dev/null | awk -F: '$1 == "fpr" { print $10; exit }')
fakeroot sh -c "pacman-key --gpgdir '$work/keyring' --init && pacman-key --gpgdir '$work/keyring' --add '$out/aurora.asc' \
    && pacman-key --gpgdir '$work/keyring' --lsign-key '$key'" > "$work/keyring.log" 2>&1 ||
    fail "could not trust the key of $out/aurora.asc: $(tail -1 "$work/keyring.log")"
# pm ARGS: this throwaway pacman.
pm() { fakeroot pacman --config "$work/pacman.conf" --root "$work/root" --dbpath "$work/db" --cachedir "$work/cache" \
    --hookdir "$work/hooks" --gpgdir "$work/keyring" --logfile "$work/pacman.log" --noconfirm "$@"; }

pm -Sy > "$work/sync.log" 2>&1 || fail "pacman could not sync [aurora] from $server: $(tail -2 "$work/sync.log")"
for package in "$out"/*.pkg.tar.zst; do
    [ -e "$package" ] || fail "$out holds no package"
    info=$(bsdtar -xOqf "$package" .PKGINFO)
    name=$(sed -n 's/^pkgname = //p' <<< "$info")
    version=$(sed -n 's/^pkgver = //p' <<< "$info")
    offered=$(pm -Si "$name" 2> /dev/null | sed -n 's/^Version *: //p' | head -1)
    [ "$offered" = "$version" ] || fail "pacman sees $name ${offered:-nowhere} in [aurora], not $version"
    pm -Sw -dd "$name" > "$work/download.log" 2>&1 ||
        fail "pacman could not download or verify $name: $(tail -2 "$work/download.log")"
    cmp -s "$work/cache/$(basename "$package")" "$package" || fail "the $name served is not the one in $out"
    echo "pacman gets $name $version from $server, signed and the same file"
done
