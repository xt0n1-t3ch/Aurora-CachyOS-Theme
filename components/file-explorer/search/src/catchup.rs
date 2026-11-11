// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Finding the folders that changed while the helper was not running, per volume: from btrfs transaction ids, or
//! from folders' modification times elsewhere. Either way the result is a list of folders to read again.

use std::fs::{self, File};
use std::io;
use std::os::unix::fs::MetadataExt;

use crate::btrfs;
use crate::index::{EntryId, Index, Kind};

/// The folders of a btrfs volume that changed after its stored generation. Commits first, since uncommitted changes
/// are not in the tree yet; returns the folders and the transaction to store as the new generation. Folders created
/// since are not among them: their parents are, and reading those finds them.
///
/// # Errors
///
/// The ioctl errors of [`btrfs::commit`] and [`btrfs::changed_inodes`].
pub fn changed_on_btrfs(index: &Index, volume: u16, root: &File) -> io::Result<(Vec<EntryId>, u64)> {
    let since = index.volumes()[volume as usize].generation;
    let committed = btrfs::commit(root)?;
    let mut dirs: Vec<EntryId> = btrfs::changed_inodes(root, since)?
        .into_iter()
        .filter(|c| c.is_dir)
        .filter_map(|c| index.dir_by_ino(volume, c.ino))
        .collect();
    dirs.sort_unstable();
    dirs.dedup();
    Ok((dirs, committed))
}

/// The folders of a volume whose modification time differs from the stored one: the rule `updatedb` follows, since
/// adding, removing or renaming an entry sets its folder's time. A folder that is gone sends its parent instead,
/// whose listing drops it.
#[must_use]
pub fn changed_by_mtime(index: &Index, volume: u16) -> Vec<EntryId> {
    let mut dirs = Vec::new();
    for dir in index.dirs_of(volume) {
        let stored = index.mtime(dir);
        match fs::symlink_metadata(index.path(dir)) {
            Ok(meta) if meta.is_dir() => {
                if stored != Some(meta.mtime() * 1_000_000_000 + meta.mtime_nsec()) {
                    dirs.push(dir);
                }
            }
            _ => {
                if let Some(parent) = index.parent(dir) {
                    dirs.push(parent);
                }
            }
        }
    }
    dirs.retain(|&d| index.kind(d) == Kind::Dir);
    dirs.sort_unstable();
    dirs.dedup();
    dirs
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::scan::scan;
    use std::path::PathBuf;

    fn temp(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("aurora-search-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        root
    }

    #[test]
    fn changed_folders_are_found_by_their_time_and_a_refresh_brings_the_index_in_step() {
        let root = temp("mtime");
        for dir in ["a/b", "c", "quiet"] {
            fs::create_dir_all(root.join(dir)).unwrap();
        }
        fs::write(root.join("c/old"), b"").unwrap();
        let mut index = Index::new();
        let meta = fs::metadata(&root).unwrap();
        let v = index.add_volume(&root, meta.ino());
        index.volume_mut(v).dev = meta.dev();
        let root_id = index.volumes()[0].root;
        let _ = scan(&mut index, vec![root_id], &|_| false, 1);
        // Some filesystems keep times in whole seconds; wait past one so the change shows.
        std::thread::sleep(std::time::Duration::from_millis(1100));
        fs::write(root.join("a/b/new"), b"").unwrap();
        fs::remove_file(root.join("c/old")).unwrap();
        fs::rename(root.join("a/b"), root.join("moved")).unwrap();
        let changed: Vec<String> =
            changed_by_mtime(&index, v).iter().map(|&d| index.path(d).display().to_string()).collect();
        let at = |p: &str| root.join(p).display().to_string();
        assert!(changed.contains(&at("c")));
        assert!(changed.contains(&at("a")));
        assert!(changed.contains(&root.display().to_string()));
        assert!(!changed.contains(&at("quiet")));
        // Reading them again: the moved folder keeps its entry and gains the new file.
        let dirs = changed_by_mtime(&index, v);
        let listings: Vec<_> = dirs
            .iter()
            .filter_map(|&d| crate::scan::read_folder(&index.path(d)).ok().map(|l| (d, l.entries)))
            .collect();
        let new = index.apply_batch(listings);
        let _ = scan(&mut index, new, &|_| false, 1);
        let names: Vec<String> = index.live_entries().map(|id| index.path(id).display().to_string()).collect();
        assert!(names.contains(&at("moved")));
        assert!(!names.contains(&at("a/b")));
        assert!(!names.contains(&at("c/old")));
        fs::remove_dir_all(&root).unwrap();
    }
}
