# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""measured.py CMD...: runs CMD, prints its 7-Zip rating plus how busy the rest of the machine was meanwhile
(foreign_pct: CPU used by other processes, as a share of all 16 threads), so a foreign build shows in each result.
The output is one JSON object with mips (the total rating of 7-Zip's "Tot:" line), seconds and foreign_pct.
"""
import json, os, resource, subprocess, sys, time
def busy():
    """Return the clock ticks all CPUs have spent busy (user, nice, system, irq, softirq) since boot."""
    f = open('/proc/stat').readline().split()[1:]
    return sum(int(x) for x in f[:3]) + int(f[5]) + int(f[6])
hz = os.sysconf('SC_CLK_TCK'); ncpu = os.cpu_count()
b0, t0, r0 = busy(), time.time(), resource.getrusage(resource.RUSAGE_CHILDREN)
out = subprocess.run(sys.argv[1:], capture_output=True, text=True, cwd='/tmp').stdout
b1, t1, r1 = busy(), time.time(), resource.getrusage(resource.RUSAGE_CHILDREN)
own = (r1.ru_utime - r0.ru_utime) + (r1.ru_stime - r0.ru_stime)
total = (b1 - b0) / hz
rating = next((int(l.split()[3]) for l in out.splitlines() if l.startswith('Tot:')), 0)
print(json.dumps({"mips": rating, "seconds": round(t1 - t0, 1), "foreign_pct": round(max(0, total - own) / (t1 - t0) / ncpu * 100, 1)}))
