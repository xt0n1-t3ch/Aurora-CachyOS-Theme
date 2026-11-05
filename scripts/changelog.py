#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""CHANGELOG.md in the Keep a Changelog 2.0.0 format, with Semantic Versioning 2.0.0.

    changelog.py check [FILE]                  list what breaks the format (exit 1 when anything does)
    changelog.py latest [FILE]                 print the newest released version
    changelog.py notes VERSION [FILE]          print a version's section: the release notes
    changelog.py release VERSION DATE URL [FILE]
                                               move [Unreleased] into "## [VERSION] - DATE" and relink the versions to
                                               URL's tags and comparisons (the repository's web address)

People write the entries; this only checks the format and does the mechanical step of a release
(https://keepachangelog.com/en/2.0.0/: "Let automation handle the mechanics, and leave the judgment to people").
"""
import os
import re
import sys

# The regular expression semver.org gives for a version (https://semver.org/spec/v2.0.0.html, FAQ).
SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
                    r"(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?"
                    r"(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$")
TYPES = ("Added", "Changed", "Deprecated", "Removed", "Fixed", "Security")
HEADING = re.compile(r"^## \[([^\]]+)\](.*)$")
DATED = re.compile(r"^ - \d{4}-\d{2}-\d{2}( \[YANKED\])?$")
SUBHEADING = re.compile(r"^### (.+)$")
LINK = re.compile(r"^\[([^\]]+)\]:\s+(\S+)\s*$")
UNRELEASED = "Unreleased"


def semver_key(version):
    """Sort key of a version by SemVer precedence: numbers as numbers, a pre-release before its release."""
    match = SEMVER.match(version)
    if not match:
        raise ValueError(f"{version} is not a Semantic Versioning version")
    major, minor, patch, pre = int(match[1]), int(match[2]), int(match[3]), match[4]
    if pre is None:
        return (major, minor, patch, 1, ())
    ids = tuple((0, int(part), "") if part.isdigit() else (1, 0, part) for part in pre.split("."))
    return (major, minor, patch, 0, ids)


def _sections(lines):
    """[(label, rest of heading, first line index, end line index)] of the ## [label] sections, in file order."""
    starts = [(i, HEADING.match(line)) for i, line in enumerate(lines) if HEADING.match(line)]
    link_start = next((i for i, line in enumerate(lines) if LINK.match(line)), len(lines))
    sections = []
    for n, (i, match) in enumerate(starts):
        end = starts[n + 1][0] if n + 1 < len(starts) else link_start
        sections.append((match[1], match[2], i, end))
    return sections


def _links(lines):
    """{lowercase label: URL} of the link reference definitions at the end of the file."""
    return {LINK.match(line)[1].lower(): LINK.match(line)[2] for line in lines if LINK.match(line)}


def problems(text):
    """What breaks the format in `text`, as messages; an empty list means the changelog is valid."""
    lines = text.split("\n")
    found = []
    if not lines or lines[0] != "# Changelog":
        found.append("the file must open with # Changelog")
    sections = _sections(lines)
    labels = [label for label, _, _, _ in sections]
    if labels.count(UNRELEASED) != 1:
        found.append("no ## [Unreleased] section" if UNRELEASED not in labels else "more than one ## [Unreleased]")
    elif labels[0] != UNRELEASED:
        found.append("## [Unreleased] must come first")
    versions = []
    for label, rest, start, end in sections:
        if label != UNRELEASED:
            if not SEMVER.match(label):
                found.append(f"## [{label}] is not a Semantic Versioning version")
            else:
                versions.append(label)
            if not DATED.match(rest):
                found.append(f"## [{label}] needs its release date as YYYY-MM-DD")
        for line in lines[start + 1:end]:
            sub = SUBHEADING.match(line)
            if sub and sub[1] not in TYPES:
                found.append(f"### {sub[1]} in [{label}] is not one of {', '.join(TYPES)}")
    if versions != sorted(versions, key=semver_key, reverse=True):
        found.append("versions are out of order: the newest comes first")
    links = _links(lines)
    for label in labels:
        if label.lower() not in links:
            found.append(f"no link for [{label}] at the end of the file")
    return found


def latest(text):
    """The newest released version in `text`, or None when nothing has been released."""
    versions = [label for label, _, _, _ in _sections(text.split("\n")) if label != UNRELEASED and SEMVER.match(label)]
    return max(versions, key=semver_key) if versions else None


def notes(text, version):
    """The body of `version`'s section, stripped: the release notes.

    Raises KeyError when `text` has no section for `version`.
    """
    lines = text.split("\n")
    for label, _, start, end in _sections(lines):
        if label == version:
            return "\n".join(lines[start + 1:end]).strip()
    raise KeyError(version)


def release(text, version, date, url):
    """The changelog with [Unreleased] released as `version` on `date` (YYYY-MM-DD), linked under `url`."""
    if not SEMVER.match(version):
        raise ValueError(f"{version} is not a Semantic Versioning version")
    previous = latest(text)
    if previous and semver_key(version) <= semver_key(previous):
        raise ValueError(f"{version} is not above the latest release, {previous}")
    if not re.match(r"^\d{4}-\d{2}-\d{2}$", date):
        raise ValueError(f"{date} is not a YYYY-MM-DD date")
    body = notes(text, UNRELEASED)
    if not body:
        raise ValueError("[Unreleased] lists no changes: nothing to release")
    lines = text.split("\n")
    unreleased = next(s for s in _sections(lines) if s[0] == UNRELEASED)
    _, _, start, end = unreleased
    released = [lines[start], "", f"## [{version}] - {date}", "", *body.split("\n"), ""]
    lines = lines[:start] + released + lines[end:]
    url = url.rstrip("/")
    out = []
    for line in lines:
        link = LINK.match(line)
        if link and link[1].lower() == UNRELEASED.lower():
            out.append(f"[Unreleased]: {url}/compare/v{version}...HEAD")
            out.append(f"[{version}]: {url}/compare/v{previous}...v{version}" if previous
                       else f"[{version}]: {url}/releases/tag/v{version}")
        else:
            out.append(line)
    return "\n".join(out)


def main(argv):
    """Runs the subcommand in `argv`; returns the exit status: 0 on success, 1 when check finds problems, 2 for bad
    usage (the usage text goes to stderr).
    """
    def read(path):
        with open(path) as f:
            return f.read()

    default = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "CHANGELOG.md")
    if len(argv) >= 1 and argv[0] == "check":
        found = problems(read(argv[1] if len(argv) > 1 else default))
        for problem in found:
            print(problem)
        return 1 if found else 0
    if len(argv) >= 1 and argv[0] == "latest":
        print(latest(read(argv[1] if len(argv) > 1 else default)) or "")
        return 0
    if len(argv) >= 2 and argv[0] == "notes":
        print(notes(read(argv[2] if len(argv) > 2 else default), argv[1]))
        return 0
    if len(argv) >= 4 and argv[0] == "release":
        path = argv[4] if len(argv) > 4 else default
        updated = release(read(path), argv[1], argv[2], argv[3])
        with open(path, "w") as f:
            f.write(updated)
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
