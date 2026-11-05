// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora`: installs, updates and removes Aurora, a macOS Tahoe-inspired theme for CachyOS and KDE Plasma 6.
//! It guides the user through the steps and runs the theme's own scripts (`install.sh`, `uninstall.sh`) to do them.

pub mod app;
pub mod download;
pub mod legal;
pub mod logo;
pub mod release;
pub mod run;
pub mod source;
pub mod steps;
pub mod system;
pub mod ui;
