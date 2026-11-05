// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora performance`: turns the measured system tuning on or off and shows which of its settings are in place.
//! The root work is `installer/root-steps.sh` (`tune` and `untune`), run once through sudo.

use std::fmt;
use std::path::Path;

use anyhow::Result;
use console::Style;

use crate::app::{self, Places};
use crate::run;

/// One setting of the tuning: the file that holds it, what it is, and what it measured (docs/performance.md).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Setting {
    pub file: &'static str,
    pub name: &'static str,
    pub measured: &'static str,
}

/// The four settings, in the order `root-steps.sh tune` writes them; tests/contract.rs keeps the files equal.
pub const SETTINGS: [Setting; 4] = [
    Setting {
        file: "/etc/scx_loader.toml",
        name: "scx_lavd CPU scheduler",
        measured: "under load the emoji picker opens in 666 ms instead of 1,099",
    },
    Setting {
        file: "/etc/tmpfiles.d/thp.conf",
        name: "huge pages put together in the background",
        measured: "apps stop waiting while memory is compacted (279,000 waits before)",
    },
    Setting {
        file: "/etc/sysctl.d/99-aurora-memory.conf",
        name: "memory kept free earlier",
        measured: "direct-reclaim stalls went from about 1,560 every 15 minutes to none",
    },
    Setting {
        file: "/etc/systemd/zram-generator.conf.d/90-aurora-zram.conf",
        name: "lz4 compression for zram",
        measured: "swapped pages load at 3.7 GB/s instead of 1.3, from the next boot",
    },
];

/// Turning the tuning on or off.
#[derive(Debug, Clone, Copy, PartialEq, Eq, clap::ValueEnum)]
pub enum Switch {
    On,
    Off,
}

/// How much of the tuning is in place.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum State {
    On,
    Off,
    /// Some of the files are there, this many; turning it on or off makes it whole again.
    Partly(usize),
}

impl fmt::Display for State {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            State::On => f.write_str("on"),
            State::Off => f.write_str("off"),
            State::Partly(count) => write!(f, "partly on ({count} of {} settings)", SETTINGS.len()),
        }
    }
}

/// The state from which of the settings' files exist; `exists` answers for one file.
#[must_use]
pub fn state(exists: impl Fn(&Path) -> bool) -> State {
    match SETTINGS.iter().filter(|setting| exists(Path::new(setting.file))).count() {
        0 => State::Off,
        count if count == SETTINGS.len() => State::On,
        count => State::Partly(count),
    }
}

/// The state of this machine.
#[must_use]
pub fn current() -> State {
    state(Path::exists)
}

/// The arguments of `root-steps.sh` that make the tuning `switch`, with the copy of Aurora at `root`.
#[must_use]
pub fn root_steps(switch: Switch, root: &Path) -> Vec<String> {
    match switch {
        Switch::On => vec!["tune".into(), root.join("performance/system").to_string_lossy().into_owned()],
        Switch::Off => vec!["untune".into()],
    }
}

/// The settings as a note: each one's name and what it measured.
fn settings_note() -> String {
    SETTINGS.iter().map(|setting| format!("{}: {}", setting.name, setting.measured)).collect::<Vec<_>>().join("\n")
}

/// `aurora performance`: with `choice`, turns the tuning on or off; without it, shows the settings and asks.
///
/// # Errors
/// When the user cancels or a root step fails.
pub fn performance(places: &Places, choice: Option<Switch>, yes: bool) -> Result<()> {
    let now = current();
    let switch = match choice {
        Some(switch) => switch,
        None if !yes && console::Term::stdout().is_term() => {
            cliclack::intro(Style::new().bold().apply_to("Performance tuning"))?;
            cliclack::note(format!("Now {now}"), settings_note())?;
            cliclack::select("What would you like?")
                .item(Switch::On, "Turn it on", "the four measured settings above")
                .item(Switch::Off, "Turn it off", "CachyOS's own scheduler and memory settings")
                .initial_value(if now == State::On { Switch::Off } else { Switch::On })
                .interact()?
        }
        None => {
            println!("Performance tuning: {now}");
            return Ok(());
        }
    };
    if (switch == Switch::On && now == State::On) || (switch == Switch::Off && now == State::Off) {
        println!("Performance tuning is already {now}.");
        return Ok(());
    }
    let source = app::source(places)?;
    let askpass = app::askpass(places);
    app::sudo_ready(askpass.as_deref())?;
    let env: Vec<(&str, &Path)> = askpass.iter().map(|dialog| ("SUDO_ASKPASS", dialog.as_path())).collect();
    let steps = root_steps(switch, &source.root);
    let mut args: Vec<&str> = Vec::new();
    if askpass.is_some() {
        args.push("-A");
    }
    let script = source.root.join("installer/root-steps.sh");
    let script = script.to_string_lossy();
    args.extend(["bash", &script]);
    args.extend(steps.iter().map(String::as_str));
    let title = if switch == Switch::On { "Turning the tuning on" } else { "Turning the tuning off" };
    run::script(Path::new("sudo"), &args, &env, &places.state, title)?;
    let done = match switch {
        Switch::On => "Performance tuning is on. zram's faster compression starts at the next boot.",
        Switch::Off => "Performance tuning is off: CachyOS's own scheduler and memory settings are back.",
    };
    println!("{done}");
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::path::Path;

    use super::{SETTINGS, State, Switch, root_steps, state};

    #[test]
    fn the_tuning_is_on_only_when_every_setting_is_in_place() {
        assert_eq!(state(|_| true), State::On);
        assert_eq!(state(|_| false), State::Off);
        assert_eq!(state(|file| file == Path::new(SETTINGS[2].file)), State::Partly(1));
    }

    #[test]
    fn a_partial_tuning_says_how_much_is_there() {
        assert_eq!(State::Partly(3).to_string(), "partly on (3 of 4 settings)");
    }

    #[test]
    fn turning_it_on_installs_the_copy_s_files_and_off_takes_them_away() {
        assert_eq!(
            root_steps(Switch::On, Path::new("/usr/share/aurora")),
            ["tune", "/usr/share/aurora/performance/system"]
        );
        assert_eq!(root_steps(Switch::Off, Path::new("/usr/share/aurora")), ["untune"]);
    }
}
