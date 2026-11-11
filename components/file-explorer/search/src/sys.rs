// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The kernel calls the crate makes through `libc`, each behind a safe function. Nothing else in the crate is unsafe.

use std::ffi::CString;
use std::io;
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

/// The `f_type` of the filesystem `fd` is on (`statfs(2)`).
///
/// # Errors
///
/// The `fstatfs` error.
pub fn fstatfs_type(fd: RawFd) -> io::Result<i64> {
    let mut stat = std::mem::MaybeUninit::<libc::statfs>::zeroed();
    // SAFETY: `stat` is a valid, writable `statfs` for the call; `fd` is borrowed from a live `File` by the caller.
    let result = unsafe { libc::fstatfs(fd, stat.as_mut_ptr()) };
    if result != 0 {
        return Err(io::Error::last_os_error());
    }
    // SAFETY: `fstatfs` returned 0, so it filled the struct.
    Ok(unsafe { stat.assume_init() }.f_type)
}

/// Hands the heap's free pages back to the system (glibc's `malloc_trim(3)`). A scan frees its listing buffers
/// in many small pieces that glibc keeps for reuse; without this they would stay in the helper's resident memory.
pub fn give_back_freed_memory() {
    // SAFETY: `malloc_trim` takes no pointers and only releases pages glibc's allocator owns and no one uses.
    unsafe {
        libc::malloc_trim(0);
    }
}

/// Lowers the calling thread to the idle I/O class and nice 19, so its reading never delays the user's programs.
/// Linux keeps both per thread (`ioprio_set` with `IOPRIO_WHO_PROCESS` and `setpriority` on a thread id).
pub fn lower_this_thread() {
    // From linux/ioprio.h: IOPRIO_CLASS_IDLE = 3, shifted by IOPRIO_CLASS_SHIFT = 13; IOPRIO_WHO_PROCESS = 1.
    const IOPRIO_IDLE: libc::c_long = 3 << 13;
    // SAFETY: plain syscalls on the calling thread (id 0 for ioprio, gettid for setpriority); no memory is passed.
    unsafe {
        let tid = libc::gettid();
        libc::syscall(libc::SYS_ioprio_set, 1 as libc::c_long, 0 as libc::c_long, IOPRIO_IDLE);
        libc::setpriority(libc::PRIO_PROCESS, libc::id_t::try_from(tid).unwrap_or(0), 19);
    }
}

/// Calls ioctl `request` on `fd` with `args` as the argument structure, read and written in place.
///
/// The caller sizes `args` to at least the structure the request names: the size is encoded in `request`, and the
/// kernel reads and writes no more than that, plus the buffer size the structure itself declares (which the caller
/// sets to the bytes that follow it in `args`).
///
/// # Errors
///
/// The ioctl's error.
pub fn ioctl_bytes(fd: RawFd, request: u64, args: &mut [u8]) -> io::Result<()> {
    let size = usize::try_from((request >> 16) & 0x3fff).unwrap_or(usize::MAX);
    if args.len() < size {
        return Err(io::Error::from(io::ErrorKind::InvalidInput));
    }
    // SAFETY: `args` is a live, writable buffer at least as large as the structure `request` encodes, and the
    // caller keeps any trailing buffer the structure declares inside `args` (see above).
    let result = unsafe { libc::ioctl(fd, request as libc::Ioctl, args.as_mut_ptr().cast::<libc::c_void>()) };
    if result < 0 { Err(io::Error::last_os_error()) } else { Ok(()) }
}

fn path_c(path: &Path) -> io::Result<CString> {
    CString::new(path.as_os_str().as_bytes()).map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))
}

fn check(result: libc::c_int) -> io::Result<libc::c_int> {
    if result < 0 { Err(io::Error::last_os_error()) } else { Ok(result) }
}

/// `f_fsid` of the filesystem `path` is on, as the 8 bytes fanotify reports in its events (`__kernel_fsid_t`).
///
/// # Errors
///
/// The `statfs` error.
pub fn fsid(path: &Path) -> io::Result<[u8; 8]> {
    let c = path_c(path)?;
    let mut stat = std::mem::MaybeUninit::<libc::statfs>::zeroed();
    // SAFETY: `c` is a NUL-terminated path and `stat` a writable `statfs`.
    check(unsafe { libc::statfs(c.as_ptr(), stat.as_mut_ptr()) })?;
    // SAFETY: `statfs` succeeded, so `f_fsid` is filled; `fsid_t` is two `c_int`, 8 bytes with no padding, and its
    // fields are private in `libc`, so its bytes are read whole.
    Ok(unsafe { std::ptr::addr_of!((*stat.as_ptr()).f_fsid).cast::<[u8; 8]>().read() })
}

