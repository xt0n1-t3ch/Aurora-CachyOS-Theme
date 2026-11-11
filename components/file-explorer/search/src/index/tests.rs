// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Tests of the index: worked examples, then random histories against a plain model of the disk.

use std::collections::BTreeMap;
use std::path::Path;

use super::*;

fn listed(name: &str, ino: u64, kind: Kind) -> Listed {
    Listed { name: name.as_bytes().to_vec(), ino, kind }
}

fn find(index: &Index, path: &str) -> Option<EntryId> {
    let mut at = index.volumes()[0].root;
    for part in path.split('/').filter(|p| !p.is_empty()) {
        at = index.children(at).into_iter().find(|&c| index.name(c) == part.as_bytes())?;
    }
    Some(at)
}

fn hits(index: &Index, needle: &str) -> Vec<String> {
    let mut out: Vec<String> = memchr::memmem::find_iter(index.folded_arena(), needle.as_bytes())
        .filter_map(|pos| index.entry_at(pos))
        .map(|id| index.path(id).display().to_string())
        .collect();
    out.sort();
    out.dedup();
    out
}

#[test]
fn an_entry_knows_its_whole_path() {
    let mut index = Index::new();
    let v = index.add_volume(Path::new("/home"), 256);
    let root = index.volumes()[v as usize].root;
    let alex = index.insert(root, b"alex", Kind::Dir, 300);
    let file = index.insert(alex, b"main.c", Kind::File, 301);
    assert_eq!(index.path(file), Path::new("/home/alex/main.c"));
    assert_eq!(index.dir_by_ino(v, 300), Some(alex));
    assert!(index.is_within(file, alex));
    assert!(!index.is_within(alex, file));
    assert_eq!(index.len(), 3);
}

#[test]
fn a_folder_moved_between_two_folders_of_a_batch_keeps_its_subtree() {
    let mut index = Index::new();
    index.add_volume(Path::new("/v"), 256);
    let root = index.volumes()[0].root;
    let a = index.insert(root, b"a", Kind::Dir, 10);
    let b = index.insert(root, b"b", Kind::Dir, 11);
    let docs = index.insert(a, b"docs", Kind::Dir, 12);
    let deep = index.insert(docs, b"deep.txt", Kind::File, 13);
    // b is listed first: the move is found even though its old folder comes later in the batch.
    let to_read = index.apply_batch(vec![(b, vec![listed("papers", 12, Kind::Dir)]), (a, vec![])]);
    assert_eq!(to_read, []);
    assert_eq!(find(&index, "b/papers"), Some(docs));
    assert_eq!(find(&index, "b/papers/deep.txt"), Some(deep));
    assert_eq!(find(&index, "a/docs"), None);
    assert_eq!(index.path(deep), Path::new("/v/b/papers/deep.txt"));
    assert_eq!(hits(&index, "docs"), Vec::<String>::new());
    assert_eq!(hits(&index, "paper"), ["/v/b/papers"]);
}

#[test]
fn a_name_that_now_points_at_another_file_is_a_new_entry() {
    let mut index = Index::new();
    index.add_volume(Path::new("/v"), 256);
    let root = index.volumes()[0].root;
    let old = index.insert(root, b"x", Kind::Dir, 10);
    index.insert(old, b"inside", Kind::File, 11);
    let to_read = index.apply_batch(vec![(root, vec![listed("x", 20, Kind::Dir)])]);
    let new = find(&index, "x").unwrap();
    assert_eq!(index.ino(new), 20);
    assert_eq!(to_read, [new]);
    assert_eq!(find(&index, "x/inside"), None);
    assert_eq!(index.len(), 2);
}

#[test]
fn new_folders_are_returned_for_reading_and_gone_names_leave() {
    let mut index = Index::new();
    index.add_volume(Path::new("/v"), 256);
    let root = index.volumes()[0].root;
    index.insert(root, b"gone", Kind::File, 5);
    let to_read = index.apply_batch(vec![(root, vec![listed("new", 6, Kind::Dir), listed("f", 7, Kind::File)])]);
    assert_eq!(to_read, [find(&index, "new").unwrap()]);
    assert_eq!(find(&index, "gone"), None);
    assert_eq!(hits(&index, "gone"), Vec::<String>::new());
}

