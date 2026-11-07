// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! What this machine runs: the Plasma, KWin, Qt and KDecoration versions (from pacman), the session type, and
//! which of Aurora's parts the login script set aside because they were built for other versions.

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;
use std::process::Command;

/// Package name to upstream version, without the package release or epoch (`kwin` to `6.8.2`).
pub type Versions = BTreeMap<String, String>;

/// How much of a version must stay the same for a part built against it to keep running.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Match {
    /// The whole upstream version: Plasma's parts use interfaces that may change in any release.
    Exact,
    /// Major and minor only (6.8 for 6.8.2): KDecoration keeps its plugin interface within a minor release.
    Minor,
}

/// A package Aurora's built parts link against.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Link {
    /// The pacman package.
    pub package: &'static str,
    /// Its name as people read it.
    pub name: &'static str,
    pub rule: Match,
}

/// What each part's build stamp records: the rule `installer/build.sh` writes (`stamp_of`) and the login script checks
/// (`installer/session/aurora.sh`).
pub const LINKS: [Link; 4] = [
    Link { package: "plasma-workspace", name: "KDE Plasma", rule: Match::Exact },
    Link { package: "kwin", name: "KWin", rule: Match::Exact },
    Link { package: "qt6-base", name: "Qt", rule: Match::Exact },
    Link { package: "kdecoration", name: "KDecoration", rule: Match::Minor },
];

/// The packages whose versions decide what Aurora's parts are built for.
pub const PACKAGES: [&str; 4] = [LINKS[0].package, LINKS[1].package, LINKS[2].package, LINKS[3].package];

/// A pacman version without its package release and epoch: `1:6.8.2-1.1` is `6.8.2`.
#[must_use]
pub fn upstream(version: &str) -> &str {
    let upstream = version.rsplit_once('-').map_or(version, |(upstream, _release)| upstream);
    upstream.split_once(':').map_or(upstream, |(_epoch, rest)| rest)
}

/// Package name to upstream version, from `pacman -Q` output (`plasma-workspace 6.8.2-1.1` gives `6.8.2`).
#[must_use]
pub fn parse_pacman_q(output: &str) -> Versions {
    output
        .lines()
        .filter_map(|line| {
            let (name, version) = line.trim().split_once(' ')?;
            Some((name.to_string(), upstream(version).to_string()))
        })
        .collect()
}

/// The installed versions of [`PACKAGES`]; empty when pacman is missing.
#[must_use]
pub fn installed_versions() -> Versions {
    Command::new("pacman")
        .arg("-Q")
        .args(PACKAGES)
        .output()
        .map(|output| parse_pacman_q(&String::from_utf8_lossy(&output.stdout)))
        .unwrap_or_default()
}

/// The part of `version` that `rule` compares.
fn compared(version: &str, rule: Match) -> &str {
    match rule {
        Match::Exact => version,
        Match::Minor => version.match_indices('.').nth(1).map_or(version, |(end, _)| &version[..end]),
    }
}

/// Whether `version` of `link` matches the version in `built`; a package `built` does not name never matches.
fn same(built: &Versions, link: &Link, version: &str) -> bool {
    built.get(link.package).is_some_and(|built| compared(built, link.rule) == compared(version, link.rule))
}

/// Whether parts built against `built` run on a system with `system`: every linked package matches by its rule. A
/// package missing on either side does not match, so a build that names no version never counts as safe.
#[must_use]
pub fn fits(built: &Versions, system: &Versions) -> bool {
    LINKS.iter().all(|link| system.get(link.package).is_some_and(|version| same(built, link, version)))
}

/// The linked packages of `other` whose version does not match the one in `built`, to say what differs.
#[must_use]
pub fn differing(built: &Versions, other: &Versions) -> Versions {
    LINKS
        .iter()
        .filter_map(|link| {
            let version = other.get(link.package)?;
            (!same(built, link, version)).then(|| (link.package.to_string(), version.clone()))
        })
        .collect()
}

/// The linked packages of `versions` the way people read them: "KDE Plasma 6.8.2, KWin 6.8.2 and Qt 6.12.0".
#[must_use]
pub fn describe(versions: &Versions) -> String {
    let named: Vec<String> = LINKS
        .iter()
        .filter_map(|link| versions.get(link.package).map(|version| format!("{} {version}", link.name)))
        .collect();
    match named.split_last() {
        None => String::new(),
        Some((last, [])) => last.clone(),
        Some((last, rest)) => format!("{} and {last}", rest.join(", ")),
    }
}

