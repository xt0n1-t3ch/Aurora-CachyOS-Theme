// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The local filesystems to index, read from `/proc/self/mountinfo` (proc(5)).

use std::path::{Path, PathBuf};

/// Filesystems on local storage. Network, memory and virtual filesystems are searched in place instead.
const LOCAL: &[&str] =
    &["btrfs", "ext4", "ext3", "ext2", "xfs", "f2fs", "bcachefs", "vfat", "exfat", "ntfs3", "fuseblk"];

/// One mounted local filesystem.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Mount {
    pub path: PathBuf,
    pub fstype: String,
    /// `major:minor` and the root of the mount within the filesystem, which tell two mounts of one place apart.
    pub device: String,
    pub root: String,
    pub read_only: bool,
}

impl Mount {
    /// Whether a folder's modification time cannot be trusted to show a change in it (FAT keeps no reliable one).
    #[must_use]
    pub fn needs_full_reads(&self) -> bool {
        matches!(self.fstype.as_str(), "vfat" | "exfat" | "fuseblk")
    }
}

/// The local mounts of a mountinfo text, each place once (bind mounts of one folder keep the first), without those
/// under `excluded` paths.
#[must_use]
pub fn local_mounts(mountinfo: &str, excluded: &[PathBuf]) -> Vec<Mount> {
    let mut out: Vec<Mount> = Vec::new();
    for line in mountinfo.lines() {
        let Some((left, right)) = line.split_once(" - ") else { continue };
        let fields: Vec<&str> = left.split(' ').collect();
        let rest: Vec<&str> = right.split(' ').collect();
        if fields.len() < 6 || rest.len() < 2 {
            continue;
        }
        let fstype = rest[0];
        if !LOCAL.contains(&fstype) {
            continue;
        }
        let path = PathBuf::from(unescape(fields[4]));
        if excluded.iter().any(|e| path.starts_with(e)) || is_snapshot(&path) {
            continue;
        }
        let mount = Mount {
            path,
            fstype: fstype.to_string(),
            device: fields[2].to_string(),
            root: unescape(fields[3]),
            read_only: fields[5].split(',').any(|o| o == "ro"),
        };
        if out.iter().any(|m| m.device == mount.device && m.root == mount.root) {
            continue;
        }
        out.push(mount);
    }
    out
}

/// Whether a path is where snapper or Timeshift keep snapshots, which repeat the system's names many times over.
#[must_use]
pub fn is_snapshot(path: &Path) -> bool {
    path.components().any(|c| c.as_os_str() == ".snapshots") || path.starts_with("/run/timeshift")
}

/// mountinfo writes space, tab, newline and backslash in octal (`\040`).
fn unescape(field: &str) -> String {
    let bytes = field.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'\\' && i + 3 < bytes.len() && bytes[i + 1..i + 4].iter().all(|b| (b'0'..=b'7').contains(b)) {
            out.push((bytes[i + 1] - b'0') * 64 + (bytes[i + 2] - b'0') * 8 + (bytes[i + 3] - b'0'));
            i += 4;
        } else {
            out.push(bytes[i]);
            i += 1;
        }
    }
    String::from_utf8_lossy(&out).into_owned()
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = "\
22 1 0:30 /@ / rw,noatime shared:1 - btrfs /dev/nvme0n1p2 rw,compress=zstd:1,subvolid=256,subvol=/@
23 22 0:30 /@home /home rw,noatime shared:2 - btrfs /dev/nvme0n1p2 rw,subvolid=257,subvol=/@home
24 22 0:22 / /proc rw,nosuid shared:3 - proc proc rw
25 22 0:31 / /tmp rw shared:4 - tmpfs tmpfs rw
26 22 259:5 / /boot rw,relatime shared:5 - vfat /dev/nvme0n1p1 rw
27 22 0:40 / /mnt/D rw,noatime shared:6 - btrfs /dev/sda1 rw,subvolid=5
28 22 0:40 / /mnt/again rw,noatime shared:7 - btrfs /dev/sda1 rw,subvolid=5
29 22 0:30 /@snapshots /.snapshots rw shared:8 - btrfs /dev/nvme0n1p2 rw,subvol=/@snapshots
30 22 0:41 / /mnt/My\\040Drive ro,relatime shared:9 - ext4 /dev/sdb1 ro
31 22 0:42 / /mnt/nas rw shared:10 - nfs4 nas:/share rw
";

    #[test]
    fn local_mounts_once_each_without_snapshots_or_network() {
        let mounts = local_mounts(SAMPLE, &[PathBuf::from("/proc")]);
        let paths: Vec<&str> = mounts.iter().map(|m| m.path.to_str().unwrap()).collect();
        assert_eq!(paths, ["/", "/home", "/boot", "/mnt/D", "/mnt/My Drive"]);
        assert!(mounts[4].read_only);
        assert!(mounts[2].needs_full_reads());
        assert!(!mounts[0].needs_full_reads());
        assert_eq!(mounts[1].root, "/@home");
    }
}
