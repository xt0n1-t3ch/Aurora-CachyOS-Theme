// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The commands: each one asks what it needs, runs the theme's own scripts through [`run::script`], and ends with
//! what to do next.

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

use anyhow::{Context, Result, bail};
use console::{Style, Term};

use crate::legal;
use crate::logo::{Colors, Logo};
use crate::release;
use crate::run;
use crate::source::{Kind, Source};
use crate::system;

/// Where this account keeps things, from the XDG variables with their usual defaults.
#[derive(Debug, Clone)]
pub struct Places {
    pub home: PathBuf,
    pub config: PathBuf,
    pub state: PathBuf,
    pub cache: PathBuf,
}

impl Places {
    /// # Errors
    /// When `HOME` is not set.
    pub fn from_env() -> Result<Places> {
        let home = PathBuf::from(env::var_os("HOME").context("HOME is not set")?);
        let xdg = |name: &str, fallback: &str| env::var_os(name).map_or_else(|| home.join(fallback), PathBuf::from);
        Ok(Places {
            config: xdg("XDG_CONFIG_HOME", ".config"),
            state: xdg("XDG_STATE_HOME", ".local/state"),
            cache: xdg("XDG_CACHE_HOME", ".cache"),
            home,
        })
    }

    /// The user's settings, which `aurora settings` changes.
    #[must_use]
    pub fn settings_file(&self) -> PathBuf {
        self.config.join("aurora/aurora.conf")
    }

    /// What the last look for updates found.
    #[must_use]
    pub fn update_record(&self) -> PathBuf {
        self.state.join("aurora/updates")
    }

    /// `checkupdates`' own copy of pacman's databases, kept in the cache so a look downloads only what changed.
    #[must_use]
    pub fn databases(&self) -> PathBuf {
        self.cache.join("aurora/checkup-db")
    }

    #[must_use]
    pub fn installed(&self) -> bool {
        self.home.join(".local/share/aurora/source").exists()
    }
}

/// What `aurora install` was asked for on the command line.
#[derive(Debug, Clone, Copy, Default)]
pub struct InstallChoices {
    pub logo: Option<Logo>,
    pub colors: Colors,
    pub performance: bool,
    pub yes: bool,
}

/// The copy of Aurora to work with, or a message that says how to get one.
///
/// # Errors
/// When no copy is found.
pub fn source(places: &Places) -> Result<Source> {
    let exe = env::current_exe().unwrap_or_default();
    let explicit = env::var_os("AURORA_SOURCE").map(PathBuf::from);
    Source::find(explicit.as_deref(), &places.home, &exe).context(
        "Aurora's files were not found. Clone https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme or install the \
         aurora-theme package, then run aurora again",
    )
}

pub(crate) fn interactive(yes: bool) -> bool {
    !yes && Term::stdout().is_term()
}

/// The password dialog sudo uses when the scripts need root, so the prompt never fights the spinners: the one
/// already set, else KDE's (ksshaskpass); behind Aurora's wrapper in the state folder, which keeps the dialog's
/// warnings out of the terminal (see [`askpass`](crate::askpass)).
pub(crate) fn askpass(places: &Places) -> Option<PathBuf> {
    let program = env::var_os("SUDO_ASKPASS").map(PathBuf::from).or_else(|| {
        ["/usr/bin/ksshaskpass", "/usr/lib/ssh/ksshaskpass"].into_iter().map(PathBuf::from).find(|path| path.is_file())
    })?;
    Some(crate::askpass::wrapped(program, &places.state.join("aurora")))
}

/// Asks for the sudo password once in the terminal when no dialog can, before any spinner starts.
pub(crate) fn sudo_ready(askpass: Option<&Path>) -> Result<()> {
    if askpass.is_none() {
        cliclack::log::info("sudo asks for your password once, for pacman and two links KWin needs")?;
        let status = Command::new("sudo").arg("-v").status().context("starting sudo")?;
        if !status.success() {
            bail!("sudo did not accept the password");
        }
    }
    Ok(())
}

