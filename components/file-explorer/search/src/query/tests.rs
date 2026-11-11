// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Tests of the query syntax over a small index.

use std::path::Path;

use super::*;
use crate::index::{Index, Kind};

fn sample() -> Index {
    let mut index = Index::new();
    index.add_volume(Path::new("/home/alex"), 1);
    let root = index.volumes()[0].root;
    let docs = index.insert(root, b"Documents", Kind::Dir, 2);
    index.insert(docs, b"Report 2026.pdf", Kind::File, 3);
    index.insert(docs, b"report-draft.odt", Kind::File, 4);
    let photos = index.insert(root, b"Photos", Kind::Dir, 5);
    index.insert(photos, b"beach.JPG", Kind::File, 6);
    index.insert(photos, b"report", Kind::Dir, 7);
    index.insert(root, b"main.c", Kind::File, 8);
    index
}

fn run(index: &Index, text: &str) -> Vec<String> {
    run_in(index, text, None)
}

fn run_in(index: &Index, text: &str, scope: Option<EntryId>) -> Vec<String> {
    let query = parse(text).unwrap();
    let mut visible = |_| true;
    let mut meta = |id: EntryId| {
        Some(Meta { size: index.ino(id) * 1024 * 1024, mtime: 1_000_000 - i64::try_from(index.ino(id)).unwrap() * DAY })
    };
    let mut context =
        Context { scope, now: 1_000_000, today: 1_000_000 - 3600, visible: &mut visible, meta: &mut meta };
    let found = query.run(index, &mut context, 50);
    found.hits.iter().map(|&id| index.path(id).display().to_string()).collect()
}

#[test]
fn a_word_matches_names_without_case_and_the_exact_name_comes_first() {
    // Then names starting with it, the shorter first.

    assert_eq!(
        run(&sample(), "report"),
        ["/home/alex/Photos/report", "/home/alex/Documents/Report 2026.pdf", "/home/alex/Documents/report-draft.odt"]
    );
}

#[test]
fn all_words_must_match_and_a_bang_excludes() {
    assert_eq!(run(&sample(), "report pdf"), ["/home/alex/Documents/Report 2026.pdf"]);
    assert_eq!(run(&sample(), "report !draft !photos/"), ["/home/alex/Documents/Report 2026.pdf"]);
}

#[test]
fn a_bar_offers_alternatives() {
    assert_eq!(run(&sample(), "beach|main"), ["/home/alex/main.c", "/home/alex/Photos/beach.JPG"]);
    assert_eq!(run(&sample(), "beach | main.c"), ["/home/alex/main.c", "/home/alex/Photos/beach.JPG"]);
}

#[test]
fn extensions_wildcards_kinds_and_paths() {
    assert_eq!(run(&sample(), "ext:jpg;pdf"), ["/home/alex/Photos/beach.JPG", "/home/alex/Documents/Report 2026.pdf"]);
    assert_eq!(run(&sample(), "*.c"), ["/home/alex/main.c"]);
    assert_eq!(run(&sample(), "r?port"), ["/home/alex/Photos/report"]);
    assert_eq!(run(&sample(), "report folder:"), ["/home/alex/Photos/report"]);
    assert_eq!(run(&sample(), "documents/ report").len(), 2);
    assert_eq!(run(&sample(), "regex:^main\\."), ["/home/alex/main.c"]);
}

#[test]
fn size_and_age_read_each_match() {
    // The sample's sizes are the inode in MiB and its age the inode in days.
    assert_eq!(
        run(&sample(), "report size:>3.5mb"),
        ["/home/alex/Photos/report", "/home/alex/Documents/report-draft.odt"]
    );
    assert_eq!(
        run(&sample(), "report dm:<5d"),
        ["/home/alex/Documents/Report 2026.pdf", "/home/alex/Documents/report-draft.odt"]
    );
    assert_eq!(run(&sample(), "ext:c dm:>7d"), ["/home/alex/main.c"]);
}

#[test]
fn a_scope_keeps_one_folder() {
    let index = sample();
    let photos = index.dir_by_ino(0, 5);
    assert_eq!(run_in(&index, "report", photos), ["/home/alex/Photos/report"]);
}

#[test]
fn hidden_entries_are_neither_shown_nor_counted() {
    let index = sample();
    let query = parse("report").unwrap();
    let mut visible = |id: EntryId| index.ino(id) != 3;
    let mut meta = |_| None;
    let mut context = Context { scope: None, now: 0, today: 0, visible: &mut visible, meta: &mut meta };
    let found = query.run(&index, &mut context, 1);
    assert_eq!(found.total, 2);
    assert_eq!(found.hits.len(), 1);
}

#[test]
fn bad_words_are_named() {
    assert_eq!(parse("size:lots").unwrap_err(), "size:lots");
    assert!(parse("regex:(").unwrap_err().starts_with("regex:("));
    assert!(parse("   ").unwrap().is_empty());
}
