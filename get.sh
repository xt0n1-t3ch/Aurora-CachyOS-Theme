#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Installs Aurora with one command:
#
#   bash <(curl -fsSL https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/get.sh)
#
#  1. trusts Aurora's signing key in pacman's keyring, after checking the downloaded key is the one named below;
#  2. adds Aurora's pacman repository, [aurora], to pacman.conf;
#  3. upgrades the system with the aurora-theme package (pacman -Syu: Arch supports no partial upgrades);
#  4. opens `aurora`, the guided install, which does the rest.
#
# Steps 1 and 2 happen once; running it again only upgrades and opens aurora. Run it as yourself: sudo asks for the
# password. AURORA_PACMAN_CONF names another pacman.conf (the tests use it).
set -euo pipefail

# The fingerprint of packaging/repo/aurora.asc; tests/installer/test_get.sh keeps the two equal.
KEY=657D8110799A4E65BE4E1AB4F028284112794CC2
REPO=https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme
KEY_URL=https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/packaging/repo/aurora.asc
SERVER=$REPO/releases/download/repo
CONF=${AURORA_PACMAN_CONF:-/etc/pacman.conf}
# The downloaded key, removed on exit.
KEY_FILE=""
trap 'rm -f "$KEY_FILE"' EXIT

# Prints a step heading in bold. Arguments: the heading words.
say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# Runs the four steps. The whole file is read before it starts, so `curl ... | bash` works as well as `bash <(...)`.
main() {
    [ "$(id -u)" -ne 0 ] || fail "run it as yourself, not with sudo: sudo asks for your password when it needs it"
    command -v pacman > /dev/null || fail "Aurora needs CachyOS or another Arch-based system with pacman"

    say "Trusting Aurora's signing key"
    if sudo pacman-key --list-keys "$KEY" > /dev/null 2>&1; then
        echo "already trusted"
    else
        KEY_FILE=$(mktemp)
        curl -fsSL "$KEY_URL" -o "$KEY_FILE"
        local got
        got=$(gpg --show-keys --with-colons "$KEY_FILE" 2> /dev/null | awk -F: '$1 == "fpr" { print $10; exit }')
        [ "$got" = "$KEY" ] || fail "the downloaded key is ${got:-not a key}, not Aurora's $KEY; nothing was changed"
        sudo pacman-key --add "$KEY_FILE"
        sudo pacman-key --lsign-key "$KEY"
    fi

    say "Adding Aurora's pacman repository"
    if grep -qx '\[aurora\]' "$CONF"; then
        echo "already in $CONF"
    else
        printf '\n[aurora]\nServer = %s\n' "$SERVER" | sudo tee -a "$CONF" > /dev/null
        echo "added [aurora] to $CONF"
    fi

    say "Installing the aurora-theme package"
    sudo pacman -Syu --needed aurora-theme

    say "Opening Aurora's guided install"
    aurora
}

# With `curl ... | bash`, the script itself is bash's input, so pacman's and aurora's questions read the terminal.
if [ ! -t 0 ] && { : < /dev/tty; } 2> /dev/null; then
    main "$@" < /dev/tty
else
    main "$@"
fi
