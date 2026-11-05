// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora`: installs, updates and removes Aurora. Run it without a command for a guided menu.

use std::process::ExitCode;

use aurora::app::{self, InstallChoices, Places};
use aurora::logo::{Colors, Logo};
use aurora::ui;
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

fn colors(on: bool) -> Colors {
    if on { Colors::On } else { Colors::Off }
}

/// The menu shown when aurora runs without a command.
fn menu(places: &Places) -> anyhow::Result<()> {
    if !places.installed() {
        return app::install(places, InstallChoices::default());
    }
    let action: &str = cliclack::select("What would you like to do?")
        .item("update", "Update", "get the newest version and install it")
        .item("logo", "Change the menu bar logo", "CachyOS, Arch Linux, Apple or Kali Linux")
        .item("rebuild", "Rebuild after a system update", "when Plasma, KWin or Qt changed")
        .item("status", "Status", "what is installed and from where")
        .item("uninstall", "Remove Aurora", "put your desktop back as it was")
        .item("license", "License and credits", "who made Aurora and the terms it comes with")
        .interact()?;
    match action {
        "update" => app::update(places, false),
        "logo" => app::logo(None, Colors::Off),
        "rebuild" => app::rebuild(places),
        "status" => app::status(places),
        "license" => app::license(places, VERSION.trim()),
        _ => app::uninstall(places, false),
    }
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    cliclack::set_theme(ui::AuroraTheme);
    let result = Places::from_env().and_then(|places| {
        // A one-line answer (status, or a logo named on the command line) skips the wordmark.
        let quiet = matches!(cli.command, Some(Action::Logo { logo: Some(_), .. } | Action::Status | Action::License));
        if !quiet {
            println!("{}", ui::banner(VERSION.trim()));
        }
        match cli.command {
            None => menu(&places),
            Some(Action::Install { logo, colors: on, performance, yes }) => {
                app::install(&places, InstallChoices { logo, colors: colors(on), performance, yes })
            }
            Some(Action::Update { yes }) => app::update(&places, yes),
            Some(Action::Rebuild) => app::rebuild(&places),
            Some(Action::Logo { logo, colors: on }) => app::logo(logo, colors(on)),
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
