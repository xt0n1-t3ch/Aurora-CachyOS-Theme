// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Updates a copy that came from a release archive: asks GitHub for the newest release, downloads its archive and
//! checksums, checks both and unpacks the archive next to the old version.

use std::fs;
use std::path::Path;

use anyhow::{Context, Result, bail};

use crate::app::Places;
use crate::release::{self, CHECKSUMS, Release};
use crate::run;
use crate::source::Source;

/// curl options for every request: HTTPS only, also across redirects (GitHub sends a download on to its storage host),
/// and TLS 1.2 or newer.
const CURL_SAFE: [&str; 7] = ["-fsSL", "--proto", "=https", "--proto-redir", "=https", "--tlsv1.2", "--max-redirs"];
const CURL_MAX_REDIRECTS: &str = "5";

fn curl(extra: &[&str]) -> Result<String> {
    let mut args: Vec<&str> = CURL_SAFE.to_vec();
    args.push(CURL_MAX_REDIRECTS);
    args.extend_from_slice(extra);
    run::capture("curl", &args)
}

/// GitHub's answer for the newest release: through `gh` when it is installed (signed in, it also reaches a private
/// repository), otherwise the public API with curl.
///
/// # Errors
/// When GitHub cannot be reached or its answer names no release archive.
pub fn latest() -> Result<Release> {
    let path = format!("repos/{}/releases/latest", release::REPOSITORY);
    let json = if run::on_path("gh") {
        run::capture("gh", &["api", &path])
    } else {
        curl(&["-H", "Accept: application/vnd.github+json", &format!("https://api.github.com/{path}")])
    }
    .context("asking GitHub for the newest release")?;
    release::parse_latest(&json)
}

fn fetch(url: &str, to: &Path) -> Result<()> {
    curl(&["-o", &to.to_string_lossy(), url]).with_context(|| format!("downloading {url}"))?;
    Ok(())
}

/// The newest release unpacked and checked, or `None` when `installed` is already the newest.
///
/// The archive is checked against the release's `SHA256SUMS`, and its listing is checked before anything is
/// unpacked, so no member can land outside its own folder.
///
/// # Errors
/// When GitHub cannot be reached, a download fails, the archive does not match its checksum, or a member would
/// unpack outside the archive's folder.
pub fn newest(places: &Places, installed: &str) -> Result<Option<Source>> {
    let spinner = cliclack::spinner();
    spinner.start("Looking for a newer release");
    let found = latest()?;
    if !release::is_newer(&found.version, installed) {
        spinner.stop(format!("Aurora {installed} is the newest release"));
        return Ok(None);
    }
    spinner.set_message(format!("Downloading Aurora {}", found.version));
    let folder = places.cache.join("aurora/releases").join(&found.version);
    fs::create_dir_all(&folder).with_context(|| format!("creating {}", folder.display()))?;
    let name = release::archive_name(&found.version);
    let archive = folder.join(&name);
    let sums = folder.join(CHECKSUMS);
    fetch(&found.archive_url, &archive)?;
    fetch(&found.checksums_url, &sums)?;
    release::verify(&fs::read_to_string(&sums)?, &name, &fs::read(&archive)?)?;

    let top = format!("aurora-{}", found.version);
    let archive_path = archive.to_string_lossy();
    release::check_members(&run::capture("tar", &["-tzf", &archive_path])?, &top)?;
    let versions = places.home.join(".local/share/aurora/versions");
    fs::create_dir_all(&versions).with_context(|| format!("creating {}", versions.display()))?;
    run::capture("tar", &["-xzf", &archive_path, "--no-same-owner", "-C", &versions.to_string_lossy()])?;
    let unpacked = versions.join(&top);
    let Some(source) = Source::at(&unpacked) else {
        bail!("the archive of {} did not unpack into {}", found.version, unpacked.display());
    };
    spinner.stop(format!("Downloaded Aurora {} and checked it against {CHECKSUMS}", found.version));
    Ok(Some(source))
}
