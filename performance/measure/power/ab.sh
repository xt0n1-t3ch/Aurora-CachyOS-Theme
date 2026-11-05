#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# ab.sh ROUNDS SECONDS: Balanced against Performance on whatever the machine is doing, in alternating windows so a
# change in the background load lands on both. Per window: CPU package watts (RAPL), the busy share of all CPUs
# (/proc/stat, so other sessions' work is visible) and burst latency (burst.c, median and p90 µs). Puts the profile
# that was active back at the end.
set -u
here=$(dirname "$(readlink -f "$0")")
rounds=${1:-3}
seconds=${2:-20}
# The RAPL package energy counter, in microjoules.
rapl=/sys/class/powercap/intel-rapl:0/energy_uj
before=$(powerprofilesctl get)
# busy: prints the busy and the total clock ticks of all CPUs from /proc/stat; iowait counts as idle.
busy() { awk '/^cpu /{print $2+$3+$4+$7+$8, $2+$3+$4+$5+$6+$7+$8}' /proc/stat; }
# window PROFILE: sets the power profile, lets it settle for 2 s, measures for SECONDS and prints one line with the
# package watts, the busy share, and the burst median and p90. burst sleeps 250 ms per sample, so SECONDS * 4 samples
# take about SECONDS seconds.
window() {
    powerprofilesctl set "$1"
    sleep 2
    read -r b1 t1 <<< "$(busy)"
    e1=$(cat $rapl)
    n1=$(date +%s%N)
    read -r med p90 <<< "$("$here/burst" $((seconds * 4)) 250)"
    n2=$(date +%s%N)
    e2=$(cat $rapl)
    read -r b2 t2 <<< "$(busy)"
    # The counter wraps at max_energy_range_uj; such a window is dropped rather than misread.
    if [ "$e2" -lt "$e1" ]; then echo "$1 wrapped"; return; fi
    awk -v p="$1" -v e=$((e2 - e1)) -v ns=$((n2 - n1)) -v b=$((b2 - b1)) -v t=$((t2 - t1)) -v m="$med" -v q="$p90" \
        'BEGIN { printf "%-12s %6.1f W  busy %5.1f %%  burst median %5d us  p90 %5d us\n", p, e / 1e6 / (ns / 1e9), 100 * b / t, m, q }'
}
for ((i = 1; i <= rounds; ++i)); do
    window balanced
    window performance
done
powerprofilesctl set "$before"
