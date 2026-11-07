// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Update notices: how often to look for a newer Aurora (the user's setting), what the last look found, and the
//! desktop notification that offers it.

use std::fmt;
use std::fs;
use std::path::Path;
use std::process::Command;
use std::time::Duration;

use anyhow::{Context, Result, bail};

/// The pacman package Aurora comes in.
pub const PACKAGE: &str = "aurora-theme";

/// A day, the period of the login timer that runs `aurora check-updates --notify`.
const DAY: Duration = Duration::from_secs(24 * 60 * 60);
/// How much earlier than its period a check still counts as due, so a timer that fires a little early or a login
/// a few hours sooner than yesterday's does not skip a whole period.
const SLACK: Duration = Duration::from_secs(4 * 60 * 60);

/// How often Aurora looks for a newer version on its own.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Frequency {
    #[default]
    Daily,
    Weekly,
    Never,
}

impl Frequency {
    /// Every choice, in the order the settings menu shows them.
    pub const ALL: [Frequency; 3] = [Frequency::Daily, Frequency::Weekly, Frequency::Never];

    /// The word in `aurora.conf`.
    #[must_use]
    pub fn id(self) -> &'static str {
        match self {
            Frequency::Daily => "daily",
            Frequency::Weekly => "weekly",
            Frequency::Never => "never",
        }
    }

    /// The word in `aurora.conf`, read back; `None` for anything else.
    #[must_use]
    pub fn from_id(id: &str) -> Option<Frequency> {
        Frequency::ALL.into_iter().find(|frequency| frequency.id() == id.trim())
    }

    /// Whether a check is due when the last one was `since` ago (`None`: never checked).
    #[must_use]
    pub fn due(self, since: Option<Duration>) -> bool {
        let period = match self {
            Frequency::Never => return false,
            Frequency::Daily => DAY,
            Frequency::Weekly => DAY * 7,
        };
        since.is_none_or(|since| since + SLACK >= period)
    }
}

impl fmt::Display for Frequency {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Frequency::Daily => "Every day",
            Frequency::Weekly => "Every week",
            Frequency::Never => "Never",
        })
    }
}

/// How fast the desktop animates: Plasma's `AnimationDurationFactor`, one step of its own speed slider each.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Speed {
    #[default]
    Smooth,
    Fast,
    Instant,
}

impl Speed {
    /// Every choice, in the order the settings menu shows them.
    pub const ALL: [Speed; 3] = [Speed::Smooth, Speed::Fast, Speed::Instant];

    /// The word in `aurora.conf`; installer/install.py's ANIMATIONS reads the same words.
    #[must_use]
    pub fn id(self) -> &'static str {
        match self {
            Speed::Smooth => "smooth",
            Speed::Fast => "fast",
            Speed::Instant => "instant",
        }
    }

    /// The word in `aurora.conf`, read back; `None` for anything else.
    #[must_use]
    pub fn from_id(id: &str) -> Option<Speed> {
        Speed::ALL.into_iter().find(|speed| speed.id() == id.trim())
    }

    /// Plasma's `AnimationDurationFactor` for this speed (kdeglobals, [KDE]).
    #[must_use]
    pub fn factor(self) -> &'static str {
        match self {
            Speed::Smooth => "1",
            Speed::Fast => "0.5",
            Speed::Instant => "0",
        }
    }
}

impl fmt::Display for Speed {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Speed::Smooth => "Smooth",
            Speed::Fast => "Fast",
            Speed::Instant => "Instant",
        })
    }
}

/// The user's settings, in `~/.config/aurora/aurora.conf`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Settings {
    pub updates: Frequency,
    pub animations: Speed,
}

impl Settings {
    /// Reads `aurora.conf`'s text; a missing or unknown value keeps its default.
    #[must_use]
    pub fn parse(text: &str) -> Settings {
        let mut settings = Settings::default();
        let mut group = "";
        for line in text.lines().map(str::trim) {
            if let Some(name) = line.strip_prefix('[').and_then(|rest| rest.strip_suffix(']')) {
                group = name;
            } else if let Some((key, value)) = line.split_once('=') {
                match (group, key.trim()) {
                    ("updates", "check") => settings.updates = Frequency::from_id(value).unwrap_or(settings.updates),
                    ("look", "animations") => {
                        settings.animations = Speed::from_id(value).unwrap_or(settings.animations);
                    }
                    _ => {}
                }
            }
        }
        settings
    }