fn system_line() -> Result<String> {
    let versions = system::installed_versions();
    let plasma = versions.get("plasma-workspace").cloned().unwrap_or_default();
    if !plasma.starts_with("6.") {
        bail!("KDE Plasma 6 is not installed (plasma-workspace: {})", if plasma.is_empty() { "none" } else { &plasma });
    }
    let session = env::var("XDG_SESSION_TYPE").unwrap_or_default();
    let get = |name: &str| versions.get(name).cloned().unwrap_or_else(|| "?".into());
    Ok(format!(
        "KDE Plasma {plasma}, KWin {}, Qt {}, {}",
        get("kwin"),
        get("qt6-base"),
        match session.as_str() {
            "wayland" => "Wayland",
            "x11" => "X11",
            "" => "an unknown session",
            other => other,
        }
    ))
}

/// Puts `icon` on the menu bar's menu. Returns how many menus changed (none when Plasma is not running).
fn show_logo(icon: &str) -> usize {
    run::capture(
        "qdbus6",
        &[
            "org.kde.plasmashell",
            "/PlasmaShell",
            "org.kde.PlasmaShell.evaluateScript",
            &crate::logo::plasma_script(icon),
        ],
    )
    .ok()
    .and_then(|out| out.trim().parse().ok())
    .unwrap_or(0)
}

/// The one question: which logo goes in the menu bar, from a single list of every logo in black and white or in its
/// colors. prompt is the question; initial and colors preselect the current choice.
fn pick_logo(prompt: &str, initial: Logo, colors: Colors) -> Result<(Logo, Colors)> {
    let choices = crate::logo::choices();
    let start = choices
        .iter()
        .position(|choice| choice.logo == initial && (choice.colors == colors || !initial.has_colors()))
        .unwrap_or(0);
    let items: Vec<(usize, &str, &str)> =
        choices.iter().enumerate().map(|(at, choice)| (at, choice.label.as_str(), "")).collect();
    let picked = cliclack::select(prompt).items(&items).initial_value(start).interact()?;
    Ok((choices[picked].logo, choices[picked].colors))
}

/// `aurora install`: checks the system, asks for the logo and the tuning, then runs `install.sh`.
///
/// # Errors
/// When the system cannot take Aurora, the user cancels, or a step fails.
pub fn install(places: &Places, choices: InstallChoices) -> Result<()> {
    let source = source(places)?;
    cliclack::intro(Style::new().bold().apply_to("Install Aurora"))?;
    if run::capture("id", &["-u"])?.trim() == "0" {
        bail!("run aurora as your own user, not with sudo: it installs into your home folder");
    }
    cliclack::log::step(format!("Detected  {}", system_line()?))?;
    let (mut logo, mut colors, performance) = (choices.logo.unwrap_or_default(), choices.colors, choices.performance);
    // One question: the logo. Choosing it starts the install; Esc leaves everything as it is.
    if interactive(choices.yes) {
        let parts = if source.root.join("prebuilt").is_dir() {
            "Everything comes ready to use: nothing is compiled."
        } else {
            "Aurora's parts are built for your Plasma first, in about 3 minutes."
        };
        cliclack::note(
            "Before it starts",
            format!(
                "{parts}\nYour current look is saved, so `aurora uninstall` can bring it back.\n\
                 Your password is asked at most once."
            ),
        )?;
        (logo, colors) = pick_logo("Choose the logo for your menu bar", logo, colors)?;
    }
    let askpass = askpass(places);
    sudo_ready(askpass.as_deref())?;
    // install.sh chooses where the parts come from: the package's prebuilt ones, or a build.
    let env: Vec<(&str, &Path)> = askpass.iter().map(|dialog| ("SUDO_ASKPASS", dialog.as_path())).collect();
    let mut args = vec!["--yes"];
    if performance {
        args.push("--performance");
    }
    run::script(&source.root.join("install.sh"), &args, &env, &places.state, "Starting the installer")?;
    let icon = logo.icon(colors);
    if show_logo(icon) == 0 {
        cliclack::log::warning(format!(
            "Aurora's menu bar is not running yet: after you log in again, run `aurora logo {}`",
            logo.id()
        ))?;
    }
    cliclack::outro_note(
        Style::new().bold().apply_to("Aurora is installed. Log out and back in to see it."),
        "aurora settings    update notices and the menu bar logo\naurora update      get the newest version\naurora uninstall   bring your previous desktop back",
    )?;
    Ok(())
}

