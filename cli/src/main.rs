// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora`: installs, updates and removes Aurora. Run it without a command for a guided menu.

use std::process::ExitCode;

use aurora::app::{self, InstallChoices, Places};
use aurora::backups;
use aurora::check::{self, Mode};
use aurora::icons;
use aurora::logo::{Colors, Logo};
use aurora::mission;
use aurora::settings;
use aurora::tuning::{self, Switch};
use aurora::ui;
use aurora::update;
use clap::{Parser, Subcommand};
use console::Style;

/// The theme's version: the copy's VERSION file, compiled in.
const VERSION: &str = include_str!("../../VERSION");

#[derive(Parser)]
#[command(
    name = "aurora",
    version = VERSION.trim(),
    about = "Installs, updates and removes Aurora, a macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6",
    after_help = "Without a command, aurora shows a menu of these.\n\nAurora by xt0n1 is free software with ABSOLUTELY NO \
                  WARRANTY; `aurora license` shows its license and terms."
)]
struct Cli {
    #[command(subcommand)]
    command: Option<Action>,
}

#[derive(Subcommand)]
enum Action {
    /// Install Aurora, step by step
    Install {
        /// The logo at the left of the menu bar: cachyos (the default), arch, apple or kali
        #[arg(long, value_name = "LOGO")]
        logo: Option<Logo>,
        /// Show that logo in its own colors instead of black and white
        #[arg(long)]
        colors: bool,
        /// Also install the measured performance tuning (scheduler, memory, zram)
        #[arg(long)]
        performance: bool,
        /// Ask nothing and use the choices given on the command line
        #[arg(long, short)]
        yes: bool,
    },
    /// Get the newest version and install it
    Update {
        /// Ask nothing
        #[arg(long, short)]
        yes: bool,
    },
    /// Build the parts again after a Plasma, KWin or Qt update
    Rebuild,
    /// Change the logo at the left of the menu bar
    Logo {
        /// cachyos, arch, apple or kali; without it, aurora asks
        logo: Option<Logo>,
        /// Show the logo in its own colors instead of black and white
        #[arg(long)]
        colors: bool,
    },
    /// Change the icon theme: Aurora's, or any other installed one
    Icons {
        /// The theme's folder name (breeze, Win11...) or aurora; without it, aurora asks
        theme: Option<String>,
    },
    /// Turn the measured performance tuning (scheduler, memory, zram) on or off
    Performance {
        /// on or off; without it, aurora shows the settings and asks
        #[arg(value_enum)]
        switch: Option<Switch>,
        /// Ask nothing
        #[arg(long, short)]
        yes: bool,
    },
    /// List the backups of your desktop from before Aurora, or put one back
    Backups {
        #[command(subcommand)]
        action: Option<BackupsAction>,
    },
    /// Use Aurora's Mission Center or the distribution's, change its settings, or roll a change back
    MissionCenter {
        #[command(subcommand)]
        action: Option<MissionAction>,
    },
    /// Change Aurora after the install: update notices, animations, the menu bar logo, icons and tuning
    Settings {
        /// How often Aurora looks for a newer version: daily, weekly or never
        #[arg(long, value_name = "WHEN")]
        updates: Option<String>,
        /// How fast the desktop animates: smooth, fast or instant
        #[arg(long, value_name = "SPEED")]
        animations: Option<String>,
    },
    /// Look for a newer Aurora now
    CheckUpdates {
        /// Only when the settings say a look is due, with a desktop notification (the login timer uses this)
        #[arg(long)]
        notify: bool,
    },
    /// Show what is installed and whether a rebuild is due
    Status,
    /// Show who made Aurora, its license and its terms
    License,
    /// Put your desktop back as it was before Aurora
    Uninstall {
        /// Ask nothing
        #[arg(long, short)]
        yes: bool,
    },
}

#[derive(Subcommand)]
enum BackupsAction {
    /// List the backups (the same as `aurora backups`)
    List,
    /// Remove Aurora and put a backup's settings back; without a name, the one `aurora uninstall` uses
    Restore {
        /// The backup's name, as `aurora backups` lists it
        name: Option<String>,
        /// Ask nothing
        #[arg(long, short)]
        yes: bool,
    },
}

