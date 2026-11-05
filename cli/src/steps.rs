// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Reads the output of `install.sh`: its steps are lines like `==> 3/6 Building the parts for Plasma 6.8.2`,
//! printed bold (`say` in install.sh), and the last one is `==> Done`.

/// One step `install.sh` announced.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Step {
    /// Its place among the steps, `(3, 6)`, when the script numbered it.
    pub position: Option<(u32, u32)>,
    pub title: String,
}

/// The step a line of output announces, if it announces one.
#[must_use]
pub fn parse(line: &str) -> Option<Step> {
    let plain = strip_ansi(line);
    let rest = plain.trim().strip_prefix("==> ")?;
    let numbered = rest.split_once(' ').and_then(|(head, title)| {
        let (number, total) = head.split_once('/')?;
        Some(((number.parse().ok()?, total.parse().ok()?), title))
    });
    Some(match numbered {
        Some((position, title)) => Step { position: Some(position), title: title.trim().to_string() },
        None => Step { position: None, title: rest.trim().to_string() },
    })
}

/// The text of a line without its terminal escape sequences (colors, bold).
#[must_use]
pub fn strip_ansi(line: &str) -> String {
    let mut plain = String::with_capacity(line.len());
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        if c == '\u{1b}' && chars.peek() == Some(&'[') {
            chars.next();
            // A CSI sequence ends with a byte in @..~.
            for end in chars.by_ref() {
                if ('@'..='~').contains(&end) {
                    break;
                }
            }
        } else {
            plain.push(c);
        }
    }
    plain
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_bold_numbered_line_is_a_step_with_its_place() {
        let step = parse("\u{1b}[1m==> 4/6 Installing into /home/user\u{1b}[0m").expect("a step");
        assert_eq!(step, Step { position: Some((4, 6)), title: "Installing into /home/user".into() });
    }

    #[test]
    fn the_last_line_is_a_step_without_a_number() {
        assert_eq!(parse("==> Done"), Some(Step { position: None, title: "Done".into() }));
    }

    #[test]
    fn ordinary_output_is_not_a_step() {
        assert_eq!(parse("all parts built in /home/user/build"), None);
        assert_eq!(parse("+ kbuildsycoca6"), None);
        assert_eq!(parse(""), None);
    }

    #[test]
    fn escape_sequences_are_dropped_and_text_kept() {
        assert_eq!(strip_ansi("\u{1b}[31merror:\u{1b}[0m no space"), "error: no space");
        assert_eq!(strip_ansi("plain"), "plain");
    }
}
