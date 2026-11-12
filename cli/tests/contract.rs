// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The CLI and the rest of the repository must agree: the logos match the menu widget's table, the step lines match
//! what `install.sh` prints, the release files match what `scripts/release.sh` builds, the versions the update
//! compares match the ones `installer/build.sh` stamps the parts with, and the author attribution the CLI shows is the
//! one NOTICE.md requires.

use std::collections::BTreeSet;
use std::fs;
use std::path::PathBuf;

use aurora::legal;
use aurora::logo::{Colors, Logo};
use aurora::mission;
use aurora::steps::{self, Step};
use aurora::system::{self, Match};

fn repo() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("..")
}

fn read(path: &str) -> String {
    fs::read_to_string(repo().join(path)).unwrap_or_else(|error| panic!("reading {path}: {error}"))
}

/// The `{id: "...", name: "...", white: "...", colored: "..."}` rows of tools.js's `logos` table.
fn widget_logos() -> Vec<[String; 4]> {
    let tools = read("components/kali-menu/package/contents/ui/code/tools.js");
    let field = |row: &str, key: &str| -> String {
        let start = row.find(&format!("{key}: \"")).unwrap_or_else(|| panic!("no {key} in {row}")) + key.len() + 3;
        row[start..].split('"').next().expect("a closing quote").to_string()
    };
    tools
        .lines()
        .filter(|line| line.trim_start().starts_with("{id: "))
        .map(|row| [field(row, "id"), field(row, "name"), field(row, "white"), field(row, "colored")])
        .collect()
}

#[test]
fn the_logos_are_the_ones_the_menu_widget_offers_in_the_same_order() {
    let ours: Vec<[String; 4]> = Logo::ALL
        .iter()
        .map(|logo| {
            let colored = if logo.has_colors() { logo.icon(Colors::On) } else { "" };
            [logo.id().into(), logo.name().into(), logo.icon(Colors::Off).into(), colored.into()]
        })
        .collect();
    assert_eq!(ours, widget_logos());
}

#[test]
fn the_default_logo_is_the_one_the_widget_and_the_panel_layout_use() {
    let default = Logo::default().icon(Colors::Off);
    assert!(
        read("components/kali-menu/package/contents/ui/code/tools.js")
            .contains(&format!("defaultIconName = \"{default}\""))
    );
    assert!(read("themes/panels/layout.js").contains(&format!("icon: \"{default}\"")));
}

#[test]
fn every_logo_icon_ships_with_the_theme() {
    for logo in Logo::ALL {
        for colors in [Colors::Off, Colors::On] {
            let icon = logo.icon(colors);
            assert!(repo().join(format!("themes/icons/apps/{icon}.svg")).is_file(), "{icon}.svg is missing");
        }
    }
}