/// A new fanotify group (`fanotify_init(2)`).
///
/// # Errors
///
/// `EPERM` without `CAP_SYS_ADMIN`, `EINVAL` on a kernel without the flags.
pub fn fanotify_init(flags: u32, event_flags: u32) -> io::Result<OwnedFd> {
    // SAFETY: plain syscall with integer arguments.
    let fd = check(unsafe { libc::fanotify_init(flags, event_flags) })?;
    // SAFETY: `fd` was just returned by the kernel and is owned by nobody else.
    Ok(unsafe { OwnedFd::from_raw_fd(fd) })
}

/// Adds a mark to a fanotify group (`fanotify_mark(2)`).
///
/// # Errors
///
/// The kernel's error for the mark.
pub fn fanotify_mark(group: RawFd, flags: u32, mask: u64, path: &Path) -> io::Result<()> {
    let c = path_c(path)?;
    // SAFETY: `c` is a NUL-terminated path; the other arguments are integers.
    check(unsafe { libc::fanotify_mark(group, flags, mask, libc::AT_FDCWD, c.as_ptr()) })?;
    Ok(())
}

/// Opens the file a handle from a fanotify event names, on the filesystem of `mount` (`open_by_handle_at(2)`), as an
/// `O_PATH` descriptor. `handle` is `struct file_handle` as the event carries it: size, type, then the bytes.
///
/// # Errors
///
/// `ESTALE` when the file is gone; `EPERM` without `CAP_DAC_READ_SEARCH`.
pub fn open_by_handle(mount: RawFd, handle: &[u8]) -> io::Result<OwnedFd> {
    if handle.len() < 8 {
        return Err(io::Error::from(io::ErrorKind::InvalidInput));
    }
    // The kernel reads `handle_bytes` from the struct; it must fit inside the slice.
    let declared = u32::from_ne_bytes(handle[..4].try_into().expect("four bytes")) as usize;
    if declared + 8 > handle.len() {
        return Err(io::Error::from(io::ErrorKind::InvalidInput));
    }
    let mut copy = handle.to_vec();
    // SAFETY: `copy` holds a whole `struct file_handle` whose declared size fits in it (checked above), and the kernel
    // reads it only; the flags are plain integers.
    let fd =
        unsafe { libc::syscall(libc::SYS_open_by_handle_at, mount, copy.as_mut_ptr(), libc::O_PATH | libc::O_CLOEXEC) };
    let fd = check(libc::c_int::try_from(fd).unwrap_or(-1))?;
    // SAFETY: `fd` was just returned by the kernel.
    Ok(unsafe { OwnedFd::from_raw_fd(fd) })
}

/// `st_dev` and `st_ino` of an open descriptor.
///
/// # Errors
///
/// The `fstat` error.
pub fn dev_ino(fd: RawFd) -> io::Result<(u64, u64)> {
    let mut stat = std::mem::MaybeUninit::<libc::stat>::zeroed();
    // SAFETY: `stat` is a writable `stat` struct.
    check(unsafe { libc::fstat(fd, stat.as_mut_ptr()) })?;
    // SAFETY: `fstat` succeeded.
    let stat = unsafe { stat.assume_init() };
    Ok((stat.st_dev, stat.st_ino))
}

/// The user, group and supplementary groups of the process at the other end of a Unix socket (`SO_PEERCRED`,
/// `SO_PEERGROUPS` since Linux 4.13).
///
/// # Errors
///
/// The `getsockopt` error.
pub fn peer(socket: RawFd) -> io::Result<(u32, u32, Vec<u32>)> {
    let mut cred = libc::ucred { pid: 0, uid: 0, gid: 0 };
    let mut size = libc::socklen_t::try_from(std::mem::size_of::<libc::ucred>()).unwrap_or(0);
    // SAFETY: `cred` and `size` are writable and `size` is the size of `cred`.
    check(unsafe {
        libc::getsockopt(socket, libc::SOL_SOCKET, libc::SO_PEERCRED, (&raw mut cred).cast(), &raw mut size)
    })?;
    let mut groups = vec![0u32; 256];
    let mut size = libc::socklen_t::try_from(groups.len() * 4).unwrap_or(0);
    // SAFETY: `groups` has room for `size` bytes, which the kernel writes at most (it says ERANGE otherwise).
    let ok = unsafe {
        libc::getsockopt(socket, libc::SOL_SOCKET, libc::SO_PEERGROUPS, groups.as_mut_ptr().cast(), &raw mut size)
    };
    if ok < 0 {
        return Err(io::Error::last_os_error());
    }
    groups.truncate(size as usize / 4);
    Ok((cred.uid, cred.gid, groups))
}

