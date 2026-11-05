// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora backups`: the copies of your desktop's settings the install saved before it changed anything, and putting
//! one back. `installer/install.py` writes them in `~/.local/state/aurora/backups/<YYYYMMDD-HHMMSS>`, each with the
//! list of settings it saved and the `restore.sh` that puts them back; `latest-backup` names the one
//! `aurora uninstall` uses.

use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result, bail};
use console::Style;

use crate::app::{self, Places};
use crate::run;

/// One backup.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Backup {
    /// The folder's name, the time it was taken (`20261104-061122`).
    pub name: String,
    pub path: PathBuf,
    /// How many settings files it saved.
    pub saved: usize,
    /// The one `aurora uninstall` restores.
    pub latest: bool,
}

const MONTHS: [&str; 12] = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

/// When a backup was taken, from its folder's name, written for people: `20261104-061122` is "4 Nov 2026, 06:11".
#[must_use]
pub fn taken_at(name: &str) -> Option<String> {
    let (date, time) = name.split_once('-')?;
    if date.len() != 8 || time.len() != 6 || !date.bytes().chain(time.bytes()).all(|byte| byte.is_ascii_digit()) {
        return None;
    }
    let month: usize = date[4..6].parse().ok()?;
    let day: u32 = date[6..8].parse().ok()?;
    let name_of_month = MONTHS.get(month.checked_sub(1)?)?;
    Some(format!("{day} {name_of_month} {}, {}:{}", &date[..4], &time[..2], &time[2..4]))
}

/// Where install.py keeps the backups: the home folder's `.local/state/aurora`, whatever `XDG_STATE_HOME` says.
fn folder(places: &Places) -> PathBuf {
    places.home.join(".local/state/aurora")
}

/// The backups under `root` (the folder holding `backups/` and `latest-backup`), newest first. Folders without a
/// `restore.sh` are not backups and are left out.
#[must_use]
pub fn list_in(root: &Path) -> Vec<Backup> {
    let latest = fs::read_to_string(root.join("latest-backup")).map(|text| PathBuf::from(text.trim())).ok();
    let Ok(entries) = fs::read_dir(root.join("backups")) else {
        return Vec::new();
    };
    let mut found: Vec<Backup> = entries
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.join("restore.sh").is_file())
        .map(|path| Backup {
            name: path.file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_default(),
            saved: fs::read_to_string(path.join("saved.txt"))
                .map_or(0, |text| text.lines().filter(|line| !line.trim().is_empty()).count()),
            latest: latest.as_deref() == Some(path.as_path()),
            path,
        })
        .collect();
    found.sort_by(|a, b| b.name.cmp(&a.name));
    found
}

/// One line for a backup in the list.
fn line(backup: &Backup) -> String {
    let when = taken_at(&backup.name).unwrap_or_else(|| backup.name.clone());
    let used = if backup.latest { ", the one `aurora uninstall` puts back" } else { "" };
    format!("{when}  {} settings saved{used}", backup.saved)
}

/// What `aurora backups` does.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Action {
    List,
    /// Puts the backup of this name back, or without one, the one `aurora uninstall` uses.
    Restore(Option<String>),
}

/// `aurora backups`: lists them, or puts one back.
///
/// # Errors
/// When there is no backup, the name matches none, the user cancels, or the restore fails.
pub fn backups(places: &Places, action: Action, yes: bool) -> Result<()> {
    let root = folder(places);
    let found = list_in(&root);
    let Action::Restore(wanted) = action else {
        if found.is_empty() {
            println!("No backup yet: the install saves your desktop the first time it runs.");
            return Ok(());
        }
        println!("Backups of your desktop from before Aurora, in {}:", root.join("backups").display());
        for backup in &found {
            println!("  {}  {}", backup.name, line(backup));
        }
        println!("Put one back with `aurora backups restore NAME`.");
        return Ok(());
    };
    let backup = match &wanted {
        Some(name) => found.iter().find(|backup| &backup.name == name),
        None => found.iter().find(|backup| backup.latest).or_else(|| found.first()),
    }
    .with_context(|| match &wanted {
        Some(name) => format!("no backup is called {name}: `aurora backups` lists them"),
        None => "no backup yet: the install saves your desktop the first time it runs".to_string(),
    })?;
    cliclack::intro(Style::new().bold().apply_to("Put a backup back"))?;
    cliclack::log::info(line(backup))?;
    if !yes
        && console::Term::stdout().is_term()
        && !cliclack::confirm("This removes Aurora and puts these settings back. Go ahead?")
            .initial_value(false)
            .interact()?
    {
        cliclack::outro_cancel("Nothing was changed")?;
        return Ok(());
    }
    if !backup.path.join("restore.sh").is_file() {
        bail!("{} has no restore.sh", backup.path.display());
    }
    let askpass = app::askpass(places);
    app::sudo_ready(askpass.as_deref())?;
    let env: Vec<(&str, &Path)> = askpass.iter().map(|dialog| ("SUDO_ASKPASS", dialog.as_path())).collect();
    run::script(&backup.path.join("restore.sh"), &[], &env, &places.state, "Putting your desktop back")?;
    cliclack::outro("Your desktop from before Aurora is back. Log out and back in to finish.")?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::fs;

    use super::{list_in, taken_at};

    #[test]
    fn a_backup_s_name_reads_as_the_day_and_time_it_was_taken() {
        assert_eq!(taken_at("20261104-061122").as_deref(), Some("4 Nov 2026, 06:11"));
        assert_eq!(taken_at("20261231-235959").as_deref(), Some("31 Dec 2026, 23:59"));
    }

    #[test]
    fn a_folder_named_otherwise_has_no_time() {
        assert_eq!(taken_at("old"), None);
        assert_eq!(taken_at("20261304-061122"), None, "there is no thirteenth month");
        assert_eq!(taken_at("2026110-0611220"), None);
    }

    #[test]
    fn backups_are_listed_newest_first_with_their_count_and_the_one_uninstall_uses() {
        let root = tempfile_dir("backups-list");
        for (name, saved) in [("20261101-090000", "a\nb\n"), ("20261104-061122", "a\nb\nc\n")] {
            let folder = root.join("backups").join(name);
            fs::create_dir_all(&folder).expect("a backup folder");
            fs::write(folder.join("restore.sh"), "#!/bin/bash\n").expect("restore.sh");
            fs::write(folder.join("saved.txt"), saved).expect("saved.txt");
        }
        fs::create_dir_all(root.join("backups/not-a-backup")).expect("a stray folder");
        fs::write(root.join("latest-backup"), format!("{}\n", root.join("backups/20261101-090000").display()))
            .expect("latest-backup");

        let found = list_in(&root);
        let names: Vec<&str> = found.iter().map(|backup| backup.name.as_str()).collect();
        assert_eq!(names, ["20261104-061122", "20261101-090000"]);
        assert_eq!(found[0].saved, 3);
        assert!(found[1].latest && !found[0].latest, "latest-backup names the older one");
        fs::remove_dir_all(&root).expect("cleanup");
    }

    #[test]
    fn without_a_backups_folder_there_are_none() {
        assert_eq!(list_in(&std::env::temp_dir().join("aurora-no-such-folder")), []);
    }

    /// A fresh folder in the temporary directory, unique to this test run.
    fn tempfile_dir(name: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("aurora-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).expect("a temporary folder");
        dir
    }
}
