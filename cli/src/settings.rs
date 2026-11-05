// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora settings`: what the user can change after the install, so the install itself asks one question.

use anyhow::{Result, bail};
use console::Style;

use crate::app::{self, Places};
use crate::logo::Colors;
use std::process::Command;

use crate::updates::{Frequency, Settings, Speed};

/// A short word on each choice of how often to look for updates.
fn hint(frequency: Frequency) -> &'static str {
    match frequency {
        Frequency::Daily => "a notification the day a new version is out",
        Frequency::Weekly => "at most one look a week",
        Frequency::Never => "only when you run `aurora update`",
    }
}

/// A short word on each animation speed.
fn speed_hint(speed: Speed) -> &'static str {
    match speed {
        Speed::Smooth => "every window, menu and panel glides into place",
        Speed::Fast => "half Plasma's time, still animated",
        Speed::Instant => "no animations at all",
    }
}

/// Applies `speed` to the running desktop: Plasma, KWin and every KDE app read kdeglobals' `AnimationDurationFactor`,
/// and `--notify` tells them at once.
fn apply_speed(places: &Places, speed: Speed) -> Result<()> {
    let kdeglobals = places.config.join("kdeglobals");
    let status = Command::new("kwriteconfig6")
        .arg("--notify")
        .arg("--file")
        .arg(&kdeglobals)
        .args(["--group", "KDE", "--key", "AnimationDurationFactor", speed.factor()])
        .status()?;
    if !status.success() {
        bail!("kwriteconfig6 could not set the animation speed");
    }
    Ok(())
}

/// `aurora settings`, with `updates` (`daily`, `weekly` or `never`) and `animations` (`smooth`, `fast` or `instant`)
/// given on the command line, or asked.
///
/// # Errors
/// When a value is unknown, the user cancels, or the settings cannot be written.
pub fn settings(places: &Places, updates: Option<&str>, animations: Option<&str>) -> Result<()> {
    let path = places.settings_file();
    let mut settings = Settings::load(&path);
    if let Some(value) = animations {
        let Some(speed) = Speed::from_id(value) else {
            bail!("--animations takes smooth, fast or instant, not {value}");
        };
        settings.animations = speed;
        settings.save(&path)?;
        apply_speed(places, speed)?;
        println!("Animations: {}", speed.to_string().to_lowercase());
    }
    if let Some(value) = updates {
        let Some(frequency) = Frequency::from_id(value) else {
            bail!("--updates takes daily, weekly or never, not {value}");
        };
        settings.updates = frequency;
        settings.save(&path)?;
        println!("Aurora looks for updates: {}", frequency.to_string().to_lowercase());
    }
    if updates.is_some() || animations.is_some() {
        return Ok(());
    }
    let what: &str = cliclack::select("What would you like to change?")
        .item("updates", "Update notices", format!("now: {}", settings.updates.to_string().to_lowercase()))
        .item("animations", "Animations", format!("now: {}", settings.animations.to_string().to_lowercase()))
        .item("logo", "Menu bar logo", "CachyOS, Arch Linux, Apple or Kali Linux, in black and white or in color")
        .interact()?;
    if what == "logo" {
        return app::logo(None, Colors::Off);
    }
    if what == "animations" {
        cliclack::intro(Style::new().bold().apply_to("Animations"))?;
        settings.animations = cliclack::select("How should the desktop animate?")
            .items(&Speed::ALL.map(|speed| (speed, speed.to_string(), speed_hint(speed))))
            .initial_value(settings.animations)
            .interact()?;
        settings.save(&path)?;
        apply_speed(places, settings.animations)?;
        cliclack::outro("Saved and applied. Change it any time with `aurora settings`.")?;
        return Ok(());
    }
    cliclack::intro(Style::new().bold().apply_to("Update notices"))?;
    settings.updates = cliclack::select("How often should Aurora look for a newer version?")
        .items(&Frequency::ALL.map(|frequency| (frequency, frequency.to_string(), hint(frequency))))
        .initial_value(settings.updates)
        .interact()?;
    settings.save(&path)?;
    cliclack::outro(format!("Saved. Change it any time with `aurora settings`. ({})", path.display()))?;
    Ok(())
}