/// The plugins the login script set aside (renamed `*.paused`) because they were built for other versions.
#[must_use]
pub fn paused_parts(plugins: &Path) -> Vec<String> {
    let mut paused = Vec::new();
    let mut folders = vec![plugins.to_path_buf()];
    while let Some(folder) = folders.pop() {
        let Ok(entries) = fs::read_dir(&folder) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                folders.push(path);
            } else if let Some(name) =
                path.file_name().and_then(|name| name.to_str()).and_then(|name| name.strip_suffix(".paused"))
            {
                paused.push(name.to_string());
            }
        }
    }
    paused.sort();
    paused
}

#[cfg(test)]
mod tests {
    use super::*;

    fn versions(pairs: &[(&str, &str)]) -> Versions {
        pairs.iter().map(|(name, version)| ((*name).to_string(), (*version).to_string())).collect()
    }

    // What aurora-theme 1.1.1-1 was built against (its .BUILDINFO, 2026-11-06).
    const BUILT: [(&str, &str); 4] =
        [("plasma-workspace", "6.8.2"), ("kwin", "6.8.2"), ("qt6-base", "6.12.0"), ("kdecoration", "6.8.2")];

    fn built_with(package: &str, version: &str) -> Versions {
        let mut system = versions(&BUILT);
        system.insert(package.to_string(), version.to_string());
        system
    }

    #[test]
    fn pacman_versions_lose_their_package_release_and_epoch() {
        let recorded = include_str!("../tests/fixtures/pacman-q.txt");
        let versions = parse_pacman_q(recorded);
        assert_eq!(versions["plasma-workspace"], "6.8.2");
        assert_eq!(versions["qt6-base"], "6.12.0");
        assert_eq!(parse_pacman_q("foo 1:2.3-4")["foo"], "2.3");
    }

    #[test]
    fn parts_fit_the_system_they_were_built_for() {
        assert!(fits(&versions(&BUILT), &versions(&BUILT)));
    }

    // installer/build.sh stamps the window buttons with KDecoration's major.minor and every other part with whole
    // versions, and the login script sets a part aside as soon as its stamp differs.
    #[test]
    fn a_kdecoration_bug_fix_release_still_fits_but_any_other_change_does_not() {
        let built = versions(&BUILT);
        assert!(fits(&built, &built_with("kdecoration", "6.8.3")));
        assert!(!fits(&built, &built_with("kdecoration", "6.9.0")));
        assert!(!fits(&built, &built_with("kwin", "6.8.3")));
        assert!(!fits(&built, &built_with("plasma-workspace", "6.8.3")));
        assert!(!fits(&built, &built_with("qt6-base", "6.12.1")));
    }

    #[test]
    fn a_build_that_names_no_version_for_a_linked_package_never_fits() {
        let mut built = versions(&BUILT);
        built.remove("kwin");
        assert!(!fits(&built, &versions(&BUILT)));
        let mut system = versions(&BUILT);
        system.remove("qt6-base");
        assert!(!fits(&versions(&BUILT), &system));
    }

    #[test]
    fn only_the_versions_that_break_the_parts_are_named_and_in_reading_order() {
        let upgraded = versions(&[
            ("plasma-workspace", "6.9.0"),
            ("kwin", "6.9.0"),
            ("qt6-base", "6.12.0"),
            ("kdecoration", "6.8.4"),
        ]);
        assert_eq!(describe(&differing(&versions(&BUILT), &upgraded)), "KDE Plasma 6.9.0 and KWin 6.9.0");
        assert_eq!(describe(&versions(&BUILT)), "KDE Plasma 6.8.2, KWin 6.8.2, Qt 6.12.0 and KDecoration 6.8.2");
        assert_eq!(describe(&versions(&[("qt6-base", "6.13.0")])), "Qt 6.13.0");
        assert_eq!(describe(&Versions::new()), "");
    }

    #[test]
    fn a_paused_plugin_is_found_in_any_plugin_folder() {
        let dir = std::env::temp_dir().join(format!("aurora-cli-paused-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(dir.join("org.kde.kdecoration3")).expect("create the folder");
        fs::write(dir.join("org.kde.kdecoration3/org.aurora.decoration.so.paused"), b"").expect("pause one");
        fs::write(dir.join("kept.so"), b"").expect("keep one");
        assert_eq!(paused_parts(&dir), ["org.aurora.decoration.so"]);
        fs::remove_dir_all(dir).expect("clean up");
    }
}
