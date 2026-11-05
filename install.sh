#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Aurora: the whole install in one command: packages, build, files, system links and the look. See README.md and
# docs/install.md.
#
#   ./install.sh [--performance] [--rebuild] [--reset-panels] [--check] [--yes]
set -euo pipefail
ROOT=$(dirname "$(readlink -f "$0")")

# Prints the help text; the callers choose the stream and the exit status.
usage() {
    cat <<'EOF'
Usage: ./install.sh [options]

Turns KDE Plasma 6 on CachyOS (or any Arch-based system) into the Aurora desktop. It installs the packages
the theme needs, builds its parts for your Plasma, saves your current settings so ./uninstall.sh can put them back,
installs everything into your home folder and applies the look. sudo asks for your password for pacman and for
two links KWin needs in the system plugin folder; nothing else outside your home folder changes.

Options:
  --performance   also apply the measured CPU scheduler, memory and zram tuning (docs/performance.md)
  --rebuild       build and install the parts again after a Plasma, KWin or Qt update
  --reset-panels  put up the menu bar and the Dock again even when they are already there
  --check         show what is missing; change nothing
  --yes           install packages without asking
  --version       print the version
  -h, --help      show this help
EOF
}

performance=false rebuild=false reset_panels=false check_only=false noconfirm=()
for arg in "$@"; do
    case "$arg" in
        --performance) performance=true ;;
        --rebuild) rebuild=true ;;
        --reset-panels) reset_panels=true ;;
        --check) check_only=true ;;
        --yes) noconfirm=(--noconfirm) ;;
        -h|--help) usage; exit 0 ;;
        --version) echo "Aurora $(cat "$ROOT/VERSION")"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

# Where the parts come from. The aurora-theme package ships them built (prebuilt/), so it builds nothing unless
# --rebuild asks for it; a checkout builds them next to its files, and a read-only copy in the user's cache. build.sh
# and install.py read BUILD_DIR.
prebuilt=false
if [ -z "${BUILD_DIR:-}" ]; then
    if [ -d "$ROOT/prebuilt" ] && ! $rebuild; then
        BUILD_DIR="$ROOT/prebuilt"
        prebuilt=true
    elif [ -w "$ROOT" ]; then
        BUILD_DIR="$ROOT/build"
    else
        BUILD_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/aurora/build"
    fi
fi
export BUILD_DIR

# Prints a step heading in bold. Arguments: the heading words.
say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
# Prints an error to stderr and exits with status 1. Arguments: the message words.
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
# Runs a command as root through sudo: with the graphical password prompt when SUDO_ASKPASS is set, in the terminal
# otherwise. Arguments: the command and its arguments.
as_root() { if [ -n "${SUDO_ASKPASS:-}" ]; then sudo -A "$@"; else sudo "$@"; fi; }

# Refuse to run as root, or on a system without pacman or KDE Plasma 6.
[ "$(id -u)" -ne 0 ] || fail "run it as your own user, not with sudo: it installs into your home folder"
command -v pacman > /dev/null || fail "this installer needs pacman (CachyOS, Arch Linux or another Arch-based system)"
plasma=$(pacman -Q plasma-workspace 2>/dev/null | awk '{print $2}' | sed 's/-[^-]*$//') || true
[ "${plasma%%.*}" = 6 ] || fail "KDE Plasma 6 is not installed (plasma-workspace: ${plasma:-none})"

# The package names in the lists, without comments and blank lines: what Aurora runs with, and, when the parts are
# built here, what builds them.
lists=("$ROOT/installer/dependencies.txt")
$prebuilt || lists+=("$ROOT/installer/build-dependencies.txt")
packages=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "${lists[@]}" | awk '{print $1}')
if $performance; then packages="$packages scx-scheds scx-tools"; fi
# `pacman -T` prints the packages of the list that are not installed.
# shellcheck disable=SC2086 # one argument per package
missing=$(pacman -T $packages || true)

