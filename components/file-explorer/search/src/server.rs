// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The helper's loop: clients on a `SOCK_SEQPACKET` socket, fanotify, scanning and catch-up, in one thread that
//! sleeps in `poll` whenever there is nothing to do.
//!
//! Protocol, one message each way, fields separated by tabs. A client sends `hello` (to hear about changes) and
//! `query`, `id`, `limit`, `now`, `today`, `scope`, `text`, where `now` and `today` are seconds since the epoch (now,
//! and the start of the client's day) and an empty `scope` searches everywhere. The helper answers `result`, `id`,
//! `total`, `more` (0 or 1), `state` (`ready`, `indexing`, `catching-up`), `error`, a newline, then one match per
//! record: its kind (`d`, `f`, `l`, `o`), a tab, its path and a NUL. When the index changes it sends `changed` to
//! every client that said hello, which asks again. A scope the index does not hold is answered with the error
//! `not-indexed`, and the client searches that folder in place instead.

use std::fs::{self, File};
use std::io;
use std::os::fd::{AsRawFd, OwnedFd};
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::MetadataExt;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use rustc_hash::FxHashSet;

use crate::access::{Access, Viewer};
use crate::config::Config;
use crate::index::{EntryId, Index, Kind};
use crate::mounts::{Mount, local_mounts};
use crate::query::{self, Context, Meta};
use crate::scan::{self, read_wave};
use crate::watch::{Change, Watch};
use crate::{btrfs, catchup, store, sys};

/// How long changes gather before their folders are read again: a burst (an archive unpacking) becomes one batch.
const SETTLE: Duration = Duration::from_millis(150);
/// The most matches one answer carries.
const LIMIT_MAX: usize = 2000;
const MESSAGE_MAX: usize = 64 * 1024;

struct Client {
    fd: OwnedFd,
    viewer: Viewer,
    hello: bool,
}

/// What the index is doing, for the answers.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum State {
    Ready,
    Indexing,
    CatchingUp,
}

/// Where the helper keeps its things and how hard it reads.
pub struct Setup {
    pub listener: OwnedFd,
    pub state_dir: PathBuf,
    pub config: Config,
    pub threads: usize,
}

pub struct Server {
    index: Index,
    access: Access,
    setup: Setup,
    watch: Option<Watch>,
    clients: Vec<Client>,
    pending: Vec<EntryId>,
    dirty: FxHashSet<EntryId>,
    dirty_since: Option<Instant>,
    full_catch_up: bool,
    state: State,
    alone_since: Instant,
    changed: bool,
}

impl Server {
    /// Loads the stored index, matches it to what is mounted now, starts watching, and finds what changed meanwhile.
    #[must_use]
    pub fn start(setup: Setup) -> Self {
        // `accept` runs in a loop until no connection waits; on a blocking socket the last call would never return.
        if let Err(error) = sys::set_nonblocking(setup.listener.as_raw_fd()) {
            eprintln!("aurora-search: the socket stays blocking: {error}");
        }
        let index = store::load(&setup.state_dir.join("index")).unwrap_or_default();
        let mut server = Self {
            index,
            access: Access::new(),
            setup,
            watch: None,
            clients: Vec::new(),
            pending: Vec::new(),
            dirty: FxHashSet::default(),
            dirty_since: None,
            full_catch_up: false,
            state: State::CatchingUp,
            alone_since: Instant::now(),
            changed: false,
        };
        let mountinfo = fs::read_to_string("/proc/self/mountinfo").unwrap_or_default();
        let mut mounts = local_mounts(&mountinfo, &server.setup.config.exclude);
        if !server.setup.config.roots.is_empty() {
            // Only the folders asked for, each with the filesystem type of the mount it is on.
            let all = local_mounts(&mountinfo, &[]);
            mounts = server
                .setup
                .config
                .roots
                .iter()
                .filter_map(|root| {
                    let on = all.iter().filter(|m| root.starts_with(&m.path)).max_by_key(|m| m.path.as_os_str().len())?;
                    Some(Mount { path: root.clone(), ..on.clone() })
                })
                .collect();
        }
        // Watch before catching up: a change during the catch-up is then both found and heard, never lost.
        server.watch = Watch::new().ok();
        if let Some(watch) = server.watch.as_mut() {
            for mount in &mounts {
                if let Err(error) = watch.add(&mount.path) {
                    eprintln!("aurora-search: not following changes on {}: {error}", mount.path.display());
                }
            }
        } else {
            eprintln!("aurora-search: fanotify is not available; changes show at the next start");
        }
        server.reconcile(&mounts);
        server.catch_up();
        server
    }

