// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora update`: gets the newest Aurora the way this copy came (git, the package or a release archive) and installs
//! it. A package from Aurora's repository comes the way its build allows ([`plan`]): with the system upgrade, on its
//! own, or not yet.

use std::process::Command;

use anyhow::{Context, Result, bail};
use console::Style;

use crate::app::{InstallChoices, Places, current_logo, install, interactive, source, tuned};
use crate::plan::{self, Install, Plan};
use crate::repo;
use crate::run;
use crate::source::{Kind, Source};
use crate::system;
use crate::updates;

/// The command that updates an aurora-theme built from the AUR, which no repository pacman uses has: the AUR helper
/// when there is one (paru first), else pacman through sudo. Either upgrades the whole system with it, so no partial
/// upgrade happens. `on_path` tells whether a program is installed; with `yes` the command asks nothing.
fn package_update(on_path: impl Fn(&str) -> bool, yes: bool) -> Vec<&'static str> {
    let quiet: &[&str] = if yes { &["--noconfirm"] } else { &[] };
    let upgrade = ["-Syu", "--needed"].iter().chain(quiet).chain(&["aurora-theme"]).copied();
    match ["paru", "yay"].into_iter().find(|helper| on_path(helper)) {
        Some(helper) => [helper].into_iter().chain(upgrade).collect(),
        None => ["sudo", "pacman"].into_iter().chain(upgrade).collect(),
    }
}

/// The version `aurora update` compares before and after, and the menu's notice with what the last look found: for the
/// package its full release from pacman (a new package release, 1.0.0-1 to 1.0.0-3, keeps the VERSION file),
/// otherwise the copy's VERSION.
#[must_use]
pub fn release_of(source: &Source) -> String {
    if source.kind == Kind::Package
        && let Some(release) =
            run::capture("pacman", &["-Q", updates::PACKAGE]).ok().and_then(|out| updates::installed_release(&out))
    {
        return release;
    }
    source.version()
}

/// Whether `aurora update` goes on to install what it brought.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Next {
    Install,
    Stop,
}

/// Runs one update command in the terminal, where pacman shows what it will change and asks first.
fn upgrade(command: &[&str]) -> Result<()> {
    let shown = command.join(" ");
    let status =
        Command::new(command[0]).args(&command[1..]).status().with_context(|| format!("starting `{shown}`"))?;
    if !status.success() {
        bail!("`{shown}` could not update aurora-theme");
    }
    Ok(())
}

/// Brings a newer aurora-theme from the repositories pacman uses the way its build allows (see [`plan`]): with the
/// whole system upgrade, on its own, or not yet. It says which and why; nothing changes when no newer one is out.
fn update_package(places: &Places, yes: bool) -> Result<Next> {
    let spinner = cliclack::spinner();
    spinner.start("Looking for a newer Aurora");
    let Some(found) = plan::look(&places.databases(), &places.cache.join("aurora/update"))? else {
        spinner.stop("No newer Aurora is out");
        return Ok(Next::Install);
    };
    let version = updates::shown(&found.version);
    spinner.stop(format!("Aurora {version} is out, built for {}", system::describe(&found.built)));
    let differs = |other: &system::Versions| system::describe(&system::differing(&found.built, other));
    let (title, install) = match found.plan() {
        Plan::Install(install @ Install::WithSystem) => {
            cliclack::log::info("That is what this PC has after a system update, so both update together")?;
            ("Updating the system with Aurora", install)
        }
        Plan::Install(install @ Install::Alone) => {
            cliclack::log::info(format!(
                "A system update would bring {}, which Aurora {version} is not built for, so that update waits for \
                 Aurora's next build",
                differs(&found.upgraded)
            ))?;
            ("Updating Aurora on its own", install)
        }
        Plan::Wait => {
            cliclack::log::warning(format!(
                "This PC has {} and a system update would bring {}: Aurora {version} runs on neither, so it waits \
                 for a build for your system. To update the system anyway, run `sudo pacman -Syu`, then `aurora \
                 rebuild` (about 3 minutes).",
                differs(&found.installed),
                differs(&found.upgraded)
            ))?;
            cliclack::outro("Nothing was changed")?;
            return Ok(Next::Stop);
        }
    };
    let command = install.command(&found.url, yes);
    cliclack::log::step(format!("{title}: {}", command.join(" ")))?;
    upgrade(&command.iter().map(String::as_str).collect::<Vec<_>>())?;
    // pacman can end without error and without the new version (a mirror behind, a package held back): say so instead
    // of installing the old one again.
    let now = run::capture("pacman", &["-Q", updates::PACKAGE]).ok().and_then(|out| updates::installed_release(&out));
    if now.as_deref() != Some(found.version.as_str()) {
        bail!(
            "pacman finished, but aurora-theme is {} instead of {}. Run `sudo pacman -Syu`, then `aurora update` again",
            now.as_deref().unwrap_or("not installed"),
            found.version
        );
    }
    Ok(Next::Install)
}

