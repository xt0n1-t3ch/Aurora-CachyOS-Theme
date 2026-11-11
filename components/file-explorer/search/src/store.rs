// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The index on disk: written atomically when the helper leaves, read when it starts.
//!
//! Layout, little-endian: the magic `AURSRCH1`, the body's length (u64) and CRC-32 (u32), then the body: the volumes
//! (path, fsid, generation, btrfs flag, root inode) and, per volume, its entries depth-first, each with the position of
//! its parent in the list (`u32::MAX` for the volume's root), kind, inode, folder mtime and name. A file that fails the
//! check, or comes from another layout, is ignored: the helper scans anew.

use std::fs::{self, File};
use std::io::{self, Write};
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};

use crate::index::{EntryId, Index, Kind};

const MAGIC: &[u8; 8] = b"AURSRCH1";
const NO_PARENT: u32 = u32::MAX;
const NO_MTIME: i64 = i64::MIN;

/// Writes `index` to `path` through a temporary file, flushed and renamed over the old one, so a crash leaves the
/// old index or the new one, never half of one.
///
/// # Errors
///
/// The I/O error of writing, flushing or renaming.
pub fn save(index: &Index, path: &Path) -> io::Result<()> {
    let body = encode(index);
    let mut bytes = Vec::with_capacity(body.len() + 20);
    bytes.extend_from_slice(MAGIC);
    bytes.extend_from_slice(&(body.len() as u64).to_le_bytes());
    bytes.extend_from_slice(&crc32fast::hash(&body).to_le_bytes());
    bytes.extend_from_slice(&body);
    let temporary = path.with_extension("new");
    {
        let mut file = File::create(&temporary)?;
        file.write_all(&bytes)?;
        file.sync_all()?;
    }
    fs::rename(&temporary, path)?;
    if let Some(folder) = path.parent() {
        File::open(folder)?.sync_all()?;
    }
    Ok(())
}

/// Reads an index written by [`save`]; `None` when there is none or it does not pass the check.
#[must_use]
pub fn load(path: &Path) -> Option<Index> {
    let bytes = fs::read(path).ok()?;
    if bytes.len() < 20 || &bytes[..8] != MAGIC {
        return None;
    }
    let length = usize::try_from(u64::from_le_bytes(bytes[8..16].try_into().ok()?)).ok()?;
    let crc = u32::from_le_bytes(bytes[16..20].try_into().ok()?);
    let body = bytes.get(20..20 + length)?;
    if crc32fast::hash(body) != crc {
        return None;
    }
    decode(body)
}

fn encode(index: &Index) -> Vec<u8> {
    let mut out = Vec::new();
    let volumes = index.volumes();
    put_u32(&mut out, u32::try_from(volumes.len()).unwrap_or(0));
    for v in volumes {
        put_bytes(&mut out, v.path.as_os_str().as_bytes());
        out.extend_from_slice(&v.fsid.to_le_bytes());
        out.extend_from_slice(&v.generation.to_le_bytes());
        out.push(u8::from(v.btrfs));
        out.extend_from_slice(&index.ino(v.root).to_le_bytes());
        out.extend_from_slice(&index.mtime(v.root).unwrap_or(NO_MTIME).to_le_bytes());
        // Depth-first: a parent is always written before its children.
        let mut rows: Vec<(u32, EntryId)> = Vec::new();
        let mut stack: Vec<(u32, EntryId)> = index.children(v.root).into_iter().map(|c| (NO_PARENT, c)).collect();
        while let Some((parent, id)) = stack.pop() {
            let position = u32::try_from(rows.len()).unwrap_or(NO_PARENT);
            rows.push((parent, id));
            stack.extend(index.children(id).into_iter().map(|c| (position, c)));
        }
        put_u32(&mut out, u32::try_from(rows.len()).unwrap_or(0));
        for (parent, id) in rows {
            put_u32(&mut out, parent);
            out.push(kind_byte(index.kind(id)));
            out.extend_from_slice(&index.ino(id).to_le_bytes());
            out.extend_from_slice(&index.mtime(id).unwrap_or(NO_MTIME).to_le_bytes());
            put_bytes(&mut out, index.name(id));
        }
    }
    out
}

