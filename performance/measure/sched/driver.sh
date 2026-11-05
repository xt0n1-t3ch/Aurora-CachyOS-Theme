#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Confirmation round, user side. Per slot announced by the root side in $D/current ("<n>-<scheduler>"): idle package
# watts, 7-Zip as ananicy ranks it (nice 16, idle I/O: how a background build runs), the same 7-Zip under a neutral
# name (raw throughput), and app latency with 32 busy threads. CPU used by everything else is logged per slot, so a
# foreign build running at the same time shows.
# The folder shared with sched-confirm.sh, the root side. It also holds measured.py and ggbench, 7-Zip under a neutral
# name so ananicy gives it no rank.
D=/tmp/gg-sched2
# The installed state links back to the checkout.
PROBE=$HOME/.local/share/aurora/source/performance/measure/probe.py
RAPL=/sys/class/powercap/intel-rapl:0/energy_uj
# busy: prints the clock ticks all CPUs spent busy (user, nice, system, irq, softirq) since boot.
busy() { awk '/^cpu /{print $2+$3+$4+$7+$8}' /proc/stat; }
seen=""
# Waits for the root side to announce a slot; "finished" ends the run.
while :; do
    slot=$(cat $D/current 2>/dev/null)
    [ "$slot" = finished ] && break
    if [ -z "$slot" ] || [ "$slot" = "$seen" ]; then sleep 0.5; continue; fi
    seen=$slot
    sleep 3
    e1=$(cat $RAPL); t1=$(date +%s%N); b1=$(busy); sleep 15; e2=$(cat $RAPL); t2=$(date +%s%N); b2=$(busy)
    idle_w=$(python3 -c "print(round(($e2-$e1)/1e6/(($t2-$t1)/1e9),1))")
    # Ticks at 100 Hz over 15 s on 16 threads, as a percentage.
    idle_busy=$(python3 -c "print(round(($b2-$b1)/100/15/16*100,1))")
    nice7z=$(python3 -I "$D/measured.py" 7z b -mmt16 1)
    raw7z=$(python3 -I "$D/measured.py" "$D/ggbench" b -mmt16 1)
    # shellcheck disable=SC2016 # the inner shell expands its own loop
    systemd-run --user --quiet --collect --unit="gg-load2-${slot}" -p RuntimeMaxSec=60 /usr/bin/bash -c \
        'for _ in $(seq 32); do (while :; do :; done) & done; wait'
    sleep 2
    # Tells the root side to measure real-time latency under this load.
    touch "$D/loadstart-$slot"
    probe=$(python3 -I "$PROBE" emoji emoji konsole explorer 2>&1)
    sleep 3
    systemctl --user stop "gg-load2-${slot}.service" 2>/dev/null
    echo "{\"slot\": \"$slot\", \"idle_w\": $idle_w, \"idle_busy_pct\": $idle_busy, \"nice7z\": ${nice7z:-null}, \"raw7z\": ${raw7z:-null}, \"probe\": $probe}" > "$D/result-$slot.json"
    # Tells the root side that the slot is finished.
    touch "$D/done-$slot"
done