    /// The text of `aurora.conf`.
    #[must_use]
    pub fn render(self) -> String {
        format!(
            "# Aurora's settings: `aurora settings` changes them.\n\n[updates]\n# How often Aurora looks for a newer \
             version: daily, weekly or never.\ncheck={}\n\n[look]\n# How fast the desktop animates: smooth, fast or \
             instant.\nanimations={}\n",
            self.updates.id(),
            self.animations.id()
        )
    }

    /// The settings at `path`, or the defaults when there is no file.
    #[must_use]
    pub fn load(path: &Path) -> Settings {
        fs::read_to_string(path).map(|text| Settings::parse(&text)).unwrap_or_default()
    }

    /// # Errors
    /// When the file cannot be written.
    pub fn save(self, path: &Path) -> Result<()> {
        write(path, &self.render())
    }
}

/// What the last look for updates found, in `~/.local/state/aurora/updates`.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Record {
    /// When it looked, in seconds since the Unix epoch.
    pub checked: Option<u64>,
    /// The newer version it found, if any.
    pub available: Option<String>,
    /// The version the user chose to skip: no notice offers it again.
    pub skipped: Option<String>,
}

impl Record {
    /// Reads the record's text; unknown lines are ignored.
    #[must_use]
    pub fn parse(text: &str) -> Record {
        let mut record = Record::default();
        for (key, value) in text.lines().filter_map(|line| line.split_once('=')) {
            let value = value.trim();
            match key.trim() {
                "checked" => record.checked = value.parse().ok(),
                "available" if !value.is_empty() => record.available = Some(value.to_string()),
                "skipped" if !value.is_empty() => record.skipped = Some(value.to_string()),
                _ => {}
            }
        }
        record
    }

    /// The record's text.
    #[must_use]
    pub fn render(&self) -> String {
        let field = |value: Option<&str>| value.unwrap_or_default().to_string();
        format!(
            "checked={}\navailable={}\nskipped={}\n",
            self.checked.map(|at| at.to_string()).unwrap_or_default(),
            field(self.available.as_deref()),
            field(self.skipped.as_deref())
        )
    }

    /// The record at `path`, or an empty one.
    #[must_use]
    pub fn load(path: &Path) -> Record {
        fs::read_to_string(path).map(|text| Record::parse(&text)).unwrap_or_default()
    }

    /// # Errors
    /// When the file cannot be written.
    pub fn save(&self, path: &Path) -> Result<()> {
        write(path, &self.render())
    }

    /// Whether a notice should offer `version`: not when the user skipped exactly that one.
    #[must_use]
    pub fn offers(&self, version: &str) -> bool {
        self.skipped.as_deref() != Some(version)
    }
}

fn write(path: &Path, text: &str) -> Result<()> {
    if let Some(folder) = path.parent() {
        fs::create_dir_all(folder).with_context(|| format!("creating {}", folder.display()))?;
    }
    fs::write(path, text).with_context(|| format!("writing {}", path.display()))
}

/// The newer version of `package` in `checkupdates` output (`name old -> new` per line), or `None`.
#[must_use]
pub fn parse_checkupdates(output: &str, package: &str) -> Option<String> {
    output.lines().find_map(|line| {
        let mut words = line.split_whitespace();
        (words.next() == Some(package)).then_some(())?;
        let (_, arrow, newest) = (words.next()?, words.next()?, words.next()?);
        (arrow == "->").then(|| newest.to_string())
    })
}

/// A package version as people read it: `1.1.0` for `1.1.0-1`; a later package release keeps its number (`1.1.0-2`).
#[must_use]
pub fn shown(version: &str) -> &str {
    version.strip_suffix("-1").unwrap_or(version)
}

/// The installed aurora-theme's full version, package release included, from `pacman -Q aurora-theme`.
#[must_use]
pub fn installed_release(pacman_q: &str) -> Option<String> {
    let mut words = pacman_q.split_whitespace();
    (words.next() == Some(PACKAGE)).then(|| words.next().map(str::to_string)).flatten()
}

