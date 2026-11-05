// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The Appropriate Legal Notices of GPL-3.0 section 0 that `aurora license` prints: the copyright, the absence of
//! warranty, the right to redistribute, and where the license and the additional terms are.

use std::path::Path;

/// The author attribution NOTICE.md requires every interactive interface to keep (GPL-3.0 section 7(b)).
pub const ATTRIBUTION: &str = "Aurora by xt0n1 (https://github.com/xt0n1-t3ch)";

/// The notices for `version`, naming `license_file` when there is a local copy of the license.
#[must_use]
pub fn notice(version: &str, license_file: Option<&Path>) -> String {
    let where_license = license_file.map_or_else(
        || "https://www.gnu.org/licenses/gpl-3.0.html".to_string(),
        |path| format!("{} (also at https://www.gnu.org/licenses/gpl-3.0.html)", path.display()),
    );
    format!(
        "Aurora {version}, a macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6.\n\
         {ATTRIBUTION}. Copyright (C) 2026 xt0n1.\n\n\
         This program comes with ABSOLUTELY NO WARRANTY. It is free software: you may redistribute it and modify it\n\
         under the GNU General Public License version 3, with the additional terms in NOTICE.md: keep the author\n\
         attribution, and mark modified versions as different from this one.\n\n\
         The license: {where_license}\n"
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_notice_names_the_author_the_copyright_and_the_missing_warranty() {
        let text = notice("1.2.0", None);
        assert!(text.contains(ATTRIBUTION), "{text}");
        assert!(text.contains("Copyright (C) 2026 xt0n1"), "{text}");
        assert!(text.contains("ABSOLUTELY NO WARRANTY"), "{text}");
        assert!(text.contains("Aurora 1.2.0"), "{text}");
    }

    #[test]
    fn the_notice_says_how_to_redistribute_and_where_the_terms_are() {
        let text = notice("1.2.0", None);
        assert!(text.contains("GNU General Public License version 3"), "{text}");
        assert!(text.contains("NOTICE.md"), "{text}");
        assert!(text.contains("https://www.gnu.org/licenses/gpl-3.0.html"), "{text}");
        let local = notice("1.2.0", Some(Path::new("/usr/share/licenses/aurora-theme/LICENSE")));
        assert!(local.contains("/usr/share/licenses/aurora-theme/LICENSE"), "{local}");
    }
}
