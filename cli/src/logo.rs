// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The logos the menu bar's menu can show, as the menu widget's settings page offers them
//! (`components/kali-menu/package/contents/ui/code/tools.js`; `tests/contract.rs` keeps the two tables equal).

use std::fmt;
use std::str::FromStr;

/// One of the logos Aurora ships for the left end of the menu bar.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub enum Logo {
    /// CachyOS's black-and-white logo is the default: it matches the rest of the menu bar.
    #[default]
    CachyOs,
    Arch,
    Apple,
    Kali,
}

/// Whether a logo that has its own colors shows them, or the black-and-white version that follows the bar's text color.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub enum Colors {
    #[default]
    Off,
    On,
}

impl Logo {
    /// Every logo, in the order the settings page and the CLI list them.
    pub const ALL: [Logo; 4] = [Logo::CachyOs, Logo::Arch, Logo::Apple, Logo::Kali];

    /// The short name the CLI takes: `aurora logo arch`.
    #[must_use]
    pub fn id(self) -> &'static str {
        match self {
            Logo::CachyOs => "cachyos",
            Logo::Arch => "arch",
            Logo::Apple => "apple",
            Logo::Kali => "kali",
        }
    }

    #[must_use]
    pub fn name(self) -> &'static str {
        match self {
            Logo::CachyOs => "CachyOS",
            Logo::Arch => "Arch Linux",
            Logo::Apple => "Apple",
            Logo::Kali => "Kali Linux",
        }
    }

    #[must_use]
    pub fn has_colors(self) -> bool {
        matches!(self, Logo::CachyOs | Logo::Arch)
    }

    /// The icon name the menu widget stores. A logo without colors gives its black-and-white icon either way.
    #[must_use]
    pub fn icon(self, colors: Colors) -> &'static str {
        match (self, colors) {
            (Logo::CachyOs, Colors::On) => "aurora-logo-cachyos",
            (Logo::CachyOs, Colors::Off) => "aurora-logo-cachyos-symbolic",
            (Logo::Arch, Colors::On) => "aurora-logo-arch",
            (Logo::Arch, Colors::Off) => "aurora-logo-arch-symbolic",
            (Logo::Apple, _) => "aurora-logo-apple-symbolic",
            (Logo::Kali, _) => "aurora-logo-kali-symbolic",
        }
    }

    /// The logo and colors a stored icon name shows, or `None` for an icon the user picked themselves.
    #[must_use]
    pub fn from_icon(icon: &str) -> Option<(Logo, Colors)> {
        Logo::ALL.into_iter().find_map(|logo| {
            if logo.has_colors() && icon == logo.icon(Colors::On) {
                Some((logo, Colors::On))
            } else if icon == logo.icon(Colors::Off) {
                Some((logo, Colors::Off))
            } else {
                None
            }
        })
    }
}

impl fmt::Display for Logo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.name())
    }
}

/// A logo name the CLI does not know.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseLogoError(String);

impl fmt::Display for ParseLogoError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let known: Vec<&str> = Logo::ALL.iter().map(|logo| logo.id()).collect();
        write!(f, "unknown logo \"{}\" (one of: {})", self.0, known.join(", "))
    }
}

impl std::error::Error for ParseLogoError {}

impl FromStr for Logo {
    type Err = ParseLogoError;

    fn from_str(text: &str) -> Result<Self, Self::Err> {
        let wanted = text.trim().to_lowercase();
        let aliases = |logo: Logo| -> &'static [&'static str] {
            match logo {
                Logo::CachyOs => &["cachyos", "cachy"],
                Logo::Arch => &["arch", "archlinux", "arch-linux"],
                Logo::Apple => &["apple", "mac", "macos"],
                Logo::Kali => &["kali", "kalilinux", "kali-linux"],
            }
        };
        Logo::ALL
            .into_iter()
            .find(|&logo| aliases(logo).contains(&wanted.as_str()))
            .ok_or_else(|| ParseLogoError(text.to_string()))
    }
}

