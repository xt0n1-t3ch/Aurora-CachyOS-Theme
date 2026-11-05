#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# get.sh, the one command of README.md's Installation: it trusts Aurora's key after checking it is the one it expects,
# adds the [aurora] section to pacman.conf once, upgrades the system with aurora-theme, then opens `aurora`. sudo,
# pacman, pacman-key, curl and aurora are stand-ins that log their arguments, curl serves the repository's own
# aurora.asc (or another key), and gpg is the real one; pacman.conf is a copy. Needs no network and no root.
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

fpr=$(gpg --show-keys --with-colons "$root/packaging/repo/aurora.asc" 2> /dev/null | awk -F: '$1 == "fpr" { print $10; exit }')
bin="$work/bin"
mkdir "$bin"
log="$work/calls.log"
# Each stand-in appends "name args" to the log. pacman-key keeps a trusted mark once --lsign-key ran.
stub() { printf '#!/bin/bash\necho "%s $*" >> "%s"\n%s\n' "$1" "$log" "${2:-}" > "$bin/$1"; chmod +x "$bin/$1"; }
stub sudo 'exec "$@"'
stub pacman
stub aurora
stub pacman-key "case \"\$1\" in --list-keys) [ -e \"$work/trusted\" ];; --lsign-key) touch \"$work/trusted\";; esac"
stub curl "while [ \$# -gt 1 ]; do [ \"\$1\" = -o ] && cp \"\${SERVED_KEY:?}\" \"\$2\"; shift; done"
export PATH="$bin:$PATH" AURORA_PACMAN_CONF="$work/pacman.conf"
printf '[options]\nArchitecture = auto\n\n[core]\nInclude = /etc/pacman.d/mirrorlist\n' > "$AURORA_PACMAN_CONF"

SERVED_KEY="$root/packaging/repo/aurora.asc" bash "$root/get.sh" < /dev/null > "$work/first.log" 2>&1; code=$?
check "the first run succeeds" "$code" 0
[ "$code" -eq 0 ] || cat "$work/first.log"
check "the key is added to pacman's keyring" "$(grep -c '^pacman-key --add ' "$log")" 1
check "the key is trusted by its fingerprint" "$(grep -c "^pacman-key --lsign-key $fpr$" "$log")" 1
check "pacman.conf gains the [aurora] section" "$(grep -cx '\[aurora\]' "$AURORA_PACMAN_CONF")" 1
check "the section reads the release named repo" "$(sed -n '/^\[aurora\]$/{n;p}' "$AURORA_PACMAN_CONF")" \
    "Server = https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/repo"
check "the system is upgraded with the package" "$(grep -c '^pacman -Syu --needed aurora-theme$' "$log")" 1
check "aurora opens last" "$(tail -n1 "$log")" "aurora "

: > "$log"
SERVED_KEY="$root/packaging/repo/aurora.asc" bash "$root/get.sh" < /dev/null > "$work/second.log" 2>&1; code=$?
check "a second run succeeds" "$code" 0
check "a second run adds the section no second time" "$(grep -cx '\[aurora\]' "$AURORA_PACMAN_CONF")" 1
check "a second run leaves the trusted key alone" "$(grep -c '^pacman-key --add\|^pacman-key --lsign' "$log")" 0
check "a second run still upgrades and opens aurora" "$(grep -c '^pacman -Syu\|^aurora' "$log")" 2

# A served key that is not Aurora's is refused before pacman trusts or installs anything.
rm "$work/trusted"
: > "$log"
cp "$AURORA_PACMAN_CONF" "$work/before.conf"
sed -i '/^\[aurora\]$/,+1d' "$AURORA_PACMAN_CONF"
export GNUPGHOME="$work/gnupg"
mkdir -m 700 "$GNUPGHOME"
gpg --batch --quiet --passphrase '' --quick-gen-key "Someone else <other@example.org>" ed25519 sign never 2> /dev/null
gpg --batch --armor --export other@example.org > "$work/other.asc" 2> /dev/null
SERVED_KEY="$work/other.asc" bash "$root/get.sh" < /dev/null > "$work/other.log" 2>&1; code=$?
gpgconf --homedir "$GNUPGHOME" --kill all 2> /dev/null
check "another key is refused" "$([ "$code" -ne 0 ] && echo refused)" refused
check "nothing is trusted, added or installed with another key" "$(grep -c '^pacman-key --add\|^pacman-key --lsign\|^pacman \|^aurora' "$log")" 0
check "pacman.conf is left alone with another key" "$(grep -cx '\[aurora\]' "$AURORA_PACMAN_CONF")" 0

# README.md's Installation shows get.sh as the one command.
check "README.md installs with get.sh" \
    "$(grep -cF 'bash <(curl -fsSL https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/get.sh)' "$root/README.md")" 1

exit "$failures"
