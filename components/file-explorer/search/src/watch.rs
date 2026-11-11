// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Following changes live with fanotify (fanotify(7)): one mark per filesystem, events naming the folder whose
//! entries changed by a file handle, which is opened to learn its device and inode.

use std::fs::File;
use std::io::{self, Read};
use std::os::fd::{AsRawFd, OwnedFd, RawFd};
use std::path::Path;

use crate::sys;

// From linux/fanotify.h.
const FAN_CLOEXEC: u32 = 0x1;
const FAN_NONBLOCK: u32 = 0x2;
const FAN_REPORT_DFID_NAME: u32 = 0x400 | 0x800;
const FAN_MARK_ADD: u32 = 0x1;
const FAN_MARK_FILESYSTEM: u32 = 0x100;
const FAN_ATTRIB: u64 = 0x4;
const FAN_MOVED_FROM: u64 = 0x40;
const FAN_MOVED_TO: u64 = 0x80;
const FAN_CREATE: u64 = 0x100;
const FAN_DELETE: u64 = 0x200;
const FAN_Q_OVERFLOW: u64 = 0x4000;
const FAN_ONDIR: u64 = 0x4000_0000;
const INFO_DFID_NAME: u8 = 2;
const INFO_DFID: u8 = 3;
const INFO_OLD_DFID_NAME: u8 = 10;
const INFO_NEW_DFID_NAME: u8 = 12;
/// `struct fanotify_event_metadata`: `event_len`, `vers`, `reserved`, `metadata_len`, mask, fd, pid.
const METADATA_SIZE: usize = 24;

/// Entries come and go, and attributes change; never `FAN_MODIFY`, which comes with every `write()`.
const MASK: u64 = FAN_CREATE | FAN_DELETE | FAN_MOVED_FROM | FAN_MOVED_TO | FAN_ATTRIB | FAN_ONDIR;

/// What an event means for the index.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Change {
    /// The entries of the folder with this device and inode changed.
    Folder { dev: u64, ino: u64 },
    /// An attribute of a name in that folder changed: permissions may have.
    Attributes { dev: u64, ino: u64 },
    /// The kernel dropped events: what changed is unknown, so a whole catch-up is due.
    Overflow,
}

/// A fanotify group with a mark on every indexed filesystem.
pub struct Watch {
    group: OwnedFd,
    /// A folder of each marked filesystem, to open handles on, with its `f_fsid`.
    mounts: Vec<([u8; 8], File)>,
}

impl Watch {
    /// A group that reports the folder of each change and its name.
    ///
    /// # Errors
    ///
    /// `EPERM` without `CAP_SYS_ADMIN`.
    pub fn new() -> io::Result<Self> {
        let group = sys::fanotify_init(FAN_CLOEXEC | FAN_NONBLOCK | FAN_REPORT_DFID_NAME, 0)?;
        Ok(Self { group, mounts: Vec::new() })
    }

    /// Marks the filesystem `path` is on. A second mark on the same filesystem only adds a place to open handles on.
    ///
    /// # Errors
    ///
    /// The mark's error.
    pub fn add(&mut self, path: &Path) -> io::Result<()> {
        sys::fanotify_mark(self.group.as_raw_fd(), FAN_MARK_ADD | FAN_MARK_FILESYSTEM, MASK, path)?;
        self.mounts.push((sys::fsid(path)?, File::open(path)?));
        Ok(())
    }

    #[must_use]
    pub fn fd(&self) -> RawFd {
        self.group.as_raw_fd()
    }

    /// Every event waiting, as changes.
    ///
    /// # Panics
    ///
    /// When the process has no descriptor left to duplicate the group's into.
    #[must_use]
    pub fn read(&self) -> Vec<Change> {
        let mut changes = Vec::new();
        let mut buffer = vec![0u8; 64 * 1024];
        let mut file = File::from(self.group.try_clone().expect("a duplicate of the group's descriptor"));
        loop {
            match file.read(&mut buffer) {
                Ok(0) | Err(_) => break,
                Ok(n) => changes.extend(parse(&buffer[..n], |fsid, handle| self.resolve(*fsid, handle))),
            }
        }
        changes
    }

    /// Device and inode of the folder a handle names: opened on a mount of its filesystem.
    fn resolve(&self, fsid: [u8; 8], handle: &[u8]) -> Option<(u64, u64)> {
        let same = self.mounts.iter().filter(|(f, _)| *f == fsid);
        let others = self.mounts.iter().filter(|(f, _)| *f != fsid);
        // btrfs reports each subvolume's own fsid; a handle opens on any mount of its filesystem, so try the others too.
        for (_, mount) in same.chain(others) {
            if let Ok(fd) = sys::open_by_handle(mount.as_raw_fd(), handle) {
                return sys::dev_ino(fd.as_raw_fd()).ok();
            }
        }
        None
    }
}