/// Makes the calling thread, and only it, check file access as another user: `setfsuid`, `setfsgid` and `setgroups`
/// through raw syscalls, which change the calling thread's credentials only (glibc's wrappers change every thread's).
/// A thread whose fsuid leaves 0 loses its filesystem capabilities until it comes back (capabilities(7)).
///
/// # Errors
///
/// The `setgroups` error; `EPERM` without `CAP_SETGID`.
pub fn act_as(uid: u32, gid: u32, groups: &[u32]) -> io::Result<()> {
    // SAFETY: `groups` is a live slice of gid_t for the length given; the other calls take integers.
    unsafe {
        if libc::syscall(libc::SYS_setgroups, groups.len(), groups.as_ptr()) < 0 {
            return Err(io::Error::last_os_error());
        }
        libc::syscall(libc::SYS_setfsgid, libc::c_long::from(gid));
        libc::syscall(libc::SYS_setfsuid, libc::c_long::from(uid));
    }
    // setfsuid reports no error; read it back to be sure the switch happened.
    // SAFETY: -1 is not a valid id, so this call only returns the current fsuid.
    let now = unsafe { libc::syscall(libc::SYS_setfsuid, -1 as libc::c_long) };
    if now == libc::c_long::from(uid) { Ok(()) } else { Err(io::Error::from(io::ErrorKind::PermissionDenied)) }
}

/// Whether the process runs as root, as the system service does; tests and development run it as the user.
#[must_use]
pub fn is_root() -> bool {
    // SAFETY: plain syscall.
    unsafe { libc::geteuid() == 0 }
}

/// Whether the calling thread's filesystem identity may access `path` with `mode` (`faccessat2` with `AT_EACCESS`,
/// so the fsuid, fsgid, groups and ACLs decide).
#[must_use]
pub fn may_access(path: &Path, mode: libc::c_int) -> bool {
    let Ok(c) = path_c(path) else { return false };
    // SAFETY: `c` is NUL-terminated; the other arguments are integers.
    unsafe { libc::syscall(libc::SYS_faccessat2, libc::AT_FDCWD, c.as_ptr(), mode, libc::AT_EACCESS) == 0 }
}

/// Waits for any of `fds` to be readable, or for `timeout_ms` (negative waits for ever); returns which are.
///
/// # Errors
///
/// The `poll` error, `EINTR` included (a signal arrived).
pub fn poll_readable(fds: &[RawFd], timeout_ms: i32) -> io::Result<Vec<bool>> {
    let mut polls: Vec<libc::pollfd> =
        fds.iter().map(|&fd| libc::pollfd { fd, events: libc::POLLIN | libc::POLLPRI, revents: 0 }).collect();
    // SAFETY: `polls` is a live array of the length given.
    check(unsafe { libc::poll(polls.as_mut_ptr(), polls.len() as libc::nfds_t, timeout_ms) })?;
    Ok(polls.iter().map(|p| p.revents != 0).collect())
}

static STOP: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

extern "C" fn on_stop(_: libc::c_int) {
    STOP.store(true, std::sync::atomic::Ordering::SeqCst);
}

/// Makes SIGTERM and SIGINT set a flag (read with [`stop_requested`]) and interrupt a waiting `poll`.
pub fn catch_stop_signals() {
    // SAFETY: the handler only stores to an atomic, which is async-signal-safe; `sigaction` is filled whole.
    unsafe {
        let mut action: libc::sigaction = std::mem::zeroed();
        action.sa_sigaction = on_stop as extern "C" fn(libc::c_int) as usize;
        libc::sigemptyset(&raw mut action.sa_mask);
        libc::sigaction(libc::SIGTERM, &raw const action, std::ptr::null_mut());
        libc::sigaction(libc::SIGINT, &raw const action, std::ptr::null_mut());
    }
}

/// Whether SIGTERM or SIGINT arrived.
#[must_use]
pub fn stop_requested() -> bool {
    STOP.load(std::sync::atomic::Ordering::SeqCst)
}

/// A listening `SOCK_SEQPACKET` Unix socket at `path`, for running without systemd (tests, development).
///
/// # Errors
///
/// The error of `socket`, `bind` or `listen`.
pub fn listen_seqpacket(path: &Path) -> io::Result<OwnedFd> {
    let (owned, address, length) = seqpacket_at(path)?;
    // SAFETY: `address` is a filled sockaddr_un of the length given.
    check(unsafe { libc::bind(owned.as_raw_fd(), (&raw const address).cast(), length) })?;
    // Every user may connect, as the systemd socket allows (SocketMode=0666); the helper checks who asks.
    std::fs::set_permissions(path, std::os::unix::fs::PermissionsExt::from_mode(0o666))?;
    // SAFETY: plain syscall.
    check(unsafe { libc::listen(owned.as_raw_fd(), 16) })?;
    Ok(owned)
}