#[test]
fn hard_links_in_two_folders_are_two_names() {
    let mut index = Index::new();
    index.add_volume(Path::new("/v"), 256);
    let root = index.volumes()[0].root;
    let a = index.insert(root, b"a", Kind::Dir, 10);
    let b = index.insert(root, b"b", Kind::Dir, 11);
    index.apply_batch(vec![(a, vec![listed("link", 50, Kind::File)]), (b, vec![listed("link", 50, Kind::File)])]);
    assert_eq!(hits(&index, "link"), ["/v/a/link", "/v/b/link"]);
}

#[test]
fn names_match_without_case_and_compaction_keeps_them() {
    let mut index = Index::new();
    index.add_volume(Path::new("/v"), 256);
    let root = index.volumes()[0].root;
    for i in 0..50u64 {
        index.insert(root, format!("Temp{i}").as_bytes(), Kind::File, 100 + i);
    }
    let keep = index.insert(root, "Ñandú Fotos".as_bytes(), Kind::Dir, 99);
    index.apply_batch(vec![(root, vec![listed("Ñandú Fotos", 99, Kind::Dir)])]);
    index.compact_if_wasteful();
    assert_eq!(hits(&index, "ñandú"), ["/v/Ñandú Fotos"]);
    assert_eq!(hits(&index, "temp"), Vec::<String>::new());
    assert_eq!(find(&index, "Ñandú Fotos"), Some(keep));
    assert_eq!(index.len(), 2);
}

#[test]
fn a_name_that_folding_keeps_is_stored_once() {
    let mut index = Index::new();
    index.add_volume(Path::new("/v"), 256);
    let root = index.volumes()[0].root;
    let lower = index.insert(root, b"notes.txt", Kind::File, 2);
    let upper = index.insert(root, b"Notes.txt", Kind::File, 3);
    // Only "Notes.txt" and its NUL are kept as written; both live folded with theirs, after the root's empty name.
    assert_eq!(index.arena_bytes(), (10, 21));
    assert_eq!(index.name(lower), b"notes.txt");
    assert_eq!(index.name(upper), b"Notes.txt");
    assert_eq!(index.path(upper), Path::new("/v/Notes.txt"));
}

/// A tiny deterministic generator, so a failing history can be replayed from its seed.
struct Rng(u64);

impl Rng {
    fn next(&mut self, below: usize) -> usize {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        usize::try_from(self.0 % u64::try_from(below).unwrap()).unwrap()
    }
}

/// The disk as a map from path to (inode, kind). Folders are paths too.
type Disk = BTreeMap<String, (u64, Kind)>;

fn listing_of(disk: &Disk, dir: &str) -> Vec<Listed> {
    let prefix = if dir.is_empty() { String::new() } else { format!("{dir}/") };
    disk.iter()
        .filter(|(p, _)| p.starts_with(&prefix) && !p[prefix.len()..].contains('/') && p.len() > prefix.len())
        .map(|(p, &(ino, kind))| listed(&p[prefix.len()..], ino, kind))
        .collect()
}

fn indexed(index: &Index) -> Disk {
    let mut out = Disk::new();
    let mut stack = vec![(index.volumes()[0].root, String::new())];
    while let Some((dir, path)) = stack.pop() {
        for child in index.children(dir) {
            let name = String::from_utf8(index.name(child).to_vec()).unwrap();
            let full = if path.is_empty() { name } else { format!("{path}/{name}") };
            out.insert(full.clone(), (index.ino(child), index.kind(child)));
            stack.push((child, full));
        }
    }
    out
}

