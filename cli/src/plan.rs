// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! How `aurora update` brings a newer Aurora package without breaking its parts. The package's `.BUILDINFO` names the
//! Plasma, KWin, Qt and KDecoration it was built against, and `checkupdates` names what a system upgrade would bring:
//! when those match, Aurora updates with the whole system; when the package matches the system as it is, Aurora
//! updates on its own; otherwise nothing changes until a build for this system is out.

use std::fs;
use std::path::Path;

use anyhow::{Context, Result, bail};

use crate::download;
use crate::run;
use crate::system::{self, Versions};
use crate::updates;

/// How much of the package is downloaded to read its `.BUILDINFO`. pacman puts that file first in the archive, and
/// the first 128 KiB held all of aurora-theme 1.1.1's (66 KiB of text), so a mebibyte leaves room to grow.
const BUILDINFO_BYTES: u64 = 1024 * 1024;
// Measured 2026-11-07 on aurora-theme 1.1.1-1: bsdtar read its whole .BUILDINFO from 128 KiB, not from 64 KiB.
const _: () = assert!(BUILDINFO_BYTES >= 128 * 1024);
/// The fields of a `.BUILDINFO` package entry, `NAME-VERSION-RELEASE-ARCH`, split from the right.
const ENTRY_FIELDS: usize = 4;

/// What `aurora update` does with a newer Aurora package.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Plan {
    /// Install it now, the way given.
    Install(Install),
    /// It is built for neither the system as it is nor the system after the upgrade: change nothing.
    Wait,
}

/// How a newer Aurora package is installed.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Install {
    /// With the whole system upgrade: it is built for what that upgrade brings, and Arch updates every package at once.
    WithSystem,
    /// On its own: it is built for the system as it is, and the upgrade would bring versions it is not built for.
    Alone,
}

impl Install {
    /// The command that installs the package at `url` this way; with `yes`, pacman asks nothing (`--noconfirm`), as
    /// `aurora update --yes` promises. `pacman -U` checks a downloaded package's signature as strictly as a
    /// repository's (pacman.conf: `RemoteFileSigLevel` takes `SigLevel` by default).
    #[must_use]
    pub fn command(self, url: &str, yes: bool) -> Vec<String> {
        let quiet: &[&str] = if yes { &["--noconfirm"] } else { &[] };
        let (verb, target): (&[&str], &str) = match self {
            Install::WithSystem => (&["-Syu", "--needed"], updates::PACKAGE),
            Install::Alone => (&["-U"], url),
        };
        ["sudo", "pacman"].iter().chain(verb).chain(quiet).chain([&target]).map(|word| (*word).to_string()).collect()
    }
}

/// Chooses the plan from what the package was `built` against, what is `installed`, and what the system upgrade would
/// leave (`upgraded`). Updating with the system comes first, so the rest of the system stays current whenever that is
/// safe for Aurora.
#[must_use]
pub fn plan(built: &Versions, installed: &Versions, upgraded: &Versions) -> Plan {
    if system::fits(built, upgraded) {
        Plan::Install(Install::WithSystem)
    } else if system::fits(built, installed) {
        Plan::Install(Install::Alone)
    } else {
        Plan::Wait
    }
}

/// The versions a package was built against, from its `.BUILDINFO`: one `installed = NAME-VERSION-RELEASE-ARCH` line
/// per package of the machine that built it, of which only the linked packages are kept.
#[must_use]
pub fn parse_buildinfo(text: &str) -> Versions {
    text.lines()
        .filter_map(|line| line.trim().strip_prefix("installed = "))
        .filter_map(|entry| {
            // Package names may contain dashes; versions, releases and architectures may not (PKGBUILD(5)).
            let mut fields = entry.rsplitn(ENTRY_FIELDS, '-');
            let (_arch, _release, version, name) = (fields.next()?, fields.next()?, fields.next()?, fields.next()?);
            system::LINKS
                .iter()
                .any(|link| link.package == name)
                .then(|| (name.to_string(), system::upstream(version).to_string()))
        })
        .collect()
}

/// What the package of Aurora `version` was built against, from its `.BUILDINFO` text.
///
/// # Errors
/// When the text does not name a version for every linked package: such a package could never be checked.
pub fn built_for(buildinfo: &str, version: &str) -> Result<Versions> {
    let built = parse_buildinfo(buildinfo);
    if built.len() < system::LINKS.len() {
        bail!("the package of Aurora {version} does not say which Plasma, KWin, Qt and KDecoration it was built for");
    }
    Ok(built)
}

