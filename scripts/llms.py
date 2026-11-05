# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Writes llms.txt and llms-full.txt, the map of Aurora for language models and coding agents (llmstxt.org, v2).

    python3 -I scripts/llms.py           writes both files from DOCS below
    python3 -I scripts/llms.py --check   fails when either file is out of date, a link has no file behind it, or
                                         llms.txt breaks the format

llms.txt is the short index: the name, a one-paragraph summary, the facts an agent needs first, then lists of links
to the raw Markdown on GitHub. llms-full.txt holds the text of those documents in one file. Both carry facts and links
only: an agent trusts what it reads there, so nothing in them is phrased as an instruction to it.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = "https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/"

SUMMARY = ("Aurora is a macOS Tahoe-inspired desktop theme for KDE Plasma 6 on CachyOS and other Arch-based "
           "systems. It adds a menu bar, a Dock, Quick Menu, a Control Center, notification banners, a Finder-style "
           "file manager and glass window buttons, keeps the shortcuts people know from Windows, and installs and "
           "uninstalls with one command.")

FACTS = [
    "Install it with one command, `curl -fsSL "
    "https://raw.githubusercontent.com/xt0n1-t3ch/Aurora-CachyOS-Theme/main/get.sh | bash`: it adds Aurora's signed pacman "
    "repository, installs the aurora-theme package (its parts come built) and opens `aurora install`, which asks one "
    "question, the menu bar's logo. From the source: `git clone https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme` "
    "and `./install.sh`.",
    "Remove it with `aurora uninstall` or `./uninstall.sh`: the install saves every setting before it changes it.",
    "It needs KDE Plasma 6 on Wayland and was tested with Plasma 6.8.2, KWin 6.8.2 and Qt 6.12.0.",
    "The Windows key opens Quick Menu, Win+E opens File Explorer, the window buttons are in Windows order, and menus "
    "show shortcuts as Ctrl+C.",
    "Nothing in KWin is patched and no packaged file is edited. Plasma's own parts are rebuilt with a patch; after a "
    "Plasma, KWin or Qt update they step aside until `aurora rebuild` builds them again.",
    "Aurora is made by xt0n1. Its own code is GPL-3.0-only with additional terms that keep the author attribution "
    "(NOTICE.md).",
]

# Section, link name, path in the repository, what it holds. "Optional" is the spec's section of links an agent can
# skip when it needs a shorter context.
DOCS = [
    ("Docs", "README", "README.md", "what Aurora looks like, its features, install and update commands, shortcuts, "
     "dependencies, removal and license"),
    ("Docs", "Install", "docs/install.md", "each install step, what it changes, and how to check the result"),
    ("Docs", "Updates", "docs/update.md", "keeping the parts working after a Plasma, KWin or Qt update"),
    ("Docs", "Removal", "docs/restore.md", "putting the previous desktop back, all of it or one part"),
    ("Docs", "Architecture", "docs/architecture.md", "how each part hooks into Plasma, KWin, systemd and D-Bus, and "
     "every file the install adds"),
    ("Docs", "Components", "docs/components.md", "each part: what it does, its source, build, tests and install"),
    ("Docs", "Building", "docs/build.md", "what installer/build.sh builds and how to build one part by hand"),
    ("Docs", "Performance tuning", "docs/performance.md", "the optional scheduler, memory and zram settings and the "
     "measurements behind them"),
    ("Development", "Tests", "tests/index.md", "every test, the command that runs it and its result"),
    ("Development", "Sandbox", "docs/sandbox.md", "the nested Plasma session the scenarios run in"),
    ("Development", "Releasing", "docs/releasing.md", "versions, the changelog, release files and the AUR package"),
    ("Optional", "Changelog", "CHANGELOG.md", "what changed in each version"),
    ("Optional", "Decisions", "docs/decisions.md", "the design and project decisions and their reasons"),
    ("Optional", "License and attribution", "NOTICE.md", "the license of each part and the attribution terms"),
]


