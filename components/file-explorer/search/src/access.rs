// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Who may see what: a match is shown only to a user who could reach it, checked by the kernel itself with that
//! user's identity (`faccessat2` with `AT_EACCESS`, so modes, groups and ACLs all count), never by imitating its rules.
//! The listing folder needs read and search permission, every folder above it search permission.

use std::io;
use std::path::{Path, PathBuf};

use rustc_hash::FxHashMap;

use crate::index::{EntryId, Index};
use crate::sys;

/// The user asking, from the socket's peer credentials.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Viewer {
    pub uid: u32,
    pub gid: u32,
    pub groups: Vec<u32>,
}

/// Answers already found per user and folder. Cleared when permissions may have changed.
#[derive(Debug, Default)]
pub struct Access {
    folders: FxHashMap<(u32, EntryId, bool), bool>,
    outside: FxHashMap<(u32, PathBuf), bool>,
}

/// While alive, the calling thread checks access as the viewer; dropping it makes the thread root again.
pub struct AsViewer {
    _private: (),
}

impl Drop for AsViewer {
    fn drop(&mut self) {
        // Back to root: if this failed the thread would keep reading as the user, so the helper must not go on.
        sys::act_as(0, 0, &[]).expect("to become root again after a user's check");
    }
}

impl Access {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Forgets every answer: permissions or folders changed.
    pub fn clear(&mut self) {
        self.folders.clear();
        self.outside.clear();
    }

    /// Makes the calling thread check as `viewer` until the guard drops.
    ///
    /// # Errors
    ///
    /// The error of switching credentials.
    pub fn as_viewer(viewer: &Viewer) -> io::Result<AsViewer> {
        sys::act_as(viewer.uid, viewer.gid, &viewer.groups)?;
        Ok(AsViewer { _private: () })
    }

    /// Whether `viewer` may see entry `id`. Call it under [`Access::as_viewer`]; root sees everything.
    pub fn visible(&mut self, index: &Index, viewer: &Viewer, id: EntryId) -> bool {
        if viewer.uid == 0 {
            return true;
        }
        let Some(parent) = index.parent(id) else {
            return self.outside_ok(viewer.uid, &index.path(id));
        };
        let mut at = parent;
        let mut listing = true;
        loop {
            let key = (viewer.uid, at, listing);
            let ok = *self.folders.entry(key).or_insert_with(|| {
                let mode = if listing { libc::R_OK | libc::X_OK } else { libc::X_OK };
                sys::may_access(&index.path(at), mode)
            });
            if !ok {
                return false;
            }
            listing = false;
            match index.parent(at) {
                Some(up) => at = up,
                None => return self.outside_ok(viewer.uid, &index.path(at)),
            }
        }
    }

    /// Search permission on every folder above a volume's root, which the index does not hold.
    fn outside_ok(&mut self, uid: u32, root: &Path) -> bool {
        for folder in root.ancestors().skip(1) {
            let key = (uid, folder.to_path_buf());
            let ok = *self.outside.entry(key).or_insert_with(|| sys::may_access(folder, libc::X_OK));
            if !ok {
                return false;
            }
        }
        true
    }
}