    /// Keeps the stored volumes still mounted at the same place on the same filesystem, adds new ones, drops the rest.
    fn reconcile(&mut self, mounts: &[Mount]) {
        let mut kept = vec![false; self.index.volumes().len()];
        for mount in mounts {
            let Ok(meta) = fs::metadata(&mount.path) else { continue };
            let fsid = sys::fsid(&mount.path).map_or(0, u64::from_ne_bytes);
            let found = self.index.volumes().iter().position(|v| {
                v.path == mount.path
                    && v.fsid == fsid
                    && self.index.is_alive(v.root)
                    && self.index.ino(v.root) == meta.ino()
            });
            let volume = if let Some(found) = found {
                kept[found] = true;
                u16::try_from(found).expect("fewer than 65536 volumes")
            } else {
                let v = self.index.add_volume(&mount.path, meta.ino());
                kept.push(true);
                self.pending.push(self.index.volumes()[v as usize].root);
                v
            };
            let info = self.index.volume_mut(volume);
            info.dev = meta.dev();
            info.fsid = fsid;
            info.btrfs = mount.fstype == "btrfs";
            if found.is_none() && info.btrfs {
                // Whatever the full scan reads is newer than this; later changes are heard.
                info.generation = File::open(&mount.path).and_then(|f| btrfs::commit(&f)).unwrap_or(0);
            }
            if found.is_some() && mount.needs_full_reads() {
                let root = self.index.volumes()[volume as usize].root;
                for child in self.index.children(root) {
                    self.index.remove(child);
                }
                self.pending.push(root);
            }
        }
        for (volume, keep) in kept.iter().enumerate() {
            let root = self.index.volumes()[volume].root;
            if !keep && self.index.is_alive(root) {
                // A nested btrfs subvolume is not a mount; it stays while it is still there on the same device.
                let path = self.index.volumes()[volume].path.clone();
                let still_there = fs::metadata(&path).is_ok_and(|m| m.ino() == self.index.ino(root));
                if still_there {
                    self.index.volume_mut(u16::try_from(volume).expect("fewer than 65536 volumes")).dev =
                        fs::metadata(&path).map_or(0, |m| m.dev());
                } else {
                    self.index.remove(root);
                }
            }
        }
    }

    /// Finds the folders that changed while the helper was off, on every volume that was not scanned whole.
    fn catch_up(&mut self) {
        let scanning: FxHashSet<EntryId> = self.pending.iter().copied().collect();
        for v in 0..self.index.volumes().len() {
            let volume = u16::try_from(v).expect("fewer than 65536 volumes");
            let info = &self.index.volumes()[v];
            if !self.index.is_alive(info.root) || scanning.contains(&info.root) {
                continue;
            }
            let path = info.path.clone();
            let dirs = if info.btrfs {
                match File::open(&path).and_then(|root| catchup::changed_on_btrfs(&self.index, volume, &root)) {
                    Ok((dirs, committed)) => {
                        self.index.volume_mut(volume).generation = committed;
                        dirs
                    }
                    Err(_) => catchup::changed_by_mtime(&self.index, volume),
                }
            } else {
                catchup::changed_by_mtime(&self.index, volume)
            };
            self.dirty.extend(dirs);
        }
        // Due at once: the catch-up has no burst to wait for.
        self.dirty_since = Instant::now().checked_sub(SETTLE).or_else(|| Some(Instant::now()));
        self.full_catch_up = false;
    }

