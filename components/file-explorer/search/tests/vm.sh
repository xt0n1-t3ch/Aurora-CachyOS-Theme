#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Runs aurora-search's root tests in a throwaway virtual machine on this PC's own kernel (virtme-ng), so they get
# root, fanotify and fresh filesystems without sudo and without touching this machine's disks.
# Usage: tests/vm.sh [TEST-NAME-FILTER]
# Needs /dev/kvm, uvx (for virtme-ng) and a static busybox; AURORA_VNG_BUSYBOX overrides where it is looked for.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUSYBOX=${AURORA_VNG_BUSYBOX:-/var/tmp/aurora-vng/pkg/usr/bin/busybox}
KERNEL=/usr/lib/modules/$(uname -r)/vmlinuz
[ -x "$BUSYBOX" ] || { echo "no static busybox at $BUSYBOX (set AURORA_VNG_BUSYBOX)" >&2; exit 2; }

# The test programs, built here as the user; the virtual machine sees this machine's files read-only.
mapfile -t TESTS < <(cd "$HERE" && cargo test --no-run --message-format=json 2> /dev/null \
    | python3 -I -c 'import json,sys
for line in sys.stdin:
    m = json.loads(line)
    if m.get("reason") == "compiler-artifact" and m.get("profile", {}).get("test") and m.get("executable") and "vm_" in m["target"]["name"]:
        print(m["executable"])')
[ "${#TESTS[@]}" -gt 0 ] || { echo "no vm_ test programs built" >&2; exit 2; }

FILTER=${1:-}
# The image and its mount point live in the machine's /tmp, an overlay in its memory (the rest is this machine's files,
# read-only); with --memory 2G virtme-ng 1.42 never booted here.
SCRIPT="set -e; modprobe loop
# A tmpfs over /srv, inside the machine only, gives a mount point outside the folders the helper never indexes.
mount -t tmpfs none /srv && mkdir -p /srv/btrfs
truncate -s 256M /tmp/btrfs.img && mkfs.btrfs -q /tmp/btrfs.img && mount -o loop /tmp/btrfs.img /srv/btrfs
export AURORA_SEARCH_BTRFS=/srv/btrfs
status=0
for t in ${TESTS[*]}; do echo \"== \$t\"; timeout 90 \$t --ignored --test-threads=1 --nocapture $FILTER || status=1; done
umount /srv/btrfs
exit \$status"
exec timeout 300 uvx --from virtme-ng vng --run "$KERNEL" --busybox "$BUSYBOX" --user root --memory 1G --cpus 2 -- "$SCRIPT"