/// The versions of the `installed` linked packages once the system is upgraded: the newer one `checkupdates` lists,
/// otherwise the one installed now.
#[must_use]
pub fn after_upgrade(installed: &Versions, checkupdates: &str) -> Versions {
    installed
        .iter()
        .map(|(name, version)| {
            let newer = updates::parse_checkupdates(checkupdates, name).map(|full| system::upstream(&full).to_string());
            (name.clone(), newer.unwrap_or_else(|| version.clone()))
        })
        .collect()
}

/// The download address `pacman -Sp` printed. It must be an https address: it goes to curl and to `pacman -U`.
///
/// # Errors
/// When pacman printed anything else, such as a file in its cache.
pub fn server_url(printed: &str) -> Result<String> {
    let url = printed.trim();
    if !url.starts_with("https://") || url.contains(char::is_whitespace) {
        bail!("pacman gives `{url}` as the address of {}, not one https address", updates::PACKAGE);
    }
    Ok(url.to_string())
}

/// A newer Aurora package in the repositories, with what it needs.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Found {
    /// Its full version, package release included (`1.1.1-1`).
    pub version: String,
    /// Where pacman downloads it from.
    pub url: String,
    /// What it was built against.
    pub built: Versions,
    /// What this system has now.
    pub installed: Versions,
    /// What this system would have after a full upgrade.
    pub upgraded: Versions,
}

impl Found {
    /// See [`plan`].
    #[must_use]
    pub fn plan(&self) -> Plan {
        plan(&self.built, &self.installed, &self.upgraded)
    }
}

