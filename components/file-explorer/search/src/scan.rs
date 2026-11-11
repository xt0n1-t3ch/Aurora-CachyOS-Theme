// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Reading folders: one listing at a time for a refresh, many at once in parallel waves for a scan.

use std::fs;
use std::io;
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::{DirEntryExt, FileTypeExt, MetadataExt};
use std::path::{Path, PathBuf};

use crate::index::{EntryId, Index, Kind, Listed};

/// How many folders a wave reads at once, at most.
const WAVE: usize = 4096;

/// A folder's names, its device and its modification time.
#[derive(Debug)]
pub struct Listing {
    pub entries: Vec<Listed>,
    pub dev: u64,
    /// Nanoseconds since the epoch.
    pub mtime: i64,
}

/// Reads a folder: its own status, then its names with their inode and type from the listing itself (`d_type`), so
/// no name is stat'ed but those whose type the filesystem does not report.
///
/// # Errors
///
/// The error of `lstat` or `opendir`: the folder is gone, or is no folder.
pub fn read_folder(path: &Path) -> io::Result<Listing> {
    let meta = fs::symlink_metadata(path)?;
    if !meta.is_dir() {
        return Err(io::Error::from(io::ErrorKind::NotADirectory));
    }
    let mut entries = Vec::new();
    for entry in fs::read_dir(path)? {
        let Ok(entry) = entry else { continue };
        let Ok(kind) = entry.file_type() else { continue };
        let kind = if kind.is_dir() {
            Kind::Dir
        } else if kind.is_file() {
            Kind::File
        } else if kind.is_symlink() {
            Kind::Link
        } else if kind.is_fifo() || kind.is_socket() || kind.is_block_device() || kind.is_char_device() {
            Kind::Other
        } else {
            continue;
        };
        entries.push(Listed { name: entry.file_name().as_bytes().to_vec(), ino: entry.ino(), kind });
    }
    Ok(Listing { entries, dev: meta.dev(), mtime: meta.mtime() * 1_000_000_000 + meta.mtime_nsec() })
}

/// What a scan met at the edge of its volume: folders on another device (mount points and nested btrfs
/// subvolumes), for the caller to decide about.
#[derive(Debug, Default)]
pub struct Edges {
    pub other_devices: Vec<(EntryId, PathBuf)>,
}

/// Reads `dirs` and every folder under them that the reading finds new, wave by wave, each wave's folders read in
/// parallel at idle priority and applied in one batch. `skip` names paths not to read (exclusions). Folders on
/// another device than their volume are left empty and returned as edges.
#[must_use]
pub fn scan(index: &mut Index, dirs: Vec<EntryId>, skip: &(dyn Fn(&Path) -> bool + Sync), threads: usize) -> Edges {
    let mut edges = Edges::default();
    let mut pending = dirs;
    while !pending.is_empty() {
        let take = pending.len().min(WAVE);
        let wave: Vec<(EntryId, PathBuf)> = pending.drain(..take).map(|id| (id, index.path(id))).collect();
        let listings = read_wave(&wave, skip, threads);
        let mut batch = Vec::with_capacity(listings.len());
        for ((id, path), listing) in wave.into_iter().zip(listings) {
            let Some(listing) = listing else { continue };
            let volume_dev = index.volumes()[index.volume_of(id) as usize].dev;
            if volume_dev != 0 && listing.dev != volume_dev {
                edges.other_devices.push((id, path));
                continue;
            }
            index.set_mtime(id, listing.mtime);
            batch.push((id, listing.entries));
        }
        pending.extend(index.apply_batch(batch));
    }
    index.compact_if_wasteful();
    edges
}

/// Reads every folder of a wave, `threads` at a time; `None` for a folder that is skipped or could not be read.
#[must_use]
pub fn read_wave(
    wave: &[(EntryId, PathBuf)],
    skip: &(dyn Fn(&Path) -> bool + Sync),
    threads: usize,
) -> Vec<Option<Listing>> {
    let read = |path: &PathBuf| if skip(path) { None } else { read_folder(path).ok() };
    let threads = threads.clamp(1, 16);
    if threads == 1 || wave.len() < 64 {
        return wave.iter().map(|(_, path)| read(path)).collect();
    }
    let chunk = wave.len().div_ceil(threads);
    std::thread::scope(|scope| {
        let handles: Vec<_> = wave
            .chunks(chunk)
            .map(|part| {
                scope.spawn(move || {
                    crate::sys::lower_this_thread();
                    part.iter().map(|(_, path)| read(path)).collect::<Vec<_>>()
                })
            })
            .collect();
        handles.into_iter().flat_map(|h| h.join().unwrap_or_default()).collect()
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_scan_reads_a_whole_tree_and_finds_its_names() {
        let root = std::env::temp_dir().join(format!("aurora-search-scan-{}", std::process::id()));
        fs::create_dir_all(root.join("a/b/c")).unwrap();
        fs::write(root.join("a/b/c/deep.txt"), b"x").unwrap();
        fs::write(root.join("top.txt"), b"x").unwrap();
        std::os::unix::fs::symlink("top.txt", root.join("link")).unwrap();
        let mut index = Index::new();
        let meta = fs::metadata(&root).unwrap();
        let v = index.add_volume(&root, meta.ino());
        index.volume_mut(v).dev = meta.dev();
        let root_id = index.volumes()[0].root;
        let edges = scan(&mut index, vec![root_id], &|_| false, 4);
        assert_eq!(edges.other_devices, []);
        // The root, a, b, c, deep.txt, top.txt and the link.
        assert_eq!(index.len(), 7);
        let deep = index.live_entries().find(|&id| index.name(id) == b"deep.txt").unwrap();
        assert_eq!(index.path(deep), root.join("a/b/c/deep.txt"));
        let link = index.live_entries().find(|&id| index.name(id) == b"link").unwrap();
        assert_eq!(index.kind(link), Kind::Link);
        fs::remove_dir_all(&root).unwrap();
    }
}