if $check_only; then
    say "Checking (nothing changes)"
    echo "Plasma $plasma"
    echo "build folder: $BUILD_DIR"
    if [ -n "$missing" ]; then echo "packages to install: ${missing//$'\n'/ }"; else echo "every package is installed"; fi
    ready=true
    [ -z "$missing" ] || ready=false
    python3 -I "$ROOT/installer/install.py" check || ready=false
    $ready && echo "ready: ./install.sh installs from the existing build" || echo "./install.sh downloads and builds what is missing"
    $ready
    exit $?
fi

# 1/6: packages.
say "1/6 Packages"
if [ -n "$missing" ]; then
    echo "installing: ${missing//$'\n'/ }"
    # shellcheck disable=SC2086 # one argument per package name
    as_root pacman -S --needed "${noconfirm[@]}" $missing
else
    echo "every package is installed"
fi

# 2/6: Aurora's themes, all in this repository: the Plasma style, colors, Kvantum, GTK, cursors and icons.
say "2/6 Themes"
for theme in plasma/Aurora colors/Aurora.colors kvantum/Aurora gtk/Aurora.tar.xz cursors/Aurora icons/base/Aurora-base.tar.xz; do
    [ -e "$ROOT/themes/$theme" ] || fail "themes/$theme is missing: this copy of Aurora is incomplete"
done
echo "all present"

# 3/6: build.
say "3/6 Building the parts for Plasma $plasma"
if $prebuilt; then
    # A part built for another Plasma, KWin or Qt is set aside at login, and Plasma uses its own, until the package
    # brings it built for this one (or `aurora rebuild` builds it here).
    echo "the package's parts, built already"
    "$ROOT/installer/build.sh" --current || echo "Plasma keeps its own for those until the next package update (or aurora rebuild)"
else
    "$ROOT/installer/build.sh" || echo "some parts were not built; Plasma keeps its own for those (see above)"
fi

# 4/6: files into the home folder: themes, fonts, icons, widgets, the built parts. The first install saves the
# settings it will change, so ./uninstall.sh can put them back.
say "4/6 Installing into $HOME"
# Runs one step of installer/install.py. Arguments: the step name and its options.
install_py() { python3 -I "$ROOT/installer/install.py" "$@"; }
if $rebuild; then
    install_py components
else
    install_py backup
    install_py files
fi

# 5/6: system. KWin starts before the login script sets the plugin path, so the window buttons and the pointing
# hand are linked into the system plugin folders. The tuning is optional and measured (docs/performance.md).
say "5/6 System"
plugins="$HOME/.local/lib/qt6/plugins"
# The root steps are collected and run in one sudo call (installer/root-steps.sh), so the password is asked once;
# the paths go to it as arguments, never inside shell code.
root_steps=()
for plugin in org.kde.kdecoration3/org.aurora.decoration.so kwin/plugins/aurora-pointer.so; do
    link="/usr/lib/qt6/plugins/$plugin"
    if [ -e "$plugins/$plugin" ] && [ "$(readlink "$link" 2>/dev/null)" != "$plugins/$plugin" ]; then
        root_steps+=(link "$plugins/$plugin" "$link")
    fi
done
if $performance; then
    root_steps+=(tune "$ROOT/performance/system")
fi
if [ ${#root_steps[@]} -gt 0 ]; then
    as_root bash "$ROOT/installer/root-steps.sh" "${root_steps[@]}"
    qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin aurora-pointer > /dev/null 2>&1 || true
    $performance && echo "performance tuning applied (zram's new compressor takes effect at the next boot)"
else
    echo "nothing to change"
fi

# 6/6: the look and the panels (a rebuild leaves both as they are).
say "6/6 Look and panels"
if $rebuild; then
    qdbus6 org.kde.KWin /KWin reconfigure > /dev/null 2>&1 || true
    echo "kept"
else
    install_py look
    if $reset_panels; then install_py panels --force; else install_py panels; fi
fi

say "Done"
if $rebuild; then
    echo "Log out and back in to load the rebuilt parts."
else
    echo "Log out and back in once, so KWin and every app start with the new look."
    echo "To go back: ./uninstall.sh"
fi
