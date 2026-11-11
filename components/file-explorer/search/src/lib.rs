// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! File Explorer's whole-disk name search.
//!
//! A root helper that systemd starts on the first connection to its socket keeps an index of every name on the local
//! disks, catches up with what changed while it was off (on btrfs from the subvolume's transaction ids, elsewhere from
//! the folders' change times), follows changes live while File Explorer is in use, answers each query within a
//! frame, and exits when nobody has asked for a while. See docs/search.md.

pub mod btrfs;
#[expect(unsafe_code, reason = "the one module that calls the kernel through libc; each block states why it is sound")]
mod sys;
