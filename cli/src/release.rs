// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Aurora's releases on GitHub: which one is newest, where its files are, and whether a download is intact.
//! `scripts/release.sh` builds `aurora-X.Y.Z.tar.gz` and `SHA256SUMS` for every version.

use std::fmt::Write as _;

use anyhow::{Context, Result, anyhow, bail};
use serde_json::Value;
use sha2::{Digest, Sha256};

/// The repository the releases live in.
pub const REPOSITORY: &str = "xt0n1-t3ch/Aurora-CachyOS-Theme";
/// The checksum file every release carries next to its archive.
pub const CHECKSUMS: &str = "SHA256SUMS";

/// The name of a version's source archive.
#[must_use]
pub fn archive_name(version: &str) -> String {
    format!("aurora-{version}.tar.gz")
}

/// The files of one release.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Release {
    pub version: String,
    pub archive_url: String,
    pub checksums_url: String,
}

/// Reads GitHub's "latest release" answer (`GET /repos/{owner}/{repo}/releases/latest`): the tag `vX.Y.Z` and the
/// download addresses of the archive and the checksums.
///
/// # Errors
/// When the answer is not JSON, has no tag, or lacks either file.
pub fn parse_latest(json: &str) -> Result<Release> {
    let answer: Value = serde_json::from_str(json).context("GitHub's answer is not JSON")?;
    let tag = answer["tag_name"].as_str().ok_or_else(|| anyhow!("the release has no tag"))?;
    let version = tag.strip_prefix('v').unwrap_or(tag).to_string();
    // The version becomes part of a folder name in the cache: only a plain SemVer version may.
    if !is_plain_version(&version) {
        bail!("the release tag {tag:?} is not a version");
    }
    let assets = answer["assets"].as_array().map(Vec::as_slice).unwrap_or_default();
    let url_of = |name: &str| -> Result<String> {
        assets
            .iter()
            .find(|asset| asset["name"].as_str() == Some(name))
            .and_then(|asset| asset["browser_download_url"].as_str())
            .ok_or_else(|| anyhow!("release {tag} has no {name}"))
            .and_then(|url| {
                // Only the theme's own release files, and only over HTTPS.
                let expected = format!("https://github.com/{REPOSITORY}/releases/download/{tag}/{name}");
                if url == expected {
                    Ok(url.to_string())
                } else {
                    Err(anyhow!("release {tag} points {name} at {url}, not at {expected}"))
                }
            })
    };
    Ok(Release { archive_url: url_of(&archive_name(&version))?, checksums_url: url_of(CHECKSUMS)?, version })
}

/// Whether `version` is a Semantic Versioning version: three numbers, then optional pre-release and build
/// identifiers made of letters, digits and hyphens.
#[must_use]
pub fn is_plain_version(version: &str) -> bool {
    let identifiers = |part: &str| {
        part.split('.').all(|id| !id.is_empty() && id.chars().all(|c| c.is_ascii_alphanumeric() || c == '-'))
    };
    let (rest, build) = version.split_once('+').map_or((version, None), |(rest, build)| (rest, Some(build)));
    let (core, pre) = rest.split_once('-').map_or((rest, None), |(core, pre)| (core, Some(pre)));
    let numbers: Vec<&str> = core.split('.').collect();
    numbers.len() == 3
        && numbers
            .iter()
            .all(|n| !n.is_empty() && n.chars().all(|c| c.is_ascii_digit()) && (n.len() == 1 || !n.starts_with('0')))
        && pre.is_none_or(identifiers)
        && build.is_none_or(identifiers)
}

/// Checks a `tar -t` listing before anything is unpacked: every member must sit inside `top`, with no absolute path
/// and no `..`, so the archive cannot write anywhere else.
///
/// # Errors
/// When the listing is empty or a member would land outside `top`.
pub fn check_members(listing: &str, top: &str) -> Result<()> {
    let mut members = 0;
    for member in listing.lines().filter(|line| !line.is_empty()) {
        let parts: Vec<&str> = member.trim_end_matches('/').split('/').collect();
        let inside = parts.first() == Some(&top) && parts.iter().all(|part| !part.is_empty() && *part != "..");
        if !inside {
            bail!("the archive holds {member:?}, outside {top}/");
        }
        members += 1;
    }
    if members == 0 {
        bail!("the archive is empty");
    }
    Ok(())
}

/// The SHA-256 a `sha256sum`-style file lists for `file`.
#[must_use]
pub fn checksum_for(sums: &str, file: &str) -> Option<String> {
    sums.lines().find_map(|line| {
        let (sum, name) = line.split_once(char::is_whitespace)?;
        (name.trim_start().trim_start_matches('*') == file).then(|| sum.to_lowercase())
    })
}

/// The SHA-256 of `bytes`, in lowercase hex as `sha256sum` prints it.
#[must_use]
pub fn sha256_hex(bytes: &[u8]) -> String {
    Sha256::digest(bytes).iter().fold(String::with_capacity(64), |mut hex, byte| {
        let _ = write!(hex, "{byte:02x}");
        hex
    })
}

/// Checks a downloaded file against the release's checksums.
///
/// # Errors
/// When the checksums do not list the file or list another sum.
pub fn verify(sums: &str, file: &str, bytes: &[u8]) -> Result<()> {
    let expected = checksum_for(sums, file).ok_or_else(|| anyhow!("{CHECKSUMS} does not list {file}"))?;
    let actual = sha256_hex(bytes);
    if actual != expected {
        bail!("{file} is damaged: its SHA-256 is {actual}, the release says {expected}");
    }
    Ok(())
}

