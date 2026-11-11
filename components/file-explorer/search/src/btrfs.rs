// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! What changed on a btrfs subvolume since a transaction, read from its metadata.
//!
//! btrfs stamps every inode item with the id of the transaction that last changed it (`btrfs_inode_item.transid`).
//! Adding, removing or renaming an entry changes its folder's inode (size and change time) in that transaction, so
//! the folders whose inode transid is above the last one the index saw are exactly the folders to read again. This
//! plays the part of the NTFS change journal for Everything.
//!
//! An inode's new state reaches the tree only when its transaction commits: until then btrfs keeps it among its
//! delayed inodes (fs/btrfs/delayed-inode.c), where a tree search does not look. So a catch-up first commits
//! ([`commit`]), then searches.

use std::fs::File;
use std::io;
use std::os::fd::AsRawFd;

/// `BTRFS_SUPER_MAGIC` from `linux/magic.h`.
const BTRFS_SUPER_MAGIC: i64 = 0x9123_683E;
const BTRFS_IOCTL_MAGIC: u64 = 0x94;
/// `BTRFS_INODE_ITEM_KEY` from `linux/btrfs_tree.h`.
const INODE_ITEM_KEY: u32 = 1;
/// `struct btrfs_ioctl_search_key`: 7 u64 bounds, 4 u32 fields, then 4 u64 reserved.
const SEARCH_KEY_SIZE: usize = 104;
/// `struct btrfs_ioctl_search_header`: transid, objectid, offset (u64), type, len (u32).
const SEARCH_HEADER_SIZE: usize = 32;
/// Room for results per `BTRFS_IOC_TREE_SEARCH_V2` call, in bytes.
const SEARCH_BUFFER: usize = 256 * 1024;
/// `struct btrfs_ioctl_get_subvol_info_args`, in bytes, and where its `generation` field is.
const SUBVOL_INFO_SIZE: usize = 504;
const SUBVOL_INFO_GENERATION: usize = 280;
/// Offsets in `struct btrfs_inode_item`: `transid` after `generation`, `mode` after five u64 and three u32.
const INODE_TRANSID: usize = 8;
const INODE_MODE: usize = 52;
const INODE_ITEM_MIN_LEN: usize = 56;

/// `_IOC(dir, type, nr, size)` from `asm-generic/ioctl.h`.
const fn ioc(dir: u64, nr: u64, size: usize) -> u64 {
    (dir << 30) | ((size as u64) << 16) | (BTRFS_IOCTL_MAGIC << 8) | nr
}
/// `BTRFS_IOC_TREE_SEARCH_V2`: `_IOWR(0x94, 17, struct btrfs_ioctl_search_args_v2)`, a key and a buffer size.
const IOC_TREE_SEARCH_V2: u64 = ioc(3, 17, SEARCH_KEY_SIZE + 8);
/// `BTRFS_IOC_GET_SUBVOL_INFO`: `_IOR(0x94, 60, struct btrfs_ioctl_get_subvol_info_args)`.
const IOC_GET_SUBVOL_INFO: u64 = ioc(2, 60, SUBVOL_INFO_SIZE);
/// `BTRFS_IOC_SUBVOL_GETFLAGS`: `_IOR(0x94, 25, __u64)`; `BTRFS_SUBVOL_RDONLY` is bit 1.
const IOC_SUBVOL_GETFLAGS: u64 = ioc(2, 25, 8);
const SUBVOL_RDONLY: u64 = 1 << 1;
/// `BTRFS_IOC_START_SYNC`: `_IOR(0x94, 24, __u64)`, which returns the id of the transaction it commits.
const IOC_START_SYNC: u64 = ioc(2, 24, 8);
/// `BTRFS_IOC_WAIT_SYNC`: `_IOW(0x94, 22, __u64)`, which waits for that transaction.
const IOC_WAIT_SYNC: u64 = ioc(1, 22, 8);

/// An inode a transaction after the asked one changed.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ChangedInode {
    /// The inode number, as `st_ino` reports it.
    pub ino: u64,
    pub is_dir: bool,
    /// The transaction that last changed it.
    pub transid: u64,
}

/// Whether `dir` is on btrfs.
///
/// # Errors
///
/// The `fstatfs` error.
pub fn is_btrfs(dir: &File) -> io::Result<bool> {
    Ok(crate::sys::fstatfs_type(dir.as_raw_fd())? == BTRFS_SUPER_MAGIC)
}

/// The id of the last transaction committed on the subvolume of `dir`. Works without privileges.
///
/// # Errors
///
/// The ioctl's error: `ENOTTY` off btrfs, `EOPNOTSUPP` before Linux 4.18.
pub fn subvolume_generation(dir: &File) -> io::Result<u64> {
    let mut args = vec![0u8; SUBVOL_INFO_SIZE];
    crate::sys::ioctl_bytes(dir.as_raw_fd(), IOC_GET_SUBVOL_INFO, &mut args)?;
    Ok(read_u64(&args, SUBVOL_INFO_GENERATION))
}

