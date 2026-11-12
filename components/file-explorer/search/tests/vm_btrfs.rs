// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! What the kernel tells the index on real filesystems, as root, in the virtual machine `tests/vm.sh` starts.
//! `AURORA_SEARCH_BTRFS` names a fresh btrfs mount there; the tests are ignored elsewhere.

use std::collections::HashSet;
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::os::unix::fs::MetadataExt;
use std::path::{Path, PathBuf};

use aurora_search::btrfs::{changed_inodes, commit, is_btrfs, subvolume_generation};

fn scratch(name: &str) -> PathBuf {
    let root = PathBuf::from(std::env::var("AURORA_SEARCH_BTRFS").expect("AURORA_SEARCH_BTRFS names a btrfs mount"));
    let dir = root.join(name);
    fs::create_dir_all(&dir).unwrap();
    dir
}

fn ino(path: &Path) -> u64 {
    fs::symlink_metadata(path).unwrap().ino()
}

fn touch(path: &Path) {
    File::create(path).unwrap();
}

fn sync_disk() {
    assert!(std::process::Command::new("sync").status().unwrap().success());
}

#[test]
#[ignore = "needs root on a scratch btrfs: tests/vm.sh"]
fn the_folders_whose_entries_changed_are_the_ones_with_a_newer_transaction() {
    let root = scratch("entries");
    for dir in ["a/b", "c", "x", "y", "e", "untouched", "z", "d/inner"] {
        fs::create_dir_all(root.join(dir)).unwrap();
    }
    for file in ["c/old", "x/f", "z/w", "untouched/u", "d/inner/file"] {
        touch(&root.join(file));
    }
    sync_disk();
    let handle = File::open(&root).unwrap();
    assert!(is_btrfs(&handle).unwrap());
    let since = subvolume_generation(&handle).unwrap();

    touch(&root.join("a/b/new"));
    fs::remove_file(root.join("c/old")).unwrap();
    fs::rename(root.join("x/f"), root.join("y/f")).unwrap();
    fs::rename(root.join("d"), root.join("e/d")).unwrap();
    OpenOptions::new().append(true).open(root.join("z/w")).unwrap().write_all(b"more").unwrap();

    // The running transaction keeps them among its delayed inodes until it commits.
    let committed = commit(&handle).unwrap();
    assert!(committed > since);
    for when in ["after our commit", "after a second one"] {
        let changed = changed_inodes(&handle, since).unwrap();
        let dirs: HashSet<u64> = changed.iter().filter(|c| c.is_dir).map(|c| c.ino).collect();
        let all: HashSet<u64> = changed.iter().map(|c| c.ino).collect();
        for dir in ["a/b", "c", "x", "y", "e"] {
            assert!(dirs.contains(&ino(&root.join(dir))), "{dir} changed ({when})");
        }
        // `entries` itself lost `d`.
        assert!(dirs.contains(&ino(&root)), "the folder d left changed ({when})");
        for dir in ["a", "untouched", "z", "e/d/inner"] {
            assert!(!dirs.contains(&ino(&root.join(dir))), "{dir} did not change ({when})");
        }
        // z/w's new bytes reach its inode at writeback, not at a commit; the index needs names, and sizes on screen
        // come from statx.
        for file in ["a/b/new", "y/f"] {
            assert!(all.contains(&ino(&root.join(file))), "{file} changed ({when})");
        }
        assert!(!all.contains(&ino(&root.join("untouched/u"))), "untouched/u did not change ({when})");
        sync_disk();
    }
    assert!(subvolume_generation(&handle).unwrap() >= committed);
    // Nothing changed after our commit.
    assert!(changed_inodes(&handle, committed).unwrap().iter().all(|c| c.transid > committed));
}

#[test]
#[ignore = "needs root on a scratch btrfs: tests/vm.sh"]
fn nothing_changed_since_the_last_transaction_reads_as_nothing() {
    let root = scratch("quiet");
    touch(&root.join("file"));
    sync_disk();
    let handle = File::open(&root).unwrap();
    let since = subvolume_generation(&handle).unwrap();
    assert_eq!(changed_inodes(&handle, since).unwrap(), []);
}
