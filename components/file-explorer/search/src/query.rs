// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Queries in the style of Everything's search syntax, matched against the index.
//!
//! Words separated by spaces must all match (AND), `|` separates alternatives (OR, weaker than AND), `!` negates a
//! word. A plain word matches anywhere in the name without regard to case; a word with `/` matches the whole path; a
//! word with `*` or `?` matches the whole name as a wildcard. `ext:jpg;png` keeps those extensions, `regex:` takes a
//! regular expression on the name, `folder:` and `file:` keep one kind, `size:` (`>10mb`, `<1kb`, `500b`) and `dm:`
//! (`today`, `yesterday`, `7d`, `>30d`) read each candidate's size and modification time, so they come last.
//! Hidden names (starting with a dot, or inside such a folder) are left out unless the query holds `hidden:`, as
//! File Explorer adds while it shows hidden files.

use memchr::memmem;
use regex::bytes::Regex;

use crate::index::{EntryId, Index, Kind, fold};

/// The most matches counted exactly; past it a result says "at least".
pub const COUNT_LIMIT: usize = 10_000;
const DAY: i64 = 86_400;

/// A size or a modification time of a candidate, read with statx when a query asks for it.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Meta {
    pub size: u64,
    /// Seconds since the epoch.
    pub mtime: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Compare {
    Less,
    More,
    Equal,
}

#[derive(Debug, Clone)]
enum Test {
    /// Folded bytes found in the folded name.
    Name(Vec<u8>),
    /// Folded bytes found in the folded whole path.
    Path(Vec<u8>),
    /// A wildcard over the whole folded name.
    Wildcard(Regex),
    Regex(Regex),
    /// Folded extensions, without the dot.
    Ext(Vec<Vec<u8>>),
    Kind(bool),
    Size(Compare, u64),
    /// Modified within the last N seconds (`Less`), longer ago (`More`), or on the day of `today` minus N days.
    Age(Compare, i64),
    Day(i64),
}

#[derive(Debug, Clone)]
struct Term {
    test: Test,
    negated: bool,
}

/// A parsed query: alternatives, each a list of terms that must all hold.
#[derive(Debug, Clone)]
pub struct Query {
    alternatives: Vec<Vec<Term>>,
    hidden: bool,
}

/// What a query found.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Found {
    /// The best matches, best first, at most the limit asked for.
    pub hits: Vec<EntryId>,
    /// How many matches there are, up to [`COUNT_LIMIT`].
    pub total: usize,
    /// Whether there are more than [`COUNT_LIMIT`].
    pub more: bool,
}

/// Parses a query.
///
/// # Errors
///
/// A message naming the word that is not understood: a bad size, age or regular expression.
///
/// # Panics
///
/// Never: there is always an alternative to add a term to.
pub fn parse(text: &str) -> Result<Query, String> {
    let mut alternatives = vec![Vec::new()];
    let mut hidden = false;
    for word in text.split_whitespace() {
        if word.eq_ignore_ascii_case("hidden:") {
            hidden = true;
            continue;
        }
        if word == "|" {
            alternatives.push(Vec::new());
            continue;
        }
        for (i, part) in word.split('|').enumerate() {
            if i > 0 {
                alternatives.push(Vec::new());
            }
            if part.is_empty() {
                continue;
            }
            let (negated, body) = part.strip_prefix('!').map_or((false, part), |rest| (true, rest));
            if body.is_empty() {
                continue;
            }
            let term = Term { test: parse_test(body)?, negated };
            alternatives.last_mut().expect("one alternative at least").push(term);
        }
    }
    alternatives.retain(|a| !a.is_empty());
    Ok(Query { alternatives, hidden })
}

fn parse_test(body: &str) -> Result<Test, String> {
    let lower = body.to_lowercase();
    if let Some(list) = lower.strip_prefix("ext:") {
        let exts =
            list.split([';', ',']).filter(|e| !e.is_empty()).map(|e| e.trim_start_matches('.').as_bytes().to_vec());
        return Ok(Test::Ext(exts.collect()));
    }
    if let Some(pattern) = body.strip_prefix("regex:") {
        return Regex::new(&format!("(?i){pattern}")).map(Test::Regex).map_err(|e| format!("regex:{pattern}: {e}"));
    }
    if lower == "folder:" {
        return Ok(Test::Kind(true));
    }
    if lower == "file:" {
        return Ok(Test::Kind(false));
    }
    if let Some(size) = lower.strip_prefix("size:") {
        let (compare, rest) = comparison(size);
        return parse_size(rest).map(|bytes| Test::Size(compare, bytes)).ok_or_else(|| format!("size:{size}"));
    }
    if let Some(age) = lower.strip_prefix("dm:") {
        return parse_age(age).ok_or_else(|| format!("dm:{age}"));
    }
    if body.contains(['*', '?']) {
        let mut pattern = String::from("(?s-u)^");
        for c in String::from_utf8_lossy(&fold(body.as_bytes())).chars() {
            match c {
                '*' => pattern.push_str(".*"),
                '?' => pattern.push_str("(?u:.)"),
                c => pattern.push_str(&regex::escape(c.encode_utf8(&mut [0; 4]))),
            }
        }
        pattern.push('$');
        return Regex::new(&pattern).map(Test::Wildcard).map_err(|e| format!("{body}: {e}"));
    }
    if body.contains('/') {
        return Ok(Test::Path(fold(body.as_bytes())));
    }
    Ok(Test::Name(fold(body.as_bytes())))
}