fn decode(body: &[u8]) -> Option<Index> {
    let mut r = Reader { bytes: body, at: 0 };
    let mut index = Index::new();
    for _ in 0..r.u32()? {
        let path = PathBuf::from(std::ffi::OsStr::from_bytes(r.bytes()?));
        let fsid = r.u64()?;
        let generation = r.u64()?;
        let btrfs = r.u8()? != 0;
        let root_ino = r.u64()?;
        let root_mtime = r.i64()?;
        let v = index.add_volume(&path, root_ino);
        let volume = index.volume_mut(v);
        volume.fsid = fsid;
        volume.generation = generation;
        volume.btrfs = btrfs;
        let root = volume.root;
        if root_mtime != NO_MTIME {
            index.set_mtime(root, root_mtime);
        }
        let count = r.u32()? as usize;
        let mut ids: Vec<EntryId> = Vec::with_capacity(count);
        for _ in 0..count {
            let parent = r.u32()?;
            let kind = kind_of(r.u8()?)?;
            let ino = r.u64()?;
            let mtime = r.i64()?;
            let name = r.bytes()?;
            let parent = if parent == NO_PARENT { root } else { *ids.get(parent as usize)? };
            let id = index.insert(parent, name, kind, ino);
            if mtime != NO_MTIME {
                index.set_mtime(id, mtime);
            }
            ids.push(id);
        }
    }
    (r.at == body.len()).then_some(index)
}

fn kind_byte(kind: Kind) -> u8 {
    match kind {
        Kind::Dir => 0,
        Kind::File => 1,
        Kind::Link => 2,
        Kind::Other => 3,
    }
}

fn kind_of(byte: u8) -> Option<Kind> {
    Some(match byte {
        0 => Kind::Dir,
        1 => Kind::File,
        2 => Kind::Link,
        3 => Kind::Other,
        _ => return None,
    })
}

fn put_u32(out: &mut Vec<u8>, value: u32) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn put_bytes(out: &mut Vec<u8>, bytes: &[u8]) {
    put_u32(out, u32::try_from(bytes.len()).unwrap_or(0));
    out.extend_from_slice(bytes);
}

struct Reader<'a> {
    bytes: &'a [u8],
    at: usize,
}

impl<'a> Reader<'a> {
    fn take(&mut self, n: usize) -> Option<&'a [u8]> {
        let slice = self.bytes.get(self.at..self.at.checked_add(n)?)?;
        self.at += n;
        Some(slice)
    }
    fn u8(&mut self) -> Option<u8> {
        Some(self.take(1)?[0])
    }
    fn u32(&mut self) -> Option<u32> {
        Some(u32::from_le_bytes(self.take(4)?.try_into().ok()?))
    }
    fn u64(&mut self) -> Option<u64> {
        Some(u64::from_le_bytes(self.take(8)?.try_into().ok()?))
    }
    fn i64(&mut self) -> Option<i64> {
        Some(i64::from_le_bytes(self.take(8)?.try_into().ok()?))
    }
    fn bytes(&mut self) -> Option<&'a [u8]> {
        let n = self.u32()? as usize;
        self.take(n)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> Index {
        let mut index = Index::new();
        let v = index.add_volume(Path::new("/home"), 256);
        index.volume_mut(v).generation = 77;
        index.volume_mut(v).btrfs = true;
        let root = index.volumes()[0].root;
        let a = index.insert(root, b"alex", Kind::Dir, 300);
        index.set_mtime(a, 12345);
        index.insert(a, "Número 1.txt".as_bytes(), Kind::File, 301);
        index.insert(a, b"link", Kind::Link, 302);
        index
    }

    fn listing(index: &Index) -> Vec<(String, Kind, u64, Option<i64>)> {
        let mut out: Vec<_> = index
            .live_entries()
            .map(|id| (index.path(id).display().to_string(), index.kind(id), index.ino(id), index.mtime(id)))
            .collect();
        out.sort_by(|a, b| a.0.cmp(&b.0));
        out
    }

    #[test]
    fn an_index_comes_back_as_it_was_saved() {
        let path = std::env::temp_dir().join(format!("aurora-search-store-{}", std::process::id()));
        let index = sample();
        save(&index, &path).unwrap();
        let back = load(&path).unwrap();
        assert_eq!(listing(&back), listing(&index));
        assert_eq!(back.volumes()[0].generation, 77);
        assert!(back.volumes()[0].btrfs);
        fs::remove_file(&path).unwrap();
    }

    #[test]
    fn a_damaged_or_cut_file_is_not_an_index() {
        let path = std::env::temp_dir().join(format!("aurora-search-damaged-{}", std::process::id()));
        save(&sample(), &path).unwrap();
        let mut bytes = fs::read(&path).unwrap();
        let last = bytes.len() - 1;
        bytes[last] ^= 0xFF;
        fs::write(&path, &bytes).unwrap();
        assert!(load(&path).is_none());
        fs::write(&path, &bytes[..bytes.len() / 2]).unwrap();
        assert!(load(&path).is_none());
        fs::write(&path, b"AURSRCH0 an older layout").unwrap();
        assert!(load(&path).is_none());
        fs::remove_file(&path).unwrap();
        assert!(load(&path).is_none());
    }
}