/// What `checkupdates` lists: one `name installed -> newest` line per package that has a newer version in the
/// repositories pacman uses, or nothing. It refreshes its own copy of pacman's databases in `databases`, so it needs
/// no root and pacman's own databases stay as they are.
///
/// # Errors
/// When `checkupdates` fails (exit status 1: no network or a broken database); 2 means nothing to update.
pub fn checkupdates(databases: &Path) -> Result<String> {
    let output = Command::new("checkupdates")
        .env("CHECKUPDATES_DB", databases)
        .output()
        .context("running checkupdates (pacman-contrib)")?;
    match output.status.code() {
        Some(0) => Ok(String::from_utf8_lossy(&output.stdout).into_owned()),
        Some(2) => Ok(String::new()),
        _ => bail!("checkupdates failed: {}", String::from_utf8_lossy(&output.stderr).trim()),
    }
}

/// The newer aurora-theme in the repositories pacman uses (see [`checkupdates`]), or `None` when the installed one
/// is the newest.
///
/// # Errors
/// When `checkupdates` fails.
pub fn package_newest(databases: &Path) -> Result<Option<String>> {
    Ok(parse_checkupdates(&checkupdates(databases)?, PACKAGE))
}

/// What the user did with the notice.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Choice {
    Update,
    Skip,
    Later,
}

impl Choice {
    /// The action `notify-send` printed: the name of the button pressed, or nothing when the notice was closed.
    #[must_use]
    pub fn from_action(printed: &str) -> Choice {
        match printed.trim() {
            "update" => Choice::Update,
            "skip" => Choice::Skip,
            _ => Choice::Later,
        }
    }
}

/// The `notify-send` arguments of the notice that offers `newest`.
#[must_use]
pub fn notice_args(newest: &str) -> Vec<String> {
    [
        "--app-name=Aurora",
        "--icon=system-software-update",
        // It stays until it is answered: an expired popup leaves notify-send waiting for a button nobody sees.
        "--expire-time=0",
        "--action=update=Update now",
        "--action=skip=Skip this version",
        "Aurora",
        &format!("New version {} is available for download", shown(newest)),
    ]
    .map(str::to_string)
    .to_vec()
}

/// Shows the notice and waits for the user's answer.
///
/// # Errors
/// When `notify-send` cannot run.
pub fn ask(newest: &str) -> Result<Choice> {
    let output = Command::new("notify-send").args(notice_args(newest)).output().context("running notify-send")?;
    Ok(Choice::from_action(&String::from_utf8_lossy(&output.stdout)))
}

#[cfg(test)]
mod tests {
    use super::*;

    const HOUR: Duration = Duration::from_secs(60 * 60);

    #[test]
    fn a_first_check_is_due_unless_checks_are_off() {
        assert!(Frequency::Daily.due(None));
        assert!(Frequency::Weekly.due(None));
        assert!(!Frequency::Never.due(None));
    }

    #[test]
    fn a_daily_check_waits_for_the_next_day_with_a_few_hours_of_slack() {
        assert!(!Frequency::Daily.due(Some(HOUR * 12)));
        assert!(Frequency::Daily.due(Some(HOUR * 20)));
        assert!(Frequency::Daily.due(Some(HOUR * 30)));
    }

    #[test]
    fn a_weekly_check_waits_for_the_next_week_and_never_means_never() {
        assert!(!Frequency::Weekly.due(Some(DAY * 3)));
        assert!(Frequency::Weekly.due(Some(DAY * 7)));
        assert!(!Frequency::Never.due(Some(DAY * 365)));
    }

    #[test]
    fn settings_read_back_what_they_wrote() {
        for updates in Frequency::ALL {
            for animations in Speed::ALL {
                let settings = Settings { updates, animations };
                assert_eq!(Settings::parse(&settings.render()), settings, "{updates:?} {animations:?}");
            }
        }
    }

    #[test]
    fn animations_are_smooth_unless_another_speed_is_chosen() {
        assert_eq!(Settings::parse("").animations, Speed::Smooth);
        assert_eq!(Settings::parse("[look]\nanimations = fast\n").animations, Speed::Fast);
        assert_eq!(Settings::parse("[look]\nanimations=warp\n").animations, Speed::Smooth);
        assert_eq!(Settings::parse("[updates]\nanimations=instant\n").animations, Speed::Smooth, "only [look] counts");
    }

    // Steps of Plasma's own speed slider (4, 2, 1.5, 1, 0.75, 0.5, 0): smooth is its designed speed.
    #[test]
    fn each_speed_is_a_step_of_plasma_s_slider() {
        assert_eq!(Speed::ALL.map(Speed::factor), ["1", "0.5", "0"]);
    }

