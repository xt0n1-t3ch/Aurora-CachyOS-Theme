# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Summarizes the alternating round: per scheduler the mean of its two slots. 7-Zip ratings are also shown scaled to
a machine free of other sessions' load (rating / (1 - foreign share)), the fairest comparison when a foreign build
ran during some slots; a star in a column name marks the scaled rating. Reads result-*.json (driver.sh) and the
matching rt-*.json (rtlat) from /tmp/gg-sched2.
"""
import glob, json, statistics as st
rows = {}
for f in sorted(glob.glob('/tmp/gg-sched2/result-*.json')):
    r = json.load(open(f)); name = r['slot'].split('-', 1)[1]
    rt = json.load(open(f.replace('result-', 'rt-')))
    rows.setdefault(name, []).append((r, rt))
def scaled(m):
    """Return the 7-Zip rating in m scaled to a machine without foreign load; the divisor stays at 0.2 or above."""
    return m['mips'] / max(0.2, 1 - m['foreign_pct'] / 100)
print(f"{'scheduler':9s} {'emoji cold/warm':>16s} {'konsole':>8s} {'explorer':>9s} {'7z bg':>7s} {'7z bg*':>7s} {'7z raw':>7s} {'7z raw*':>8s} {'foreign%':>9s} {'idle W':>7s} {'rt p99':>7s} {'rt max':>7s}")
for name in ['eevdf', 'lavd', 'bpfland', 'cosmos']:
    if name not in rows:
        continue
    rs = rows[name]
    m = lambda f: st.mean(f(r, rt) for r, rt in rs)
    print(f"{name:9s} {m(lambda r, t: r['probe'][0]['ms']):7.0f}/{m(lambda r, t: r['probe'][1]['ms']):<8.0f} {m(lambda r, t: r['probe'][2]['ms']):8.0f} {m(lambda r, t: r['probe'][3]['ms']):9.0f} "
          f"{m(lambda r, t: r['nice7z']['mips']):7.0f} {m(lambda r, t: scaled(r['nice7z'])):7.0f} {m(lambda r, t: r['raw7z']['mips']):7.0f} {m(lambda r, t: scaled(r['raw7z'])):8.0f} "
          f"{m(lambda r, t: (r['nice7z']['foreign_pct'] + r['raw7z']['foreign_pct']) / 2):9.1f} {m(lambda r, t: r['idle_w']):7.1f} {m(lambda r, t: t['p99']):7.0f} {max(t['max'] for r, t in rs):7d}   (n={len(rs)})")
