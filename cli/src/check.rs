// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora check-updates`: looks for a newer Aurora and offers it. The login timer runs it with `--notify` on the
//! user's schedule (`aurora settings`); a desktop notification then offers the update, and its button opens it in
//! Konsole.

use std::env;
use std::process::Command;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};

use crate::app::{self, Places};
use crate::download;
use crate::release;
use crate::source::Kind;
use crate::updates::{self, Choice, Record, Settings};

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
    let newest = match source.kind {
        Kind::Package => updates::package_newest(&places.databases())?,
        Kind::Archive | Kind::Checkout => {
            Some(download::latest()?.version).filter(|version| release::is_newer(version, &installed))
        }
    };
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

/// Opens `aurora update` in Konsole, which stays open when it ends so its last lines can be read.
fn open_update() -> Result<()> {
    let aurora = env::current_exe().context("finding the aurora program")?;
    Command::new("konsole").args(["--hold", "-e"]).arg(aurora).arg("update").spawn().context("opening Konsole")?;
    Ok(())
}

/// The line the menu shows when the last look found a version newer than `installed`.
#[must_use]
pub fn notice(places: &Places, installed: &str) -> Option<String> {
    let available = Record::load(&places.update_record()).available?;
    let shown = updates::shown(&available);
    (shown != installed).then(|| format!("Aurora {shown} is available: choose Update"))
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