fn comparison(text: &str) -> (Compare, &str) {
    if let Some(rest) = text.strip_prefix('>') {
        (Compare::More, rest)
    } else if let Some(rest) = text.strip_prefix('<') {
        (Compare::Less, rest)
    } else {
        (Compare::Equal, text.strip_prefix('=').unwrap_or(text))
    }
}

fn parse_size(text: &str) -> Option<u64> {
    let digits = text.find(|c: char| !c.is_ascii_digit() && c != '.').unwrap_or(text.len());
    let number: f64 = text[..digits].parse().ok()?;
    let unit: f64 = match &text[digits..] {
        "" | "b" => 1.0,
        "k" | "kb" => 1024.0,
        "m" | "mb" => 1024.0 * 1024.0,
        "g" | "gb" => 1024.0 * 1024.0 * 1024.0,
        "t" | "tb" => 1024.0_f64.powi(4),
        _ => return None,
    };
    #[expect(clippy::cast_possible_truncation, clippy::cast_sign_loss, reason = "a size typed by a person")]
    Some((number * unit).round() as u64)
}

fn parse_age(text: &str) -> Option<Test> {
    match text {
        "today" => return Some(Test::Day(0)),
        "yesterday" => return Some(Test::Day(1)),
        _ => {}
    }
    let (compare, rest) = comparison(text);
    let digits = rest.find(|c: char| !c.is_ascii_digit()).unwrap_or(rest.len());
    let count: i64 = rest[..digits].parse().ok()?;
    let unit = match &rest[digits..] {
        "h" => 3600,
        "d" | "" => DAY,
        "w" => 7 * DAY,
        "m" => 30 * DAY,
        "y" => 365 * DAY,
        _ => return None,
    };
    // A bare `7d` means "within the last 7 days", as `<7d` does.
    let compare = if compare == Compare::Equal { Compare::Less } else { compare };
    Some(Test::Age(compare, count * unit))
}

/// What a query run needs from the outside world.
pub struct Context<'a> {
    /// Only this folder and what is inside it, when set.
    pub scope: Option<EntryId>,
    /// The seconds since the epoch now, and at the start of the asker's local day.
    pub now: i64,
    pub today: i64,
    /// Whether the asker may see an entry; checked only on matches.
    pub visible: &'a mut dyn FnMut(EntryId) -> bool,
    /// The size and time of an entry, for `size:` and `dm:`; `None` when it is gone.
    pub meta: &'a mut dyn FnMut(EntryId) -> Option<Meta>,
}

