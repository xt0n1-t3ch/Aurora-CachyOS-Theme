// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora settings`: what the user can change after the install, so the install itself asks one question.

use anyhow::{Result, bail};
use console::Style;

use crate::app::{self, Places};
use crate::logo::Colors;
use crate::updates::{Frequency, Settings};

/// A short word on each choice of how often to look for updates.
fn hint(frequency: Frequency) -> &'static str {
    match frequency {
        Frequency::Daily => "a notification the day a new version is out",
        Frequency::Weekly => "at most one look a week",
        Frequency::Never => "only when you run `aurora update`",
    }
}

/// `aurora settings`, with `updates` given on the command line (`daily`, `weekly` or `never`) or asked.
///
/// # Errors
/// When the value is unknown, the user cancels, or the settings cannot be written.
pub fn settings(places: &Places, updates: Option<&str>) -> Result<()> {
    let path = places.settings_file();
    let mut settings = Settings::load(&path);
    if let Some(value) = updates {
        let Some(frequency) = Frequency::from_id(value) else {
            bail!("--updates takes daily, weekly or never, not {value}");
        };
        settings.updates = frequency;
        settings.save(&path)?;
        println!("Aurora looks for updates: {}", frequency.to_string().to_lowercase());
        return Ok(());
    }
    let what: &str = cliclack::select("What would you like to change?")
        .item("updates", "Update notices", format!("now: {}", settings.updates.to_string().to_lowercase()))
        .item("logo", "Menu bar logo", "CachyOS, Arch Linux, Apple or Kali Linux, in black and white or in color")
        .interact()?;
    if what == "logo" {
        return app::logo(None, Colors::Off);
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
