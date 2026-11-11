// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The kernel calls the crate makes through `libc`, each behind a safe function. Nothing else in the crate is unsafe.

use std::io;
use std::os::fd::RawFd;

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