/// `aurora rebuild`: builds the parts again for the Plasma, KWin and Qt installed now.
///
/// # Errors
/// When a step fails.
pub fn rebuild(places: &Places) -> Result<()> {
    let source = source(places)?;
    cliclack::intro(Style::new().bold().apply_to("Rebuild Aurora's parts"))?;
    cliclack::log::step(format!("Detected  {}", system_line()?))?;
    let askpass = askpass(places);
    sudo_ready(askpass.as_deref())?;
    let env: Vec<(&str, &Path)> = askpass.iter().map(|dialog| ("SUDO_ASKPASS", dialog.as_path())).collect();
    run::script(&source.root.join("install.sh"), &["--rebuild", "--yes"], &env, &places.state, "Starting the rebuild")?;
    cliclack::outro("Done. Log out and back in to load the rebuilt parts.")?;
    Ok(())
}

/// `aurora logo`: shows another logo in the menu bar, right away.
///
/// # Errors
/// When the user cancels or Plasma is not running.
pub fn logo(choice: Option<Logo>, colors: Colors) -> Result<()> {
    let (logo, colors) = match choice {
        Some(logo) => (logo, colors),
        None if interactive(false) => {
            cliclack::intro(Style::new().bold().apply_to("Menu bar logo"))?;
            pick_logo("Which logo goes at the left of the menu bar?", Logo::default(), colors)?
        }
        None => bail!("name a logo: aurora logo <cachyos|arch|apple|kali> [--colors]"),
    };
    if show_logo(logo.icon(colors)) == 0 {
        bail!("no menu bar was found: is Plasma running with Aurora's menu bar?");
    }
    let how = if colors == Colors::On && logo.has_colors() { "in its colors" } else { "in black and white" };
    if choice.is_none() {
        cliclack::outro(format!("The menu bar shows the {logo} logo {how}."))?;
    } else {
        println!("The menu bar shows the {logo} logo {how}.");
    }
    Ok(())
}

/// `aurora uninstall`: puts back the desktop from the backup the first install made.
///
/// # Errors
/// When the user cancels or the restore fails.
pub fn uninstall(places: &Places, yes: bool) -> Result<()> {
    let source = source(places)?;
    cliclack::intro(Style::new().bold().apply_to("Remove Aurora"))?;
    if interactive(yes)
        && !cliclack::confirm("Put your desktop back as it was before Aurora?").initial_value(false).interact()?
    {
        cliclack::outro_cancel("Nothing was changed")?;
        return Ok(());
    }
    let askpass = askpass(places);
    sudo_ready(askpass.as_deref())?;
    let env: Vec<(&str, &Path)> = askpass.iter().map(|dialog| ("SUDO_ASKPASS", dialog.as_path())).collect();
    run::script(&source.root.join("uninstall.sh"), &[], &env, &places.state, "Putting your desktop back")?;
    cliclack::outro("Aurora is removed. Log out and back in to finish.")?;
    Ok(())
}

/// The logo the menu bar shows now, when it is one of Aurora's.
#[must_use]
pub fn current_logo() -> Option<(Logo, Colors)> {
    let script = "var ps = panels(); for (var i = 0; i < ps.length; i++) { var ws = ps[i].widgets('org.aurora.logomenu'); \
                  for (var j = 0; j < ws.length; j++) { ws[j].currentConfigGroup = ['Advanced']; print(ws[j].readConfig('icon', '')); } }";
    let out =
        run::capture("qdbus6", &["org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", script])
            .ok()?;
    Logo::from_icon(out.lines().next()?.trim())
}