impl Query {
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.alternatives.is_empty()
    }

    /// The best `limit` visible matches, and how many there are.
    pub fn run(&self, index: &Index, context: &mut Context<'_>, limit: usize) -> Found {
        let mut candidates: Vec<(u32, EntryId)> = Vec::new();
        for alternative in &self.alternatives {
            self.collect(index, alternative, context.scope, &mut candidates);
        }
        candidates.sort_unstable();
        candidates.dedup_by_key(|c| c.1);
        // The same entry can come from two alternatives with two scores; keep its best.
        let mut seen = rustc_hash::FxHashSet::default();
        let mut found = Found { hits: Vec::new(), total: 0, more: false };
        for (_, id) in candidates {
            if !seen.insert(id) {
                continue;
            }
            if !self.passes_meta(id, context) || !(context.visible)(id) {
                continue;
            }
            if found.total == COUNT_LIMIT {
                found.more = true;
                break;
            }
            found.total += 1;
            if found.hits.len() < limit {
                found.hits.push(id);
            }
        }
        found
    }

    fn collect(&self, index: &Index, terms: &[Term], scope: Option<EntryId>, out: &mut Vec<(u32, EntryId)>) {
        let first_word = terms.iter().find_map(|t| match (&t.test, t.negated) {
            (Test::Name(word), false) => Some(word.as_slice()),
            _ => None,
        });
        // The longest positive name word finds the candidates with one pass of SIMD search over every name.
        let driver = terms
            .iter()
            .filter_map(|t| match (&t.test, t.negated) {
                (Test::Name(word), false) if !word.is_empty() => Some(word.as_slice()),
                _ => None,
            })
            .max_by_key(|w| w.len());
        let hidden = self.hidden;
        let mut consider = |id: EntryId| {
            if scope.is_some_and(|s| !index.is_within(id, s)) || (!hidden && is_hidden(index, id, scope)) {
                return;
            }
            if terms.iter().all(|t| holds_on_name(index, id, t)) {
                out.push((score(index, id, first_word), id));
            }
        };
        if let Some(word) = driver {
            let mut last = None;
            for pos in memmem::find_iter(index.folded_arena(), word) {
                if let Some(id) = index.entry_at(pos).filter(|&id| Some(id) != last) {
                    last = Some(id);
                    consider(id);
                }
            }
        } else {
            for id in index.live_entries() {
                if index.parent(id).is_some() {
                    consider(id);
                }
            }
        }
    }

    fn passes_meta(&self, id: EntryId, context: &mut Context<'_>) -> bool {
        let needs_meta = self
            .alternatives
            .iter()
            .flatten()
            .any(|t| matches!(t.test, Test::Size(..) | Test::Age(..) | Test::Day(..)));
        if !needs_meta {
            return true;
        }
        let Some(meta) = (context.meta)(id) else { return false };
        // An entry passes when one alternative holds whole; name terms held already for the alternative that found it,
        // so check each alternative's meta terms only.
        self.alternatives.iter().any(|alternative| {
            alternative.iter().all(|t| match &t.test {
                Test::Size(compare, bytes) => t.negated != compares(*compare, meta.size, *bytes),
                Test::Age(compare, seconds) => {
                    let age = context.now - meta.mtime;
                    t.negated != (if *compare == Compare::More { age > *seconds } else { age <= *seconds })
                }
                Test::Day(back) => {
                    let start = context.today - back * DAY;
                    t.negated != (meta.mtime >= start && meta.mtime < start + DAY)
                }
                _ => true,
            })
        })
    }
}

/// Whether the entry or a folder above it, below the scope, has a name starting with a dot.
fn is_hidden(index: &Index, id: EntryId, scope: Option<EntryId>) -> bool {
    let mut at = Some(id);
    while let Some(current) = at {
        if Some(current) == scope {
            return false;
        }
        if index.name(current).first() == Some(&b'.') {
            return true;
        }
        at = index.parent(current);
    }
    false
}

fn compares(compare: Compare, value: u64, bound: u64) -> bool {
    match compare {
        Compare::Less => value < bound,
        Compare::More => value > bound,
        Compare::Equal => value == bound,
    }
}

fn holds_on_name(index: &Index, id: EntryId, term: &Term) -> bool {
    let name = index.folded_name(id);
    let holds = match &term.test {
        Test::Name(word) => memmem::find(name, word).is_some(),
        Test::Path(word) => memmem::find(&fold(index.path(id).as_os_str().as_encoded_bytes()), word).is_some(),
        Test::Wildcard(pattern) => pattern.is_match(name),
        Test::Regex(pattern) => pattern.is_match(index.name(id)),
        Test::Ext(exts) => {
            index.kind(id) != Kind::Dir
                && name
                    .iter()
                    .rposition(|&b| b == b'.')
                    .is_some_and(|dot| exts.iter().any(|e| &name[dot + 1..] == e.as_slice()))
        }
        Test::Kind(folder) => (index.kind(id) == Kind::Dir) == *folder,
        // Checked later, on matches only.
        Test::Size(..) | Test::Age(..) | Test::Day(..) => return true,
    };
    holds != term.negated
}

/// Lower is better: the name equal to the first word, then starting with it, then a word of the name starting with
/// it, then anywhere; within each, shallower paths and shorter names first.
fn score(index: &Index, id: EntryId, word: Option<&[u8]>) -> u32 {
    let name = index.folded_name(id);
    let rank = match word {
        Some(w) if name == w => 0,
        Some(w) if name.starts_with(w) => 1,
        Some(w) if memmem::find_iter(name, w).any(|at| at > 0 && !name[at - 1].is_ascii_alphanumeric()) => 2,
        Some(_) | None => 3,
    };
    let mut depth = 0u32;
    let mut at = id;
    while let Some(parent) = index.parent(at) {
        depth += 1;
        at = parent;
    }
    let length = u32::try_from(name.len()).unwrap_or(u32::MAX).min(0xFFF);
    (rank << 28) | (depth.min(0xFFF) << 12) | length
}

#[cfg(test)]
mod tests;