    /// Runs until no client has been there for the idle time, or a stop signal; then saves the index if it changed.
    ///
    /// # Errors
    ///
    /// The error of saving the index.
    pub fn run(&mut self) -> io::Result<()> {
        loop {
            if sys::stop_requested() {
                break;
            }
            let busy = !self.pending.is_empty() || self.dirty_ready() || self.full_catch_up;
            if !busy && self.clients.is_empty() && self.alone_since.elapsed() >= self.setup.config.idle {
                break;
            }
            let timeout = if busy {
                0
            } else if let Some(since) = self.dirty_since {
                millis(SETTLE.saturating_sub(since.elapsed()))
            } else if self.clients.is_empty() {
                millis(self.setup.config.idle.saturating_sub(self.alone_since.elapsed()))
            } else {
                -1
            };
            let mut fds = vec![self.setup.listener.as_raw_fd()];
            fds.push(self.watch.as_ref().map_or(-1, Watch::fd));
            fds.extend(self.clients.iter().map(|c| c.fd.as_raw_fd()));
            let ready = match sys::poll_readable(&fds, timeout) {
                Ok(ready) => ready,
                Err(e) if e.kind() == io::ErrorKind::Interrupted => continue,
                Err(e) => return Err(e),
            };
            if ready[0] {
                self.accept();
            }
            if ready[1] {
                self.hear();
            }
            for (i, &is_ready) in ready.iter().enumerate().skip(2).rev() {
                if is_ready {
                    self.serve(i - 2);
                }
            }
            if self.full_catch_up {
                self.catch_up();
            }
            if self.dirty_ready() {
                self.refresh();
            }
            if !self.pending.is_empty() {
                self.state = if self.state == State::CatchingUp { State::CatchingUp } else { State::Indexing };
                self.scan_wave();
                if self.pending.is_empty() {
                    self.index.compact_if_wasteful();
                    self.notify();
                }
            }
            if self.pending.is_empty() && self.dirty.is_empty() {
                self.state = State::Ready;
            }
        }
        self.save()
    }

    fn dirty_ready(&self) -> bool {
        !self.dirty.is_empty() && self.dirty_since.is_some_and(|since| since.elapsed() >= SETTLE)
    }

    fn accept(&mut self) {
        while let Ok(fd) = sys::accept(self.setup.listener.as_raw_fd()) {
            let Ok((uid, gid, groups)) = sys::peer(fd.as_raw_fd()) else { continue };
            self.clients.push(Client { fd, viewer: Viewer { uid, gid, groups }, hello: false });
        }
    }

    /// Reads fanotify's events into the folders to read again.
    fn hear(&mut self) {
        let Some(watch) = self.watch.as_ref() else { return };
        for change in watch.read() {
            match change {
                Change::Overflow => self.full_catch_up = true,
                Change::Attributes { .. } => self.access.clear(),
                Change::Folder { dev, ino } => {
                    let volume = self.index.volumes().iter().position(|v| v.dev == dev && self.index.is_alive(v.root));
                    if let Some(dir) = volume.and_then(|v| self.index.dir_by_ino(u16::try_from(v).ok()?, ino)) {
                        self.dirty.insert(dir);
                        self.dirty_since.get_or_insert_with(Instant::now);
                    }
                }
            }
        }
    }

    /// Reads the changed folders again, in one batch, and queues the new folders for scanning.
    fn refresh(&mut self) {
        let dirs: Vec<(EntryId, PathBuf)> =
            self.dirty.drain().filter(|&d| self.index.is_alive(d)).map(|d| (d, self.index.path(d))).collect();
        self.dirty_since = None;
        let skip = |p: &Path| self.setup.config.excludes(p);
        let listings = read_wave(&dirs, &skip, self.setup.threads);
        let mut batch = Vec::new();
        for ((dir, _), listing) in dirs.into_iter().zip(listings) {
            if let Some(listing) = listing {
                self.index.set_mtime(dir, listing.mtime);
                batch.push((dir, listing.entries));
            }
        }
        if batch.is_empty() {
            return;
        }
        self.pending.extend(self.index.apply_batch(batch));
        self.access.clear();
        self.changed = true;
        self.notify();
    }

    fn scan_wave(&mut self) {
        let config = &self.setup.config;
        let skip = |p: &Path| config.excludes(p);
        let edges = scan::scan_step(&mut self.index, &mut self.pending, &skip, self.setup.threads);
        self.changed = true;
        // A folder on another device that no volume covers is a nested btrfs subvolume: index it as a volume of its
        // own, unless it is a read-only snapshot.
        for (_, path) in edges.other_devices {
            if self.index.volumes().iter().any(|v| v.path == path) || config.excludes(&path) {
                continue;
            }
            let Ok(handle) = File::open(&path) else { continue };
            if !btrfs::is_btrfs(&handle).unwrap_or(false) || btrfs::is_read_only(&handle).unwrap_or(true) {
                continue;
            }
            let Ok(meta) = handle.metadata() else { continue };
            let v = self.index.add_volume(&path, meta.ino());
            let info = self.index.volume_mut(v);
            info.dev = meta.dev();
            info.btrfs = true;
            info.fsid = sys::fsid(&path).map_or(0, u64::from_ne_bytes);
            info.generation = btrfs::commit(&handle).unwrap_or(0);
            self.pending.push(self.index.volumes()[v as usize].root);
        }
    }

