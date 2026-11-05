// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Where Aurora's files are: a git checkout, the aurora-theme package's copy in `/usr/share/aurora`, or an unpacked release
//! archive. The CLI runs that copy's `install.sh` and `uninstall.sh`.

use std::fs;
use std::path::{Path, PathBuf};

/// Where the aurora-theme package puts Aurora's files.
pub const PACKAGED: &str = "/usr/share/aurora";

/// How this copy of Aurora was obtained, which decides how it updates.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    /// A git clone: `git pull`.
    Checkout,
    /// The AUR package: the package manager updates it.
    Package,
    /// An unpacked release archive: the next release is downloaded.
    Archive,
}

/// Where the aurora-theme package puts its files and its program.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Package {
    pub root: PathBuf,
    pub program: PathBuf,
}

impl Package {
    /// The package as pacman installs it.
    #[must_use]
    pub fn system() -> Package {
        Package { root: PathBuf::from(PACKAGED), program: PathBuf::from("/usr/bin/aurora") }
    }
}

/// One copy of Aurora's files.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Source {
    pub root: PathBuf,
    pub kind: Kind,
}

impl Source {
    /// The copy at `root`, if `root` holds Aurora (an `install.sh` and a `VERSION` file).
    #[must_use]
    pub fn at(root: &Path) -> Option<Source> {
        if !(root.join("install.sh").is_file() && root.join("VERSION").is_file()) {
            return None;
        }
        let root = fs::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
        let kind = if root == Path::new(PACKAGED) {
            Kind::Package
        } else if root.join(".git").exists() {
            Kind::Checkout
        } else {
            Kind::Archive
        };
        Some(Source { root, kind })
    }

    /// The copy to use, in this order: `AURORA_SOURCE`; the package's own copy when this program is the package's;
    /// the one installed on this account (the `source` link install.py leaves in `~/.local/share/aurora`); the
    /// checkout this program was built in; the package's.
    #[must_use]
    pub fn find(explicit: Option<&Path>, home: &Path, exe: &Path) -> Option<Source> {
        Source::find_with(explicit, home, exe, &Package::system())
    }

    /// [`Source::find`] with the package's places given, so tests need no package installed.
    #[must_use]
    pub fn find_with(explicit: Option<&Path>, home: &Path, exe: &Path, package: &Package) -> Option<Source> {
        let installed = home.join(".local/share/aurora/source");
        let around_exe = exe.ancestors().skip(1).find(|dir| Source::at(dir).is_some()).map(Path::to_path_buf);
        // The package's program installs the package's files, even over an install from an older checkout.
        let own_package = (exe == package.program).then(|| package.root.clone());
        explicit
            .map(Path::to_path_buf)
            .into_iter()
            .chain(own_package)
            .chain([installed])
            .chain(around_exe)
            .chain([package.root.clone()])
            .find_map(|candidate| Source::at(&candidate))
    }

    /// The version in its `VERSION` file.
    #[must_use]
    pub fn version(&self) -> String {
        fs::read_to_string(self.root.join("VERSION")).map(|text| text.trim().to_string()).unwrap_or_default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn aurora_copy(dir: &Path) {
        fs::create_dir_all(dir).expect("create the copy");
        fs::write(dir.join("install.sh"), "#!/bin/sh\n").expect("write install.sh");
        fs::write(dir.join("VERSION"), "1.2.0\n").expect("write VERSION");
    }

    fn temp(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("aurora-cli-test-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        dir
    }

    #[test]
    fn a_folder_with_a_git_repository_is_a_checkout_and_one_without_is_an_archive() {
        let dir = temp("kinds");
        aurora_copy(&dir.join("archive"));
        aurora_copy(&dir.join("checkout"));
        fs::create_dir_all(dir.join("checkout/.git")).expect("create .git");
        assert_eq!(Source::at(&dir.join("archive")).map(|s| s.kind), Some(Kind::Archive));
        assert_eq!(Source::at(&dir.join("checkout")).map(|s| s.kind), Some(Kind::Checkout));
        assert_eq!(Source::at(&dir), None);
        assert_eq!(Source::at(&dir.join("archive")).map(|s| s.version()).as_deref(), Some("1.2.0"));
        fs::remove_dir_all(dir).expect("clean up");
    }

    #[test]
    fn the_installed_copy_wins_over_the_one_around_the_program() {
        let dir = temp("find");
        let installed = dir.join("installed");
        let checkout = dir.join("checkout");
        aurora_copy(&installed);
        aurora_copy(&checkout);
        let home = dir.join("home");
        fs::create_dir_all(home.join(".local/share/aurora")).expect("create the state folder");
        let exe = checkout.join("build/cli/aurora");
        assert_eq!(
            Source::find(None, &home, &exe).map(|s| s.root),
            Some(fs::canonicalize(&checkout).expect("canonical"))
        );
        std::os::unix::fs::symlink(&installed, home.join(".local/share/aurora/source")).expect("link the source");
        assert_eq!(
            Source::find(None, &home, &exe).map(|s| s.root),
            Some(fs::canonicalize(&installed).expect("canonical"))
        );
        assert_eq!(
            Source::find(Some(&checkout), &home, &exe).map(|s| s.root),
            Some(fs::canonicalize(&checkout).expect("canonical"))
        );
        fs::remove_dir_all(dir).expect("clean up");
    }

    #[test]
    fn the_package_s_program_installs_the_package_s_copy_over_an_older_installed_one() {
        let dir = temp("package");
        let (packaged, older) = (dir.join("usr/share/aurora"), dir.join("old-checkout"));
        aurora_copy(&packaged);
        aurora_copy(&older);
        let home = dir.join("home");
        fs::create_dir_all(home.join(".local/share/aurora")).expect("create the state folder");
        std::os::unix::fs::symlink(&older, home.join(".local/share/aurora/source")).expect("link the older copy");
        let package = Package { root: packaged.clone(), program: dir.join("usr/bin/aurora") };
        let canonical = |path: &Path| fs::canonicalize(path).expect("canonical");
        assert_eq!(
            Source::find_with(None, &home, &package.program, &package).map(|s| s.root),
            Some(canonical(&packaged)),
            "the package's program"
        );
        assert_eq!(
            Source::find_with(None, &home, &home.join(".local/bin/aurora"), &package).map(|s| s.root),
            Some(canonical(&older)),
            "a copy of the program in the home folder keeps the installed copy"
        );
        assert_eq!(
            Source::find_with(Some(&older), &home, &package.program, &package).map(|s| s.root),
            Some(canonical(&older)),
            "AURORA_SOURCE still wins"
        );
        fs::remove_dir_all(dir).expect("clean up");
    }
}
