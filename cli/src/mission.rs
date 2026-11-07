// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora mission-center`: Aurora's build of Mission Center (`components/mission-center`) in place of the
//! distribution's, its settings, and a way back. Before every change it saves a snapshot of Mission Center's settings
//! and of which build is installed, and it keeps Aurora's package of every build it installs, so `rollback` can put
//! both back. snap-pac, where it is installed, also snapshots the system at every pacman transaction.

use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

mod preferences;
mod snapshots;

use anyhow::{Context, Result, bail};
use console::{Style, Term};

use crate::app::{self, Places};
use crate::backups;
use crate::run;
pub use preferences::SCHEMA;
use preferences::{OFFERS, checked, current, reset, set, shown, write};
pub use snapshots::{Installed, Snapshot, package_file, recipe_version, restore_command, snapshots_in};

/// The package of Aurora's build (`components/mission-center/PKGBUILD`; tests/contract.rs keeps the names equal).
pub const PACKAGE: &str = "mission-center-improved";
/// The distribution's package, which Aurora's build replaces.
pub const STOCK: &str = "mission-center";
/// Where dconf keeps Mission Center's settings ([`SCHEMA`]).
const DCONF_PATH: &str = "/io/missioncenter/MissionCenter/";
/// The recipe's files, in `components/mission-center` of the copy Aurora runs from: the PKGBUILD and what it reads.
pub const RECIPE: [&str; 7] = [
    "PKGBUILD",
    "mission-center-improved.install",
    "mission-center-improved.patch",
    "magpie-improved.patch",
    "app-rummage-improved.patch",
    "graph-widget-improved.patch",
    "aurora-mission-center-nethogs.hook",
];

/// What `aurora mission-center` was asked to do.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Action {
    /// Install Aurora's build.
    On,
    /// Go back to the distribution's build.
    Off,
    /// Put back a snapshot: the named one, or the newest.
    Rollback(Option<String>),
    /// List the snapshots.
    Snapshots,
    /// Change settings: `KEY=VALUE` pairs to set and keys to reset; with neither, the menu (or the list off a terminal).
    Settings { set: Vec<String>, reset: Vec<String> },
}

fn folder(places: &Places) -> PathBuf {
    places.state.join("aurora/mission-center")
}

/// Aurora's packages of Mission Center that this account built or installed, kept for a rollback.
fn store(places: &Places) -> PathBuf {
    folder(places).join("packages")
}

/// Aurora's package of `version`: kept by Aurora, or still in pacman's cache.
fn kept_package(places: &Places, version: &str) -> Option<PathBuf> {
    let name = package_file(version);
    [store(places).join(&name), Path::new("/var/cache/pacman/pkg").join(&name)].into_iter().find(|path| path.is_file())
}

/// Saves what is installed and Mission Center's settings, and returns the snapshot's name.
fn save_snapshot(places: &Places) -> Result<String> {
    let name = run::capture("date", &["+%Y%m%d-%H%M%S"])?.trim().to_string();
    let snapshot = folder(places).join("snapshots").join(&name);
    fs::create_dir_all(&snapshot).with_context(|| format!("creating {}", snapshot.display()))?;
    fs::write(snapshot.join("installed"), Installed::now().render())?;
    fs::write(snapshot.join("settings.ini"), run::capture("dconf", &["dump", DCONF_PATH])?)?;
    Ok(name)
}

/// Loads a snapshot's settings in place of the current ones: keys it does not name go back to their defaults.
fn load_settings(file: &Path) -> Result<()> {
    run::capture("dconf", &["reset", "-f", DCONF_PATH])?;
    let text = fs::read(file).with_context(|| format!("reading {}", file.display()))?;
    let mut load =
        Command::new("dconf").args(["load", DCONF_PATH]).stdin(Stdio::piped()).spawn().context("starting dconf")?;
    load.stdin.take().context("dconf's input")?.write_all(&text)?;
    if !load.wait()?.success() {
        bail!("dconf could not load {}", file.display());
    }
    Ok(())
}

/// Runs one pacman or makepkg command in the terminal, where it shows what it changes and asks first.
fn run_here(words: &[String], what: &str) -> Result<()> {
    cliclack::log::step(format!("{what}: {}", words.join(" ")))?;
    let status =
        Command::new(&words[0]).args(&words[1..]).status().with_context(|| format!("starting {}", words[0]))?;
    if !status.success() {
        bail!("`{}` did not finish", words.join(" "));
    }
    Ok(())
}