/// Whether the subvolume `dir` is the root of is read-only, as snapshots usually are.
///
/// # Errors
///
/// The ioctl's error; `ENOTTY` off btrfs.
pub fn is_read_only(dir: &File) -> io::Result<bool> {
    let mut flags = [0u8; 8];
    crate::sys::ioctl_bytes(dir.as_raw_fd(), IOC_SUBVOL_GETFLAGS, &mut flags)?;
    Ok(u64::from_ne_bytes(flags) & SUBVOL_RDONLY != 0)
}

/// Commits the running transaction of the filesystem `dir` is on and waits for it; returns its id. Every change made
/// before the call is then in the tree, stamped with that id or an earlier one. Dirty file data is not written: a
/// commit carries metadata (and data only with the `flushoncommit` mount option).
///
/// # Errors
///
/// The ioctl's error.
pub fn commit(dir: &File) -> io::Result<u64> {
    let mut transid = [0u8; 8];
    crate::sys::ioctl_bytes(dir.as_raw_fd(), IOC_START_SYNC, &mut transid)?;
    crate::sys::ioctl_bytes(dir.as_raw_fd(), IOC_WAIT_SYNC, &mut transid)?;
    Ok(u64::from_ne_bytes(transid))
}

/// Every inode of the subvolume of `dir` that a committed transaction after `since` changed. Needs `CAP_SYS_ADMIN`.
/// Changes of the running transaction show only after [`commit`].
///
/// # Errors
///
/// The ioctl's error: `EPERM` without the capability, `ENOTTY` off btrfs.
pub fn changed_inodes(dir: &File, since: u64) -> io::Result<Vec<ChangedInode>> {
    let mut changed = Vec::new();
    let mut min_objectid = 0u64;
    let mut args = vec![0u8; SEARCH_KEY_SIZE + 8 + SEARCH_BUFFER];
    loop {
        args.fill(0);
        // tree_id 0 is the subvolume the descriptor is on.
        write_u64(&mut args, 8, min_objectid);
        write_u64(&mut args, 16, u64::MAX);
        write_u64(&mut args, 24, 0);
        write_u64(&mut args, 32, u64::MAX);
        // Only metadata blocks written after `since`: a block holding a changed item is always among them.
        write_u64(&mut args, 40, since.saturating_add(1));
        write_u64(&mut args, 48, u64::MAX);
        write_u32(&mut args, 56, INODE_ITEM_KEY);
        write_u32(&mut args, 60, INODE_ITEM_KEY);
        write_u32(&mut args, 64, u32::MAX);
        write_u64(&mut args, SEARCH_KEY_SIZE, SEARCH_BUFFER as u64);
        crate::sys::ioctl_bytes(dir.as_raw_fd(), IOC_TREE_SEARCH_V2, &mut args)?;

        let returned = read_u32(&args, 64) as usize;
        if returned == 0 {
            return Ok(changed);
        }
        let items = &args[SEARCH_KEY_SIZE + 8..];
        let mut at = 0;
        let mut last_objectid = min_objectid;
        for _ in 0..returned {
            let objectid = read_u64(items, at + 8);
            let kind = read_u32(items, at + 24);
            let len = read_u32(items, at + 28) as usize;
            let item = &items[at + SEARCH_HEADER_SIZE..at + SEARCH_HEADER_SIZE + len];
            at += SEARCH_HEADER_SIZE + len;
            last_objectid = objectid;
            // The search bounds whole keys, so items of other types between two inodes can come back too.
            if kind != INODE_ITEM_KEY || len < INODE_ITEM_MIN_LEN {
                continue;
            }
            // The block is newer than `since`; the inode itself may not be, if it only shares the block.
            let transid = read_u64(item, INODE_TRANSID);
            if transid > since {
                let mode = read_u32(item, INODE_MODE);
                changed.push(ChangedInode { ino: objectid, is_dir: mode & libc::S_IFMT == libc::S_IFDIR, transid });
            }
        }
        // An inode has one inode item, so the next search starts at the next inode.
        match last_objectid.checked_add(1) {
            Some(next) => min_objectid = next,
            None => return Ok(changed),
        }
    }
}

fn read_u64(bytes: &[u8], at: usize) -> u64 {
    u64::from_ne_bytes(bytes[at..at + 8].try_into().expect("eight bytes"))
}

fn read_u32(bytes: &[u8], at: usize) -> u32 {
    u32::from_ne_bytes(bytes[at..at + 4].try_into().expect("four bytes"))
}

fn write_u64(bytes: &mut [u8], at: usize, value: u64) {
    bytes[at..at + 8].copy_from_slice(&value.to_ne_bytes());
}

fn write_u32(bytes: &mut [u8], at: usize, value: u32) {
    bytes[at..at + 4].copy_from_slice(&value.to_ne_bytes());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ioctl_numbers_are_the_kernels() {
        // The values `linux/btrfs.h` gives on x86-64, printed by a C program with the same macros.
        assert_eq!(IOC_TREE_SEARCH_V2, 0xc070_9411);
        assert_eq!(IOC_GET_SUBVOL_INFO, 0x81f8_943c);
        assert_eq!(IOC_START_SYNC, 0x8008_9418);
        assert_eq!(IOC_SUBVOL_GETFLAGS, 0x8008_9419);
        assert_eq!(IOC_WAIT_SYNC, 0x4008_9416);
    }
}
