# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""The repository's description and topics on GitHub: the words GitHub's search and topic pages find it by.

    python3 -I scripts/github_meta.py           shows what GitHub has now and what would change; sends nothing
    python3 -I scripts/github_meta.py --check   checks DESCRIPTION and TOPICS against GitHub's limits, offline
    python3 -I scripts/github_meta.py --apply   sends them with gh to the repository `origin` names

--apply edits the repository's public page, so it is run on purpose, like a push. The social preview picture
(assets/readme/social-preview.jpg, written by assets/readme/make.py) has no API; it is uploaded by hand under
Settings, Social preview.
"""
import json
import re
import subprocess
import sys

DESCRIPTION = ("A macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6: menu bar, Dock, Quick Menu, Control "
               "Center, a Finder-style file manager and glass window buttons, with the shortcuts you know from Windows. "
               "One command installs it, one undoes it.")

# What people search for, most specific first: the desktop, the look, the system, then the community's words.
TOPICS = ["kde-plasma", "plasma6", "kde", "kde-theme", "plasma-theme", "kwin", "wayland",
          "macos", "macos-tahoe", "macos-theme", "liquid-glass",
          "cachyos", "arch-linux", "aur", "linux", "linux-desktop", "linux-theme", "linux-customization",
          "unixporn", "rice"]


def problems(description, topics):
    """What GitHub would refuse: more than 20 topics, a topic that is not lowercase letters, digits and hyphens of at
    most 50 characters, a repeated topic, or a description over 350 characters."""
    found = []
    if len(topics) > 20:
        found.append(f"{len(topics)} topics; GitHub takes 20")
    found += [f"topic {topic!r} is not lowercase letters, digits and hyphens of at most 50 characters"
              for topic in topics if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,49}", topic)]
    found += [f"topic {topic!r} is listed twice" for topic in sorted({t for t in topics if topics.count(t) > 1})]
    if len(description) > 350:
        found.append(f"the description is {len(description)} characters; GitHub takes 350")
    return found


def gh(*arguments):
    return subprocess.run(["gh", *arguments], capture_output=True, text=True, check=True).stdout


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode not in ("", "--check", "--apply"):
        sys.exit(__doc__)
    failures = problems(DESCRIPTION, TOPICS)
    for failure in failures:
        print(f"FAIL {failure}")
    if failures:
        return 1
    if mode == "--check":
        print(f"PASS the description ({len(DESCRIPTION)} characters) and the {len(TOPICS)} topics fit GitHub's limits")
        return 0
    now = json.loads(gh("repo", "view", "--json", "nameWithOwner,description,repositoryTopics"))
    current = [topic["name"] for topic in now.get("repositoryTopics") or []]
    print(f"repository: {now['nameWithOwner']}")
    print(f"description now: {now.get('description') or '(none)'}")
    print(f"description new: {DESCRIPTION}")
    print(f"topics added:    {', '.join(t for t in TOPICS if t not in current) or '(none)'}")
    print(f"topics removed:  {', '.join(t for t in current if t not in TOPICS) or '(none)'}")
    if mode != "--apply":
        print("nothing sent; --apply sends it")
        return 0
    command = ["repo", "edit", "--description", DESCRIPTION, "--add-topic", ",".join(TOPICS)]
    stale = [t for t in current if t not in TOPICS]
    if stale:
        command += ["--remove-topic", ",".join(stale)]
    gh(*command)
    print("sent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
