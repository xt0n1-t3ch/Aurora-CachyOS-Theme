#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The part of the install that runs as root. install.sh runs it once through sudo, so the password is asked once.
# Each step is a word followed by its arguments; paths arrive as arguments and are never read as shell code, so a home
# folder or checkout whose path holds quotes or $(...) cannot run anything as root.
#
#   root-steps.sh link SOURCE LINK [STEP...]   links LINK to SOURCE (KWin's plugins, see install.sh step 5)
#   root-steps.sh tune FOLDER [STEP...]        installs the performance tuning in FOLDER (performance/system)
set -euo pipefail

# Prints a usage error to stderr and exits with status 2. Arguments: the message words.
usage() { echo "root-steps.sh: $*" >&2; exit 2; }

while [ $# -gt 0 ]; do
    case "$1" in
        link)
            [ $# -ge 3 ] || usage "link needs SOURCE and LINK"
            ln -sfn -- "$2" "$3"
            shift 3 ;;
        tune)
            [ $# -ge 2 ] || usage "tune needs the folder of the tuning files"
            system=$2
            install -Dm644 "$system/scx_loader.toml" /etc/scx_loader.toml
            systemctl enable --now scx_loader.service
            install -Dm644 "$system/thp.conf" /etc/tmpfiles.d/thp.conf
            echo defer > /sys/kernel/mm/transparent_hugepage/defrag
            install -Dm644 "$system/99-aurora-memory.conf" /etc/sysctl.d/99-aurora-memory.conf
            sysctl -q --system
            install -Dm644 "$system/90-aurora-zram.conf" /etc/systemd/zram-generator.conf.d/90-aurora-zram.conf
            shift 2 ;;
        *) usage "unknown step: $1" ;;
    esac
done
