# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Keeps a license header on every file Aurora's own code is made of.

    python3 -I scripts/license_headers.py --check   lists the files without one; exits 1 when there are any
    python3 -I scripts/license_headers.py --apply   adds the header to them, in each language's comment syntax

The header names the author, the license (GPL-3.0-only) and where the additional terms of NOTICE.md are, as GPL-3.0
section 7 asks for terms added under it. Files that are not Aurora's own keep their own notices and are left out:
Plasma's sources (upstream/, vendor/), the adapted menu bar style, Plasma style, Kvantum theme and cursors, generated
protocol files,
data the code reads byte for byte, and binaries.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = ["SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)",
          "SPDX-License-Identifier: GPL-3.0-only",
          "Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md."]

# Comment syntax by file extension, or by name for files without one.
LINE = {".cpp": "//", ".h": "//", ".qml": "//", ".js": "//", ".rs": "//",
        ".py": "#", ".sh": "#", ".toml": "#", ".conf": "#", ".desktop": "#", ".service": "#", ".portal": "#",
        ".profile": "#", ".colorscheme": "#", ".cmake": "#", ".install": "#"}
NAMES = {"CMakeLists.txt": "#", "dependencies.txt": "#"}
XML = {".rc", ".xml"}

# Paths that are not Aurora's own code, or that must stay byte for byte as they are.
SKIP_PARTS = ("/upstream/", "/vendor/", "tests/sandbox/home/", "themes/cursors/", "cli/tests/fixtures/",
              "packaging/aur/", "components/menu-bar-style/package/", "themes/plasma/Aurora/", "themes/kvantum/")
# The forks keep their upstream authors' files as they came; Aurora's own files in them carry
# their own notice already.
SKIP_PREFIXES = ("components/kali-menu/", "components/control-center/")
SKIP_FILES = {"VERSION", "tests/helpers/fakeinput/fake-input.xml", "tests/helpers/fakeinput/fake-input-protocol.c",
              "tests/helpers/fakeinput/fake-input-client-protocol.h"}


def comment_for(path):
    """The comment syntax for `path`: a line prefix, "xml", or None when the file takes no header."""
    name = os.path.basename(path)
    if name in NAMES:
        return NAMES[name]
    extension = os.path.splitext(name)[1]
    if extension in XML:
        return "xml"
    if extension in LINE:
        return LINE[extension]
    if not extension:
        with open(os.path.join(ROOT, path), "rb") as f:
            start = f.read(64)
        # A script without an extension, known by its shebang; binaries and data stay out.
        if start.startswith(b"#!") and (b"sh" in start.split(b"\n")[0] or b"python" in start.split(b"\n")[0]):
            return "#"
    return None


def owned_files():
    """Every tracked file of Aurora's own code that takes a header."""
    tracked = subprocess.run(["git", "-C", ROOT, "ls-files"], capture_output=True, text=True, check=True).stdout
    for path in tracked.splitlines():
        if path in SKIP_FILES or path.startswith(SKIP_PREFIXES) or any(part in f"/{path}" for part in SKIP_PARTS):
            continue
        if not os.path.isfile(os.path.join(ROOT, path)) or os.path.islink(os.path.join(ROOT, path)):
            continue
        if comment_for(path):
            yield path


def has_header(path):
    with open(os.path.join(ROOT, path), errors="replace") as f:
        return "SPDX-License-Identifier" in f.read(4096)


def with_header(path, text):
    """`text` with the header added after any shebang, encoding line or XML declaration."""
    style = comment_for(path)
    lines = text.split("\n")
    at = 0
    if lines and (lines[0].startswith("#!") or lines[0].startswith("<?xml")):
        at = 1
    if style == "xml":
        block = ["<!--"] + [f"  {line}" for line in HEADER] + ["-->"]
    else:
        block = [f"{style} {line}" for line in HEADER]
    return "\n".join(lines[:at] + block + lines[at:])


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("--check", "--apply"):
        sys.exit(__doc__)
    missing = [path for path in owned_files() if not has_header(path)]
    if sys.argv[1] == "--apply":
        for path in missing:
            full = os.path.join(ROOT, path)
            with open(full) as f:
                text = f.read()
            mode = os.stat(full).st_mode
            with open(full, "w") as f:
                f.write(with_header(path, text))
            os.chmod(full, mode)
        print(f"added the header to {len(missing)} files")
        return 0
    for path in missing:
        print(f"no license header: {path}")
    if missing:
        print(f"FAIL {len(missing)} files of Aurora's own code have no license header (scripts/license_headers.py --apply)")
        return 1
    print(f"PASS every file of Aurora's own code has a license header ({len(list(owned_files()))} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