/// Whether the optional performance tuning is installed, even in part (an update keeps it, and makes it whole).
#[must_use]
pub fn tuned() -> bool {
    crate::tuning::current() != crate::tuning::State::Off
}

/// The status line about Aurora's built parts: none before an install, otherwise whether the login script set any
/// aside because Plasma, KWin or Qt changed since they were built.
fn parts_line(installed: bool, paused: &[String]) -> String {
    if !installed {
        "none installed yet".to_string()
    } else if paused.is_empty() {
        "built for this Plasma".to_string()
    } else {
        format!("set aside until `aurora rebuild`: {}", paused.join(", "))
    }
}

/// `aurora license`: the Appropriate Legal Notices of GPL-3.0 section 0, with the license from the copy Aurora runs
/// from, or from the package's licenses folder.
///
/// # Errors
/// When the terminal cannot be written.
pub fn license(places: &Places, version: &str) -> Result<()> {
    let packaged = Path::new("/usr/share/licenses/aurora-theme/LICENSE");
    let local = source(places)
        .ok()
        .map(|found| found.root.join("LICENSE"))
        .filter(|path| path.exists())
        .or_else(|| packaged.exists().then(|| packaged.to_path_buf()));
    print!("{}", legal::notice(version, local.as_deref()));
    Ok(())
}

/// `aurora status`: what is installed, from where, and whether a rebuild is due.
///
/// # Errors
/// When the terminal cannot be written.
pub fn status(places: &Places) -> Result<()> {
    let found = source(places).ok();
    let versions = system::installed_versions();
    let get = |name: &str| versions.get(name).cloned().unwrap_or_else(|| "?".into());
    let from = match &found {
        Some(source) => format!(
            "{} ({}, {})",
            source.version(),
            match source.kind {
                Kind::Checkout => "git checkout",
                Kind::Package => "pacman package",
                Kind::Archive => "release archive",
            },
            source.root.display()
        ),
        None => "not found".into(),
    };
    let paused = system::paused_parts(&places.home.join(".local/lib/qt6/plugins"));
    let parts = parts_line(places.installed(), &paused);
    let logo = current_logo().map_or_else(
        || "a custom icon, or Plasma is not running".to_string(),
        |(logo, colors)| format!("{logo}, {}", if colors == Colors::On { "in its colors" } else { "black and white" }),
    );
    let lines = [
        ("Aurora", from),
        (
            "Repository",
            match found.as_ref().map(|source| source.kind) {
                Some(Kind::Package) => crate::repo::describe(),
                _ => "not used by this copy".into(),
            },
        ),
        ("Installed", if places.installed() { "yes".into() } else { "no, run `aurora install`".into() }),
        ("System", format!("KDE Plasma {}, KWin {}, Qt {}", get("plasma-workspace"), get("kwin"), get("qt6-base"))),
        ("Parts", parts),
        ("Menu logo", logo),
        ("Tuning", crate::tuning::current().to_string()),
        ("Mission Center", crate::mission::Installed::now().to_string()),
    ];
    let width = lines.iter().map(|(key, _)| key.len()).max().unwrap_or(0);
    let body: Vec<String> = lines.iter().map(|(key, value)| format!("{key:<width$}   {value}")).collect();
    cliclack::intro(Style::new().bold().apply_to("Aurora status"))?;
    cliclack::note("", body.join("\n"))?;
    cliclack::outro(
        Style::new().dim().apply_to(format!("Release builds: https://github.com/{}/releases", release::REPOSITORY)),
    )?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::parts_line;

    #[test]
    fn before_an_install_no_parts_are_reported_as_built() {
        assert_eq!(parts_line(false, &[]), "none installed yet");
    }

    #[test]
    fn installed_parts_are_built_for_this_plasma_unless_some_were_set_aside() {
        assert_eq!(parts_line(true, &[]), "built for this Plasma");
        let paused = ["system-tray".to_string(), "quick-menu".to_string()];
        assert_eq!(parts_line(true, &paused), "set aside until `aurora rebuild`: system-tray, quick-menu");
    }
}
