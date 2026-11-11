// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! A client of the helper's socket, for the `aurora-search-query` command and the tests. File Explorer speaks the same
//! protocol from C++ (see `server`).

use std::io;
use std::os::fd::{AsRawFd, OwnedFd};
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};

/// One answer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Answer {
    pub total: usize,
    pub more: bool,
    pub state: String,
    pub error: String,
    /// Kind letter and path of each match, best first.
    pub hits: Vec<(char, PathBuf)>,
}

/// A connection to the helper.
pub struct Client {
    fd: OwnedFd,
    next_id: u64,
}

impl Client {
    /// Connects to the helper's socket.
    ///
    /// # Errors
    ///
    /// The `socket` or `connect` error.
    pub fn connect(path: &Path) -> io::Result<Self> {
        let fd = crate::sys::connect_seqpacket(path)?;
        Ok(Self { fd, next_id: 1 })
    }

    /// Asks to hear about changes.
    ///
    /// # Errors
    ///
    /// The `send` error.
    pub fn hello(&self) -> io::Result<()> {
        crate::sys::send(self.fd.as_raw_fd(), b"hello")
    }

    /// Asks a query and waits for its answer, skipping change notices.
    ///
    /// # Errors
    ///
    /// The socket's error, or `UnexpectedEof` when the helper left.
    pub fn query(&mut self, text: &str, scope: Option<&Path>, limit: usize) -> io::Result<Answer> {
        let id = self.next_id;
        self.next_id += 1;
        let now = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_secs());
        let mut message = format!("query\t{id}\t{limit}\t{now}\t{}\t", now - now % 86_400).into_bytes();
        message.extend_from_slice(scope.map_or(&[][..], |s| s.as_os_str().as_bytes()));
        message.push(b'\t');
        message.extend_from_slice(text.as_bytes());
        crate::sys::send(self.fd.as_raw_fd(), &message)?;
        loop {
            let reply = self.receive()?;
            if let Some(answer) = parse_answer(&reply, id) {
                return Ok(answer);
            }
        }
    }

    /// Waits for the next message; true when it is a change notice.
    ///
    /// # Errors
    ///
    /// The socket's error, or `UnexpectedEof` when the helper left.
    pub fn wait_changed(&self) -> io::Result<bool> {
        Ok(self.receive()? == b"changed\n")
    }

    fn receive(&self) -> io::Result<Vec<u8>> {
        let mut buffer = vec![0u8; 128 * 1024];
        loop {
            crate::sys::poll_readable(&[self.fd.as_raw_fd()], -1)?;
            match crate::sys::receive(self.fd.as_raw_fd(), &mut buffer) {
                Ok(Some(n)) => return Ok(buffer[..n].to_vec()),
                Ok(None) => return Err(io::Error::from(io::ErrorKind::UnexpectedEof)),
                Err(e) if e.kind() == io::ErrorKind::WouldBlock => {}
                Err(e) => return Err(e),
            }
        }
    }
}

/// The answer in `reply` to query `id`; `None` for anything else.
#[must_use]
pub fn parse_answer(reply: &[u8], id: u64) -> Option<Answer> {
    let newline = reply.iter().position(|&b| b == b'\n')?;
    let header = String::from_utf8_lossy(&reply[..newline]);
    let fields: Vec<&str> = header.split('\t').collect();
    if fields.len() < 6 || fields[0] != "result" || fields[1].parse::<u64>().ok()? != id {
        return None;
    }
    let hits = reply[newline + 1..]
        .split(|&b| b == 0)
        .filter(|r| r.len() > 2 && r[1] == b'\t')
        .map(|r| (char::from(r[0]), PathBuf::from(std::ffi::OsStr::from_bytes(&r[2..]))))
        .collect();
    Some(Answer {
        total: fields[2].parse().ok()?,
        more: fields[3] == "1",
        state: fields[4].to_string(),
        error: fields[5].to_string(),
        hits,
    })
}
