// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The helper from start to exit, as root in the virtual machine of `tests/vm.sh`, on a fresh btrfs at
//! `AURORA_SEARCH_BTRFS`: it indexes, answers, follows changes live, hides what a user may not reach, leaves when
//! idle, and on the next start finds what changed meanwhile.

use std::fs;
use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use std::time::{Duration, Instant};

use aurora_search::client::{Answer, Client};

const HELPER: &str = env!("CARGO_BIN_EXE_aurora-search");
const QUERY: &str = env!("CARGO_BIN_EXE_aurora-search-query");

fn root() -> PathBuf {
    PathBuf::from(std::env::var("AURORA_SEARCH_BTRFS").expect("AURORA_SEARCH_BTRFS names a btrfs mount"))
}

fn start(state: &Path, socket: &Path) -> Child {
    let config = state.join("search.conf");
    fs::write(&config, "idle_seconds = 3\n").unwrap();
    let child = Command::new(HELPER)
        .args([
            "--socket",
            socket.to_str().unwrap(),
            "--state",
            state.to_str().unwrap(),
            "--config",
            config.to_str().unwrap(),
        ])
        .spawn()
        .unwrap();
    let deadline = Instant::now() + Duration::from_secs(10);
    while !socket.exists() {
        assert!(Instant::now() < deadline, "the helper made its socket");
        std::thread::sleep(Duration::from_millis(20));
    }
    child
}

/// Asks until the helper says ready and the answer satisfies `want`, for at most a few seconds.
fn ask_until(client: &mut Client, text: &str, want: impl Fn(&Answer) -> bool) -> Answer {
    let deadline = Instant::now() + Duration::from_secs(10);
    loop {
        let answer = client.query(text, None, 100).unwrap();
        if answer.state == "ready" && want(&answer) {
            return answer;
        }
        assert!(Instant::now() < deadline, "no answer to {text:?} as wanted; last: {answer:?}");
        std::thread::sleep(Duration::from_millis(50));
    }
}

fn paths(answer: &Answer) -> Vec<PathBuf> {
    answer.hits.iter().map(|(_, p)| p.clone()).collect()
}

#[test]
#[ignore = "needs root on a scratch btrfs: tests/vm.sh"]
fn the_helper_indexes_follows_hides_leaves_and_catches_up() {
    let disk = root().join("helper");
    fs::create_dir_all(disk.join("docs")).unwrap();
    fs::create_dir_all(disk.join("photos/2026")).unwrap();
    fs::write(disk.join("docs/Report 2026.pdf"), b"x").unwrap();
    fs::write(disk.join("photos/2026/beach.jpg"), b"x").unwrap();
    for i in 0..2000 {
        fs::write(disk.join(format!("photos/2026/img{i:04}.jpg")), b"").unwrap();
    }
    fs::create_dir_all(disk.join("secret")).unwrap();
    fs::write(disk.join("secret/secret-plan.txt"), b"x").unwrap();
    fs::set_permissions(disk.join("secret"), fs::Permissions::from_mode(0o700)).unwrap();

    let state = PathBuf::from("/tmp/aurora-search-state");
    fs::create_dir_all(&state).unwrap();
    let socket = state.join("search.sock");
    let mut helper = start(&state, &socket);
    let mut client = Client::connect(&socket).unwrap();
    client.hello().unwrap();

    // Indexed: a word finds its file, case aside.
    let answer = ask_until(&mut client, "report", |a| !a.hits.is_empty());
    assert_eq!(paths(&answer), [disk.join("docs/Report 2026.pdf")]);
    let answer = ask_until(&mut client, "ext:jpg", |a| a.total == 2001);
    assert!(!answer.more);

    // Live: created, renamed and removed names show within moments, announced by a change notice.
    fs::create_dir_all(disk.join("new")).unwrap();
    fs::write(disk.join("new/fresh-name.txt"), b"x").unwrap();
    ask_until(&mut client, "fresh-name", |a| paths(a) == [disk.join("new/fresh-name.txt")]);
    fs::rename(disk.join("new"), disk.join("docs/moved")).unwrap();
    ask_until(&mut client, "fresh-name", |a| paths(a) == [disk.join("docs/moved/fresh-name.txt")]);
    fs::remove_file(disk.join("docs/moved/fresh-name.txt")).unwrap();
    ask_until(&mut client, "fresh-name", |a| a.total == 0);

    // Root sees the secret; a user who cannot enter its folder does not, and its count does not give it away.
    ask_until(&mut client, "secret-plan", |a| a.total == 1);
    let as_nobody = Command::new("setpriv")
        .args([
            "--reuid=65534",
            "--regid=65534",
            "--clear-groups",
            QUERY,
            "--socket",
            socket.to_str().unwrap(),
            "secret-plan",
        ])
        .output()
        .unwrap();
    let printed = String::from_utf8_lossy(&as_nobody.stdout);
    assert!(as_nobody.status.success(), "{printed} {}", String::from_utf8_lossy(&as_nobody.stderr));
    assert!(printed.starts_with("0\t"), "nobody saw: {printed}");
    let as_nobody = Command::new("setpriv")
        .args([
            "--reuid=65534",
            "--regid=65534",
            "--clear-groups",
            QUERY,
            "--socket",
            socket.to_str().unwrap(),
            "beach",
        ])
        .output()
        .unwrap();
    assert!(String::from_utf8_lossy(&as_nobody.stdout).contains("beach.jpg"), "nobody sees what everyone may");

    // A scope keeps one folder; a folder the index does not hold says so.
    let scoped = client.query("img", Some(&disk.join("docs")), 10).unwrap();
    assert_eq!(scoped.total, 0);
    assert_eq!(client.query("x", Some(Path::new("/proc")), 10).unwrap().error, "not-indexed");

    // Idle: alone for its idle time, the helper saves and leaves.
    drop(client);
    let deadline = Instant::now() + Duration::from_secs(15);
    while helper.try_wait().unwrap().is_none() {
        assert!(Instant::now() < deadline, "the helper left when idle");
        std::thread::sleep(Duration::from_millis(100));
    }
    assert!(helper.wait().unwrap().success());
    assert!(state.join("index").exists());

    // Off: names change while no helper runs; the next start finds them without reading everything again.
    fs::write(disk.join("photos/while-off.txt"), b"x").unwrap();
    fs::remove_file(disk.join("photos/2026/beach.jpg")).unwrap();
    fs::remove_file(&socket).ok();
    let mut helper = start(&state, &socket);
    let mut client = Client::connect(&socket).unwrap();
    let started = Instant::now();
    ask_until(&mut client, "while-off", |a| paths(a) == [disk.join("photos/while-off.txt")]);
    assert!(started.elapsed() < Duration::from_secs(3), "the catch-up took {:?}", started.elapsed());
    assert_eq!(client.query("beach", None, 10).unwrap().total, 0);
    drop(client);
    helper.kill().ok();
    helper.wait().ok();
}