    #[test]
    fn settings_without_a_file_or_with_an_unknown_value_check_daily() {
        assert_eq!(Settings::parse("").updates, Frequency::Daily);
        assert_eq!(Settings::parse("[updates]\ncheck=hourly\n").updates, Frequency::Daily);
        assert_eq!(Settings::parse("[other]\ncheck=never\n").updates, Frequency::Daily, "only [updates] counts");
        assert_eq!(Settings::parse("[updates]\n check = weekly \n").updates, Frequency::Weekly);
    }

    #[test]
    fn the_record_reads_back_what_it_wrote() {
        let full =
            Record { checked: Some(1_793_850_000), available: Some("1.1.0-1".into()), skipped: Some("1.0.5-1".into()) };
        assert_eq!(Record::parse(&full.render()), full);
        assert_eq!(Record::parse(&Record::default().render()), Record::default());
    }

    #[test]
    fn a_skipped_version_is_not_offered_again_but_the_next_one_is() {
        let record = Record { skipped: Some("1.1.0-1".into()), ..Record::default() };
        assert!(!record.offers("1.1.0-1"));
        assert!(record.offers("1.2.0-1"));
        assert!(Record::default().offers("1.1.0-1"));
    }

    // Recorded on CachyOS, 2026-11-05, pacman-contrib 1.13.1 (one line per package: name, installed, ->, newest),
    // with an aurora-theme line in the same format.
    const CHECKUPDATES: &str = "ca-certificates-mozilla 3.132-1.1 -> 3.133-1.1\nelectron43 43.8.6-1 -> 43.8.7-1\n\
                                aurora-theme 1.0.0-1 -> 1.1.0-1\nfirefox 159.0-1.1 -> 159.0.1-1.1\n";

    #[test]
    fn checkupdates_names_the_newer_aurora_among_the_other_packages() {
        assert_eq!(parse_checkupdates(CHECKUPDATES, PACKAGE).as_deref(), Some("1.1.0-1"));
    }

    #[test]
    fn checkupdates_without_aurora_finds_nothing_even_for_a_similar_name() {
        let others = "aurora-theme-git 1.0-1 -> 1.1-1\nfirefox 159.0-1.1 -> 159.0.1-1.1\n";
        assert_eq!(parse_checkupdates(others, PACKAGE), None);
        assert_eq!(parse_checkupdates("", PACKAGE), None);
    }

    #[test]
    fn a_first_package_release_shows_as_its_version_and_a_later_one_keeps_its_number() {
        assert_eq!(shown("1.1.0-1"), "1.1.0");
        assert_eq!(shown("1.1.0-2"), "1.1.0-2");
        assert_eq!(shown("1.1.0"), "1.1.0");
    }

    // A package release (1.0.0-1 to 1.0.0-3) changes no VERSION file, so `aurora update` compares this instead.
    #[test]
    fn the_installed_package_release_is_read_with_its_package_number() {
        assert_eq!(installed_release("aurora-theme 1.0.0-3\n").as_deref(), Some("1.0.0-3"));
        assert_eq!(installed_release("error: package 'aurora-theme' was not found\n"), None);
        assert_eq!(installed_release(""), None);
    }

    #[test]
    fn the_button_pressed_decides_and_closing_the_notice_means_later() {
        assert_eq!(Choice::from_action("update\n"), Choice::Update);
        assert_eq!(Choice::from_action("skip\n"), Choice::Skip);
        assert_eq!(Choice::from_action(""), Choice::Later);
    }

    #[test]
    fn the_notice_is_titled_aurora_names_the_new_version_and_offers_both_buttons() {
        let args = notice_args("1.1.0-1");
        let text: Vec<&str> = args.iter().filter(|arg| !arg.starts_with("--")).map(String::as_str).collect();
        assert_eq!(text, ["Aurora", "New version 1.1.0 is available for download"]);
        assert!(args.contains(&"--action=update=Update now".to_string()), "{args:?}");
        assert!(args.contains(&"--action=skip=Skip this version".to_string()), "{args:?}");
    }

    // A popup that expires goes to the history while notify-send still waits for a button, so the notice would be
    // missed by anyone away from the screen for five seconds. 0 means it stays until it is answered (the
    // Desktop Notifications Specification, expire_timeout).
    #[test]
    fn the_notice_stays_on_screen_until_it_is_answered() {
        assert!(notice_args("1.1.0-1").contains(&"--expire-time=0".to_string()));
    }
}
