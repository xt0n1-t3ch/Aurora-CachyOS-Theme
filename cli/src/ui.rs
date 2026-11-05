// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! How the CLI looks: Aurora's wordmark in the colors of an aurora, and a prompt theme in the same colors.

use std::fmt::Write as _;

use cliclack::{Theme, ThemeState};
use console::{Color, Style};

/// The aurora the wordmark runs through, left to right: mint, sky, violet, pink.
const AURORA: [(u8, u8, u8); 4] = [(79, 242, 176), (56, 189, 248), (167, 139, 250), (244, 114, 182)];

/// Who made Aurora; the banner credits them under the description.
pub const AUTHOR: &str = "xt0n1";

/// "AURORA" in half-block letters: four terminal rows, each holding two rows of the 6 by 8 pixel letters.
const WORDMARK: [&str; 4] = [
    "▄█▀▀█▄ ██  ██ ██▀▀█▄ ▄█▀▀█▄ ██▀▀█▄ ▄█▀▀█▄",
    "██  ██ ██  ██ ██▄▄█▀ ██  ██ ██▄▄█▀ ██  ██",
    "██▀▀██ ██  ██ ██▀█▄  ██  ██ ██▀█▄  ██▀▀██",
    "██  ██ ▀█▄▄█▀ ██  ██ ▀█▄▄█▀ ██  ██ ██  ██",
];

fn mix(from: (u8, u8, u8), to: (u8, u8, u8), t: f32) -> (u8, u8, u8) {
    let channel = |a: u8, b: u8| -> u8 {
        let value = f32::from(a) + (f32::from(b) - f32::from(a)) * t;
        // Rounded and clamped to 0..=255 first, so the cast cannot lose anything.
        #[expect(clippy::cast_possible_truncation, clippy::cast_sign_loss, reason = "clamped to 0..=255")]
        let byte = value.round().clamp(0.0, 255.0) as u8;
        byte
    };
    (channel(from.0, to.0), channel(from.1, to.1), channel(from.2, to.2))
}

/// The aurora color at `t`, from 0 (left) to 1 (right).
#[must_use]
pub fn aurora_at(t: f32) -> (u8, u8, u8) {
    // Three stretches between the four colors.
    let stops = 3.0_f32;
    let at = (t.clamp(0.0, 1.0) * stops).min(stops - 0.0001);
    #[expect(clippy::cast_possible_truncation, clippy::cast_sign_loss, reason = "at is in 0..stops")]
    let index = at.floor() as usize;
    mix(AURORA[index], AURORA[index + 1], at - at.floor())
}

fn paint(text: &str, rgb: (u8, u8, u8)) -> String {
    Style::new().fg(Color::TrueColor(rgb.0, rgb.1, rgb.2)).bold().apply_to(text).to_string()
}

/// `text` in bold, each letter a step further along the aurora.
fn gradient(text: &str) -> String {
    let count = f32::from(u16::try_from(text.chars().count()).unwrap_or(u16::MAX).max(1));
    (0_u16..)
        .zip(text.chars())
        .map(|(index, letter)| paint(&letter.to_string(), aurora_at(f32::from(index) / count)))
        .collect()
}

/// The wordmark with the aurora running through it, then what Aurora is, who made it and its version.
#[must_use]
pub fn banner(version: &str) -> String {
    let width = f32::from(u16::try_from(WORDMARK[0].chars().count()).unwrap_or(u16::MAX).max(1));
    let mut out = String::from("\n");
    for row in WORDMARK {
        out.push_str("   ");
        for (column, letter) in (0_u16..).zip(row.chars()) {
            let piece = letter.to_string();
            out.push_str(&if letter == ' ' { piece } else { paint(&piece, aurora_at(f32::from(column) / width)) });
        }
        out.push('\n');
    }
    let tagline = Style::new().white().apply_to("A macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6");
    let credit = format!("{} {}", Style::new().white().apply_to("Developed by"), gradient(AUTHOR));
    let _ =
        write!(out, "\n   {tagline}\n   {credit}   {}\n", Style::new().dim().apply_to(format!("version {version}")));
    out
}

/// The prompts' colors: violet bars, mint for what is done, sky for what is asked.
pub struct AuroraTheme;

fn rgb(color: (u8, u8, u8)) -> Style {
    Style::new().fg(Color::TrueColor(color.0, color.1, color.2))
}

impl Theme for AuroraTheme {
    fn bar_color(&self, state: &ThemeState) -> Style {
        match state {
            ThemeState::Active => rgb(AURORA[2]),
            ThemeState::Cancel => Style::new().red(),
            ThemeState::Error(_) => Style::new().yellow(),
            ThemeState::Submit => rgb(AURORA[2]).dim(),
        }
    }

    fn state_symbol_color(&self, state: &ThemeState) -> Style {
        match state {
            ThemeState::Submit => rgb(AURORA[0]),
            ThemeState::Active => rgb(AURORA[1]),
            _ => self.bar_color(state),
        }
    }

    fn radio_symbol(&self, state: &ThemeState, selected: bool) -> String {
        match state {
            ThemeState::Active if selected => rgb(AURORA[0]).apply_to("●").to_string(),
            ThemeState::Active => Style::new().dim().apply_to("○").to_string(),
            _ => String::new(),
        }
    }

    fn spinner_chars(&self) -> String {
        "◒◐◓◑◇".into()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_aurora_starts_mint_and_ends_pink() {
        assert_eq!(aurora_at(0.0), AURORA[0]);
        assert_eq!(aurora_at(1.0), mix(AURORA[2], AURORA[3], 0.9999));
        assert_eq!(aurora_at(-3.0), AURORA[0]);
    }

    #[test]
    fn halfway_between_two_stops_is_their_mix() {
        // Three stretches between four colors: t = 1/6 is halfway from mint to sky.
        assert_eq!(aurora_at(1.0 / 6.0), mix(AURORA[0], AURORA[1], 0.5));
    }

    #[test]
    fn the_banner_spells_the_name_and_says_what_it_is() {
        let plain = crate::steps::strip_ansi(&banner("1.2.0"));
        assert!(WORDMARK.iter().all(|row| plain.contains(row)));
        assert!(plain.contains("A macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6"));
        assert!(plain.contains("version 1.2.0"));
    }

    #[test]
    fn the_wordmark_is_four_rows_of_one_width() {
        // Four terminal rows of half blocks draw letters eight pixels tall; equal widths keep the colours in columns.
        assert_eq!(WORDMARK.len(), 4);
        let width = WORDMARK[0].chars().count();
        assert!(WORDMARK.iter().all(|row| row.chars().count() == width), "{WORDMARK:?}");
    }

    #[test]
    fn the_banner_credits_its_author() {
        let plain = crate::steps::strip_ansi(&banner("1.2.0"));
        assert!(plain.contains("Developed by xt0n1"), "{plain}");
    }
}