    fn notify(&mut self) {
        self.clients.retain(|c| !c.hello || sys::send(c.fd.as_raw_fd(), b"changed\n").is_ok());
    }

    /// Reads and answers a client's messages; drops it when it left.
    fn serve(&mut self, at: usize) {
        let mut buffer = vec![0u8; MESSAGE_MAX];
        loop {
            match sys::receive(self.clients[at].fd.as_raw_fd(), &mut buffer) {
                Ok(Some(n)) => {
                    let message = String::from_utf8_lossy(&buffer[..n]).into_owned();
                    let reply = self.handle(at, &message);
                    if let Some(reply) = reply {
                        let _ = sys::send(self.clients[at].fd.as_raw_fd(), &reply);
                    }
                }
                Ok(None) => {
                    self.clients.remove(at);
                    if self.clients.is_empty() {
                        self.alone_since = Instant::now();
                    }
                    return;
                }
                Err(_) => return,
            }
        }
    }

    fn handle(&mut self, at: usize, message: &str) -> Option<Vec<u8>> {
        let fields: Vec<&str> = message.trim_end_matches('\n').split('\t').collect();
        match fields.first().copied() {
            Some("hello") => {
                self.clients[at].hello = true;
                None
            }
            Some("query") if fields.len() >= 7 => Some(self.answer(at, &fields)),
            _ => None,
        }
    }

    fn answer(&mut self, at: usize, fields: &[&str]) -> Vec<u8> {
        let id = fields[1];
        let limit = fields[2].parse::<usize>().unwrap_or(200).min(LIMIT_MAX);
        let now = fields[3].parse::<i64>().unwrap_or(0);
        let today = fields[4].parse::<i64>().unwrap_or(0);
        let text = fields[6..].join("\t");
        let state = match self.state {
            State::Ready => "ready",
            State::Indexing => "indexing",
            State::CatchingUp => "catching-up",
        };
        let header = |total: usize, more: bool, error: &str| {
            format!("result\t{id}\t{total}\t{}\t{state}\t{error}\n", u8::from(more))
        };
        let query = match query::parse(&text) {
            Ok(query) => query,
            Err(error) => return header(0, false, &error).into_bytes(),
        };
        let scope = if fields[5].is_empty() {
            None
        } else {
            match self.index.lookup(Path::new(fields[5])) {
                Some(entry) => Some(entry),
                None => return header(0, false, "not-indexed").into_bytes(),
            }
        };
        if query.is_empty() {
            return header(0, false, "").into_bytes();
        }
        let viewer = self.clients[at].viewer.clone();
        let Ok(_guard) = Access::as_viewer(&viewer) else {
            return header(0, false, "credentials").into_bytes();
        };
        let (index, access) = (&self.index, &mut self.access);
        let mut visible = |id: EntryId| access.visible(index, &viewer, id);
        let mut meta =
            |id: EntryId| fs::symlink_metadata(index.path(id)).ok().map(|m| Meta { size: m.size(), mtime: m.mtime() });
        let mut context = Context { scope, now, today, visible: &mut visible, meta: &mut meta };
        let found = query.run(index, &mut context, limit);
        let mut out = header(found.total, found.more, "").into_bytes();
        for id in found.hits {
            out.push(match index.kind(id) {
                Kind::Dir => b'd',
                Kind::File => b'f',
                Kind::Link => b'l',
                Kind::Other => b'o',
            });
            out.push(b'\t');
            out.extend_from_slice(index.path(id).as_os_str().as_bytes());
            out.push(0);
            if out.len() > MESSAGE_MAX - 4096 {
                break;
            }
        }
        out
    }

    fn save(&self) -> io::Result<()> {
        if !self.changed {
            return Ok(());
        }
        store::save(&self.index, &self.setup.state_dir.join("index"))
    }
}

fn millis(duration: Duration) -> i32 {
    i32::try_from(duration.as_millis()).unwrap_or(i32::MAX)
}