#[test]
fn a_recorded_install_run_reads_as_its_six_steps_and_done() {
    let run = fs::read_to_string(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/install-run.log"))
        .expect("the recorded run");
    let found: Vec<Step> = run.lines().filter_map(steps::parse).collect();
    let titles: Vec<&str> = found.iter().map(|step| step.title.as_str()).collect();
    assert_eq!(
        titles,
        [
            "Packages",
            "Themes",
            "Building the parts for Plasma 6.8.2",
            "Installing into /home/user",
            "System",
            "Look and panels",
            "Done",
        ]
    );
    assert_eq!(found[2].position, Some((3, 6)));
    assert_eq!(found[6].position, None);
}

#[test]
fn install_sh_still_prints_its_steps_the_way_the_cli_reads_them() {
    let script = read("install.sh");
    assert!(script.contains(r#"say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }"#));
    assert!(script.matches("say \"").count() >= 7, "install.sh announces its steps with say");
    // Everything Aurora installs is in the repository: no step downloads another project.
    assert!(script.contains(r#"say "2/6 Themes""#), "the second step checks Aurora's own themes");
    assert!(!script.contains("submodule"), "install.sh fetches no git submodules");
}

#[test]
fn the_release_files_are_named_as_release_sh_builds_them() {
    let script = read("scripts/release.sh");
    assert!(script.contains(r#"name="aurora-$version""#));
    assert!(script.contains("SHA256SUMS"));
    assert_eq!(aurora::release::archive_name("1.2.0"), "aurora-1.2.0.tar.gz");
    assert_eq!(aurora::release::CHECKSUMS, "SHA256SUMS");
}

#[test]
fn the_cli_shows_the_attribution_notice_md_requires() {
    let notice = read("NOTICE.md");
    assert!(
        notice.contains(&format!("\"{}\"", legal::ATTRIBUTION)),
        "NOTICE.md does not require {}",
        legal::ATTRIBUTION
    );
    assert!(legal::ATTRIBUTION.contains(aurora::ui::AUTHOR));
}

#[test]
fn the_animation_speeds_are_the_ones_the_installer_applies() {
    let install = read("installer/install.py");
    let table = install.lines().find(|line| line.starts_with("ANIMATIONS = {")).expect("install.py has ANIMATIONS");
    let want: String = aurora::updates::Speed::ALL
        .iter()
        .map(|speed| format!("\"{}\": \"{}\"", speed.id(), speed.factor()))
        .collect::<Vec<_>>()
        .join(", ");
    assert_eq!(table, format!("ANIMATIONS = {{{want}}}"));
}

/// The files after `install -Dm644 ... ` in root-steps.sh's tune step, and the paths its untune step removes.
fn root_steps_files() -> (Vec<String>, Vec<String>) {
    let script = read("installer/root-steps.sh");
    // Only the tune step: other steps (the search helper's) install files of their own.
    let tune = script.split("\n        tune)").nth(1).and_then(|step| step.split(";;").next()).expect("a tune step");
    let installed = tune
        .lines()
        .filter_map(|line| line.trim().strip_prefix("install -Dm644 "))
        .map(|rest| rest.split_whitespace().last().expect("a target").to_string())
        .collect();
    let untune = script.split("untune)").nth(1).expect("an untune step");
    let removed = untune
        .split("rm -f ")
        .nth(1)
        .expect("untune removes files")
        .split("systemctl")
        .next()
        .expect("then the loader")
        .split_whitespace()
        .filter(|word| word.starts_with('/'))
        .map(str::to_string)
        .collect();
    (installed, removed)
}

#[test]
fn performance_turns_on_and_off_exactly_the_files_root_steps_writes_and_removes() {
    let ours: Vec<String> = aurora::tuning::SETTINGS.iter().map(|setting| setting.file.to_string()).collect();
    let (installed, removed) = root_steps_files();
    assert_eq!(ours, installed, "root-steps.sh tune");
    assert_eq!(ours, removed, "root-steps.sh untune");
    // uninstall.sh's restore script removes the same files (installer/install.py, RESTORE).
    let install = read("installer/install.py");
    for file in &ours {
        assert!(install.contains(file.as_str()), "the restore script leaves {file} behind");
    }
}

/// Each package `stamp_of` in installer/build.sh reads with `version_of`, and whether the stamp keeps only its
/// major.minor version (`| cut -d. -f1,2`).
fn stamped_packages() -> BTreeSet<(String, Match)> {
    let script = read("installer/build.sh");
    let body =
        script.split("stamp_of() {").nth(1).expect("build.sh has stamp_of").split("\n}").next().expect("its end");
    body.split("version_of ")
        .skip(1)
        .map(|rest| {
            let name: String = rest.chars().take_while(|c| c.is_ascii_alphanumeric() || *c == '-').collect();
            let command = rest.split(";;").next().unwrap_or(rest);
            let rule = if command.contains("| cut -d. -f1,2") { Match::Minor } else { Match::Exact };
            (name, rule)
        })
        .collect()
}

// `aurora update` installs a release only when its parts fit (system::fits), so it must compare the same packages, as
// closely as build.sh stamps them and the login script checks the stamps.
#[test]
fn the_update_compares_the_versions_build_sh_stamps_the_parts_with() {
    let ours: BTreeSet<(String, Match)> =
        system::LINKS.iter().map(|link| (link.package.to_string(), link.rule)).collect();
    assert_eq!(ours, stamped_packages());
}

// `aurora mission-center` builds the recipe from a copy of these files and installs the package by its name, so the
// recipe must be whole and name the same package, install script and sources.
#[test]
fn the_mission_center_recipe_is_the_one_the_cli_builds_and_installs() {
    let pkgbuild = read("components/mission-center/PKGBUILD");
    assert!(pkgbuild.contains(&format!("pkgname={}\n", mission::PACKAGE)), "the PKGBUILD builds another package");
    assert!(pkgbuild.contains(&format!("provides=({})", mission::STOCK)), "it must stand in for the distribution's");
    assert!(pkgbuild.contains(&format!("conflicts=({})", mission::STOCK)), "it must replace the distribution's");
    assert!(mission::recipe_version(&pkgbuild).is_some(), "the PKGBUILD has no pkgver and pkgrel");
    for file in mission::RECIPE {
        assert!(repo().join("components/mission-center").join(file).is_file(), "{file} is missing from the recipe");
        if file != "PKGBUILD" && !file.ends_with(".install") {
            assert!(pkgbuild.contains(&format!("        {file}")), "the PKGBUILD's sources leave out {file}");
        }
    }
    assert!(pkgbuild.contains("install=mission-center-improved.install\n"), "the install script is not the recipe's");
}

#[test]
fn aurora_s_icon_theme_is_the_one_the_install_applies() {
    let install = read("installer/install.py");
    assert!(
        install.contains(&format!("ICON_THEME = \"{}\"", aurora::icons::AURORA)),
        "install.py's ICON_THEME is not {}",
        aurora::icons::AURORA
    );
}

#[test]
fn the_repository_aurora_repairs_is_the_one_get_sh_sets_up() {
    let get = read("get.sh");
    let value = |name: &str| -> String {
        get.lines()
            .find_map(|line| line.strip_prefix(&format!("{name}=")))
            .unwrap_or_else(|| panic!("get.sh sets no {name}"))
            .to_string()
    };
    assert_eq!(aurora::repo::SERVER, value("SERVER").replace("$REPO", &value("REPO")));
    assert_eq!(aurora::repo::KEY, value("KEY"));
    assert_eq!(aurora::repo::KEY_FILE, read("packaging/repo/aurora.asc"));
}

#[test]
fn the_key_aurora_carries_has_the_fingerprint_it_trusts() {
    let output = std::process::Command::new("gpg")
        .args(["--show-keys", "--with-colons", &repo().join("packaging/repo/aurora.asc").to_string_lossy()])
        .output()
        .expect("gpg runs");
    let listing = String::from_utf8_lossy(&output.stdout);
    let fingerprint = listing.lines().find_map(|line| line.strip_prefix("fpr:")).map(|rest| rest.trim_matches(':'));
    assert_eq!(fingerprint, Some(aurora::repo::KEY));
}
