# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Checks that a package was built for the Plasma, KWin, Qt and KDecoration that every repository of this PC has, so
`aurora update` installs it on CachyOS and on Arch alike instead of waiting for another build.

    python3 -I scripts/built_for.py PACKAGE DBPATH         the package's .BUILDINFO against `pacman -Si` in DBPATH
    python3 -I scripts/built_for.py --from BUILDINFO SI    a .BUILDINFO file against saved `pacman -Si` output

The package is built in a clean chroot of Arch's packages (scripts/clean-build.sh), and a CachyOS user has CachyOS's
rebuilds of them; `aurora update` compares the two by the rules of cli/src/system.rs (LINKS), which this script reads,
so the release and the command never disagree. DBPATH is a fresh copy of the repositories, such as the one
`checkupdates` writes. Exits 1 when any repository has another version than the package was built for.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYSTEM_RS = os.path.join(ROOT, "cli/src/system.rs")
LINK = re.compile(r'Link \{ package: "(?P<package>[^"]+)", name: "(?P<name>[^"]+)", rule: Match::(?P<rule>\w+) \}')
INSTALLED = re.compile(r"^installed = (?P<entry>\S+)$", re.MULTILINE)
# A .BUILDINFO entry is NAME-VERSION-RELEASE-ARCH; the name itself may hold dashes.
ENTRY_FIELDS = 4


def links():
    """The packages `aurora update` matches and how: (package, name shown, rule) from cli/src/system.rs."""
    with open(SYSTEM_RS) as file:
        return [(m["package"], m["name"], m["rule"]) for m in LINK.finditer(file.read())]


def upstream(version):
    """A pacman version without its release and epoch, as system::upstream: `1:6.8.2-1.1` is `6.8.2`."""
    version = version.rsplit("-", 1)[0]
    return version.split(":", 1)[-1]


def key(version, rule):
    """What the rule compares: the whole upstream version (Exact) or its first two numbers (Minor)."""
    version = upstream(version)
    return ".".join(version.split(".")[:2]) if rule == "Minor" else version


def built(buildinfo):
    """Package name to version from a .BUILDINFO's `installed =` lines."""
    found = {}
    for match in INSTALLED.finditer(buildinfo):
        parts = match["entry"].rsplit("-", ENTRY_FIELDS - 1)
        if len(parts) == ENTRY_FIELDS:
            found[parts[0]] = f"{parts[1]}-{parts[2]}"
    return found


def repositories(si):
    """(repository, package, version) for every entry of `pacman -Si` output."""
    entries = []
    for block in si.strip().split("\n\n"):
        fields = dict(line.split(":", 1) for line in block.splitlines() if ":" in line and not line.startswith(" "))
        fields = {name.strip(): value.strip() for name, value in fields.items()}
        if {"Repository", "Name", "Version"} <= fields.keys():
            entries.append((fields["Repository"], fields["Name"], fields["Version"]))
    return entries


def compare_link(link, have, entries):
    """Prints every repository's version of one linked package against the build's; returns the disagreements."""
    package, name, rule = link
    if package not in have:
        print(f"the .BUILDINFO does not say which {name} ({package}) the package was built for")
        return 1
    failed = 0
    for repository, version in [(r, v) for r, entry, v in entries if entry == package]:
        same = key(version, rule) == key(have[package], rule)
        print(f"{'same' if same else 'DIFFERENT'}  {name}: built for {upstream(have[package])}, "
              f"{repository} has {upstream(version)}")
        failed += not same
    return failed


def compare(buildinfo, si):
    """Prints each repository's version against the build's; returns the number of disagreements."""
    have, entries = built(buildinfo), repositories(si)
    failed = sum(compare_link(link, have, entries) for link in links())
    if failed:
        print(f"{failed} {'difference' if failed == 1 else 'differences'}: `aurora update` would wait instead of "
              "installing this package. Release once every repository has the same versions.")
    return failed


def main():
    args = sys.argv[1:]
    if args[:1] == ["--from"] and len(args[1:]) == 2:
        with open(args[1]) as file:
            buildinfo = file.read()
        with open(args[2]) as file:
            si = file.read()
    elif len(args) == 2:
        package, dbpath = args
        buildinfo = subprocess.run(["bsdtar", "-xOqf", package, ".BUILDINFO"], capture_output=True, text=True,
                                   check=True).stdout
        si = subprocess.run(["pacman", "-Si", "--dbpath", dbpath, *[p for p, _, _ in links()]], capture_output=True,
                            text=True, check=True).stdout
    else:
        sys.exit(__doc__)
    return 1 if compare(buildinfo, si) else 0


if __name__ == "__main__":
    sys.exit(main())
