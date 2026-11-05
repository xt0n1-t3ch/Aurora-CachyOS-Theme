// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! What this machine runs: the Plasma, KWin, Qt and KDecoration versions (from pacman), the session type, and
//! which of Aurora's parts the login script set aside because they were built for other versions.

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;
use std::process::Command;

/// The packages whose versions decide what Aurora's parts are built for.
pub const PACKAGES: [&str; 4] = ["plasma-workspace", "kwin", "qt6-base", "kdecoration"];

/// Package name to upstream version, from `pacman -Q` output (`plasma-workspace 6.8.2-1.1` gives `6.8.2`).
#[must_use]
pub fn parse_pacman_q(output: &str) -> BTreeMap<String, String> {
    output
        .lines()
        .filter_map(|line| {
            let (name, version) = line.trim().split_once(' ')?;
            let upstream = version.rsplit_once('-').map_or(version, |(upstream, _release)| upstream);
            let upstream = upstream.split_once(':').map_or(upstream, |(_epoch, rest)| rest);
            Some((name.to_string(), upstream.to_string()))
        })
        .collect()
}

/// The installed versions of [`PACKAGES`]; empty when pacman is missing.
#[must_use]
pub fn installed_versions() -> BTreeMap<String, String> {
    Command::new("pacman")
        .arg("-Q")
        .args(PACKAGES)
        .output()
        .map(|output| parse_pacman_q(&String::from_utf8_lossy(&output.stdout)))
        .unwrap_or_default()
}

/// The plugins the login script set aside (renamed `*.paused`) because they were built for other versions.
#[must_use]
pub fn paused_parts(plugins: &Path) -> Vec<String> {
    let mut paused = Vec::new();
    let mut folders = vec![plugins.to_path_buf()];
    while let Some(folder) = folders.pop() {
        let Ok(entries) = fs::read_dir(&folder) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                folders.push(path);
            } else if let Some(name) =
                path.file_name().and_then(|name| name.to_str()).and_then(|name| name.strip_suffix(".paused"))
            {
                paused.push(name.to_string());
            }
        }
    }
    paused.sort();
    paused
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pacman_versions_lose_their_package_release_and_epoch() {
        let recorded = include_str!("../tests/fixtures/pacman-q.txt");
        let versions = parse_pacman_q(recorded);
        assert_eq!(versions["plasma-workspace"], "6.8.2");
        assert_eq!(versions["qt6-base"], "6.12.0");
        assert_eq!(parse_pacman_q("foo 1:2.3-4")["foo"], "2.3");
    }

    #[test]
    fn a_paused_plugin_is_found_in_any_plugin_folder() {
        let dir = std::env::temp_dir().join(format!("aurora-cli-paused-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(dir.join("org.kde.kdecoration3")).expect("create the folder");
        fs::write(dir.join("org.kde.kdecoration3/org.aurora.decoration.so.paused"), b"").expect("pause one");
        fs::write(dir.join("kept.so"), b"").expect("keep one");
        assert_eq!(paused_parts(&dir), ["org.aurora.decoration.so"]);
        fs::remove_dir_all(dir).expect("clean up");
    }
}
