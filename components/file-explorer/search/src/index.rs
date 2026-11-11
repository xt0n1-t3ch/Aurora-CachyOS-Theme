// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Every name the helper knows: one tree per volume, kept in step with the disk by batches of re-read folders.
//!
//! Entries live in one vector and link to their parent, first child and siblings, so a move is two link changes and
//! keeps the whole subtree. Names live twice in two arenas: as written (for paths) and case-folded (for matching), each
//! name followed by a NUL, so one SIMD substring search runs over every name at once and a hit can never span two
//! names (a query holds no NUL). Renamed and removed names stay in the arenas until a compaction.

use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};

use rustc_hash::{FxHashMap, FxHashSet};

/// An entry's place in the index. Ids are reused after removal.
pub type EntryId = u32;
const NONE: EntryId = EntryId::MAX;

/// What a name is, from the folder listing's `d_type`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Kind {
    Dir,
    File,
    Link,
    Other,
}

/// One name of a folder listing.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Listed {
    pub name: Vec<u8>,
    pub ino: u64,
    pub kind: Kind,
}

#[derive(Debug, Clone, Copy)]
struct Entry {
    parent: EntryId,
    first_child: EntryId,
    next_sibling: EntryId,
    prev_sibling: EntryId,
    name_off: u32,
    fold_off: u32,
    name_len: u16,
    fold_len: u16,
    volume: u16,
    kind: Kind,
    alive: bool,
    ino: u64,
}

/// A volume: a tree whose root is a folder of the system (a mount point or a btrfs subvolume).
#[derive(Debug, Clone)]
pub struct Volume {
    pub path: PathBuf,
    pub root: EntryId,
}

/// The index. Not thread-safe; the helper's main thread owns it.
#[derive(Debug, Default)]
pub struct Index {
    entries: Vec<Entry>,
    free: Vec<EntryId>,
    names: Vec<u8>,
    folded: Vec<u8>,
    /// (folded offset, entry) in offset order, to map a hit back to its entry.
    fold_table: Vec<(u32, EntryId)>,
    /// Folders by (volume, inode), for the btrfs catch-up and fanotify's folder handles.
    dirs: FxHashMap<(u16, u64), EntryId>,
    /// Folders' modification times in nanoseconds, for the catch-up on filesystems without transaction ids.
    mtimes: FxHashMap<EntryId, i64>,
    volumes: Vec<Volume>,
    live: usize,
    dead_bytes: usize,
}

/// The case-folded form of a name that queries match: ASCII lowercased in place, other UTF-8 lowercased by Unicode
/// rules, bytes that are not UTF-8 left as they are.
#[must_use]
pub fn fold(name: &[u8]) -> Vec<u8> {
    if name.is_ascii() {
        return name.to_ascii_lowercase();
    }
    match std::str::from_utf8(name) {
        Ok(text) => text.to_lowercase().into_bytes(),
        Err(_) => name.to_ascii_lowercase(),
    }
}

impl Index {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Adds a volume rooted at `path`, whose root folder has inode `ino`; returns its number.
    ///
    /// # Panics
    ///
    /// Past 65535 volumes.
    pub fn add_volume(&mut self, path: &Path, ino: u64) -> u16 {
        let volume = u16::try_from(self.volumes.len()).expect("fewer than 65536 volumes");
        let root = self.allocate(NONE, b"", Kind::Dir, ino, volume);
        self.volumes.push(Volume { path: path.to_path_buf(), root });
        volume
    }

    #[must_use]
    pub fn volumes(&self) -> &[Volume] {
        &self.volumes
    }

    /// The number of live entries, volume roots included.
    #[must_use]
    pub fn len(&self) -> usize {
        self.live
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.live == 0
    }

    #[must_use]
    pub fn is_alive(&self, id: EntryId) -> bool {
        self.entries.get(id as usize).is_some_and(|e| e.alive)
    }

    #[must_use]
    pub fn name(&self, id: EntryId) -> &[u8] {
        let e = &self.entries[id as usize];
        &self.names[e.name_off as usize..e.name_off as usize + e.name_len as usize]
    }

