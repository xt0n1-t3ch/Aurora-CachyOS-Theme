#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# The same desktop actions while 32 busy threads run in the background, like a big build or test run.
# shellcheck disable=SC2016 # the inner shell expands its own loop
systemd-run --user --quiet --collect --unit=gg-loadtest -p RuntimeMaxSec=45 /usr/bin/bash -c \
    'for _ in $(seq 32); do (while :; do :; done) & done; wait'
sleep 3
echo "sched_ext: $(cat /sys/kernel/sched_ext/state) $(cat /sys/kernel/sched_ext/root/ops 2>/dev/null)"
# The installed state links back to the checkout.
python3 -I "$HOME/.local/share/aurora/source/performance/measure/probe.py" emoji emoji konsole explorer
systemctl --user stop gg-loadtest.service 2>/dev/null