#[derive(Subcommand)]
enum MissionAction {
    /// Install Aurora's build of Mission Center in place of the distribution's
    On {
        /// Ask nothing
        #[arg(long, short)]
        yes: bool,
    },
    /// Go back to the distribution's Mission Center; its settings stay
    Off,
    /// Put back a snapshot taken before a change, build and settings; without a name, the newest
    Rollback {
        /// The snapshot's name, as `aurora mission-center snapshots` lists it
        name: Option<String>,
        /// Ask nothing
        #[arg(long, short)]
        yes: bool,
    },
    /// List the snapshots taken before each change
    Snapshots,
    /// Change Mission Center's settings: a menu, or every key and value outside a terminal
    Settings {
        /// Set KEY to VALUE after checking what the key takes (any key of io.missioncenter.MissionCenter)
        #[arg(long = "set", value_name = "KEY=VALUE")]
        set: Vec<String>,
        /// Put KEY back to its default
        #[arg(long, value_name = "KEY")]
        reset: Vec<String>,
    },
}

/// The library's action and its `--yes` for a `mission-center` subcommand.
fn mission_action(action: Option<MissionAction>) -> (Option<mission::Action>, bool) {
    match action {
        None => (None, false),
        Some(MissionAction::On { yes }) => (Some(mission::Action::On), yes),
        Some(MissionAction::Off) => (Some(mission::Action::Off), false),
        Some(MissionAction::Rollback { name, yes }) => (Some(mission::Action::Rollback(name)), yes),
        Some(MissionAction::Snapshots) => (Some(mission::Action::Snapshots), false),
        Some(MissionAction::Settings { set, reset }) => (Some(mission::Action::Settings { set, reset }), false),
    }
}

fn colors(on: bool) -> Colors {
    if on { Colors::On } else { Colors::Off }
}

/// The menu shown when aurora runs without a command.
fn menu(places: &Places) -> anyhow::Result<()> {
    if !places.installed() {
        return app::install(places, InstallChoices::default());
    }
    if let Ok(source) = app::source(places) {
        check::refresh(places, &source)?;
        if let Some(line) = check::notice(places, &update::release_of(&source), source.kind) {
            cliclack::log::info(line)?;
        }
    }
    let action: &str = cliclack::select("What would you like to do?")
        .item("update", "Update", "get the newest version and install it")
        .item("settings", "Settings", "update notices, animations, the menu bar logo, icons, tuning and Mission Center")
        .item("logo", "Change the menu bar logo", "CachyOS, Arch Linux, Apple or Kali Linux")
        .item("icons", "Change the icons", "Aurora's, or any other installed icon theme")
        .item(
            "performance",
            "Performance tuning",
            format!("the measured scheduler, memory and zram settings, now {}", tuning::current()),
        )
        .item(
            "mission-center",
            "Mission Center",
            format!("Aurora's build or the distribution's, now {}", mission::Installed::now()),
        )
        .item("rebuild", "Rebuild after a system update", "when Plasma, KWin or Qt changed")
        .item("status", "Status", "what is installed and from where")
        .item("backups", "Backups", "the copies of your desktop from before Aurora")
        .item("uninstall", "Remove Aurora", "put your desktop back as it was")
        .item("license", "License and credits", "who made Aurora and the terms it comes with")
        .interact()?;
    match action {
        "update" => update::update(places, false),
        "settings" => settings::settings(places, None, None),
        "logo" => app::logo(None, Colors::Off),
        "icons" => icons::icons(places, None),
        "performance" => tuning::performance(places, None, false),
        "backups" => backups::backups(places, backups::Action::List, false),
        "mission-center" => mission::mission_center(places, None, false),
        "rebuild" => app::rebuild(places),
        "status" => app::status(places),
        "license" => app::license(places, VERSION.trim()),
        _ => app::uninstall(places, false),
    }
}

