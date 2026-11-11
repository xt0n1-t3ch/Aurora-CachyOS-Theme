// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! `aurora-search-query`: asks the helper from a terminal, as File Explorer does.
//!
//! Usage: aurora-search-query [--socket PATH] [--scope DIR] [--limit N] [--wait-ready] TEXT...
//! Prints the total, the state, then one match per line (kind letter, tab, path). `--wait-ready` asks again until
//! the helper says its index is ready.

use std::path::PathBuf;
use std::process::ExitCode;

use aurora_search::client::Client;

fn main() -> ExitCode {
    let mut socket = PathBuf::from("/run/aurora/search.sock");
    let mut scope: Option<PathBuf> = None;
    let mut limit = 50;
    let mut wait = false;
    let mut words = Vec::new();
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--socket" => socket = args.next().map_or(socket, PathBuf::from),
            "--scope" => scope = args.next().map(PathBuf::from),
            "--limit" => limit = args.next().and_then(|v| v.parse().ok()).unwrap_or(limit),
            "--wait-ready" => wait = true,
            "--version" => {
                println!("aurora-search-query {}", env!("CARGO_PKG_VERSION"));
                return ExitCode::SUCCESS;
            }
            _ => words.push(arg),
        }
    }
    let mut client = match Client::connect(&socket) {
        Ok(client) => client,
        Err(e) => {
            eprintln!("aurora-search-query: {}: {e}", socket.display());
            return ExitCode::FAILURE;
        }
    };
    loop {
        let answer = match client.query(&words.join(" "), scope.as_deref(), limit) {
            Ok(answer) => answer,
            Err(e) => {
                eprintln!("aurora-search-query: {e}");
                return ExitCode::FAILURE;
            }
        };
        if wait && answer.state != "ready" {
            std::thread::sleep(std::time::Duration::from_millis(100));
            continue;
        }
        println!("{}{}\t{}\t{}", answer.total, if answer.more { "+" } else { "" }, answer.state, answer.error);
        for (kind, path) in answer.hits {
            println!("{kind}\t{}", path.display());
        }
        return if answer.error.is_empty() { ExitCode::SUCCESS } else { ExitCode::from(3) };
    }
}
