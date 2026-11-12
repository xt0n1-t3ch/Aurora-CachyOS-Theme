// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The helper's settings, from `/etc/aurora/search.conf`: `key = value` lines, `#` for comments.
//!
//! `idle_minutes` (default 5) is how long the helper stays without a client; `exclude` (repeatable) names a folder
//! never to index; `root` (repeatable) indexes only that folder instead of every local mount, for tests and the
//! sandbox. Unknown keys and bad values are ignored, so a typo never stops the search.

use std::path::{Path, PathBuf};
use std::time::Duration;

const DEFAULT_IDLE: Duration = Duration::from_secs(5 * 60);

/// Folders never indexed: virtual filesystems, caches that change all the time, and the helper's own state.
const ALWAYS_EXCLUDED: &[&str] = &["/proc", "/sys", "/dev", "/run", "/tmp", "/var/tmp", "/var/lib/aurora/search"];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Config {
    pub idle: Duration,
    pub exclude: Vec<PathBuf>,
    /// When set, only these folders are indexed, each as a volume, instead of every local mount (tests, the sandbox).
    pub roots: Vec<PathBuf>,
}

impl Default for Config {
    fn default() -> Self {
        Self { idle: DEFAULT_IDLE, exclude: ALWAYS_EXCLUDED.iter().map(PathBuf::from).collect(), roots: Vec::new() }
    }
}

impl Config {
    /// The settings of a file; the defaults when it is missing.
    #[must_use]
    pub fn load(path: &Path) -> Self {
        std::fs::read_to_string(path).map(|text| Self::parse(&text)).unwrap_or_default()
    }

    #[must_use]
    pub fn parse(text: &str) -> Self {
        let mut config = Self::default();
        for line in text.lines() {
            let line = line.split('#').next().unwrap_or("").trim();
            let Some((key, value)) = line.split_once('=') else { continue };
            let value = value.trim().trim_matches('"');
            match key.trim() {
                "idle_minutes" => {
                    if let Ok(minutes) = value.parse::<u64>() {
                        config.idle = Duration::from_secs(minutes.clamp(1, 24 * 60) * 60);
                    }
                }
                // For tests, which cannot wait minutes.
                "idle_seconds" => {
                    if let Ok(seconds) = value.parse::<u64>() {
                        config.idle = Duration::from_secs(seconds.max(1));
                    }
                }
                "exclude" if value.starts_with('/') => config.exclude.push(PathBuf::from(value)),
                "root" if value.starts_with('/') => config.roots.push(PathBuf::from(value)),
                _ => {}
            }
        }
        config
    }

    #[must_use]
    pub fn excludes(&self, path: &Path) -> bool {
        self.exclude.iter().any(|e| path.starts_with(e)) || crate::mounts::is_snapshot(path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn settings_are_read_and_mistakes_are_ignored() {
        let config = Config::parse(
            "# mine\nidle_minutes = 10\nexclude = \"/srv/backups\"\nexclude = relative\nidle_minutes = soon\n",
        );
        assert_eq!(config.idle, Duration::from_secs(600));
        assert!(config.excludes(Path::new("/srv/backups/2026")));
        assert!(config.excludes(Path::new("/proc/1")));
        assert!(config.excludes(Path::new("/.snapshots/12/snapshot")));
        assert!(!config.excludes(Path::new("/home/alex")));
        assert_eq!(Config::parse(""), Config::default());
    }
}
