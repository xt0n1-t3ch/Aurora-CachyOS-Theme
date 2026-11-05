# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Puts the listed tray plasmoids behind the tray's arrow and lets the others show only while active.

This is how macOS keeps rarely used controls in Control Center. Plasma 6.8 reads these keys from the tray's own
[General] group, which desktop scripting cannot reach (see patch_tray.py), so run it while plasmashell is stopped.

    tray_hide.py APPLETSRC ITEM[,ITEM...]
"""
import os
import re
import subprocess
import sys

# kwriteconfig6 reads a relative --file as relative to ~/.config, so the target is made absolute first.
appletsrc, items = os.path.abspath(sys.argv[1]), sys.argv[2]


def tray_groups(path):
    """The (containment id, applet id) pair of every system tray in the panel file at `path`."""
    current, trays = None, []
    for line in open(path).read().splitlines():
        header = re.fullmatch(r"\[Containments\]\[(\d+)\]\[Applets\]\[(\d+)\]", line.strip())
        if header:
            current = header.groups()
        elif line.startswith("["):
            current = None
        elif current and line.strip() == "plugin=org.kde.plasma.systemtray":
            trays.append(current)
    return trays


for panel, tray in tray_groups(appletsrc):
    base = ["kwriteconfig6", "--file", appletsrc, "--group", "Containments", "--group", panel, "--group", "Applets",
            "--group", tray, "--group", "General"]
    subprocess.run(base + ["--key", "showAllItems", "--type", "bool", "false"], check=True)
    subprocess.run(base + ["--key", "hiddenItems", items], check=True)
    print(f"tray {panel}/{tray}: hidden {items}")
