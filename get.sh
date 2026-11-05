#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Installs Aurora with one command:
#
#   curl -fsSL https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/get.sh | bash
#
#  1. trusts Aurora's signing key in pacman's keyring, after checking the downloaded key is the one named below;
#  2. adds Aurora's pacman repository, [aurora], to pacman.conf;
#  3. upgrades the system with the aurora-theme package (pacman -Syu: Arch supports no partial upgrades);
#  4. opens `aurora install`, the guided install, which asks for the menu bar's logo and does the rest; on a desktop
#     that has Aurora already, it installs this version over it.
#
# Steps 1 and 2 happen once; running it again only upgrades and opens aurora. Run it as yourself: sudo asks for the
# password. AURORA_PACMAN_CONF names another pacman.conf and AURORA_CLI another aurora (the tests use them).
set -euo pipefail

# The fingerprint of packaging/repo/aurora.asc; tests/installer/test_get.sh keeps the two equal.
KEY=657D8110799A4E65BE4E1AB4F028284112794CC2
REPO=https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme
KEY_URL=https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/packaging/repo/aurora.asc
SERVER=$REPO/releases/download/repo
CONF=${AURORA_PACMAN_CONF:-/etc/pacman.conf}
# The package's aurora, by its path: an older copy from a source install (~/.local/bin) can come first on PATH.
CLI=${AURORA_CLI:-/usr/bin/aurora}
# The downloaded key, removed on exit.
KEY_FILE=""
trap 'rm -f "$KEY_FILE"' EXIT

# "AURORA" in half-block letters, the same wordmark as the aurora command's banner (cli/src/ui.rs).
WORDMARK=(
    "▄█▀▀█▄ ██  ██ ██▀▀█▄ ▄█▀▀█▄ ██▀▀█▄ ▄█▀▀█▄"
    "██  ██ ██  ██ ██▄▄█▀ ██  ██ ██▄▄█▀ ██  ██"
    "██▀▀██ ██  ██ ██▀█▄  ██  ██ ██▀█▄  ██▀▀██"
    "██  ██ ▀█▄▄█▀ ██  ██ ▀█▄▄█▀ ██  ██ ██  ██"
)
# The aurora the wordmark runs through, left to right: mint, sky, violet, pink (cli/src/ui.rs, AURORA).
STOPS=(79 242 176 56 189 248 167 139 250 244 114 182)

# Colors only on a terminal. Arguments: an SGR code (such as 1 or 38;2;R;G;B). Prints the escape or nothing.
sgr() { [ -t 1 ] && printf '\033[%sm' "$1"; return 0; }
# Prints the wordmark with the aurora running through it, then what this is and who made it.
banner() {
    local LC_ALL=C.UTF-8 row column at index stop part r g b width=${#WORDMARK[0]}
    echo
    for row in "${WORDMARK[@]}"; do
        printf '   '
        for ((column = 0; column < ${#row}; column++)); do
            # Three stretches between the four colors, in thousandths.
            at=$((column * 3000 / width)) index=$((at / 1000)) part=$((at % 1000))
            stop=$((index * 3))
            r=$((STOPS[stop] + (STOPS[stop + 3] - STOPS[stop]) * part / 1000))
            g=$((STOPS[stop + 1] + (STOPS[stop + 4] - STOPS[stop + 1]) * part / 1000))
            b=$((STOPS[stop + 2] + (STOPS[stop + 5] - STOPS[stop + 2]) * part / 1000))
            sgr "1;38;2;$r;$g;$b"
            printf '%s' "${row:column:1}"
        done
        sgr 0
        echo
    done
    printf '\n   %sThe macOS Tahoe look for KDE Plasma, made for CachyOS%s\n' "$(sgr 1)" "$(sgr 0)"
    printf '   %sby xt0n1%s\n\n' "$(sgr 2)" "$(sgr 0)"
}
# The steps, drawn like the aurora command's prompts: a violet bar, sky for what runs, mint for what is done.
VIOLET='38;2;167;139;250' SKY='38;2;56;189;248' MINT='38;2;79;242;176'
STEPS=4
step_number=0
step_started=0
# Milliseconds since the epoch.
now_ms() { echo $(($(date +%s%N) / 1000000)); }
# Prints a span of milliseconds the way people read it: 0.4 s, 12.0 s, 1 min 5 s. Arguments: the milliseconds.
span() {
    if [ "$1" -lt 60000 ]; then
        printf '%d.%d s' $(($1 / 1000)) $(($1 % 1000 / 100))
    else
        printf '%d min %d s' $(($1 / 60000)) $(($1 % 60000 / 1000))
    fi
}
bar() { printf '%s│%s\n' "$(sgr "$VIOLET")" "$(sgr 0)"; }
# Prints the next step and starts its clock. Arguments: the words.
step() {
    step_number=$((step_number + 1))
    step_started=$(now_ms)
    bar
    printf '%s◆%s  %s%d/%d%s  %s\n' "$(sgr "$SKY")" "$(sgr 0)" "$(sgr 2)" "$step_number" "$STEPS" "$(sgr 0)" "$*"
}
# Prints, under the step, in a line, why it is there. Arguments: the words.
detail() { printf '%s│%s      %s%s%s\n' "$(sgr "$VIOLET")" "$(sgr 0)" "$(sgr 2)" "$*" "$(sgr 0)"; }
# Prints that the step is done, with how long it took. Arguments: the words.
done_step() {
    printf '%s✓%s  %s  %s(%s)%s\n' "$(sgr "$MINT")" "$(sgr 0)" "$*" "$(sgr 2)" "$(span $(($(now_ms) - step_started)))" \
        "$(sgr 0)"
}
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '%s└  %s%s\n' "$(sgr 31)" "$*" "$(sgr 0)" >&2; exit 1; }

# Plasma's version, as plasmashell prints it ("plasmashell 6.8.2"), or nothing.
plasma_version() { plasmashell --version 2> /dev/null | awk '{ print $2; exit }'; }
# The session as people write it: Wayland, X11, or what XDG_SESSION_TYPE says.
session_name() {
    case "${XDG_SESSION_TYPE:-}" in
        wayland) echo Wayland ;;
        x11) echo X11 ;;
        *) echo "${XDG_SESSION_TYPE:-a desktop session}" ;;
    esac
}
# The system's name from os-release, such as "CachyOS Linux".
system_name() { (. /etc/os-release 2> /dev/null && echo "${PRETTY_NAME:-${NAME:-Linux}}") || echo Linux; }