/// Turns the bytes of a fanotify read into changes; `resolve` turns an fsid and a `struct file_handle` into a
/// device and inode (`None` when the folder is gone).
///
/// # Panics
///
/// Never: every slice is checked against the event's length first.
pub fn parse(bytes: &[u8], mut resolve: impl FnMut(&[u8; 8], &[u8]) -> Option<(u64, u64)>) -> Vec<Change> {
    let mut out = Vec::new();
    let mut at = 0;
    while at + METADATA_SIZE <= bytes.len() {
        let event_len = u32::from_ne_bytes(bytes[at..at + 4].try_into().expect("four bytes")) as usize;
        let metadata_len = u16::from_ne_bytes(bytes[at + 6..at + 8].try_into().expect("two bytes")) as usize;
        let mask = u64::from_ne_bytes(bytes[at + 8..at + 16].try_into().expect("eight bytes"));
        if event_len < METADATA_SIZE || at + event_len > bytes.len() {
            break;
        }
        if mask & FAN_Q_OVERFLOW != 0 {
            out.push(Change::Overflow);
        }
        let mut info = at + metadata_len;
        while info + 4 <= at + event_len {
            let kind = bytes[info];
            let len = u16::from_ne_bytes(bytes[info + 2..info + 4].try_into().expect("two bytes")) as usize;
            if len < 4 || info + len > at + event_len {
                break;
            }
            if matches!(kind, INFO_DFID_NAME | INFO_DFID | INFO_OLD_DFID_NAME | INFO_NEW_DFID_NAME) && len >= 20 {
                let fsid: [u8; 8] = bytes[info + 4..info + 12].try_into().expect("eight bytes");
                let handle_bytes =
                    u32::from_ne_bytes(bytes[info + 12..info + 16].try_into().expect("four bytes")) as usize;
                let end = (info + 20 + handle_bytes).min(info + len);
                if let Some((dev, ino)) = resolve(&fsid, &bytes[info + 12..end]) {
                    out.push(
                        if mask & FAN_ATTRIB != 0
                            && mask & (FAN_CREATE | FAN_DELETE | FAN_MOVED_FROM | FAN_MOVED_TO) == 0
                        {
                            Change::Attributes { dev, ino }
                        } else {
                            Change::Folder { dev, ino }
                        },
                    );
                }
            }
            info += len;
        }
        at += event_len;
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    /// One event as the kernel lays it out: metadata, then one folder record with a handle and a name.
    fn event(mask: u64, record: u8, handle: &[u8], name: &[u8]) -> Vec<u8> {
        let mut info = vec![record, 0, 0, 0];
        info.extend_from_slice(&[7u8; 8]);
        info.extend_from_slice(&u32::try_from(handle.len()).unwrap().to_ne_bytes());
        info.extend_from_slice(&1i32.to_ne_bytes());
        info.extend_from_slice(handle);
        info.extend_from_slice(name);
        info.push(0);
        while info.len() % 4 != 0 {
            info.push(0);
        }
        let len = u16::try_from(info.len()).unwrap();
        info[2..4].copy_from_slice(&len.to_ne_bytes());
        let mut out = Vec::new();
        out.extend_from_slice(&u32::try_from(METADATA_SIZE + info.len()).unwrap().to_ne_bytes());
        out.extend_from_slice(&[3, 0]);
        out.extend_from_slice(&u16::try_from(METADATA_SIZE).unwrap().to_ne_bytes());
        out.extend_from_slice(&mask.to_ne_bytes());
        out.extend_from_slice(&(-1i32).to_ne_bytes());
        out.extend_from_slice(&0i32.to_ne_bytes());
        out.extend_from_slice(&info);
        out
    }

    #[test]
    fn events_name_the_folder_that_changed() {
        let mut bytes = event(FAN_CREATE, INFO_DFID_NAME, &[1, 2, 3, 4, 5, 6, 7, 8], b"new.txt");
        bytes.extend(event(FAN_ATTRIB | FAN_ONDIR, INFO_DFID_NAME, &[9, 9, 9, 9], b"locked"));
        bytes.extend(event(FAN_Q_OVERFLOW, 0, &[], b""));
        let changes = parse(&bytes, |fsid, handle| {
            assert_eq!(fsid, &[7u8; 8]);
            // The handle keeps its size and type in front of its bytes.
            Some((5, u64::from(handle[8])))
        });
        assert_eq!(
            changes,
            [Change::Folder { dev: 5, ino: 1 }, Change::Attributes { dev: 5, ino: 9 }, Change::Overflow]
        );
    }

    #[test]
    fn a_cut_or_odd_buffer_stops_without_reading_past_it() {
        let bytes = event(FAN_DELETE, INFO_DFID_NAME, &[1, 2, 3, 4], b"gone");
        assert_eq!(parse(&bytes[..bytes.len() - 3], |_, _| Some((1, 1))), []);
        assert_eq!(parse(&[0u8; 10], |_, _| Some((1, 1))), []);
        // A folder that is gone by the time the event is read sends nothing.
        assert_eq!(parse(&bytes, |_, _| None), []);
    }
}