/// Looks for a newer aurora-theme in the repositories pacman uses and reads what it was built for, without root and
/// without touching pacman's own databases. `databases` is where `checkupdates` keeps its copy of them; `scratch` is a
/// folder for the start of the package.
///
/// # Errors
/// When `checkupdates`, pacman, the download or bsdtar fail, or the package does not say what it was built for.
pub fn look(databases: &Path, scratch: &Path) -> Result<Option<Found>> {
    let listed = updates::checkupdates(databases)?;
    let Some(version) = updates::parse_checkupdates(&listed, updates::PACKAGE) else {
        return Ok(None);
    };
    // pacman prints a file of its cache when the package is there already; an empty cache folder makes it print the
    // repository's address instead.
    let empty = scratch.join("empty-cache");
    fs::create_dir_all(&empty).with_context(|| format!("creating {}", empty.display()))?;
    let (databases_arg, empty_arg) = (databases.to_string_lossy(), empty.to_string_lossy());
    let args = ["-Sp", "--dbpath", &databases_arg, "--cachedir", &empty_arg, "--print-format", "%l", updates::PACKAGE];
    let url = server_url(&run::capture("pacman", &args)?)?;
    let start = scratch.join("package-start");
    download::head(&url, &start, BUILDINFO_BYTES)?;
    let buildinfo = run::capture("bsdtar", &["-xOqf", &start.to_string_lossy(), ".BUILDINFO"]);
    // The start of the package is only read once; a file left behind is overwritten by the next look.
    let _ = fs::remove_file(&start);
    let built =
        built_for(&buildinfo.with_context(|| format!("reading what Aurora {version} was built for"))?, &version)?;
    let installed = system::installed_versions();
    let upgraded = after_upgrade(&installed, &listed);
    Ok(Some(Found { version, url, built, installed, upgraded }))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn versions(pairs: &[(&str, &str)]) -> Versions {
        pairs.iter().map(|(name, version)| ((*name).to_string(), (*version).to_string())).collect()
    }

    fn plasma(version: &str) -> Versions {
        versions(&[("plasma-workspace", version), ("kwin", version), ("qt6-base", "6.12.0"), ("kdecoration", version)])
    }

    // Recorded from aurora-theme 1.1.1-1's .BUILDINFO (built 2026-11-06 on CachyOS, makepkg 7.1.0), trimmed to its
    // header, the first and last packages, the linked packages and kwindowsystem, whose name starts like kwin's.
    const BUILDINFO: &str = include_str!("../tests/fixtures/buildinfo.txt");

    #[test]
    fn the_buildinfo_names_the_versions_the_parts_were_built_against() {
        assert_eq!(parse_buildinfo(BUILDINFO), plasma("6.8.2"));
    }

    #[test]
    fn a_package_that_does_not_name_every_linked_version_is_refused() {
        assert_eq!(built_for(BUILDINFO, "1.1.1-1").expect("a whole .BUILDINFO"), plasma("6.8.2"));
        let without_kwin = BUILDINFO.lines().filter(|line| !line.contains("= kwin-6")).collect::<Vec<_>>().join("\n");
        assert!(built_for(&without_kwin, "1.1.1-1").is_err());
        assert!(built_for("", "1.1.1-1").is_err());
    }

    #[test]
    fn a_package_whose_name_starts_like_a_linked_one_is_not_taken_for_it() {
        let lookalikes = "installed = kwindowsystem-6.31.0-1.1-x86_64_v3\ninstalled = kwin-x11-6.8.2-1.1-x86_64_v3\n\
                          installed = plasma-workspace-wallpapers-6.8.2-1.1-any\n";
        assert_eq!(parse_buildinfo(lookalikes), Versions::new());
        assert_eq!(parse_buildinfo("installed = kwin-1:6.8.2-2.1-x86_64\n")["kwin"], "6.8.2", "an epoch is dropped");
    }

    // Recorded on CachyOS, 2026-11-07, pacman-contrib 1.13.1: an update that brought Aurora 1.1.1 with two unrelated
    // packages and nothing of Plasma.
    const UNRELATED: &str = "cachyos-drm-awaiter 7-1 -> 8-1\nclaude-desktop 4.16336.0-1 -> 4.23118.0-1\n\
                             aurora-theme 1.1.0-1 -> 1.1.1-1\n";

    #[test]
    fn an_upgrade_that_brings_no_new_plasma_keeps_the_installed_versions() {
        assert_eq!(after_upgrade(&plasma("6.8.2"), UNRELATED), plasma("6.8.2"));
    }

    #[test]
    fn an_upgrade_that_brings_a_new_plasma_is_read_without_package_releases() {
        let listed = "kwin 6.8.2-2.1 -> 6.9.0-1.1\nplasma-workspace 6.8.2-1.1 -> 6.9.0-1.1\n\
                      kdecoration 6.8.2-1.1 -> 6.9.0-1.1\nkwindowsystem 6.31.0-1.1 -> 6.32.0-1.1\n";
        assert_eq!(after_upgrade(&plasma("6.8.2"), listed), plasma("6.9.0"));
    }

    // The update recorded above: Aurora 1.1.1 was built for the Plasma installed, and the upgrade changed no Plasma.
    #[test]
    fn a_release_built_for_what_the_upgrade_brings_updates_with_the_whole_system() {
        let installed = plasma("6.8.2");
        let upgraded = after_upgrade(&installed, UNRELATED);
        assert_eq!(plan(&plasma("6.8.2"), &installed, &upgraded), Plan::Install(Install::WithSystem));
        assert_eq!(plan(&plasma("6.9.0"), &plasma("6.8.2"), &plasma("6.9.0")), Plan::Install(Install::WithSystem));
    }

    #[test]
    fn a_release_built_for_this_system_installs_alone_while_the_repositories_have_a_newer_plasma() {
        assert_eq!(plan(&plasma("6.8.2"), &plasma("6.8.2"), &plasma("6.9.0")), Plan::Install(Install::Alone));
    }

    #[test]
    fn a_release_built_for_neither_changes_nothing() {
        assert_eq!(plan(&plasma("6.9.0"), &plasma("6.8.2"), &plasma("6.9.1")), Plan::Wait);
    }

    #[test]
    fn with_the_system_pacman_upgrades_everything_and_alone_it_installs_the_signed_package_from_its_address() {
        let url = "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/repo/aurora-theme-1.2.0-1-x86_64.pkg.tar.zst";
        assert_eq!(Install::WithSystem.command(url, false), ["sudo", "pacman", "-Syu", "--needed", "aurora-theme"]);
        assert_eq!(Install::Alone.command(url, false), ["sudo", "pacman", "-U", url]);
    }

    #[test]
    fn asked_for_no_questions_pacman_asks_none() {
        let url = "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/repo/aurora-theme-1.3.5-1-x86_64.pkg.tar.zst";
        assert_eq!(
            Install::WithSystem.command(url, true),
            ["sudo", "pacman", "-Syu", "--needed", "--noconfirm", "aurora-theme"]
        );
        assert_eq!(Install::Alone.command(url, true), ["sudo", "pacman", "-U", "--noconfirm", url]);
    }

    #[test]
    fn only_one_https_address_is_taken_from_pacman() {
        let url = "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/repo/aurora-theme-1.1.1-1-x86_64.pkg.tar.zst";
        assert_eq!(server_url(&format!("{url}\n")).expect("an https address"), url);
        assert!(server_url("file:///var/cache/pacman/pkg/aurora-theme-1.1.1-1-x86_64.pkg.tar.zst\n").is_err());
        assert!(server_url("http://example.com/aurora-theme.pkg.tar.zst").is_err());
        assert!(server_url(&format!("{url}\n{url}\n")).is_err());
        assert!(server_url("").is_err());
    }
}
