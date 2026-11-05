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
# sudo -v only checks the password; any other sudo runs its command.
# shellcheck disable=SC2016 # written into the stand-in, expanded when it runs
stub sudo 'case "$1" in -v) exit 0 ;; esac; exec "$@"'
# shellcheck disable=SC2016 # written into the stand-in, expanded when it runs
stub pacman 'case "$1" in -Q) echo "aurora-theme 1.0.0-1" ;; esac'
# Plasma's version, as `plasmashell --version` prints it; PLASMA changes it.
# shellcheck disable=SC2016 # written into the stand-in, expanded when it runs
stub plasmashell 'echo "plasmashell ${PLASMA:-6.8.2}"'
stub aurora
# The package's aurora; the stub above stands for an older copy earlier on PATH (a source install's ~/.local/bin).
mkdir "$work/usr-bin"
# shellcheck disable=SC2016 # written into the stand-in, expanded when it runs
printf '#!/bin/bash\necho "package-aurora $*" >> "%s"\necho "${AURORA_BANNER:-}" > "%s"\n' "$log" "$work/banner" > "$work/usr-bin/aurora"
chmod +x "$work/usr-bin/aurora"
stub pacman-key "case \"\$1\" in --list-keys) [ -e \"$work/trusted\" ];; --lsign-key) touch \"$work/trusted\";; esac"
# curl writes the served key where -o says, and without -o prints get.sh itself, as the one command fetches it.
stub curl "out=; while [ \$# -gt 0 ]; do [ \"\$1\" = -o ] && out=\$2; shift; done
if [ -n \"\$out\" ]; then cp \"\${SERVED_KEY:?}\" \"\$out\"; else cat \"$root/get.sh\"; fi"
# The real sudo must stay out of reach: a failed sudo counts toward pam_faillock's lockout (3 in a row lock the
# account for 10 minutes). PATH holds only the stand-ins and /usr/bin, and HOME is empty, so no startup file of the
# user's can put the real sudo first again.
mkdir "$work/home"
export PATH="$bin:/usr/bin" HOME="$work/home" XDG_SESSION_TYPE=wayland AURORA_PACMAN_CONF="$work/pacman.conf" AURORA_CLI="$work/usr-bin/aurora"
printf '[options]\nArchitecture = auto\n\n[core]\nInclude = /etc/pacman.d/mirrorlist\n' > "$AURORA_PACMAN_CONF"

SERVED_KEY="$root/packaging/repo/aurora.asc" bash "$root/get.sh" < /dev/null > "$work/first.log" 2>&1; code=$?
check "the first run succeeds" "$code" 0
[ "$code" -eq 0 ] || cat "$work/first.log"
check "Aurora's wordmark comes first" "$(grep -m1 -v '^$' "$work/first.log")" "   ▄█▀▀█▄ ██  ██ ██▀▀█▄ ▄█▀▀█▄ ██▀▀█▄ ▄█▀▀█▄"
check "the password is asked once, before anything changes" "$(grep -m1 '^sudo' "$log")" "sudo -v -p    Password for $(id -un): "
check "aurora is told the banner is on screen already" "$(cat "$work/banner")" shown
plain=$(sed 's/\x1b\[[0-9;]*m//g' "$work/first.log")
check "it names the system it installs on, its session as people write it" \
    "$(grep -c 'Your system .*KDE Plasma 6.8.2 · Wayland$' <<< "$plain")" 1
check "the four steps are numbered" "$(grep -oE '◆  [1-4]/4' <<< "$plain" | tr '\n' ' ')" "◆  1/4 ◆  2/4 ◆  3/4 ◆  4/4 "
check "each step ends with a tick and its time" "$(grep -cE '^✓  .+ \([0-9]+\.[0-9] s\)$' <<< "$plain")" 4
check "the end names the installed version" "$(grep -c 'Aurora 1.0.0 is installed' <<< "$plain")" 1
check "the key is added to pacman's keyring" "$(grep -c '^pacman-key --add ' "$log")" 1
check "the key is trusted by its fingerprint" "$(grep -c "^pacman-key --lsign-key $fpr$" "$log")" 1
check "pacman.conf gains the [aurora] section" "$(grep -cx '\[aurora\]' "$AURORA_PACMAN_CONF")" 1
check "the section reads the release named repo" "$(sed -n '/^\[aurora\]$/{n;p}' "$AURORA_PACMAN_CONF")" \
    "Server = https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/repo"
check "the system is upgraded with the package" "$(grep -c '^pacman -Syu --needed aurora-theme$' "$log")" 1
check "the package's aurora install opens last, not an older aurora on PATH" "$(tail -n1 "$log")" "package-aurora install"

: > "$log"
SERVED_KEY="$root/packaging/repo/aurora.asc" bash "$root/get.sh" < /dev/null > "$work/second.log" 2>&1; code=$?
check "a second run succeeds" "$code" 0
check "a second run adds the section no second time" "$(grep -cx '\[aurora\]' "$AURORA_PACMAN_CONF")" 1
check "a second run leaves the trusted key alone" "$(grep -c '^pacman-key --add\|^pacman-key --lsign' "$log")" 0
check "a second run still upgrades and opens aurora" "$(grep -c '^pacman -Syu\|^package-aurora' "$log")" 2

# A desktop without Plasma 6 is told so before anything changes.
: > "$log"
PLASMA=5.27.11 SERVED_KEY="$root/packaging/repo/aurora.asc" bash "$root/get.sh" < /dev/null > "$work/plasma5.log" 2>&1; code=$?
check "Plasma 5 is refused" "$([ "$code" -ne 0 ] && echo refused)" refused
check "nothing runs with sudo on Plasma 5" "$(grep -c '^sudo' "$log")" 0
check "the refusal says what Aurora needs" "$(grep -c 'Aurora needs KDE Plasma 6' "$work/plasma5.log")" 1

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
check "nothing is trusted, added or installed with another key" "$(grep -c '^pacman-key --add\|^pacman-key --lsign\|^pacman \|aurora ' "$log")" 0
check "pacman.conf is left alone with another key" "$(grep -cx '\[aurora\]' "$AURORA_PACMAN_CONF")" 0

# README.md's Installation shows get.sh as the one command, piped into bash: fish (CachyOS's default shell) has no
# process substitution, so `bash <(curl ...)` fails there.
command="curl -fsSL https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/get.sh | bash"
check "README.md installs with get.sh piped into bash" "$(grep -cxF "$command" "$root/README.md")" 1
# Each shell starts without the user's startup files, which could put the real curl and sudo first on PATH.
for shell in "fish --no-config" "zsh -f" "bash --norc"; do
    command -v "${shell%% *}" > /dev/null || continue
    : > "$log"
    # shellcheck disable=SC2086 # the shell and its flag are two words
    found=$($shell -c 'command -v sudo')
    check "${shell%% *} finds the stand-in sudo, never the real one" "$found" "$bin/sudo"
    [ "$found" = "$bin/sudo" ] || continue
    # shellcheck disable=SC2086
    SERVED_KEY="$root/packaging/repo/aurora.asc" setsid $shell -c "$command" > "$work/${shell%% *}.log" 2>&1
    check "the command installs and opens the package's aurora in ${shell%% *}" "$(tail -n1 "$log")" "package-aurora install"
done


exit "$failures"