/// The Plasma script that shows `icon` on every menu widget of every panel, through
/// `qdbus6 org.kde.plasmashell /PlasmaShell evaluateScript`. It prints how many widgets it changed.
#[must_use]
pub fn plasma_script(icon: &str) -> String {
    // The icon is one of ours or an icon name the user typed: keep it to the characters icon names use.
    let icon: String = icon.chars().filter(|c| c.is_ascii_alphanumeric() || "-_.".contains(*c)).collect();
    format!(
        "var changed = 0; var ps = panels();\n\
         for (var i = 0; i < ps.length; i++) {{ var ws = ps[i].widgets('org.aurora.logomenu');\n  \
         for (var j = 0; j < ws.length; j++) {{ ws[j].currentConfigGroup = ['Advanced']; ws[j].writeConfig('icon', '{icon}'); changed++; }} }}\n\
         print(changed);"
    )
}

/// One entry of the install's single question: a logo, in black and white or in its colors, with what the list
/// shows for it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Choice {
    pub logo: Logo,
    pub colors: Colors,
    pub label: String,
}

/// Every logo the menu bar can show, as one list: each logo in black and white, then the ones that have colors in
/// their colors. The default comes first.
#[must_use]
pub fn choices() -> Vec<Choice> {
    let plain = Logo::ALL.map(|logo| Choice { logo, colors: Colors::Off, label: logo.name().to_string() });
    let colored = Logo::ALL.into_iter().filter(|logo| logo.has_colors()).map(|logo| Choice {
        logo,
        colors: Colors::On,
        label: format!("{} in its colors", logo.name()),
    });
    plain.into_iter().chain(colored).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn one_list_offers_every_logo_and_the_colors_of_those_that_have_them() {
        let choices = choices();
        assert_eq!(choices.first().map(|c| (c.logo, c.colors)), Some((Logo::default(), Colors::Off)));
        for logo in Logo::ALL {
            assert!(choices.iter().any(|c| c.logo == logo && c.colors == Colors::Off), "{logo:?} in black and white");
            assert_eq!(
                choices.iter().any(|c| c.logo == logo && c.colors == Colors::On),
                logo.has_colors(),
                "{logo:?} in its colors"
            );
        }
        let mut seen = std::collections::HashSet::new();
        assert!(choices.iter().all(|c| seen.insert(c.label.clone())), "every label is different");
        assert_eq!(
            choices.iter().find(|c| c.colors == Colors::On).map(|c| c.label.as_str()),
            Some("CachyOS in its colors")
        );
    }

    #[test]
    fn a_logo_is_named_by_its_short_name_or_a_common_alias() {
        assert_eq!("cachyos".parse(), Ok(Logo::CachyOs));
        assert_eq!("Arch Linux".replace(' ', "-").parse(), Ok(Logo::Arch));
        assert_eq!(" MacOS ".parse(), Ok(Logo::Apple));
        assert_eq!("kali".parse(), Ok(Logo::Kali));
    }

    #[test]
    fn an_unknown_logo_name_lists_the_known_ones() {
        let error = "windows".parse::<Logo>().expect_err("windows is not a logo");
        assert_eq!(error.to_string(), "unknown logo \"windows\" (one of: cachyos, arch, apple, kali)");
    }

    #[test]
    fn a_logo_without_colors_gives_the_same_icon_either_way() {
        assert_eq!(Logo::Apple.icon(Colors::On), Logo::Apple.icon(Colors::Off));
        assert_ne!(Logo::CachyOs.icon(Colors::On), Logo::CachyOs.icon(Colors::Off));
    }

    #[test]
    fn every_icon_maps_back_to_its_logo_and_colors() {
        for logo in Logo::ALL {
            assert_eq!(Logo::from_icon(logo.icon(Colors::Off)), Some((logo, Colors::Off)));
            if logo.has_colors() {
                assert_eq!(Logo::from_icon(logo.icon(Colors::On)), Some((logo, Colors::On)));
            }
        }
        assert_eq!(Logo::from_icon("start-here-kde-symbolic"), None);
    }

    #[test]
    fn the_plasma_script_sets_the_icon_on_every_menu_widget() {
        let script = plasma_script("aurora-logo-arch");
        assert!(script.contains("widgets('org.aurora.logomenu')"));
        assert!(script.contains("writeConfig('icon', 'aurora-logo-arch')"));
    }

    #[test]
    fn the_plasma_script_cannot_be_broken_out_of_by_the_icon_name() {
        let script = plasma_script("x'); evil(); ('");
        assert!(script.contains("writeConfig('icon', 'xevil')"), "{script}");
    }
}