/// Whether `candidate` is a later version than `installed`, by Semantic Versioning precedence.
#[must_use]
pub fn is_newer(candidate: &str, installed: &str) -> bool {
    precedence(candidate) > precedence(installed)
}

/// A sort key: the three numbers, then a release above its pre-releases, then the pre-release identifiers.
fn precedence(version: &str) -> (Vec<u64>, bool, Vec<(bool, u64, String)>) {
    let core_and_pre = version.split('+').next().unwrap_or_default();
    let (core, pre) = core_and_pre.split_once('-').map_or((core_and_pre, None), |(core, pre)| (core, Some(pre)));
    let numbers = core.split('.').map(|part| part.parse().unwrap_or(0)).collect();
    let identifiers = pre
        .unwrap_or_default()
        .split('.')
        .filter(|part| !part.is_empty())
        .map(|part| match part.parse::<u64>() {
            Ok(number) => (false, number, String::new()),
            Err(_) => (true, 0, part.to_string()),
        })
        .collect();
    (numbers, pre.is_none(), identifiers)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The fields GitHub's REST API returns for a release that matter here
    /// (<https://docs.github.com/en/rest/releases/releases#get-the-latest-release>).
    const LATEST: &str = r#"{
        "tag_name": "v1.2.0", "name": "Aurora 1.2.0", "draft": false, "prerelease": false,
        "assets": [
            {"name": "aurora-1.2.0.tar.gz", "browser_download_url": "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/v1.2.0/aurora-1.2.0.tar.gz"},
            {"name": "SHA256SUMS", "browser_download_url": "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/v1.2.0/SHA256SUMS"}
        ]
    }"#;

    #[test]
    fn the_latest_release_gives_its_version_and_its_two_files() {
        let release = parse_latest(LATEST).expect("a release");
        assert_eq!(release.version, "1.2.0");
        assert!(release.archive_url.ends_with("/v1.2.0/aurora-1.2.0.tar.gz"));
        assert!(release.checksums_url.ends_with("/v1.2.0/SHA256SUMS"));
    }

    #[test]
    fn a_release_without_its_archive_is_refused() {
        let error = parse_latest(r#"{"tag_name": "v1.2.0", "assets": []}"#).expect_err("no archive");
        assert_eq!(error.to_string(), "release v1.2.0 has no aurora-1.2.0.tar.gz");
    }

    #[test]
    fn checksums_are_found_by_file_name_in_text_or_binary_mode() {
        let sums = "AB12  aurora-1.2.0.tar.gz\ncd34 *release-notes.md\n";
        assert_eq!(checksum_for(sums, "aurora-1.2.0.tar.gz").as_deref(), Some("ab12"));
        assert_eq!(checksum_for(sums, "release-notes.md").as_deref(), Some("cd34"));
        assert_eq!(checksum_for(sums, "other"), None);
    }

    #[test]
    fn a_download_matches_its_checksum_or_is_refused() {
        // sha256("abc"), FIPS 180-2 appendix B.1.
        let sums = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  a.tar.gz\n";
        assert!(verify(sums, "a.tar.gz", b"abc").is_ok());
        assert!(verify(sums, "a.tar.gz", b"abd").is_err());
        assert!(verify(sums, "b.tar.gz", b"abc").is_err());
    }

    #[test]
    fn a_tag_that_is_not_a_plain_version_is_refused() {
        // The version names a folder in the cache, so it must not be able to climb out of it.
        for tag in ["v../../.bashrc", "v1.2.0/../../x", "v1.2", "v1.2.0 ", "latest"] {
            let json = LATEST.replace("\"v1.2.0\"", &format!("{tag:?}"));
            assert!(parse_latest(&json).is_err(), "{tag} was accepted");
        }
    }

    #[test]
    fn a_download_outside_the_theme_s_releases_is_refused() {
        let elsewhere = LATEST.replace(
            "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/v1.2.0/aurora",
            "https://example.org/aurora",
        );
        assert!(parse_latest(&elsewhere).is_err());
        let plain_http = LATEST.replace(
            "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/v1.2.0/SHA256SUMS",
            "http://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/v1.2.0/SHA256SUMS",
        );
        assert!(parse_latest(&plain_http).is_err());
    }

    #[test]
    fn an_archive_unpacks_only_inside_its_own_folder() {
        let good = "aurora-1.2.0/\naurora-1.2.0/install.sh\naurora-1.2.0/themes/x/y.svg\n";
        assert!(check_members(good, "aurora-1.2.0").is_ok());
        for bad in ["/etc/passwd", "aurora-1.2.0/../../.bashrc", "other/install.sh", "aurora-1.2.0x/a", ".."] {
            assert!(check_members(&format!("aurora-1.2.0/\n{bad}\n"), "aurora-1.2.0").is_err(), "{bad} was accepted");
        }
        assert!(check_members("", "aurora-1.2.0").is_err(), "an empty archive was accepted");
    }

    #[test]
    fn versions_compare_by_semver_precedence() {
        assert!(is_newer("1.10.0", "1.9.3"));
        assert!(is_newer("2.0.0", "2.0.0-rc.1"));
        assert!(is_newer("1.0.0-rc.2", "1.0.0-rc.1"));
        assert!(is_newer("1.0.0-rc.1", "1.0.0-beta"));
        assert!(!is_newer("1.2.0", "1.2.0"));
        assert!(!is_newer("1.2.0+build.5", "1.2.0"));
        assert!(is_newer("0.1.0", "0.0.0"));
    }
}
