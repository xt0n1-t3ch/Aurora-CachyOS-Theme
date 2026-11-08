// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Mission Center's settings through gsettings: the values each key takes, the settings `aurora mission-center
//! settings` offers by name, and the checked writes.

use anyhow::{Context, Result, anyhow, bail};

use crate::run;

/// Mission Center's `GSettings` schema.
pub const SCHEMA: &str = "io.missioncenter.MissionCenter";

/// The values a setting takes, from `gsettings range SCHEMA KEY`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Range {
    /// `type b`: true or false.
    Flag,
    /// `range i 10 600` and the like: a whole number from the first to the second.
    Between(i64, i64),
    /// `type i`, `type t` and the other whole-number types.
    Number,
    /// `enum` and its names, one per line.
    OneOf(Vec<String>),
    /// `type s`: text.
    Text,
    /// Any other type: the value goes to gsettings as written, and gsettings checks it.
    Other,
}

impl Range {
    /// Reads `gsettings range` output.
    #[must_use]
    pub fn parse(output: &str) -> Range {
        let mut lines = output.lines();
        let head: Vec<&str> = lines.next().unwrap_or_default().split_whitespace().collect();
        match head[..] {
            ["type", "b"] => Range::Flag,
            ["type", "s"] => Range::Text,
            ["type", "y" | "n" | "q" | "i" | "u" | "x" | "t"] => Range::Number,
            ["range", _, min, max] => match (min.parse(), max.parse()) {
                (Ok(min), Ok(max)) => Range::Between(min, max),
                _ => Range::Other,
            },
            ["enum"] => Range::OneOf(
                lines.map(|line| line.trim().trim_matches('\'').to_string()).filter(|name| !name.is_empty()).collect(),
            ),
            _ => Range::Other,
        }
    }

    /// `value` written the way `gsettings set` takes it.
    ///
    /// # Errors
    /// When this range does not allow `value`; the message names `key` and what it takes.
    pub fn check(&self, key: &str, value: &str) -> Result<String> {
        let value = value.trim();
        let number = || value.parse::<i64>().map_err(|_| anyhow!("{key} takes a whole number, not `{value}`"));
        match self {
            Range::Flag => match value {
                "true" | "on" | "yes" => Ok("true".into()),
                "false" | "off" | "no" => Ok("false".into()),
                _ => bail!("{key} takes true or false, not `{value}`"),
            },
            Range::Between(min, max) => {
                let number = number()?;
                if number < *min || number > *max {
                    bail!("{key} takes {min} to {max}, not {number}");
                }
                Ok(number.to_string())
            }
            Range::Number => Ok(number()?.to_string()),
            Range::OneOf(names) if names.iter().any(|name| name == value) => Ok(quote(value)),
            Range::OneOf(names) => bail!("{key} takes {}, not `{value}`", names.join(", ")),
            Range::Text => Ok(quote(value)),
            Range::Other => Ok(value.to_string()),
        }
    }
}

/// `text` as a `GVariant` string: in single quotes, with backslashes and quotes escaped.
fn quote(text: &str) -> String {
    format!("'{}'", text.replace('\\', "\\\\").replace('\'', "\\'"))
}

/// A setting the menu offers, with its name as people read it and the values it offers.
pub struct Offer {
    pub key: &'static str,
    pub name: &'static str,
    pub choices: &'static [(&'static str, &'static str)],
}

const ON_OFF: &[(&str, &str)] = &[("true", "On"), ("false", "Off")];

/// The settings the menu offers; `--set` reaches every key of the schema.
pub const OFFERS: [Offer; 10] = [
    Offer {
        key: "app-update-interval-u64",
        name: "Refresh",
        // In steps of 50 ms (the schema's summary): 20 is a second.
        choices: &[
            ("10", "Every half second"),
            ("20", "Every second"),
            ("40", "Every 2 seconds"),
            ("100", "Every 5 seconds"),
        ],
    },
    Offer {
        key: "performance-page-data-points",
        name: "Points on each chart",
        choices: &[("30", "30"), ("60", "60"), ("120", "120"), ("300", "300")],
    },
    Offer {
        key: "window-interface-style",
        name: "Look",
        choices: &[("system", "Like the system"), ("light", "Light"), ("dark", "Dark")],
    },
    Offer { key: "performance-smooth-graphs", name: "Smooth graphs", choices: ON_OFF },
    Offer { key: "performance-sliding-graphs", name: "Sliding graphs", choices: ON_OFF },
    Offer { key: "apps-page-merged-process-stats", name: "One row per app", choices: ON_OFF },
    Offer { key: "apps-page-heat-tint", name: "Heat tint on busy values", choices: ON_OFF },
    Offer { key: "performance-page-kernel-times", name: "Kernel time in the CPU graphs", choices: ON_OFF },
    Offer { key: "performance-show-fans", name: "Fans in the sidebar", choices: ON_OFF },
    Offer { key: "performance-show-batteries", name: "Batteries in the sidebar", choices: ON_OFF },
];

/// The `(value, label, hint)` items of a cliclack select for `offer`.
#[must_use]
pub fn items(offer: &Offer) -> Vec<(&'static str, &'static str, &'static str)> {
    offer.choices.iter().map(|(value, label)| (*value, *label, "")).collect()
}