/// Whether to draw the wordmark first. A one-line answer (status, or a logo named on the command line) skips it, and
/// so does a run from get.sh, which drew it already and says so with `AURORA_BANNER=shown`.
fn shows_banner(command: Option<&Action>, banner: Option<&str>) -> bool {
    let one_line = matches!(
        command,
        Some(
            Action::Logo { logo: Some(_), .. }
                | Action::Icons { theme: Some(_) }
                | Action::Performance { switch: Some(_), .. }
                | Action::Backups { action: None | Some(BackupsAction::List) }
                | Action::MissionCenter { action: Some(MissionAction::Snapshots) }
                | Action::Settings { updates: Some(_), .. }
                | Action::Settings { animations: Some(_), .. }
                | Action::CheckUpdates { .. }
                | Action::Status
                | Action::License
        )
    );
    let scripted = matches!(
        command,
        Some(Action::MissionCenter { action: Some(MissionAction::Settings { set, reset }) })
            if !set.is_empty() || !reset.is_empty()
    );
    !one_line && !scripted && banner != Some("shown")
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    cliclack::set_theme(ui::AuroraTheme);
    let result = Places::from_env().and_then(|places| {
        if shows_banner(cli.command.as_ref(), std::env::var("AURORA_BANNER").ok().as_deref()) {
            println!("{}", ui::banner(VERSION.trim()));
        }
        match cli.command {
            None => menu(&places),
            Some(Action::Install { logo, colors: on, performance, yes }) => {
                app::install(&places, InstallChoices { logo, colors: colors(on), performance, yes })
            }
            Some(Action::Update { yes }) => update::update(&places, yes),
            Some(Action::Rebuild) => app::rebuild(&places),
            Some(Action::Logo { logo, colors: on }) => app::logo(logo, colors(on)),
            Some(Action::Icons { theme }) => icons::icons(&places, theme.as_deref()),
            Some(Action::Performance { switch, yes }) => tuning::performance(&places, switch, yes),
            Some(Action::Backups { action }) => match action {
                None | Some(BackupsAction::List) => backups::backups(&places, backups::Action::List, false),
                Some(BackupsAction::Restore { name, yes }) => {
                    backups::backups(&places, backups::Action::Restore(name), yes)
                }
            },
            Some(Action::Settings { updates, animations }) => {
                settings::settings(&places, updates.as_deref(), animations.as_deref())
            }
            Some(Action::CheckUpdates { notify }) => {
                check::check_updates(&places, if notify { Mode::Notify } else { Mode::Print })
            }
            Some(Action::MissionCenter { action }) => {
                let (action, yes) = mission_action(action);
                mission::mission_center(&places, action, yes)
            }
            Some(Action::Status) => app::status(&places),
            Some(Action::License) => app::license(&places, VERSION.trim()),
            Some(Action::Uninstall { yes }) => app::uninstall(&places, yes),
        }
    });
    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            let _ = cliclack::outro_cancel(Style::new().red().apply_to(format!("{error:#}")));
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{Action, BackupsAction, MissionAction, shows_banner};
    use aurora::tuning::Switch;

    #[test]
    fn the_menu_and_the_install_open_with_the_wordmark() {
        assert!(shows_banner(None, None));
        assert!(shows_banner(Some(&Action::Rebuild), None));
    }

    #[test]
    fn a_run_from_get_sh_does_not_draw_the_wordmark_twice() {
        assert!(!shows_banner(None, Some("shown")));
    }

    #[test]
    fn one_line_answers_skip_the_wordmark() {
        assert!(!shows_banner(Some(&Action::Status), None));
        assert!(!shows_banner(Some(&Action::License), None));
        assert!(!shows_banner(Some(&Action::CheckUpdates { notify: true }), None), "the login timer prints nothing");
        assert!(!shows_banner(Some(&Action::Settings { updates: Some("weekly".into()), animations: None }), None));
        assert!(!shows_banner(Some(&Action::Settings { updates: None, animations: Some("fast".into()) }), None));
        assert!(
            shows_banner(Some(&Action::Settings { updates: None, animations: None }), None),
            "the settings menu is a menu"
        );
        assert!(!shows_banner(Some(&Action::Icons { theme: Some("breeze".into()) }), None));
        assert!(!shows_banner(Some(&Action::Performance { switch: Some(Switch::On), yes: false }), None));
        assert!(!shows_banner(Some(&Action::Backups { action: None }), None), "the list is a few lines");
        assert!(shows_banner(
            Some(&Action::Backups { action: Some(BackupsAction::Restore { name: None, yes: false }) }),
            None
        ));
    }

    #[test]
    fn mission_center_settings_from_a_script_skip_the_wordmark_and_the_menu_keeps_it() {
        let settings = |set: &[&str], reset: &[&str]| Action::MissionCenter {
            action: Some(MissionAction::Settings {
                set: set.iter().map(ToString::to_string).collect(),
                reset: reset.iter().map(ToString::to_string).collect(),
            }),
        };
        assert!(!shows_banner(Some(&settings(&["app-update-interval=10"], &[])), None));
        assert!(!shows_banner(Some(&settings(&[], &["app-update-interval"])), None));
        assert!(shows_banner(Some(&settings(&[], &[])), None), "the settings menu is a menu");
    }
}