# Runs the four steps. The whole file is read before it starts, so `curl ... | bash` works as well as `bash <(...)`.
main() {
    local started plasma
    started=$(now_ms)
    banner
    [ "$(id -u)" -ne 0 ] || fail "Please run this as your normal user, without sudo. It asks for your password when it needs it."
    command -v pacman > /dev/null || fail "Aurora is made for CachyOS and other Arch-based systems with pacman."
    plasma=$(plasma_version)
    [ "${plasma%%.*}" = 6 ] || fail "Aurora needs KDE Plasma 6, and this system has ${plasma:+Plasma $plasma}${plasma:-no Plasma}. Nothing was changed."
    printf '   %sDetected%s  %s, KDE Plasma %s, %s\n\n' "$(sgr 2)" "$(sgr 0)" "$(system_name)" "$plasma" \
        "$(session_name)"
    printf '%s┌%s  %sInstalling Aurora%s\n' "$(sgr "$VIOLET")" "$(sgr 0)" "$(sgr 1)" "$(sgr 0)"

    # One password, asked first and explained, before anything changes; the steps below reuse it.
    step "Permission to install"
    detail "pacman needs your password to add Aurora. You only type it once."
    sudo -v -p "   Password: " || fail "The password was not accepted. Nothing was changed."
    done_step "Thank you"

    step "Aurora's signature"
    detail "Every Aurora download is signed, so pacman can check it really comes from Aurora."
    if sudo pacman-key --list-keys "$KEY" > /dev/null 2>&1; then
        done_step "Already trusted"
    else
        KEY_FILE=$(mktemp)
        curl -fsSL "$KEY_URL" -o "$KEY_FILE"
        local got
        got=$(gpg --show-keys --with-colons "$KEY_FILE" 2> /dev/null | awk -F: '$1 == "fpr" { print $10; exit }')
        [ "$got" = "$KEY" ] || fail "The downloaded key is not Aurora's ($KEY), so nothing was trusted or changed."
        sudo pacman-key --add "$KEY_FILE" > /dev/null 2>&1
        sudo pacman-key --lsign-key "$KEY" > /dev/null 2>&1
        done_step "Verified and trusted"
    fi

    step "Aurora's download source"
    detail "pacman learns where to find Aurora and, later, its updates."
    if grep -qx '\[aurora\]' "$CONF"; then
        # A section left by an older address or a test server is pointed back at Aurora's own.
        if sed -n '/^\[aurora\]$/{n;p}' "$CONF" | grep -qxF "Server = $SERVER"; then
            done_step "Already set up"
        else
            sudo sed -i "/^\[aurora\]\$/{n;s#^Server = .*#Server = $SERVER#}" "$CONF"
            done_step "Pointed at Aurora's address"
        fi
    else
        printf '\n[aurora]\nServer = %s\n' "$SERVER" | sudo tee -a "$CONF" > /dev/null
        done_step "Added to $CONF"
    fi

    step "Downloading Aurora"
    detail "It comes ready to use, nothing to compile. Your system updates come along, as Arch requires."
    sudo pacman -Syu --needed aurora-theme
    done_step "Installed"

    local version
    version=$(pacman -Q aurora-theme 2> /dev/null | awk '{ sub(/-1$/, "", $2); print $2 }')
    bar
    printf '%s◇%s  %sAurora%s is ready for KDE Plasma %s%s%s, in %s%s\n' "$(sgr "$MINT")" "$(sgr 0)" "$(sgr 1)" \
        "${version:+ $version}" "$plasma" "$(sgr 0)" "$(sgr 2)" "$(span $(($(now_ms) - started)))" "$(sgr 0)"
    bar
    printf '%s└%s  Last step: choose the logo for your menu bar\n' "$(sgr "$VIOLET")" "$(sgr 0)"
    # The banner is on screen already, so aurora does not draw it twice.
    AURORA_BANNER=shown "$CLI" install
}

# With `curl ... | bash`, the script itself is bash's input, so pacman's and aurora's questions read the terminal.
if [ ! -t 0 ] && { : < /dev/tty; } 2> /dev/null; then
    main "$@" < /dev/tty
else
    main "$@"
fi
