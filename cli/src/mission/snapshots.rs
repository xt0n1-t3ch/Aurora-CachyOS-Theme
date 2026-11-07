// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Which Mission Center is installed, the snapshots `aurora mission-center` saves before a change, and the pacman
//! command that puts a snapshot's build back.

use std::fmt;
use std::fs;
use std::path::Path;
use std::process::Command;

use anyhow::{Result, bail};

use super::{PACKAGE, STOCK};
use crate::backups;

/// Which Mission Center is installed.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Installed {
    /// Aurora's build, at its full version (`1.2.0.r52.g039b197-3`).
    Aurora(String),
    /// The distribution's, at its full version.
    Stock(String),
    None,
}

impl Installed {
    /// Reads what `pacman -Q mission-center-improved mission-center` prints on standard output (one `name version`
    /// line per installed package), or a snapshot's `installed` file, which holds the same line.
    #[must_use]
    pub fn parse(text: &str) -> Installed {
        let mut found = Installed::None;
        for line in text.lines() {
            let mut words = line.split_whitespace();
            match (words.next(), words.next()) {
                (Some(PACKAGE), Some(version)) => return Installed::Aurora(version.to_string()),
                (Some(STOCK), Some(version)) => found = Installed::Stock(version.to_string()),
                _ => {}
            }
        }
        found
    }

    /// The text [`Installed::parse`] reads back: the `name version` line, or nothing.
    #[must_use]
    pub fn render(&self) -> String {
        match self {
            Installed::Aurora(version) => format!("{PACKAGE} {version}\n"),
            Installed::Stock(version) => format!("{STOCK} {version}\n"),
            Installed::None => String::new(),
        }
    }

    /// What is installed now; nothing when pacman cannot be asked.
    #[must_use]
    pub fn now() -> Installed {
        Command::new("pacman")
            .args(["-Q", PACKAGE, STOCK])
            .output()
            .map_or(Installed::None, |output| Installed::parse(&String::from_utf8_lossy(&output.stdout)))
    }
}

impl fmt::Display for Installed {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Installed::Aurora(version) => write!(f, "Aurora's build {version}"),
            Installed::Stock(version) => write!(f, "the distribution's {version}"),
            Installed::None => f.write_str("not installed"),
        }
    }
}

/// The full version (`pkgver-pkgrel`) a PKGBUILD builds, from its `pkgver=` and `pkgrel=` lines.
#[must_use]
pub fn recipe_version(pkgbuild: &str) -> Option<String> {
    let value = |key: &str| {
        pkgbuild
            .lines()
            .find_map(|line| line.strip_prefix(key)?.strip_prefix('=').map(|value| value.trim().to_string()))
    };
    Some(format!("{}-{}", value("pkgver")?, value("pkgrel")?))
}

/// The file name of Aurora's package at `version`, as makepkg and pacman's cache name it.
#[must_use]
pub fn package_file(version: &str) -> String {
    format!("{PACKAGE}-{version}-x86_64.pkg.tar.zst")
}

/// One snapshot: its name, the time it was taken (`YYYYmmdd-HHMMSS`), and the build installed then.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Snapshot {
    pub name: String,
    pub installed: Installed,
}

/// The snapshots in `root`, newest first. A folder is one when its name is a time and it holds an `installed` file.
#[must_use]
pub fn snapshots_in(root: &Path) -> Vec<Snapshot> {
    let mut found: Vec<Snapshot> = fs::read_dir(root)
        .into_iter()
        .flatten()
        .flatten()
        .filter_map(|entry| {
            let name = entry.file_name().into_string().ok()?;
            backups::taken_at(&name)?;
            let installed = fs::read_to_string(entry.path().join("installed")).ok()?;
            Some(Snapshot { name, installed: Installed::parse(&installed) })
        })
        .collect();
    found.sort_by(|a, b| b.name.cmp(&a.name));
    found
}