/// A `SOCK_SEQPACKET` Unix socket connected to `path`.
///
/// # Errors
///
/// The error of `socket` or `connect`.
pub fn connect_seqpacket(path: &Path) -> io::Result<OwnedFd> {
    let (owned, address, length) = seqpacket_at(path)?;
    // SAFETY: `address` is a filled sockaddr_un of the length given.
    check(unsafe { libc::connect(owned.as_raw_fd(), (&raw const address).cast(), length) })?;
    Ok(owned)
}

/// A new `SOCK_SEQPACKET` Unix socket and the address of `path`.
fn seqpacket_at(path: &Path) -> io::Result<(OwnedFd, libc::sockaddr_un, libc::socklen_t)> {
    // SAFETY: plain syscall with integer arguments.
    let fd = check(unsafe { libc::socket(libc::AF_UNIX, libc::SOCK_SEQPACKET | libc::SOCK_CLOEXEC, 0) })?;
    // SAFETY: `fd` was just returned by the kernel.
    let owned = unsafe { OwnedFd::from_raw_fd(fd) };
    let bytes = path.as_os_str().as_bytes();
    // SAFETY: an all-zero sockaddr_un is valid.
    let mut address: libc::sockaddr_un = unsafe { std::mem::zeroed() };
    if bytes.len() >= address.sun_path.len() {
        return Err(io::Error::from(io::ErrorKind::InvalidInput));
    }
    address.sun_family = libc::sa_family_t::try_from(libc::AF_UNIX).unwrap_or(1);
    for (slot, &b) in address.sun_path.iter_mut().zip(bytes) {
        *slot = b.cast_signed();
    }
    let length = libc::socklen_t::try_from(std::mem::size_of::<libc::sockaddr_un>()).unwrap_or(0);
    Ok((owned, address, length))
}

/// The listening socket systemd passed this process (`sd_listen_fds(3)`: descriptor 3 when `LISTEN_PID` is this
/// process and `LISTEN_FDS` is at least 1); `None` without one.
#[must_use]
pub fn systemd_socket() -> Option<OwnedFd> {
    let ours = std::env::var("LISTEN_PID").ok().and_then(|p| p.parse::<u32>().ok()) == Some(std::process::id());
    let count = std::env::var("LISTEN_FDS").ok().and_then(|n| n.parse::<i32>().ok()).unwrap_or(0);
    // SAFETY: systemd hands this process descriptor 3 as its socket, checked above; nothing else owns it.
    (ours && count >= 1).then(|| unsafe { OwnedFd::from_raw_fd(3) })
}

/// Makes a descriptor non-blocking, so draining it with a loop stops when it is empty.
///
/// # Errors
///
/// The `fcntl` error.
pub fn set_nonblocking(fd: RawFd) -> io::Result<()> {
    // SAFETY: plain fcntl calls with integer arguments.
    let flags = check(unsafe { libc::fcntl(fd, libc::F_GETFL) })?;
    // SAFETY: as above.
    check(unsafe { libc::fcntl(fd, libc::F_SETFL, flags | libc::O_NONBLOCK) })?;
    Ok(())
}

/// Accepts one connection on a listening socket, close-on-exec and non-blocking.
///
/// # Errors
///
/// The `accept4` error.
pub fn accept(listener: RawFd) -> io::Result<OwnedFd> {
    // SAFETY: null address pointers are allowed; the flags are integers.
    let fd = check(unsafe {
        libc::accept4(listener, std::ptr::null_mut(), std::ptr::null_mut(), libc::SOCK_CLOEXEC | libc::SOCK_NONBLOCK)
    })?;
    // SAFETY: `fd` was just returned by the kernel.
    Ok(unsafe { OwnedFd::from_raw_fd(fd) })
}

/// Sends one message on a `SOCK_SEQPACKET` socket, without SIGPIPE when the other end left.
///
/// # Errors
///
/// The `send` error; `EPIPE` when the peer closed.
pub fn send(fd: RawFd, message: &[u8]) -> io::Result<()> {
    // SAFETY: `message` is a live slice of the length given.
    let sent = unsafe { libc::send(fd, message.as_ptr().cast(), message.len(), libc::MSG_NOSIGNAL) };
    if sent < 0 { Err(io::Error::last_os_error()) } else { Ok(()) }
}

/// Receives one message; `Ok(None)` when the other end closed, `WouldBlock` when nothing is waiting.
///
/// # Errors
///
/// The `recv` error.
pub fn receive(fd: RawFd, buffer: &mut [u8]) -> io::Result<Option<usize>> {
    // SAFETY: `buffer` is a live, writable slice of the length given.
    let got = unsafe { libc::recv(fd, buffer.as_mut_ptr().cast(), buffer.len(), libc::MSG_DONTWAIT) };
    if got < 0 {
        return Err(io::Error::last_os_error());
    }
    let got = usize::try_from(got).unwrap_or(0);
    Ok((got > 0).then_some(got))
}