    #[must_use]
    pub fn folded_name(&self, id: EntryId) -> &[u8] {
        let e = &self.entries[id as usize];
        &self.folded[e.fold_off as usize..e.fold_off as usize + e.fold_len as usize]
    }

    #[must_use]
    pub fn kind(&self, id: EntryId) -> Kind {
        self.entries[id as usize].kind
    }

    #[must_use]
    pub fn ino(&self, id: EntryId) -> u64 {
        self.entries[id as usize].ino
    }

    #[must_use]
    pub fn volume_of(&self, id: EntryId) -> u16 {
        self.entries[id as usize].volume
    }

    /// The parent folder, or `None` for a volume root.
    #[must_use]
    pub fn parent(&self, id: EntryId) -> Option<EntryId> {
        let parent = self.entries[id as usize].parent;
        (parent != NONE).then_some(parent)
    }

    /// The folder of `volume` with inode `ino`.
    #[must_use]
    pub fn dir_by_ino(&self, volume: u16, ino: u64) -> Option<EntryId> {
        self.dirs.get(&(volume, ino)).copied().filter(|&id| self.is_alive(id))
    }

    /// Every folder of `volume`, for the catch-up that compares modification times.
    pub fn dirs_of(&self, volume: u16) -> impl Iterator<Item = EntryId> + '_ {
        self.dirs.iter().filter(move |((v, _), _)| *v == volume).map(|(_, &id)| id)
    }

    #[must_use]
    pub fn mtime(&self, dir: EntryId) -> Option<i64> {
        self.mtimes.get(&dir).copied()
    }

    pub fn set_mtime(&mut self, dir: EntryId, nanos: i64) {
        self.mtimes.insert(dir, nanos);
    }

    /// Every live entry, volume roots included, in no particular order.
    ///
    /// # Panics
    ///
    /// Never: ids are below 4 billion by construction.
    pub fn live_entries(&self) -> impl Iterator<Item = EntryId> + '_ {
        self.entries
            .iter()
            .enumerate()
            .filter(|(_, e)| e.alive)
            .map(|(id, _)| EntryId::try_from(id).expect("fewer than 4 billion entries"))
    }

    /// The children of a folder, newest first.
    #[must_use]
    pub fn children(&self, dir: EntryId) -> Vec<EntryId> {
        let mut out = Vec::new();
        let mut child = self.entries[dir as usize].first_child;
        while child != NONE {
            out.push(child);
            child = self.entries[child as usize].next_sibling;
        }
        out
    }

    /// The whole path of an entry.
    #[must_use]
    pub fn path(&self, id: EntryId) -> PathBuf {
        let mut parts = Vec::new();
        let mut at = id;
        while let Some(parent) = self.parent(at) {
            parts.push(at);
            at = parent;
        }
        let mut path = self.volumes[self.entries[at as usize].volume as usize].path.clone();
        for &part in parts.iter().rev() {
            path.push(OsStr::from_bytes(self.name(part)));
        }
        path
    }

    /// Whether `id` is `ancestor` or inside it.
    #[must_use]
    pub fn is_within(&self, id: EntryId, ancestor: EntryId) -> bool {
        let mut at = Some(id);
        while let Some(current) = at {
            if current == ancestor {
                return true;
            }
            at = self.parent(current);
        }
        false
    }

    /// Adds a name under `parent`.
    pub fn insert(&mut self, parent: EntryId, name: &[u8], kind: Kind, ino: u64) -> EntryId {
        let volume = self.entries[parent as usize].volume;
        let id = self.allocate(parent, name, kind, ino, volume);
        self.link(id, parent);
        id
    }

    fn allocate(&mut self, parent: EntryId, name: &[u8], kind: Kind, ino: u64, volume: u16) -> EntryId {
        let (name_off, fold_off, name_len, fold_len) = self.push_name(name);
        let entry = Entry {
            parent,
            first_child: NONE,
            next_sibling: NONE,
            prev_sibling: NONE,
            name_off,
            fold_off,
            name_len,
            fold_len,
            volume,
            kind,
            alive: true,
            ino,
        };
        let id = if let Some(id) = self.free.pop() {
            self.entries[id as usize] = entry;
            id
        } else {
            self.entries.push(entry);
            EntryId::try_from(self.entries.len() - 1).expect("fewer than 4 billion entries")
        };
        self.fold_table.push((fold_off, id));
        if kind == Kind::Dir {
            self.dirs.insert((volume, ino), id);
        }
        self.live += 1;
        id
    }

    fn push_name(&mut self, name: &[u8]) -> (u32, u32, u16, u16) {
        let folded = fold(name);
        let name_off = u32::try_from(self.names.len()).expect("names under 4 GiB");
        let fold_off = u32::try_from(self.folded.len()).expect("names under 4 GiB");
        self.names.extend_from_slice(name);
        self.names.push(0);
        self.folded.extend_from_slice(&folded);
        self.folded.push(0);
        (
            name_off,
            fold_off,
            u16::try_from(name.len()).unwrap_or(u16::MAX),
            u16::try_from(folded.len()).unwrap_or(u16::MAX),
        )
    }

    fn link(&mut self, id: EntryId, parent: EntryId) {
        let first = self.entries[parent as usize].first_child;
        let e = &mut self.entries[id as usize];
        e.parent = parent;
        e.prev_sibling = NONE;
        e.next_sibling = first;
        if first != NONE {
            self.entries[first as usize].prev_sibling = id;
        }
        self.entries[parent as usize].first_child = id;
    }

    fn unlink(&mut self, id: EntryId) {
        let Entry { parent, prev_sibling, next_sibling, .. } = self.entries[id as usize];
        if prev_sibling == NONE {
            if parent != NONE {
                self.entries[parent as usize].first_child = next_sibling;
            }
        } else {
            self.entries[prev_sibling as usize].next_sibling = next_sibling;
        }
        if next_sibling != NONE {
            self.entries[next_sibling as usize].prev_sibling = prev_sibling;
        }
        // Unlinked twice (a detached entry that is then removed) must not touch the old parent again.
        let e = &mut self.entries[id as usize];
        e.parent = NONE;
        e.prev_sibling = NONE;
        e.next_sibling = NONE;
    }

    /// Moves `id` under `parent` as `name`, with its whole subtree.
    pub fn attach(&mut self, id: EntryId, parent: EntryId, name: &[u8]) {
        self.unlink(id);
        if self.name(id) != name {
            self.dead_bytes += self.entries[id as usize].name_len as usize + 1;
            let (name_off, fold_off, name_len, fold_len) = self.push_name(name);
            let e = &mut self.entries[id as usize];
            (e.name_off, e.fold_off, e.name_len, e.fold_len) = (name_off, fold_off, name_len, fold_len);
            self.fold_table.push((fold_off, id));
        }
        self.link(id, parent);
    }

    /// Removes an entry and everything under it.
    pub fn remove(&mut self, id: EntryId) {
        self.unlink(id);
        let mut stack = vec![id];
        while let Some(at) = stack.pop() {
            let mut child = self.entries[at as usize].first_child;
            while child != NONE {
                stack.push(child);
                child = self.entries[child as usize].next_sibling;
            }
            let e = self.entries[at as usize];
            if e.kind == Kind::Dir && self.dirs.get(&(e.volume, e.ino)) == Some(&at) {
                self.dirs.remove(&(e.volume, e.ino));
            }
            self.mtimes.remove(&at);
            self.dead_bytes += e.name_len as usize + 1;
            let slot = &mut self.entries[at as usize];
            slot.alive = false;
            slot.first_child = NONE;
            self.free.push(at);
            self.live -= 1;
        }
    }

    /// Brings folders in step with fresh listings of them. All removals come first, so a name that left one folder of
    /// the batch for another is moved with its subtree instead of being dropped and found again. Returns the new
    /// folders, whose contents are still to be read.
    pub fn apply_batch(&mut self, listings: Vec<(EntryId, Vec<Listed>)>) -> Vec<EntryId> {
        let mut detached: FxHashMap<(u16, u64, Kind), Vec<EntryId>> = FxHashMap::default();
        let mut additions: Vec<(EntryId, Listed)> = Vec::new();
        for (dir, listing) in listings {
            if !self.is_alive(dir) || self.kind(dir) != Kind::Dir {
                continue;
            }
            let mut wanted: FxHashMap<&[u8], &Listed> = listing.iter().map(|l| (l.name.as_slice(), l)).collect();
            for child in self.children(dir) {
                let e = self.entries[child as usize];
                match wanted.get(self.name(child)) {
                    Some(l) if l.ino == e.ino && l.kind == e.kind => {
                        let name = self.name(child).to_vec();
                        wanted.remove(name.as_slice());
                    }
                    _ => {
                        self.unlink(child);
                        detached.entry((e.volume, e.ino, e.kind)).or_default().push(child);
                    }
                }
            }
            let new: FxHashSet<&[u8]> = wanted.keys().copied().collect();
            additions.extend(listing.iter().filter(|l| new.contains(l.name.as_slice())).map(|l| (dir, l.clone())));
        }
        let mut to_read = Vec::new();
        for (parent, listed) in additions {
            if !self.is_alive(parent) {
                continue;
            }
            let volume = self.entries[parent as usize].volume;
            let moved = detached.get_mut(&(volume, listed.ino, listed.kind)).and_then(Vec::pop);
            if let Some(id) = moved {
                // A folder moved into its own subtree cannot happen; the kernel refuses that rename.
                self.attach(id, parent, &listed.name);
            } else {
                let id = self.insert(parent, &listed.name, listed.kind, listed.ino);
                if listed.kind == Kind::Dir {
                    to_read.push(id);
                }
            }
        }
        for ids in detached.into_values() {
            for id in ids {
                self.remove(id);
            }
        }
        to_read
    }

    /// The case-folded names, each followed by a NUL, for substring search.
    #[must_use]
    pub fn folded_arena(&self) -> &[u8] {
        &self.folded
    }

    /// The live entry whose folded name holds byte `pos` of the folded arena.
    #[must_use]
    pub fn entry_at(&self, pos: usize) -> Option<EntryId> {
        let pos = u32::try_from(pos).ok()?;
        let row = self.fold_table.partition_point(|&(off, _)| off <= pos).checked_sub(1)?;
        let (off, id) = self.fold_table[row];
        let e = self.entries.get(id as usize)?;
        (e.alive && e.fold_off == off && pos < off + u32::from(e.fold_len)).then_some(id)
    }

    /// Rewrites the arenas without the names of removed and renamed entries once those take over half of them.
    ///
    /// # Panics
    ///
    /// Never: the arenas only shrink.
    pub fn compact_if_wasteful(&mut self) {
        if self.dead_bytes * 2 < self.names.len() {
            return;
        }
        let mut names = Vec::with_capacity(self.names.len() - self.dead_bytes);
        let mut folded = Vec::with_capacity(self.folded.len());
        let mut table = Vec::with_capacity(self.live);
        for id in 0..self.entries.len() {
            let e = self.entries[id];
            if !e.alive {
                continue;
            }
            let name_off = u32::try_from(names.len()).expect("names under 4 GiB");
            let fold_off = u32::try_from(folded.len()).expect("names under 4 GiB");
            names.extend_from_slice(&self.names[e.name_off as usize..=e.name_off as usize + e.name_len as usize]);
            folded.extend_from_slice(&self.folded[e.fold_off as usize..=e.fold_off as usize + e.fold_len as usize]);
            let id = EntryId::try_from(id).expect("fewer than 4 billion entries");
            table.push((fold_off, id));
            let slot = &mut self.entries[id as usize];
            slot.name_off = name_off;
            slot.fold_off = fold_off;
        }
        self.names = names;
        self.folded = folded;
        self.fold_table = table;
        self.dead_bytes = 0;
    }
}

#[cfg(test)]
mod tests;
