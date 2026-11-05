// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The CLI and the rest of the repository must agree: the logos match the menu widget's table, the step lines match
//! what `install.sh` prints, the release files match what `scripts/release.sh` builds, and the author attribution the
//! CLI shows is the one NOTICE.md requires.

use std::fs;
use std::path::PathBuf;

use aurora::legal;
use aurora::logo::{Colors, Logo};
use aurora::steps::{self, Step};

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
