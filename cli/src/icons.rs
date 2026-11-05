// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora icons`: the desktop's icon theme: Aurora's own, or any other installed one, applied at once through
//! Plasma's `plasma-changeicons`.

use std::fs;
use std::path::{Path, PathBuf};

use anyhow::{Result, bail};
use console::Style;

use crate::app::Places;
use crate::run;

/// Aurora's icon theme, which the install applies.
pub const AURORA: &str = "Aurora-dark";

/// An icon theme the desktop can use.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IconTheme {
    /// Its folder's name, which Plasma's settings store.
    pub id: String,
    /// The name it gives itself.
    pub name: String,
}

/// The theme an `index.theme` describes, when it is an icon theme a person would pick: it lists icon folders
/// (cursor themes list none) and is not `Hidden` (the bases other themes inherit from are).
#[must_use]
pub fn parse_index(id: &str, text: &str) -> Option<IconTheme> {
    let mut in_theme = false;
    let (mut name, mut directories, mut hidden) = (None, false, false);
    for line in text.lines().map(str::trim) {
        if line.starts_with('[') {
            in_theme = line == "[Icon Theme]";
            continue;
        }
        if !in_theme {
            continue;
        }
        match line.split_once('=') {
            Some(("Name", value)) => name = Some(value.trim().to_string()),
            Some(("Directories", value)) => directories = !value.trim().is_empty(),
            Some(("Hidden", value)) => hidden = value.trim().eq_ignore_ascii_case("true"),
            _ => {}
        }
    }
    (directories && !hidden).then(|| IconTheme { id: id.to_string(), name: name.unwrap_or_else(|| id.to_string()) })
}

/// The icon themes in `folders`, the first folder winning a name found twice, sorted by name; Aurora's comes first.
#[must_use]
pub fn installed_in(folders: &[PathBuf]) -> Vec<IconTheme> {
    let mut themes: Vec<IconTheme> = Vec::new();
    for folder in folders {
        let Ok(entries) = fs::read_dir(folder) else { continue };
        for entry in entries.filter_map(Result::ok) {
            let id = entry.file_name().to_string_lossy().into_owned();
            if themes.iter().any(|theme| theme.id == id) {
                continue;
            }
            if let Some(theme) =
                fs::read_to_string(entry.path().join("index.theme")).ok().and_then(|text| parse_index(&id, &text))
            {
                themes.push(theme);
            }
        }
    }
    themes.sort_by_key(|theme| (theme.id != AURORA, theme.name.to_lowercase()));
    themes
}

/// Where icon themes are installed: the user's own first, then the system's.
fn folders(places: &Places) -> Vec<PathBuf> {
    vec![places.home.join(".local/share/icons"), places.home.join(".icons"), PathBuf::from("/usr/share/icons")]
}

/// The icon theme Plasma uses now.
fn current() -> String {
    run::capture("kreadconfig6", &["--file", "kdeglobals", "--group", "Icons", "--key", "Theme"])
        .map(|out| out.trim().to_string())
        .unwrap_or_default()
}

/// How a theme shows in the list and the answer.
fn label(theme: &IconTheme) -> String {
    if theme.id == AURORA { format!("{} (Aurora's)", theme.name) } else { theme.name.clone() }
}

/// `aurora icons`: applies `choice` (an icon theme's folder name, or `aurora`), or asks.
///
/// # Errors
/// When the theme is not installed, the user cancels, or Plasma cannot apply it.
pub fn icons(places: &Places, choice: Option<&str>) -> Result<()> {
    let themes = installed_in(&folders(places));
    let now = current();
    let theme = if let Some(wanted) = choice {
        let wanted = if wanted.eq_ignore_ascii_case("aurora") { AURORA } else { wanted };
        let Some(theme) = themes.iter().find(|theme| theme.id.eq_ignore_ascii_case(wanted)) else {
            let ids: Vec<&str> = themes.iter().map(|theme| theme.id.as_str()).collect();
            bail!("{wanted} is not an installed icon theme; these are: {}", ids.join(", "));
        };
        theme.clone()
    } else {
        cliclack::intro(Style::new().bold().apply_to("Icons"))?;
        let items: Vec<(usize, String, &str)> = themes
            .iter()
            .enumerate()
            .map(|(at, theme)| (at, label(theme), if theme.id == now { "in use" } else { "" }))
            .collect();
        let start = themes.iter().position(|theme| theme.id == now).unwrap_or(0);
        let picked =
            cliclack::select("Which icons should the desktop use?").items(&items).initial_value(start).interact()?;
        themes[picked].clone()
    };
    apply(&theme.id)?;
    let line = format!("The desktop uses the {} icons.", label(&theme));
    if choice.is_none() {
        cliclack::outro(line)?;
    } else {
        println!("{line}");
    }
    Ok(())
}

/// Applies the icon theme `id` to Plasma and every KDE app now, as System Settings does.
fn apply(id: &str) -> Result<()> {
    let tool = Path::new("/usr/lib/plasma-changeicons");
    let program = if tool.exists() { tool.to_string_lossy().into_owned() } else { "plasma-changeicons".to_string() };
    // plasma-changeicons answers with an error when the theme is already the current one; the result is read back.
    let _ = run::capture(&program, &[id]);
    if current() != id {
        bail!("Plasma did not take the {id} icons");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::fs;

    use super::{AURORA, IconTheme, installed_in, parse_index};

    #[test]
    fn an_icon_theme_names_itself_and_lists_its_folders() {
        let text = "[Icon Theme]\nName=Breeze Dark\nInherits=breeze\nDirectories=actions/16,apps/48\n";
        assert_eq!(
            parse_index("breeze-dark", text),
            Some(IconTheme { id: "breeze-dark".into(), name: "Breeze Dark".into() })
        );
    }

    #[test]
    fn cursor_themes_and_hidden_bases_are_not_offered() {
        assert_eq!(parse_index("breeze_cursors", "[Icon Theme]\nName=Breeze Cursors\nInherits=x\n"), None);
        assert_eq!(parse_index("Aurora-base", "[Icon Theme]\nName=Aurora-base\nHidden=true\nDirectories=a/16\n"), None);
    }

    #[test]
    fn keys_of_other_groups_do_not_count() {
        let text = "[Icon Theme]\nName=Win11\n[actions/16]\nDirectories=ignored\n";
        assert_eq!(parse_index("Win11", text), None, "Directories belongs to [Icon Theme]");
    }

    #[test]
    fn aurora_s_icons_come_first_then_the_rest_by_name_and_a_user_copy_wins() {
        let root = std::env::temp_dir().join(format!("aurora-icons-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        let (user, system) = (root.join("user"), root.join("system"));
        for (folder, id, name) in [
            (&user, "Win11", "Win11 (mine)"),
            (&system, "Win11", "Win11"),
            (&system, "breeze", "Breeze"),
            (&user, AURORA, "Aurora Dark"),
        ] {
            fs::create_dir_all(folder.join(id)).expect("a theme folder");
            fs::write(folder.join(id).join("index.theme"), format!("[Icon Theme]\nName={name}\nDirectories=apps/48\n"))
                .expect("index.theme");
        }
        let names: Vec<String> = installed_in(&[user, system]).into_iter().map(|theme| theme.name).collect();
        assert_eq!(names, ["Aurora Dark", "Breeze", "Win11 (mine)"]);
        fs::remove_dir_all(&root).expect("cleanup");
    }
}
