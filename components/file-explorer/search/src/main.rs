// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora-search`: File Explorer's whole-disk name search helper. systemd starts it on the first connection to
//! `/run/aurora/search.sock` (socket activation) and it leaves after a few idle minutes. See docs/search.md.
//!
//! Usage: aurora-search [--socket PATH] [--state DIR] [--config FILE] [--threads N]
//! Without `--socket` it takes the listening socket systemd passes (`LISTEN_FDS`, descriptor 3).

use std::path::PathBuf;
use std::process::ExitCode;

use aurora_search::config::Config;
use aurora_search::server::{Server, Setup};

fn main() -> ExitCode {
    let mut args = std::env::args().skip(1);
    let mut socket: Option<PathBuf> = None;
    let mut state =
        std::env::var_os("STATE_DIRECTORY").map_or_else(|| PathBuf::from("/var/lib/aurora/search"), PathBuf::from);
    let mut config = PathBuf::from("/etc/aurora/search.conf");
    let mut threads = std::thread::available_parallelism().map_or(2, |n| n.get().min(8));
    while let Some(arg) = args.next() {
        match (arg.as_str(), args.next()) {
            ("--socket", Some(v)) => socket = Some(PathBuf::from(v)),
            ("--state", Some(v)) => state = PathBuf::from(v),
            ("--config", Some(v)) => config = PathBuf::from(v),
            ("--threads", Some(v)) => threads = v.parse().unwrap_or(threads),
            ("--version", None) => {
                println!("aurora-search {}", env!("CARGO_PKG_VERSION"));
                return ExitCode::SUCCESS;
            }
            _ => {
                eprintln!("usage: aurora-search [--socket PATH] [--state DIR] [--config FILE] [--threads N]");
                return ExitCode::from(2);
            }
        }
    }
    let listener = if let Some(path) = socket {
        let _ = std::fs::remove_file(&path);
        match aurora_search::server_socket(&path) {
            Ok(fd) => fd,
            Err(e) => {
                eprintln!("aurora-search: {}: {e}", path.display());
                return ExitCode::FAILURE;
            }
        }
    } else if let Some(fd) = aurora_search::systemd_socket() {
        fd
    } else {
        eprintln!("aurora-search: no socket: start it through aurora-search.socket, or pass --socket");
        return ExitCode::from(2);
    };
    if let Err(e) = std::fs::create_dir_all(&state) {
        eprintln!("aurora-search: {}: {e}", state.display());
        return ExitCode::FAILURE;
    }
    aurora_search::catch_stop_signals();
    let setup = Setup { listener, state_dir: state, config: Config::load(&config), threads };
    let mut server = Server::start(setup);
    match server.run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("aurora-search: {e}");
            ExitCode::FAILURE
        }
    }
}