/// `aurora update`: gets the newest version the way this copy came, then installs it.
///
/// # Errors
/// When the update cannot be fetched or installed.
pub fn update(places: &Places, yes: bool) -> Result<()> {
    let source = source(places)?;
    let before = release_of(&source);
    cliclack::intro(Style::new().bold().apply_to("Update Aurora"))?;
    let newest = match source.kind {
        Kind::Checkout => {
            let spinner = cliclack::spinner();
            spinner.start("Pulling the newest commits");
            let root = source.root.to_string_lossy().into_owned();
            run::capture("git", &["-C", &root, "pull", "--ff-only"])?;
            spinner.stop("Pulled the newest commits");
            source.clone()
        }
        Kind::Package => {
            // Aurora's repository first: without its section pacman counts the package as one from the AUR, and without
            // its key pacman refuses every new build.
            let ours = repo::ensure(&places.cache.join("aurora/update"), yes)?;
            // pacman -Qm lists a package that no repository has: one built from the AUR, which its helper updates.
            if !ours && run::capture("pacman", &["-Qmq", updates::PACKAGE]).is_ok() {
                let command = package_update(run::on_path, yes);
                cliclack::log::step(format!(
                    "Updating the system and the aurora-theme package: {}",
                    command.join(" ")
                ))?;
                upgrade(&command)?;
            } else if update_package(places, yes)? == Next::Stop {
                return Ok(());
            }
            source.clone()
        }
        Kind::Archive => crate::download::newest(places, &before)?.unwrap_or_else(|| source.clone()),
    };
    let after = release_of(&newest);
    if after == before
        && !yes
        && interactive(false)
        && !cliclack::confirm(format!("Aurora {} is the newest. Install it again anyway?", updates::shown(&after)))
            .initial_value(false)
            .interact()?
    {
        cliclack::outro("Aurora is up to date.")?;
        return Ok(());
    }
    cliclack::log::step(format!("Installing Aurora {}", updates::shown(&after)))?;
    install(
        places,
        InstallChoices {
            yes: true,
            logo: current_logo().map(|(logo, _)| logo),
            colors: current_logo().map(|(_, colors)| colors).unwrap_or_default(),
            performance: tuned(),
        },
    )?;
    crate::check::updated(places)
}

#[cfg(test)]
mod tests {
    use super::package_update;

    // An aurora-theme built from the AUR is in no repository: the helper rebuilds it and upgrades the whole system
    // with it, since Arch does not support partial upgrades (pacman -S without -y may also miss the update).
    #[test]
    fn the_package_updates_with_the_whole_system_through_an_aur_helper_when_there_is_one() {
        assert_eq!(package_update(|program| program == "yay", false), ["yay", "-Syu", "--needed", "aurora-theme"]);
        assert_eq!(package_update(|_| true, false), ["paru", "-Syu", "--needed", "aurora-theme"]);
    }

    #[test]
    fn without_an_aur_helper_pacman_updates_the_package_with_the_whole_system() {
        assert_eq!(package_update(|_| false, false), ["sudo", "pacman", "-Syu", "--needed", "aurora-theme"]);
    }

    // Without a terminal nobody answers pacman's question, and the update would wait for ever.
    #[test]
    fn asked_for_no_questions_the_helper_and_pacman_ask_none() {
        assert_eq!(package_update(|_| true, true), ["paru", "-Syu", "--needed", "--noconfirm", "aurora-theme"]);
        assert_eq!(
            package_update(|_| false, true),
            ["sudo", "pacman", "-Syu", "--needed", "--noconfirm", "aurora-theme"]
        );
    }
}
