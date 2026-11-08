// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora check-updates`: looks for a newer Aurora and offers it. The login timer runs it with `--notify` on the
//! user's schedule (`aurora settings`); a desktop notification then offers the update, and its button opens it in
//! Konsole. The menu looks again when the last look is a few minutes old, and offers only a version newer than the one
//! installed, however Aurora was updated in between (pacman, paru or `aurora update`).

use std::env;
use std::process::Command;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};

use crate::app::{self, Places};
use crate::download;
use crate::release;
use crate::source::{Kind, Source};
use crate::updates::{self, Choice, Record, Settings};

/// How old the last look may be before the menu looks again: a few minutes, so a release is seen as soon as the menu
/// opens after it, without making every quick run of `aurora` wait for the repositories (about 4 s here).
const FRESH: Duration = Duration::from_secs(5 * 60);

/// How the result is told.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Mode {
    /// On the schedule in the settings, with a desktop notification (the login timer).
    Notify,
    /// Now, in the terminal.
    Print,
}

/// # Errors
/// When the installed copy cannot be found, the newest version cannot be learned, or the record cannot be written.
pub fn check_updates(places: &Places, mode: Mode) -> Result<()> {
    let settings = Settings::load(&places.settings_file());
    let path = places.update_record();
    let mut record = Record::load(&path);
    let now = SystemTime::now().duration_since(UNIX_EPOCH).context("the clock is before 1970")?.as_secs();
    let since = record.checked.map(|at| Duration::from_secs(now.saturating_sub(at)));
    if mode == Mode::Notify && !settings.updates.due(since) {
        return Ok(());
    }
    let source = app::source(places)?;
    let installed = source.version();
    let newest = newest(places, &source)?;
    record.checked = Some(now);
    record.available.clone_from(&newest);
    record.save(&path)?;
    let Some(newest) = newest else {
        if mode == Mode::Print {
            println!("Aurora {installed} is the newest version.");
        }
        return Ok(());
    };
    match mode {
        Mode::Print => {
            println!("Aurora {} is available (you have {installed}): run `aurora update`.", updates::shown(&newest));
        }
        Mode::Notify if record.offers(&newest) => match updates::ask(&newest)? {
            Choice::Update => open_update()?,
            Choice::Skip => {
                record.skipped = Some(newest);
                record.save(&path)?;
            }
            Choice::Later => {}
        },
        Mode::Notify => {}
    }
    Ok(())
}

/// The version newer than the installed one that the repositories (for the package) or the GitHub releases (for an
/// archive or a checkout) have, if any.
fn newest(places: &Places, source: &Source) -> Result<Option<String>> {
    Ok(match source.kind {
        Kind::Package => updates::package_newest(&places.databases())?,
        Kind::Archive | Kind::Checkout => {
            Some(download::latest()?.version).filter(|version| release::is_newer(version, &source.version()))
        }
    })
}

/// Looks for a newer Aurora when the last look is older than [`FRESH`], for the menu: the record is saved and nothing
/// is printed. A look that fails (no network) leaves the record as it was.
///
/// # Errors
/// When the clock is before 1970 or the record cannot be written.
pub fn refresh(places: &Places, source: &Source) -> Result<()> {
    let path = places.update_record();
    let mut record = Record::load(&path);
    let now = SystemTime::now().duration_since(UNIX_EPOCH).context("the clock is before 1970")?.as_secs();
    if record.checked.is_some_and(|at| Duration::from_secs(now.saturating_sub(at)) < FRESH) {
        return Ok(());
    }
    let spinner = cliclack::spinner();
    spinner.start("Looking for a newer Aurora");
    match newest(places, source) {
        Ok(found) => {
            record.checked = Some(now);
            record.available = found;
            record.save(&path)?;
            spinner.clear();
        }
        Err(_) => spinner.clear(),
    }
    Ok(())
}

/// Opens `aurora update` in Konsole, which stays open when it ends so its last lines can be read.
fn open_update() -> Result<()> {
    let aurora = env::current_exe().context("finding the aurora program")?;
    Command::new("konsole").args(["--hold", "-e"]).arg(aurora).arg("update").spawn().context("opening Konsole")?;
    Ok(())
}

/// Whether `available`, as the last look noted it, is newer than `installed`: by pacman's rules for the package (its
/// full release, `1.3.1-1`), by semantic versioning for an archive or a checkout (its `VERSION`).
#[must_use]
pub fn is_newer(available: &str, installed: &str, kind: Kind) -> bool {
    match kind {
        Kind::Package => updates::is_newer_package(available, installed),
        Kind::Archive | Kind::Checkout => release::is_newer(available, installed),
    }
}

/// The line the menu shows when the last look found a version newer than `installed` (see [`is_newer`]). A version
/// that is not newer any more, because Aurora was updated another way since, is forgotten.
#[must_use]
pub fn notice(places: &Places, installed: &str, kind: Kind) -> Option<String> {
    let path = places.update_record();
    let mut record = Record::load(&path);
    let available = record.available.clone()?;
    if is_newer(&available, installed, kind) {
        return Some(format!("Aurora {} is available: choose Update", updates::shown(&available)));
    }
    record.available = None;
    let _ = record.save(&path);
    None
}

/// Forgets the version the last look found, once `aurora update` installed it.
///
/// # Errors
/// When the record cannot be written.
pub fn updated(places: &Places) -> Result<()> {
    let path = places.update_record();
    let mut record = Record::load(&path);
    if record.available.take().is_some() {
        record.save(&path)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::path::PathBuf;

    fn places(name: &str) -> Places {
        let root = std::env::temp_dir().join(format!("aurora-check-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        Places { home: root.clone(), config: root.join("config"), state: root.join("state"), cache: root.join("cache") }
    }

    fn noted(places: &Places, available: &str) -> PathBuf {
        let path = places.update_record();
        Record { checked: Some(1), available: Some(available.to_string()), skipped: None }
            .save(&path)
            .expect("the record is written");
        path
    }

    // The menu on 1.3.1 still said "Aurora 1.3.0 is available": the look had noted 1.3.0, then pacman installed 1.3.1.
    #[test]
    fn an_older_noted_version_is_not_offered_and_is_forgotten() {
        let places = places("older");
        let path = noted(&places, "1.3.0-1");
        assert_eq!(notice(&places, "1.3.1-1", Kind::Package), None);
        assert_eq!(Record::load(&path).available, None);
        assert_eq!(Record::load(&path).checked, Some(1));
        let _ = fs::remove_dir_all(&places.home);
    }

    #[test]
    fn the_installed_version_itself_is_not_offered() {
        let places = places("same");
        noted(&places, "1.3.1-1");
        assert_eq!(notice(&places, "1.3.1-1", Kind::Package), None);
        let _ = fs::remove_dir_all(&places.home);
    }

    #[test]
    fn a_newer_version_is_offered_and_kept() {
        let places = places("newer");
        let path = noted(&places, "1.3.1-2");
        assert_eq!(
            notice(&places, "1.3.1-1", Kind::Package).as_deref(),
            Some("Aurora 1.3.1-2 is available: choose Update")
        );
        assert_eq!(Record::load(&path).available.as_deref(), Some("1.3.1-2"));
        let _ = fs::remove_dir_all(&places.home);
    }

    #[test]
    fn an_archive_compares_its_version_by_semver() {
        assert!(is_newer("1.4.0", "1.3.1", Kind::Archive));
        assert!(!is_newer("1.3.0", "1.3.1", Kind::Checkout));
        assert!(!is_newer("1.3.1", "1.3.1", Kind::Archive));
    }
}
