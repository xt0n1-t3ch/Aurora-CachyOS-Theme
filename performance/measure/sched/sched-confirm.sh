#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Confirmation round, root side (driver.sh is the user side): EEVDF and the finalists alternate twice (A B C D A B C D),
# so a load from another session that comes and goes hits every scheduler alike. scx_loader (lavd, as configured)
# comes back at the end.
set -u
D=/tmp/gg-sched2
systemctl stop scx_loader.service
sleep 1
# slot NAME [COMMAND...]: with a command, starts that sched_ext scheduler and waits up to 20 s for the kernel to
# report it enabled; without one, the kernel's own EEVDF runs. Announces NAME to driver.sh, runs rtlat for 15 s once
# driver.sh starts its load, waits for driver.sh to finish, then stops the scheduler.
slot() {
    local name=$1; shift
    local pid=""
    if [ $# -gt 0 ]; then
        "$@" > "$D/$name.log" 2>&1 & pid=$!
        for _ in $(seq 80); do [ "$(cat /sys/kernel/sched_ext/state)" = enabled ] && break; sleep 0.25; done
    fi
    echo "$(cat /sys/kernel/sched_ext/state) $(cat /sys/kernel/sched_ext/root/ops 2>/dev/null)" > "$D/state-$name"
    echo "$name" > "$D/current"
    for _ in $(seq 1200); do [ -e "$D/loadstart-$name" ] && break; sleep 0.25; done
    "$D/rtlat" 15 > "$D/rt-$name.json"
    for _ in $(seq 1200); do [ -e "$D/done-$name" ] && break; sleep 0.25; done
    if [ -n "$pid" ]; then kill -INT "$pid"; wait "$pid"; fi
    sleep 2
}
for round in 1 2; do
    slot "$round-eevdf"
    slot "$round-lavd" /usr/bin/scx_lavd
    slot "$round-cosmos" /usr/bin/scx_cosmos
    slot "$round-bpfland" /usr/bin/scx_bpfland
done
echo finished > "$D/current"
systemctl start scx_loader.service
journalctl -k --since "-30min" --no-pager -o short-iso | grep -i "sched_ext" > "$D/kernel.log"