/// Builds Aurora's package from the recipe in `recipe` with makepkg (it asks for the tools it needs through sudo and
/// removes them after) and returns the package, kept in the store.
fn build_here(places: &Places, recipe: &Path, version: &str) -> Result<PathBuf> {
    let work = places.cache.join("aurora/mission-center");
    let copy = work.join("recipe");
    fs::create_dir_all(&copy).with_context(|| format!("creating {}", copy.display()))?;
    for file in RECIPE {
        fs::copy(recipe.join(file), copy.join(file)).with_context(|| format!("copying {file}"))?;
    }
    let store = store(places);
    fs::create_dir_all(&store)?;
    cliclack::log::step("Building Aurora's Mission Center here, about ten minutes: makepkg --syncdeps --rmdeps")?;
    let status = Command::new("makepkg")
        .args(["--syncdeps", "--rmdeps", "--cleanbuild", "--noconfirm"])
        .current_dir(&copy)
        .env("PKGDEST", &store)
        .env("BUILDDIR", work.join("build"))
        .env("SRCDEST", work.join("sources"))
        .status()
        .context("starting makepkg")?;
    let package = store.join(package_file(version));
    if !status.success() || !package.is_file() {
        bail!("makepkg did not build {}", package.display());
    }
    Ok(package)
}

/// Installs Aurora's build: from Aurora's repository when pacman knows the package there, else built here.
fn turn_on(places: &Places, yes: bool) -> Result<()> {
    let source = app::source(places)?;
    let recipe = source.root.join("components/mission-center");
    let wanted = fs::read_to_string(recipe.join("PKGBUILD"))
        .ok()
        .and_then(|text| recipe_version(&text))
        .context("the copy of Aurora has no Mission Center recipe")?;
    let now = Installed::now();
    if now == Installed::Aurora(wanted.clone())
        && !confirm(yes, "Aurora's Mission Center is installed. Install it again?")?
    {
        return Ok(());
    }
    let snapshot = save_snapshot(places)?;
    cliclack::log::info(format!("Saved Mission Center as it is ({now}) as snapshot {snapshot}"))?;
    if run::capture("pacman", &["-Si", PACKAGE]).is_ok() {
        run_here(&words(&["sudo", "pacman", "-S", PACKAGE]), "Installing it from Aurora's repository")?;
        if let Some(cached) = kept_package(places, &wanted).filter(|path| !path.starts_with(store(places))) {
            fs::create_dir_all(store(places))?;
            fs::copy(&cached, store(places).join(package_file(&wanted)))?;
        }
    } else {
        let package = match kept_package(places, &wanted) {
            Some(package) => package,
            None => build_here(places, &recipe, &wanted)?,
        };
        run_here(&words(&["sudo", "pacman", "-U", &package.to_string_lossy()]), "Installing it")?;
    }
    cliclack::outro(format!(
        "Aurora's Mission Center {wanted} is installed. `aurora mission-center rollback` puts back {now}."
    ))?;
    Ok(())
}

fn words(list: &[&str]) -> Vec<String> {
    list.iter().map(|word| (*word).to_string()).collect()
}

fn confirm(yes: bool, question: &str) -> Result<bool> {
    if yes || !Term::stdout().is_term() {
        return Ok(yes);
    }
    Ok(cliclack::confirm(question).initial_value(false).interact()?)
}

/// Puts back a snapshot: the build it had, then its settings. A snapshot of the state before comes first, so the
/// rollback can be rolled back.
fn rollback(places: &Places, name: Option<&str>, yes: bool) -> Result<()> {
    let snapshots = snapshots_in(&folder(places).join("snapshots"));
    let chosen = match name {
        Some(name) => snapshots.iter().find(|snapshot| snapshot.name == name),
        None => snapshots.first(),
    }
    .with_context(|| format!("no snapshot {}: `aurora mission-center snapshots` lists them", name.unwrap_or("yet")))?;
    let when = backups::taken_at(&chosen.name).unwrap_or_else(|| chosen.name.clone());
    if !confirm(yes, &format!("Put back Mission Center as it was on {when} ({})?", chosen.installed))? {
        cliclack::outro_cancel("Nothing was changed")?;
        return Ok(());
    }
    let now = Installed::now();
    let package = match &chosen.installed {
        Installed::Aurora(version) => kept_package(places, version),
        _ => None,
    };
    let command = restore_command(&chosen.installed, &now, package.as_deref())?;
    let before = save_snapshot(places)?;
    cliclack::log::info(format!("Saved Mission Center as it is ({now}) as snapshot {before}"))?;
    if let Some(command) = command {
        run_here(&command, "Putting the build back")?;
    }
    load_settings(&folder(places).join("snapshots").join(&chosen.name).join("settings.ini"))?;
    cliclack::outro(format!("Mission Center is as it was on {when}: {}", chosen.installed))?;
    Ok(())
}

fn print_snapshots(places: &Places) {
    let snapshots = snapshots_in(&folder(places).join("snapshots"));
    if snapshots.is_empty() {
        println!("No snapshots yet: every change by `aurora mission-center` saves one first.");
    }
    for snapshot in snapshots {
        let when = backups::taken_at(&snapshot.name).unwrap_or_default();
        println!("{}   {when}   {}", snapshot.name, snapshot.installed);
    }
}

