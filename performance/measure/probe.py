# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Measures how long desktop actions take: trigger -> the action's window shows up in KWin.

For each new window it also reads the process's CPU time, time spent waiting for a CPU (schedstat) and major page
faults (pages read from disk or swap). Run: python3 -I probe.py ACTION [ACTION...], with ACTION one of the names in
ACTIONS. It prints a JSON list with one entry per action; "ms" is null when no window appeared within 15 s. Each
measured process is sent SIGTERM afterwards, so the next action starts from a closed app.
"""
import json
import os
import subprocess
import sys
import time

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "kwin_watch.js")

ACTIONS = {
    # name: (how to trigger, window resourceClass substring)
    "emoji": (["qdbus6", "org.kde.kglobalaccel", "/component/org_kde_plasma_emojier_desktop",
               "org.kde.kglobalaccel.Component.invokeShortcut", "_launch"], "emojier"),
    "explorer": (["qdbus6", "org.kde.kglobalaccel", "/component/file_explorer_desktop",
                  "org.kde.kglobalaccel.Component.invokeShortcut", "_launch"], "file-explorer"),
    "konsole": (["qdbus6", "org.kde.kglobalaccel", "/component/org_kde_konsole_desktop",
                 "org.kde.kglobalaccel.Component.invokeShortcut", "_launch"], "konsole"),
    "settings": (["kstart", "systemsettings"], "systemsettings"),
    "texteditor": (["kstart", "gnome-text-editor"], "TextEditor"),
}


def qdbus(*args):
    """Run qdbus6 against org.kde.KWin with args and return its output without the trailing newline."""
    return subprocess.run(["qdbus6", "org.kde.KWin", *args], capture_output=True, text=True).stdout.strip()


def task_stats(pid):
    """CPU ns, runqueue wait ns and major faults summed over the process's threads."""
    run = wait = 0
    try:
        for tid in os.listdir(f"/proc/{pid}/task"):
            with open(f"/proc/{pid}/task/{tid}/schedstat") as f:
                r, w, _ = f.read().split()
                run += int(r)
                wait += int(w)
        # The command name can hold spaces and parentheses, so the fields are counted after the last ")".
        with open(f"/proc/{pid}/stat") as f:
            majflt = int(f.read().rsplit(")", 1)[1].split()[9])
    except (FileNotFoundError, ProcessLookupError):
        return None
    return run, wait, majflt


def watch_windows(since_ms, cls):
    """Return (stamp ms, pid, class) of the first GGLAT line since since_ms whose class contains cls, or None.

    kwin_watch.js writes the lines; KWin's output reaches the user journal, which is read here.
    """
    out = subprocess.run(["journalctl", "--user", "-b", "--since", "-2min", "-o", "cat", "--no-pager"],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        if "GGLAT " not in line:
            continue
        stamp, pid, rclass = line.split("GGLAT ", 1)[1].split(" ", 2)
        if int(stamp) >= since_ms and cls.lower() in rclass.lower():
            return int(stamp), int(pid), rclass
    return None


def main():
    """Load kwin_watch.js, run each action named on the command line and print the results as JSON."""
    script_id = qdbus("/Scripting", "org.kde.kwin.Scripting.loadScript", SCRIPT, "gg_latency")
    qdbus(f"/Scripting/Script{script_id}", "org.kde.kwin.Script.run")
    results = []
    try:
        for name in sys.argv[1:]:
            trigger, cls = ACTIONS[name]
            start = int(time.time() * 1000)
            subprocess.run(trigger, capture_output=True)
            hit = None
            while hit is None and int(time.time() * 1000) - start < 15000:
                time.sleep(0.05)
                hit = watch_windows(start, cls)
            if hit is None:
                results.append({"action": name, "ms": None})
                continue
            stamp, pid, rclass = hit
            stats = task_stats(pid)
            run, wait, majflt = stats if stats else (0, 0, 0)
            results.append({"action": name, "ms": stamp - start, "pid": pid, "cpu_ms": run // 1000000,
                            "wait_ms": wait // 1000000, "majflt": majflt})
            time.sleep(0.4)
            os.kill(pid, 15)
            time.sleep(1.0)
    finally:
        qdbus("/Scripting", "org.kde.kwin.Scripting.unloadScript", "gg_latency")
    print(json.dumps(results))


main()
