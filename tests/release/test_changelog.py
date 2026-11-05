# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""CHANGELOG.md follows Keep a Changelog 2.0.0 and SemVer, and a release moves [Unreleased] into a dated version.

    python3 -I tests/release/test_changelog.py
"""
import importlib.util
import os
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("changelog", os.path.join(ROOT, "scripts/changelog.py"))
changelog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(changelog)

REPO = "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme"
HEADER = """# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
"""
VALID = HEADER + f"""
## [Unreleased]

### Added

- Quick Menu apps can be dragged within their section.

## [1.0.0] - 2026-11-04

### Added

- One command installs the theme.

[Unreleased]: {REPO}/compare/v1.0.0...HEAD
[1.0.0]: {REPO}/releases/tag/v1.0.0
"""


class Check(unittest.TestCase):
    """changelog.problems accepts the format and names each way a changelog can break it."""

    def test_a_changelog_in_the_format_has_no_problems(self):
        self.assertEqual(changelog.problems(VALID), [])

    def test_the_real_changelog_is_in_the_format(self):
        with open(os.path.join(ROOT, "CHANGELOG.md")) as f:
            self.assertEqual(changelog.problems(f.read()), [])

    def test_a_missing_unreleased_section_is_a_problem(self):
        text = VALID.replace("## [Unreleased]\n", "")
        self.assertIn("no ## [Unreleased] section", changelog.problems(text))

    def test_a_version_heading_needs_an_iso_date(self):
        text = VALID.replace("## [1.0.0] - 2026-11-04", "## [1.0.0] - 04/11/2026")
        self.assertTrue(any("1.0.0" in p and "date" in p for p in changelog.problems(text)))

    def test_only_the_six_types_of_change_are_sections(self):
        text = VALID.replace("### Added\n\n- One command", "### Improved\n\n- One command")
        self.assertTrue(any("Improved" in p for p in changelog.problems(text)))

    def test_versions_must_be_semver_and_newest_first(self):
        older_first = VALID.replace("## [1.0.0] - 2026-11-04", "## [0.9.0] - 2026-11-01\n\n### Fixed\n\n- A.\n\n## [1.0.0] - 2026-11-04")
        older_first += f"[0.9.0]: {REPO}/releases/tag/v0.9.0\n"
        self.assertTrue(any("order" in p for p in changelog.problems(older_first)))
        not_semver = VALID.replace("[1.0.0]", "[1.0]")
        self.assertTrue(any("1.0" in p and "Semantic" in p for p in changelog.problems(not_semver)))

    def test_every_version_needs_its_link(self):
        text = VALID.replace(f"[1.0.0]: {REPO}/releases/tag/v1.0.0\n", "")
        self.assertTrue(any("link" in p and "1.0.0" in p for p in changelog.problems(text)))


class Version(unittest.TestCase):
    """VERSION agrees with the changelog."""

    def test_the_version_file_names_the_latest_release(self):
        with open(os.path.join(ROOT, "CHANGELOG.md")) as f:
            released = changelog.latest(f.read()) or "0.0.0"
        with open(os.path.join(ROOT, "VERSION")) as f:
            self.assertEqual(f.read().strip(), released)


class Release(unittest.TestCase):
    """changelog.release, changelog.notes and changelog.semver_key."""

    def test_a_release_moves_unreleased_into_a_dated_version_and_relinks(self):
        text = changelog.release(VALID, "1.1.0", "2026-11-05", REPO)
        self.assertEqual(changelog.problems(text), [])
        self.assertIn("## [Unreleased]\n\n## [1.1.0] - 2026-11-05\n\n### Added\n\n- Quick Menu apps", text)
        self.assertIn(f"[Unreleased]: {REPO}/compare/v1.1.0...HEAD", text)
        self.assertIn(f"[1.1.0]: {REPO}/compare/v1.0.0...v1.1.0", text)
        self.assertIn(f"[1.0.0]: {REPO}/releases/tag/v1.0.0", text)

    def test_the_first_release_links_to_its_tag(self):
        first = HEADER + "\n## [Unreleased]\n\n### Added\n\n- Everything.\n\n[Unreleased]: " + REPO + "/commits/HEAD\n"
        text = changelog.release(first, "1.0.0", "2026-11-04", REPO)
        self.assertIn(f"[1.0.0]: {REPO}/releases/tag/v1.0.0", text)
        self.assertEqual(changelog.problems(text), [])

    def test_a_release_without_changes_is_refused(self):
        empty = VALID.replace("### Added\n\n- Quick Menu apps can be dragged within their section.\n\n", "")
        with self.assertRaises(ValueError):
            changelog.release(empty, "1.1.0", "2026-11-05", REPO)

    def test_a_version_not_above_the_latest_is_refused(self):
        for version in ("1.0.0", "0.9.9", "1.1"):
            with self.assertRaises(ValueError):
                changelog.release(VALID, version, "2026-11-05", REPO)

    def test_the_notes_of_a_version_are_its_section(self):
        self.assertEqual(changelog.notes(VALID, "1.0.0"), "### Added\n\n- One command installs the theme.")
        with self.assertRaises(KeyError):
            changelog.notes(VALID, "9.9.9")

    def test_semver_orders_numbers_not_text(self):
        self.assertGreater(changelog.semver_key("1.10.0"), changelog.semver_key("1.9.0"))
        self.assertGreater(changelog.semver_key("2.0.0"), changelog.semver_key("2.0.0-rc.1"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
