# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Copies the tray settings of one Plasma panel file into the trays of another.

The new tray then shows the same items as the old one: active ones in the bar and idle ones behind the arrow.
install.py panels uses the backup as the source; the sandbox uses a real desktop's file. Plasma 6.8 nests the tray
in its panel and reads these from the tray's own [General] group (systemtray.cpp configScheme()), which the
desktop scripting API cannot reach, so the file is written while plasmashell is off.

    patch_tray.py TARGET_APPLETSRC SOURCE_APPLETSRC [--screenshot-shortcut]

--screenshot-shortcut gives the tray the global shortcut Meta+Alt+F12, so a sandbox scenario can open its popup.
"""
import os
import re
import subprocess
import sys

# kwriteconfig6 and kreadconfig6 read a relative --file as relative to ~/.config, so both paths are made absolute.
appletsrc, real_appletsrc = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
screenshot_shortcut = "--screenshot-shortcut" in sys.argv[3:]

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

def read_key(path, panel, tray, key):
    """The value of `key` in the [General] group of one tray in the panel file at `path`."""
    out = subprocess.run(["kreadconfig6", "--file", path, "--group", "Containments", "--group", panel, "--group", "Applets",
                          "--group", tray, "--group", "General", "--key", key], capture_output=True, text=True).stdout.strip()
    return out

# The first tray of the source file is the one copied.
real_panel, real_tray = tray_groups(real_appletsrc)[0]
settings = {key: read_key(real_appletsrc, real_panel, real_tray, key)
            for key in ("extraItems", "knownItems", "hiddenItems", "shownItems", "showAllItems")}
# An unset key reads as empty, and showAllItems then defaults to false. The spacing is the one layout.js sets.
settings["showAllItems"] = settings["showAllItems"] or "false"
settings["iconSpacing"] = "6"

def write(path, groups, key, value):
    """Sets `key` in the nested config `groups` of the file at `path` with kwriteconfig6."""
    args = ["kwriteconfig6", "--file", path]
    for g in groups:
        args += ["--group", g]
    subprocess.run(args + ["--key", key, value], check=True)

for panel, tray in tray_groups(appletsrc):
    base = ["Containments", panel, "Applets", tray]
    for key, value in settings.items():
        write(appletsrc, base + ["General"], key, value)
    if screenshot_shortcut:
        write(appletsrc, base + ["Shortcuts"], "global", "Meta+Alt+F12")
    print(f"tray {panel}/{tray} <- real tray {real_panel}/{real_tray}:", {k: (v[:60] + "…" if len(v) > 60 else v) for k, v in settings.items()})
