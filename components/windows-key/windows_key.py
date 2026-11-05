#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""windows_key.py quick-menu|stock|status: what the Windows key does when it is pressed on its own.

  quick-menu  Quick Menu (KRunner) opens with it and closes with the next press, as Start does on Windows and
             Command-Space on a Mac (the theme's choice, see docs/decisions.md, Look and behaviour).
  stock      Plasma's own binding: "Activate Application Launcher", which opens a launcher applet such as Kickoff.
  status     prints which action owns the key, changes nothing.

Since Plasma 6.1 a lone modifier is an ordinary kglobalaccel shortcut; kwinrc [ModifierOnlyShortcuts] is no longer
read (libkwin 6.8.2 has no such string). So the key moves between the two actions over kglobalaccel's D-Bus API,
the way System Settings rebinds it, and every other key of both actions stays as it is. It only talks to the
session bus it runs on, so the sandbox scenarios use it as they find it."""
import json
import subprocess
import sys

# Qt::Key_Meta, the value Plasma registers for "Activate Application Launcher" (shellcorona.cpp, Plasma 6.8).
META = 16777250
LAUNCHER = ["plasmashell", "activate application launcher", "plasmashell", "Activate Application Launcher"]
QUICK_MENU = ["org.kde.krunner.desktop", "_launch", "KRunner", "KRunner"]
# KGlobalAccel::SetPresent | KGlobalAccel::NoAutoloading: take these keys as they are, do not reload saved ones.
SET_EXACTLY = 6
ACCEL = ["busctl", "--user", "--json=short", "call", "org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel"]


def call(*arguments):
    """Calls a KGlobalAccel method with busctl and returns the first value of the reply, or None for an empty reply."""
    reply = subprocess.run([*ACCEL, *arguments], check=True, capture_output=True, text=True).stdout
    return json.loads(reply)["data"][0] if reply.strip() else None


def keys(action):
    """Returns the key codes bound to `action`, an action id list as in LAUNCHER."""
    return call("shortcut", "as", str(len(action)), *action)


def set_keys(action, wanted):
    """Binds exactly the key codes in `wanted` to `action`, without reloading its saved keys."""
    call("setShortcut", "asaiu", str(len(action)), *action, str(len(wanted)), *map(str, wanted), str(SET_EXACTLY))


def owner():
    """Returns the id of the action that holds the Windows key."""
    return call("action", "i", str(META))


def move(source, target):
    """Takes the Windows key from `source` and gives it to `target`, leaving their other keys alone.

    Exits with a message when the key does not end up with `target`.
    """
    # kglobalaccel drops a key another action holds, so the source lets go first.
    set_keys(source, [key for key in keys(source) if key != META])
    current = keys(target)
    if META not in current:
        set_keys(target, [META, *current])
    if owner()[:2] != target[:2]:
        sys.exit(f"win_key: the Windows key still belongs to {owner()[:2]}, not {target[:2]}")


def main():
    """Runs the mode named on the command line, then prints who holds the key.

    Any other argument exits with the usage text.
    """
    mode = sys.argv[1] if len(sys.argv) == 2 else ""
    if mode == "quick-menu":
        move(LAUNCHER, QUICK_MENU)
    elif mode == "stock":
        move(QUICK_MENU, LAUNCHER)
    elif mode != "status":
        sys.exit(__doc__)
    print("Windows key:", " / ".join(owner()[:2]) or "nothing")
    print("  Quick Menu keys:", keys(QUICK_MENU), " launcher keys:", keys(LAUNCHER))


if __name__ == "__main__":
    main()