#[test]
fn random_histories_end_where_the_disk_is() {
    for seed in 1..=300u64 {
        let mut rng = Rng(seed.wrapping_mul(0x9E37_79B9_7F4A_7C15));
        let mut disk = Disk::new();
        let mut index = Index::new();
        index.add_volume(Path::new("/v"), 1);
        let mut next_ino = 2;
        for _ in 0..40 {
            // Change the disk a few times, remembering which folders saw entries come and go, by inode, as fanotify's
            // folder handles and the btrfs catch-up name them.
            let mut touched: Vec<u64> = Vec::new();
            let ino_of = |disk: &Disk, dir: &str| if dir.is_empty() { 1 } else { disk[dir].0 };
            for _ in 0..=rng.next(4) {
                let dirs: Vec<String> = std::iter::once(String::new())
                    .chain(disk.iter().filter(|(_, v)| v.1 == Kind::Dir).map(|(p, _)| p.clone()))
                    .collect();
                let dir = dirs[rng.next(dirs.len())].clone();
                // Two names that folding keeps and two it changes, so both ways a name is stored are exercised.
                let names = ["a", "B", "c", "Dé"];
                let name = names[rng.next(4)];
                let path = if dir.is_empty() { name.to_string() } else { format!("{dir}/{name}") };
                let parent_of = |p: &str| p.rsplit_once('/').map_or(String::new(), |(d, _)| d.to_string());
                match rng.next(3) {
                    0 if !disk.contains_key(&path) => {
                        let kind = if rng.next(2) == 0 { Kind::Dir } else { Kind::File };
                        disk.insert(path.clone(), (next_ino, kind));
                        next_ino += 1;
                        touched.push(ino_of(&disk, &dir));
                    }
                    1 if disk.contains_key(&path) => {
                        let prefix = format!("{path}/");
                        disk.retain(|p, _| p != &path && !p.starts_with(&prefix));
                        touched.push(ino_of(&disk, &dir));
                    }
                    2 if disk.contains_key(&path) => {
                        // Move it to another folder that is not inside it, under a free name.
                        let target_dirs: Vec<String> = dirs
                            .iter()
                            .filter(|d| *d != &path && !d.starts_with(&format!("{path}/")))
                            .cloned()
                            .collect();
                        let to = target_dirs[rng.next(target_dirs.len())].clone();
                        let new_name = format!("m{}", rng.next(1000));
                        let new_path = if to.is_empty() { new_name } else { format!("{to}/{new_name}") };
                        if disk.contains_key(&new_path) {
                            continue;
                        }
                        touched.push(ino_of(&disk, &parent_of(&path)));
                        touched.push(ino_of(&disk, &to));
                        let prefix = format!("{path}/");
                        let moved: Vec<(String, (u64, Kind))> = disk
                            .iter()
                            .filter(|(p, _)| *p == &path || p.starts_with(&prefix))
                            .map(|(p, v)| (p.clone(), *v))
                            .collect();
                        for (p, v) in moved {
                            disk.remove(&p);
                            disk.insert(format!("{new_path}{}", &p[path.len()..]), v);
                        }
                    }
                    _ => {}
                }
            }
            // Re-read the touched folders that still exist, as fanotify or the catch-up would name them, then read
            // every new folder the batch returns, as the scanner would.
            touched.sort_unstable();
            touched.dedup();
            let mut listings = Vec::new();
            for ino in touched {
                let now = if ino == 1 {
                    Some(String::new())
                } else {
                    disk.iter().find(|(_, v)| v.0 == ino && v.1 == Kind::Dir).map(|(p, _)| p.clone())
                };
                // A folder removed since is not listed; its parent's listing drops it.
                if let (Some(path), Some(id)) = (now, index.dir_by_ino(0, ino)) {
                    listings.push((id, listing_of(&disk, &path)));
                }
            }
            let mut to_read = index.apply_batch(listings);
            while let Some(dir) = to_read.pop() {
                let path = indexed_path(&index, dir);
                to_read.extend(index.apply_batch(vec![(dir, listing_of(&disk, &path))]));
            }
            index.compact_if_wasteful();
            assert_eq!(indexed(&index), disk, "seed {seed}");
        }
    }
}

fn indexed_path(index: &Index, id: EntryId) -> String {
    let full = index.path(id);
    full.strip_prefix("/v").unwrap().display().to_string()
}
