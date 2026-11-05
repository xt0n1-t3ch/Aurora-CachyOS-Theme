# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""merge_colors.py SCHEME KDEGLOBALS: copy the color groups of a KDE .colors scheme (Colors:*, ColorEffects:*, WM*) and
its name into kdeglobals, as plasma-apply-colorscheme does."""
import configparser
import sys

scheme, kdeglobals = sys.argv[1], sys.argv[2]
src = configparser.RawConfigParser(strict=False, interpolation=None)
src.optionxform = str
src.read(scheme)
dst = configparser.RawConfigParser(strict=False, interpolation=None)
dst.optionxform = str
dst.read(kdeglobals)
for sec in src.sections():
    if sec.startswith(("Colors:", "ColorEffects:", "WM")) or sec == "General":
        if sec == "General":
            for k in ("ColorScheme", "Name", "shadeSortColumn"):
                if src.has_option(sec, k):
                    dst.has_section(sec) or dst.add_section(sec)
                    dst.set(sec, "ColorScheme" if k == "Name" else k, src.get(sec, k))
            continue
        if dst.has_section(sec):
            dst.remove_section(sec)
        dst.add_section(sec)
        for k, v in src.items(sec):
            dst.set(sec, k, v)
with open(kdeglobals, "w") as f:
    dst.write(f, space_around_delimiters=False)