/// The pacman command that brings the installed Mission Center from `now` to `wanted`, or `None` when they are the
/// same. `package` is Aurora's package file of the wanted build, when that is Aurora's.
///
/// # Errors
/// When Aurora's build is wanted and its package is not kept anymore.
pub fn restore_command(wanted: &Installed, now: &Installed, package: Option<&Path>) -> Result<Option<Vec<String>>> {
    if wanted == now {
        return Ok(None);
    }
    let words: Vec<String> = match (wanted, now) {
        (Installed::Aurora(version), _) => {
            let Some(file) = package else {
                bail!(
                    "Aurora's Mission Center {version} is not kept anymore: `aurora mission-center on` builds it again"
                );
            };
            vec!["sudo".into(), "pacman".into(), "-U".into(), file.to_string_lossy().into_owned()]
        }
        (Installed::Stock(_), _) => vec!["sudo".into(), "pacman".into(), "-S".into(), STOCK.into()],
        (Installed::None, Installed::Aurora(_)) => vec!["sudo".into(), "pacman".into(), "-R".into(), PACKAGE.into()],
        (Installed::None, Installed::Stock(_)) => vec!["sudo".into(), "pacman".into(), "-R".into(), STOCK.into()],
        (Installed::None, Installed::None) => return Ok(None),
    };
    Ok(Some(words))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn aurora_s_build_is_told_from_the_distribution_s_and_from_none() {
        let both = "mission-center 1.2.0-1\nmission-center-improved 1.2.0.r52.g039b197-3\n";
        assert_eq!(Installed::parse(both), Installed::Aurora("1.2.0.r52.g039b197-3".into()));
        assert_eq!(Installed::parse("mission-center 1.2.0-1\n"), Installed::Stock("1.2.0-1".into()));
        assert_eq!(Installed::parse(""), Installed::None);
        assert_eq!(Installed::parse("mission-center-improved-git 1.0-1\n"), Installed::None);
    }

    #[test]
    fn a_snapshot_s_installed_file_reads_back_what_was_written() {
        for installed in
            [Installed::Aurora("1.2.0.r52.g039b197-3".into()), Installed::Stock("1.2.0-1".into()), Installed::None]
        {
            assert_eq!(Installed::parse(&installed.render()), installed);
        }
    }

    // components/mission-center/PKGBUILD, trimmed to its version lines.
    #[test]
    fn the_recipe_s_version_is_its_pkgver_and_pkgrel() {
        let pkgbuild = "pkgname=mission-center-improved\n_mc_commit=039b197\npkgver=1.2.0.r52.g039b197\npkgrel=3\n";
        assert_eq!(recipe_version(pkgbuild).as_deref(), Some("1.2.0.r52.g039b197-3"));
        assert_eq!(recipe_version("pkgver=1.0\n"), None);
        assert_eq!(
            package_file("1.2.0.r52.g039b197-3"),
            "mission-center-improved-1.2.0.r52.g039b197-3-x86_64.pkg.tar.zst"
        );
    }

    #[test]
    fn snapshots_are_listed_newest_first_and_other_folders_are_not_snapshots() {
        let root = std::env::temp_dir().join(format!("aurora-mission-snapshots-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        for (name, installed) in [("20261107-011500", "mission-center 1.2.0-1\n"), ("20261107-013000", "")] {
            fs::create_dir_all(root.join(name)).expect("create a snapshot");
            fs::write(root.join(name).join("installed"), installed).expect("write it");
        }
        fs::create_dir_all(root.join("notes")).expect("create another folder");
        fs::create_dir_all(root.join("20261107-020000")).expect("create a folder with no installed file");
        let names: Vec<String> = snapshots_in(&root).into_iter().map(|snapshot| snapshot.name).collect();
        assert_eq!(names, ["20261107-013000", "20261107-011500"]);
        fs::remove_dir_all(root).expect("clean up");
    }

    #[test]
    fn a_rollback_reinstalls_aurora_s_kept_package_or_the_distribution_s_or_removes_mission_center() {
        let aurora = Installed::Aurora("1.2.0.r52.g039b197-2".into());
        let stock = Installed::Stock("1.2.0-1".into());
        let file = Path::new("/kept/mission-center-improved-1.2.0.r52.g039b197-2-x86_64.pkg.tar.zst");
        assert_eq!(
            restore_command(&aurora, &stock, Some(file)).expect("a kept package").expect("a command"),
            ["sudo", "pacman", "-U", "/kept/mission-center-improved-1.2.0.r52.g039b197-2-x86_64.pkg.tar.zst"]
        );
        assert_eq!(
            restore_command(&stock, &aurora, None).expect("a repository package").expect("a command"),
            ["sudo", "pacman", "-S", "mission-center"]
        );
        assert_eq!(
            restore_command(&Installed::None, &aurora, None).expect("a removal").expect("a command"),
            ["sudo", "pacman", "-R", "mission-center-improved"]
        );
        assert_eq!(restore_command(&aurora, &aurora, None).expect("nothing to do"), None);
        assert!(restore_command(&aurora, &stock, None).is_err(), "Aurora's package is gone");
    }
}
