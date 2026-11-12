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
use std::io::{self, BufReader, BufWriter, Read, Seek, SeekFrom, Write};
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};

use crate::index::{EntryId, Index, Kind};

const MAGIC: &[u8; 8] = b"AURSRCH1";
const NO_PARENT: u32 = u32::MAX;
const NO_MTIME: i64 = i64::MIN;
/// The read and write buffer: big enough that the disk sees long transfers, small next to the index.
const BUFFER: usize = 1 << 20;
/// The fewest bytes a row can take: parent, kind, inode, mtime and a name's length.
const ROW_MIN: u64 = 4 + 1 + 8 + 8 + 4;

/// Writes `index` to `path` through a temporary file, flushed and renamed over the old one, so a crash leaves the
/// old index or the new one, never half of one. The body streams to the file as it is encoded; the length and CRC
/// are written into the header at the end, so saving holds no copy of the index in memory.
///
/// # Errors
///
/// The I/O error of writing, flushing or renaming.
pub fn save(index: &Index, path: &Path) -> io::Result<()> {
    let temporary = path.with_extension("new");
    {
        let mut out = BufWriter::with_capacity(BUFFER, File::create(&temporary)?);
        out.write_all(MAGIC)?;
        out.write_all(&[0; 12])?;
        let mut body = Checked { inner: &mut out, crc: crc32fast::Hasher::new(), length: 0 };
        encode(index, &mut body)?;
        let (length, crc) = (body.length, body.crc.finalize());
        let mut file = out.into_inner().map_err(io::IntoInnerError::into_error)?;
        file.seek(SeekFrom::Start(MAGIC.len() as u64))?;
        file.write_all(&length.to_le_bytes())?;
        file.write_all(&crc.to_le_bytes())?;
        file.sync_all()?;
    }
    fs::rename(&temporary, path)?;
    if let Some(folder) = path.parent() {
        File::open(folder)?.sync_all()?;
    }
    Ok(())
}

/// Reads an index written by [`save`]; `None` when there is none or it does not pass the check. The file is read in
/// a stream while the index is built, and the index is dropped when the CRC at the end does not match.
#[must_use]
pub fn load(path: &Path) -> Option<Index> {
    let mut file = BufReader::with_capacity(BUFFER, File::open(path).ok()?);
    let mut head = [0; 20];
    file.read_exact(&mut head).ok()?;
    if &head[..8] != MAGIC {
        return None;
    }
    let length = u64::from_le_bytes(head[8..16].try_into().ok()?);
    let crc = u32::from_le_bytes(head[16..20].try_into().ok()?);
    let mut body = Checked { inner: file.take(length), crc: crc32fast::Hasher::new(), length: 0 };
    let index = decode(&mut Reader { inner: &mut body, budget: length, name: Vec::new() })?;
    (body.length == length && body.crc.finalize() == crc).then_some(index)
}

fn encode(index: &Index, out: &mut impl Write) -> io::Result<()> {
    let volumes = index.volumes();
    put_u32(out, u32::try_from(volumes.len()).unwrap_or(0))?;
    for v in volumes {
        put_bytes(out, v.path.as_os_str().as_bytes())?;
        out.write_all(&v.fsid.to_le_bytes())?;
        out.write_all(&v.generation.to_le_bytes())?;
        out.write_all(&[u8::from(v.btrfs)])?;
        out.write_all(&index.ino(v.root).to_le_bytes())?;
        out.write_all(&index.mtime(v.root).unwrap_or(NO_MTIME).to_le_bytes())?;
        put_u32(out, u32::try_from(count_below(index, v.root)).unwrap_or(0))?;
        // Depth-first: a parent is always written before its children, which name it by its position.
        let mut written: u32 = 0;
        let mut stack: Vec<(u32, EntryId)> = index.children(v.root).into_iter().map(|c| (NO_PARENT, c)).collect();
        while let Some((parent, id)) = stack.pop() {
            put_u32(out, parent)?;
            out.write_all(&[kind_byte(index.kind(id))])?;
            out.write_all(&index.ino(id).to_le_bytes())?;
            out.write_all(&index.mtime(id).unwrap_or(NO_MTIME).to_le_bytes())?;
            put_bytes(out, index.name(id))?;
            stack.extend(index.children(id).into_iter().map(|c| (written, c)));
            written += 1;
        }
    }
    Ok(())
}

/// How many entries are inside `dir`, at any depth.
fn count_below(index: &Index, dir: EntryId) -> usize {
    let mut count = 0;
    let mut stack = index.children(dir);
    while let Some(id) = stack.pop() {
        count += 1;
        stack.extend(index.children(id));
    }
    count
}

fn decode(r: &mut Reader<'_, impl Read>) -> Option<Index> {
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
        // The count is not checked until the CRC at the end, so a damaged one may reserve no more rows than the bytes
        // left could hold.
        let mut ids: Vec<EntryId> = Vec::with_capacity(count.min(usize::try_from(r.budget / ROW_MIN).unwrap_or(0)));
        for _ in 0..count {
            let parent = r.u32()?;
            let kind = kind_of(r.u8()?)?;
            let ino = r.u64()?;
            let mtime = r.i64()?;
            let parent = if parent == NO_PARENT { root } else { *ids.get(parent as usize)? };
            let id = index.insert(parent, r.bytes()?, kind, ino);
            if mtime != NO_MTIME {
                index.set_mtime(id, mtime);
            }
            ids.push(id);
        }
    }
    (r.budget == 0).then_some(index)
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

fn put_u32(out: &mut impl Write, value: u32) -> io::Result<()> {
    out.write_all(&value.to_le_bytes())
}

fn put_bytes(out: &mut impl Write, bytes: &[u8]) -> io::Result<()> {
    put_u32(out, u32::try_from(bytes.len()).unwrap_or(0))?;
    out.write_all(bytes)
}

/// A stream that keeps the CRC-32 and the length of what passes through it.
struct Checked<T> {
    inner: T,
    crc: crc32fast::Hasher,
    length: u64,
}

impl<T: Write> Write for Checked<T> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let n = self.inner.write(buf)?;
        self.crc.update(&buf[..n]);
        self.length += n as u64;
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.inner.flush()
    }
}

impl<T: Read> Read for Checked<T> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let n = self.inner.read(buf)?;
        self.crc.update(&buf[..n]);
        self.length += n as u64;
        Ok(n)
    }
}

/// Reads the body's fields; `budget` is how many bytes of the body are left.
struct Reader<'a, R> {
    inner: &'a mut R,
    budget: u64,
    /// The last name read, reused so a name costs no allocation.
    name: Vec<u8>,
}

impl<R: Read> Reader<'_, R> {
    fn take<const N: usize>(&mut self) -> Option<[u8; N]> {
        self.budget = self.budget.checked_sub(N as u64)?;
        let mut out = [0; N];
        self.inner.read_exact(&mut out).ok()?;
        Some(out)
    }
    fn u8(&mut self) -> Option<u8> {
        Some(self.take::<1>()?[0])
    }
    fn u32(&mut self) -> Option<u32> {
        Some(u32::from_le_bytes(self.take()?))
    }
    fn u64(&mut self) -> Option<u64> {
        Some(u64::from_le_bytes(self.take()?))
    }
    fn i64(&mut self) -> Option<i64> {
        Some(i64::from_le_bytes(self.take()?))
    }
    fn bytes(&mut self) -> Option<&[u8]> {
        let n = self.u32()?;
        self.budget = self.budget.checked_sub(u64::from(n))?;
        self.name.resize(n as usize, 0);
        self.inner.read_exact(&mut self.name).ok()?;
        Some(&self.name)
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