def section_order():
    seen = []
    for section, *_ in DOCS:
        if section not in seen:
            seen.append(section)
    return seen


def index():
    """The text of llms.txt."""
    lines = ["# Aurora", "", f"> {SUMMARY}", "", "Key facts:", ""]
    lines += [f"- {fact}" for fact in FACTS]
    for section in section_order():
        lines += ["", f"## {section}", ""]
        lines += [f"- [{name}]({RAW}{path}): {note}" for s, name, path, note in DOCS if s == section]
        if section == "Optional":
            lines.append(f"- [Everything above in one file]({RAW}llms-full.txt): the text of every document listed here")
    return "\n".join(lines) + "\n"


def plain(markdown):
    """`markdown` without the README's pictures: a heading drawn as a picture becomes a Markdown heading, and any other
    picture becomes its description."""
    def heading(match):
        title = match.group(2).split(":")[0].strip()
        return f"{'#' * int(match.group(1))} {title}"
    text = re.sub(r'<h([1-6])[^>]*><img [^>]*alt="([^"]*)"[^>]*></h\1>', heading, markdown)
    text = re.sub(r'<img [^>]*alt="([^"]*)"[^>]*>', r"(Picture: \1)", text)
    text = re.sub(r"<!--.*?-->\n?", "", text, flags=re.S)
    text = re.sub(r"<br\s*/?>", "\n", text)
    text = LAYOUT.sub("", text)
    # The table's pictures are left indented, and four spaces would make them a code block.
    text = "\n".join(line.strip() if "(Picture:" in line or not line.strip() else line for line in text.split("\n"))
    return re.sub(r"\n{3,}", "\n\n", text)


def full():
    """The text of llms-full.txt: each document of DOCS, in order, under a line naming its path."""
    parts = [f"# Aurora, the full documentation\n\n> {SUMMARY}\n"]
    for _, name, path, _ in DOCS:
        with open(os.path.join(ROOT, path)) as f:
            parts.append(f"\n---\n\nSource: {path} ({name})\n\n{plain(f.read()).strip()}\n")
    return "".join(parts)


def problems(text):
    """What makes `text` (llms.txt) break the format or point at nothing."""
    found = []
    lines = text.split("\n")
    if not lines[0].startswith("# "):
        found.append("the first line is not the H1 with the name")
    if not any(line.startswith("> ") for line in lines[1:4]):
        found.append("no blockquote summary right after the H1")
    links = re.findall(r"^- \[[^\]]+\]\(([^)]+)\)(?:: .+)?$", text, flags=re.M)
    for url in links:
        if not url.startswith(RAW):
            found.append(f"a link outside the repository: {url}")
        elif not os.path.exists(os.path.join(ROOT, url[len(RAW):])):
            found.append(f"a link to a file that does not exist: {url[len(RAW):]}")
    if len(text.encode()) > 10_000:
        found.append("llms.txt is over 10 kB; the detail belongs behind its links")
    return found


# The HTML the README lays its pictures out with; text for a model carries none of it.
LAYOUT = re.compile(r"</?(?:img|a|p|br|table|tr|td|th|div|picture|source|sub|sup)\b[^>]*>")


def main():
    files = {"llms.txt": index(), "llms-full.txt": full()}
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        failures = problems(files["llms.txt"])
        failures += [f"llms-full.txt keeps the HTML tag {tag}" for tag in sorted(set(LAYOUT.findall(files["llms-full.txt"])))]
        for name, text in files.items():
            path = os.path.join(ROOT, name)
            if not os.path.exists(path) or open(path).read() != text:
                failures.append(f"{name} is out of date: run python3 -I scripts/llms.py")
        for failure in failures:
            print(f"FAIL {failure}")
        if not failures:
            print(f"PASS llms.txt and llms-full.txt are current and llms.txt follows the format ({len(DOCS)} links)")
        return 1 if failures else 0
    for name, text in files.items():
        with open(os.path.join(ROOT, name), "w") as f:
            f.write(text)
        print(f"{name}: {len(text.encode()) // 1024} kB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
