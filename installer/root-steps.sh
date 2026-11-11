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
#   root-steps.sh untune [STEP...]             puts CachyOS's scheduler and memory settings back
#   root-steps.sh search BUILT SYSTEM [STEP...] installs File Explorer's search helper from BUILT (build/search) with
#                                              the unit files of SYSTEM (installer/system), or only turns on the
#                                              package's copy, and starts its socket
#   root-steps.sh unsearch [STEP...]           stops and removes the search helper and its index
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
            # The scheduler and its loader; CachyOS ships them, and --needed leaves them alone when they are there.
            pacman -S --needed --noconfirm scx-scheds scx-tools > /dev/null
            install -Dm644 "$system/scx_loader.toml" /etc/scx_loader.toml
            systemctl enable --now scx_loader.service
            install -Dm644 "$system/thp.conf" /etc/tmpfiles.d/thp.conf
            echo defer > /sys/kernel/mm/transparent_hugepage/defrag
            install -Dm644 "$system/99-aurora-memory.conf" /etc/sysctl.d/99-aurora-memory.conf
            sysctl -q --system
            install -Dm644 "$system/90-aurora-zram.conf" /etc/systemd/zram-generator.conf.d/90-aurora-zram.conf
            shift 2 ;;
        untune)
            # The files tune writes, then CachyOS's own scheduler and huge page defrag, as uninstall.sh does.
            rm -f /etc/scx_loader.toml /etc/tmpfiles.d/thp.conf /etc/sysctl.d/99-aurora-memory.conf \
                /etc/systemd/zram-generator.conf.d/90-aurora-zram.conf
            systemctl disable --now scx_loader.service
            echo defer+madvise > /sys/kernel/mm/transparent_hugepage/defrag
            sysctl -q --system
            shift ;;
        search)
            [ $# -ge 3 ] || usage "search needs the built helper's folder and the unit files' folder"
            # The package installs the helper and its units in /usr; a checkout installs them in /usr/local and /etc.
            if [ ! -f /usr/lib/systemd/system/aurora-search.socket ]; then
                install -Dm755 "$2/aurora-search" /usr/local/lib/aurora/aurora-search
                install -Dm755 "$2/aurora-search-query" /usr/local/bin/aurora-search-query
                install -Dm644 "$3/aurora-search.socket" /etc/systemd/system/aurora-search.socket
                sed 's|@HELPER@|/usr/local/lib/aurora/aurora-search|' "$3/aurora-search.service.in" \
                    > /etc/systemd/system/aurora-search.service
                chmod 644 /etc/systemd/system/aurora-search.service
                [ -e /etc/aurora/search.conf ] || install -Dm644 "$3/search.conf" /etc/aurora/search.conf
            fi
            systemctl daemon-reload
            systemctl enable --now aurora-search.socket
            # A running helper finishes with the old program; the next connection starts the new one.
            systemctl try-restart aurora-search.service
            shift 3 ;;
        unsearch)
            systemctl disable --now aurora-search.socket aurora-search.service 2> /dev/null || true
            rm -f /etc/systemd/system/aurora-search.socket /etc/systemd/system/aurora-search.service
            rm -rf /usr/local/lib/aurora /usr/local/bin/aurora-search-query /var/lib/aurora/search
            systemctl daemon-reload
            shift ;;
        *) usage "unknown step: $1" ;;
    esac
done
