#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds a package for every x86-64 processor in a clean chroot of Arch Linux's own packages, then starts each of its
# programs and libraries on an emulated older processor: scripts/clean-build.sh PKGBUILD_DIR OUT_DIR
#
# A package built on this PC links objects of its own toolchain, and CachyOS builds that toolchain for x86-64-v3: its
# glibc marks every program as needing x86-64-v3 (issue #14) and its libgcc uses newer instructions (issue #11). The
# chroot (devtools' mkarchroot and makechrootpkg, packaging/repo/chroot-pacman.conf and Arch's generic makepkg.conf)
# holds only Arch's packages, built for the x86-64 baseline. scripts/built_for.py then checks that `aurora update`
# installs the result on CachyOS and Arch alike, and scripts/old_cpu.py runs it in that chroot under QEMU's user mode
# on a processor without AVX2. Root comes through sudo, asked once (SUDO_ASKPASS for a
# graphical prompt). AURORA_CHROOT names another chroot folder than build/chroot.
set -euo pipefail
ROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
# Prints a step heading in bold. Arguments: the heading words.
say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

src=${1:-}
out=${2:-}
[ -n "$src" ] && [ -n "$out" ] || fail "usage: scripts/clean-build.sh PKGBUILD_DIR OUT_DIR"
[ -f "$src/PKGBUILD" ] || fail "$src has no PKGBUILD"
for tool in mkarchroot makechrootpkg arch-nspawn qemu-x86_64-static; do
    command -v "$tool" > /dev/null || fail "$tool is missing: sudo pacman -S devtools qemu-user-static"
done
src=$(readlink -f "$src")
mkdir -p "$out"
out=$(readlink -f "$out")
chroot=$(readlink -m "${AURORA_CHROOT:-$ROOT/build/chroot}")
pacman_conf="$ROOT/packaging/repo/chroot-pacman.conf"
# Arch's own makepkg.conf for x86_64 (-march=x86-64 -mtune=generic), from devtools, not this PC's.
makepkg_conf=/usr/share/devtools/makepkg.conf.d/x86_64.conf
[ -f "$makepkg_conf" ] || fail "$makepkg_conf is missing: sudo pacman -S devtools"

# One password prompt for the whole build: sudo's timestamp is renewed every minute until the script ends.
sudo ${SUDO_ASKPASS:+-A} -v
( while sudo -n -v 2> /dev/null; do sleep 60; done ) &
keepalive=$!
trap 'kill "$keepalive" 2> /dev/null' EXIT

if [ -d "$chroot/root" ]; then
    say "Updating the clean chroot"
    sudo arch-nspawn -C "$pacman_conf" -M "$makepkg_conf" "$chroot/root" pacman -Syu --needed --noconfirm ccache
else
    say "Creating the clean chroot"
    mkdir -p "$chroot"
    sudo mkarchroot -C "$pacman_conf" -M "$makepkg_conf" "$chroot/root" base-devel ccache
fi
# The C++ parts compile through ccache when it is installed (installer/build.sh). Its cache lives outside the chroot's
# copies, which -c below throws away, so a release compiles only what changed since the last one. Paths under
# /build count relative to the build folder, so the version in the sources' folder name (aurora-1.3.1) does not
# change the key, and debug info skips the build folder's path for the same reason (ccache(1), base_dir, hash_dir).
# The cache holds only what this chroot's own compilers wrote.
cache="$chroot/ccache"
mkdir -p "$cache"
printf 'cache_dir = %s\nbase_dir = /build\nhash_dir = false\nmax_size = 5G\n' "$cache" |
    sudo tee "$chroot/root/etc/ccache.conf" > /dev/null

say "Building in the clean chroot"
# -c starts from a fresh copy of the chroot, -d lends it the compiler cache; PKGDEST and SRCDEST pass through
# makechrootpkg's sudo.
(cd "$src" && PKGDEST="$out" makechrootpkg -c -d "$cache" -r "$chroot" -- --noconfirm)
mapfile -t packages < <(cd "$src" && PKGDEST="$out" makepkg --config "$makepkg_conf" --packagelist)
for package in "${packages[@]}"; do
    [ -f "$package" ] || fail "makechrootpkg did not write $package"
done

say "Checking that aurora update installs it"
# The chroot holds Arch's Plasma and a CachyOS user has CachyOS's rebuild of it; `aurora update` installs the package
# only when they are the same versions. checkupdates writes a fresh copy of this PC's repositories without root and
# without touching pacman's own databases.
repositories="$chroot/repositories"
CHECKUPDATES_DB="$repositories" checkupdates > /dev/null || [ $? -eq 2 ] || fail "checkupdates could not read the repositories"
for package in "${packages[@]}"; do
    python3 -I "$ROOT/scripts/built_for.py" "$package" "$repositories"
done

say "Starting the package on an older processor"
copy="$chroot/$(id -un)"
for package in "${packages[@]}"; do
    sudo cp "$package" "$copy/root/"
    sudo arch-nspawn "$copy" pacman -U --noconfirm "/root/$(basename "$package")"
    sudo arch-nspawn "$copy" pacman -S --needed --noconfirm python
    python3 -I "$ROOT/scripts/old_cpu.py" "$copy" "$package"
done