/// `--set` and `--reset`: every value is checked first, so a wrong one changes nothing and saves no snapshot.
fn apply(places: &Places, set_pairs: &[String], resets: &[String]) -> Result<()> {
    let mut changes = Vec::new();
    for pair in set_pairs {
        let (key, value) = pair.split_once('=').with_context(|| format!("--set takes KEY=VALUE, not {pair}"))?;
        changes.push((key.trim(), checked(key.trim(), value)?));
    }
    for key in resets {
        preferences::range(key)?;
    }
    save_snapshot(places)?;
    for (key, value) in &changes {
        write(key, value)?;
        println!("{key} = {}", current(key));
    }
    for key in resets {
        reset(key)?;
        println!("{key} = {} (the default)", current(key));
    }
    Ok(())
}

/// `aurora mission-center settings`: `--set` and `--reset` from a script, else the menu, else the list.
fn settings(places: &Places, set_pairs: &[String], resets: &[String]) -> Result<()> {
    if !set_pairs.is_empty() || !resets.is_empty() {
        return apply(places, set_pairs, resets);
    }
    if !Term::stdout().is_term() {
        print!("{}", run::capture("gsettings", &["list-recursively", SCHEMA])?);
        return Ok(());
    }
    cliclack::intro(Style::new().bold().apply_to("Mission Center settings"))?;
    let snapshot = save_snapshot(places)?;
    cliclack::log::info(format!("Saved the settings as they are as snapshot {snapshot}"))?;
    loop {
        let mut menu = cliclack::select("What would you like to change?");
        for (index, offer) in OFFERS.iter().enumerate() {
            menu = menu.item(index, offer.name, format!("now: {}", shown(offer, &current(offer.key))));
        }
        let index = menu
            .item(OFFERS.len(), "Reset the sidebar's order", "back to Mission Center's own order")
            .item(OFFERS.len() + 1, "Reset the device colors", "back to Mission Center's own colors")
            .item(OFFERS.len() + 2, "Done", "")
            .interact()?;
        match index.checked_sub(OFFERS.len()) {
            Some(0) => reset("performance-sidebar-order")?,
            Some(1) => reset("performance-sidebar-device-colors")?,
            Some(_) => break,
            None => {
                let offer = &OFFERS[index];
                let value = cliclack::select(offer.name).items(&preferences::items(offer)).interact()?;
                set(offer.key, value)?;
            }
        }
    }
    cliclack::outro(
        "Mission Center takes the settings at once. `aurora mission-center rollback` puts the old ones back.",
    )?;
    Ok(())
}

/// `aurora mission-center`: with an action, does it; without one, shows what is installed and asks.
///
/// # Errors
/// When the user cancels, or pacman, makepkg, dconf or gsettings fail.
pub fn mission_center(places: &Places, action: Option<Action>, yes: bool) -> Result<()> {
    let action = match action {
        Some(action) => action,
        None if Term::stdout().is_term() => {
            cliclack::intro(Style::new().bold().apply_to("Mission Center"))?;
            let now = Installed::now();
            let newest = snapshots_in(&folder(places).join("snapshots")).into_iter().next();
            let rollback_hint = newest.map_or_else(
                || "no snapshot yet".to_string(),
                |snapshot| {
                    format!("to {}, {}", backups::taken_at(&snapshot.name).unwrap_or_default(), snapshot.installed)
                },
            );
            cliclack::note(
                format!("Now {now}"),
                "Aurora's build counts each app's GPU, memory and network exactly, \
                 keeps magpie lean, reads fans as the hardware does and lets you color each device.",
            )?;
            match cliclack::select("What would you like?")
                .item("on", "Use Aurora's build", "from Aurora's repository, or built here")
                .item("off", "Use the distribution's", "Mission Center as CachyOS ships it")
                .item("settings", "Settings", "refresh, charts, look, graphs, sidebar")
                .item("rollback", "Roll back", rollback_hint)
                .interact()?
            {
                "on" => Action::On,
                "off" => Action::Off,
                "settings" => Action::Settings { set: Vec::new(), reset: Vec::new() },
                _ => Action::Rollback(None),
            }
        }
        None => {
            println!("Mission Center: {}", Installed::now());
            return Ok(());
        }
    };
    match action {
        Action::On => turn_on(places, yes),
        Action::Off => {
            let now = Installed::now();
            if matches!(now, Installed::Stock(_)) {
                println!("The distribution's Mission Center is installed.");
                return Ok(());
            }
            let snapshot = save_snapshot(places)?;
            cliclack::log::info(format!("Saved Mission Center as it is ({now}) as snapshot {snapshot}"))?;
            run_here(&words(&["sudo", "pacman", "-S", STOCK]), "Installing the distribution's Mission Center")?;
            cliclack::outro("The distribution's Mission Center is installed; your settings stay.")?;
            Ok(())
        }
        Action::Rollback(name) => rollback(places, name.as_deref(), yes),
        Action::Snapshots => {
            print_snapshots(places);
            Ok(())
        }
        Action::Settings { set, reset } => settings(places, &set, &reset),
    }
}