/// The label of the offer's choice that `value` (as gsettings prints it) is, or `value` itself.
#[must_use]
pub fn shown(offer: &Offer, value: &str) -> String {
    let bare = value.trim_start_matches("uint64 ").trim_matches('\'');
    offer
        .choices
        .iter()
        .find(|(choice, _)| *choice == bare)
        .map_or_else(|| bare.to_string(), |(_, label)| (*label).to_string())
}

/// What `key` takes, as gsettings reports it.
///
/// # Errors
/// When the schema has no such key.
pub fn range(key: &str) -> Result<Range> {
    let output = run::capture("gsettings", &["range", SCHEMA, key]).with_context(|| format!("no setting {key}"))?;
    Ok(Range::parse(&output))
}

/// `value` checked against what `key` takes, written the way `gsettings set` takes it.
///
/// # Errors
/// When the schema has no such key or the value is outside its range.
pub fn checked(key: &str, value: &str) -> Result<String> {
    range(key)?.check(key, value)
}

/// Sets `key` to a value that [`checked`] returned.
///
/// # Errors
/// When gsettings fails.
pub fn write(key: &str, value: &str) -> Result<()> {
    run::capture("gsettings", &["set", SCHEMA, key, value]).map(drop)
}

/// Sets `key` to `value` after checking it.
///
/// # Errors
/// When the value is not one `key` takes, or gsettings fails.
pub fn set(key: &str, value: &str) -> Result<()> {
    write(key, &checked(key, value)?)
}

/// Puts `key` back to its default.
///
/// # Errors
/// When gsettings fails.
pub fn reset(key: &str) -> Result<()> {
    run::capture("gsettings", &["reset", SCHEMA, key]).map(drop)
}

/// The value of `key` as gsettings prints it, or nothing.
#[must_use]
pub fn current(key: &str) -> String {
    run::capture("gsettings", &["get", SCHEMA, key]).map(|value| value.trim().to_string()).unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    // gsettings range, recorded on CachyOS 2026-11-07 with mission-center-improved 1.2.0.r52.g039b197-2.
    #[test]
    fn a_setting_s_range_is_read_from_gsettings() {
        assert_eq!(Range::parse("type b\n"), Range::Flag);
        assert_eq!(Range::parse("type s\n"), Range::Text);
        assert_eq!(Range::parse("range t 10 200\n"), Range::Between(10, 200));
        assert_eq!(
            Range::parse("enum\n'system'\n'light'\n'dark'\n"),
            Range::OneOf(vec!["system".into(), "light".into(), "dark".into()])
        );
        assert_eq!(Range::parse("type i\n"), Range::Number);
        assert_eq!(Range::parse("type as\n"), Range::Other);
    }

    #[test]
    fn a_value_is_checked_against_its_range_and_written_for_gsettings() {
        let refresh = Range::Between(10, 200);
        assert_eq!(refresh.check("app-update-interval-u64", "20").expect("in range"), "20");
        assert!(refresh.check("app-update-interval-u64", "5").is_err(), "below the range");
        assert!(refresh.check("app-update-interval-u64", "201").is_err(), "above the range");
        assert!(refresh.check("app-update-interval-u64", "fast").is_err(), "not a number");
        assert_eq!(Range::Number.check("performance-page-cpu-graph", "2").expect("a number"), "2");
        assert_eq!(Range::Flag.check("performance-smooth-graphs", "on").expect("a switch"), "true");
        assert_eq!(Range::Flag.check("performance-smooth-graphs", "false").expect("a switch"), "false");
        assert!(Range::Flag.check("performance-smooth-graphs", "maybe").is_err());
        let look = Range::OneOf(vec!["system".into(), "light".into(), "dark".into()]);
        assert_eq!(look.check("window-interface-style", "dark").expect("a name"), "'dark'");
        assert!(look.check("window-interface-style", "blue").is_err());
        assert_eq!(Range::Text.check("performance-sidebar-order", "cpu;it's").expect("text"), "'cpu;it\\'s'");
        assert_eq!(Range::Other.check("some-list", "['a']").expect("as written"), "['a']");
    }

    #[test]
    fn the_menu_names_the_current_value_by_its_label() {
        assert_eq!(shown(&OFFERS[0], "uint64 20"), "Every second");
        assert_eq!(shown(&OFFERS[2], "'dark'"), "Dark");
        assert_eq!(shown(&OFFERS[0], "uint64 33"), "33");
        assert_eq!(items(&OFFERS[3]), [("true", "On", ""), ("false", "Off", "")]);
    }

    // Every value the menu offers must pass the schema's own range (recorded above), or choosing it would fail.
    #[test]
    fn every_value_the_menu_offers_is_one_the_schema_takes() {
        let ranges = [
            ("app-update-interval-u64", Range::Between(10, 200)),
            ("performance-page-data-points", Range::Between(10, 600)),
            ("window-interface-style", Range::OneOf(vec!["system".into(), "light".into(), "dark".into()])),
        ];
        for offer in &OFFERS {
            let range =
                ranges.iter().find(|(key, _)| *key == offer.key).map_or(Range::Flag, |(_, range)| range.clone());
            for (value, _) in offer.choices {
                assert!(range.check(offer.key, value).is_ok(), "{} = {value}", offer.key);
            }
        }
    }
}
