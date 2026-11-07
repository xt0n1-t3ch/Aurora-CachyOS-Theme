// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora settings`: what the user can change after the install, so the install itself asks one question.

use anyhow::{Result, bail};
use console::Style;

use crate::app::{self, Places};
use crate::logo::Colors;
use std::process::Command;
use std::thread;
use std::time::Duration;

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
fn apply_speed(speed: Speed) -> Result<()> {
    let status = Command::new("kwriteconfig6").args(speed_args(speed)).status()?;
    if !status.success() {
        bail!("kwriteconfig6 could not set the animation speed");
    }
    // Each KWin effect works out its durations when it loads and keeps them, and KWin takes the new speed on its own
    // timer (Workspace::reconfigure, 200 ms): the sliding popups effect reloads until it reports SlideInTime x speed,
    // then every effect reloads.
    let slide_in =
        capture("kreadconfig6", &["--file", "kwinrc", "--group", "Effect-slidingpopups", "--key", "SlideInTime"])
            .trim()
            .parse()
            .unwrap_or(DEFAULT_SLIDE_IN);
    let want = expected_slide_in(slide_in, speed.factor());
    for _ in 0..30 {
        capture("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.reconfigureEffect", "slidingpopups"]);
        if slide_in_duration(&capture("qdbus6", &["org.kde.KWin", "/KWin", "supportInformation"])) == Some(want) {
            break;
        }
        thread::sleep(Duration::from_millis(100));
    }
    for effect in effects(&capture("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.loadedEffects"])) {
        capture("qdbus6", &["org.kde.KWin", "/Effects", "org.kde.kwin.Effects.reconfigureEffect", effect]);
    }
    Ok(())
}

/// The `kwriteconfig6` arguments that set the speed and tell the running desktop. The file goes by its name, not its
/// path: `KConfig` announces a change under the name it was opened with, and KWin, Plasma and the apps listen to
/// `/kdeglobals` (`KConfigWatcher`), so a full path reaches none of them.
fn speed_args(speed: Speed) -> [&'static str; 8] {
    ["--notify", "--file", "kdeglobals", "--group", "KDE", "--key", "AnimationDurationFactor", speed.factor()]
}

/// KWin's own slide-in time for popups when kwinrc names none (src/plugins/slidingpopups).
const DEFAULT_SLIDE_IN: u32 = 200;

/// A command's stdout, or nothing when it cannot run.
fn capture(program: &str, args: &[&str]) -> String {
    Command::new(program)
        .args(args)
        .output()
        .map(|out| String::from_utf8_lossy(&out.stdout).into_owned())
        .unwrap_or_default()
}

/// The slide-in time KWin reports once it runs at `factor` (`Effect::animationTime`: the time times the factor, at
/// least 1 ms).
fn expected_slide_in(slide_in: u32, factor: &str) -> u32 {
    let factor: f64 = factor.parse().unwrap_or(1.0);
    #[expect(clippy::cast_possible_truncation, clippy::cast_sign_loss, reason = "a positive time of a few hundred ms")]
    let millis = (f64::from(slide_in) * factor) as u32;
    millis.max(1)
}

/// The sliding popups effect's slide-in time in KWin's support information.
fn slide_in_duration(support: &str) -> Option<u32> {
    support
        .lines()
        .find_map(|line| line.trim().strip_prefix("slideInDuration:"))
        .and_then(|value| value.trim().parse().ok())
}

/// The effect names in `qdbus6 ... loadedEffects` output, one per line.
fn effects(listing: &str) -> Vec<&str> {
    listing.split_whitespace().collect()
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
        apply_speed(speed)?;
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
        .item("icons", "Icons", "Aurora's, or any other installed icon theme")
        .item("performance", "Performance tuning", format!("now: {}", crate::tuning::current()))
        .item("mission-center", "Mission Center", format!("now: {}", crate::mission::Installed::now()))
        .interact()?;
    match what {
        "logo" => return app::logo(None, Colors::Off),
        "icons" => return crate::icons::icons(places, None),
        "performance" => return crate::tuning::performance(places, None, false),
        "mission-center" => return crate::mission::mission_center(places, None, false),
        _ => {}
    }
    if what == "animations" {
        cliclack::intro(Style::new().bold().apply_to("Animations"))?;
        settings.animations = cliclack::select("How should the desktop animate?")
            .items(&Speed::ALL.map(|speed| (speed, speed.to_string(), speed_hint(speed))))
            .initial_value(settings.animations)
            .interact()?;
        settings.save(&path)?;
        apply_speed(settings.animations)?;
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

#[cfg(test)]
mod tests {
    use super::{Speed, effects, expected_slide_in, slide_in_duration, speed_args};

    // Recorded from `qdbus6 org.kde.KWin /KWin supportInformation`, KWin 6.8.2, 2026-11-05 (Effect Settings part).
    const SUPPORT: &str = "Effect Settings:\n----------------\nslidingpopups\n-------------\nslideInDuration: 187\n\
                           slideOutDuration: 187\n";

    #[test]
    fn the_speed_is_announced_under_the_name_every_app_listens_to() {
        let args = speed_args(Speed::Smooth);
        assert_eq!(args[0], "--notify");
        assert_eq!(args[2], "kdeglobals", "a full path is announced under the path, and nobody listens there");
        assert_eq!(args[6..], ["AnimationDurationFactor", "1"]);
    }

    #[test]
    fn the_witness_duration_is_read_from_kwin_s_support_information() {
        assert_eq!(slide_in_duration(SUPPORT), Some(187));
        assert_eq!(slide_in_duration("Loaded Effects:\nblur\n"), None);
    }

    #[test]
    fn kwin_reports_the_slide_in_time_times_the_speed_and_never_zero() {
        assert_eq!(expected_slide_in(250, "1"), 250);
        assert_eq!(expected_slide_in(250, "0.75"), 187);
        assert_eq!(expected_slide_in(250, "0"), 1);
    }

    #[test]
    fn every_loaded_effect_is_reloaded() {
        assert_eq!(effects("scale\nmagiclamp\nslidingpopups\n"), ["scale", "magiclamp", "slidingpopups"]);
        assert!(effects("").is_empty(), "no KWin, nothing to reload");
    }
}
