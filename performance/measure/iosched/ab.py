#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Kyber against ADIOS on each NVMe disk, in alternating windows so a change in the background load lands on both.

Each window sets the disk's I/O scheduler, starts background writers of 1 MiB blocks and times 4 KiB random reads
that skip the page cache, the reads an app makes while it opens. The writers go flat out with direct writes by
default; `--rate` holds each one to that many MB/s, and `--buffered` sends its writes through the page cache as a
download or a build does. `--rest` leaves the disk quiet that many seconds before each window, so a drive that slows
down under sustained writes recovers between windows. It also runs one window per scheduler with the disk quiet.
It prints the read latency (median, p99, p99.9 in µs), the reads per second and the MB/s written per window, writes
them as JSON to OUT, and puts each disk's scheduler back at the end, whatever happens. Runs as root (the scheduler is
in sysfs); the test files are deleted at the end.

    sudo python3 -I ab.py OUT DISK:FOLDER [DISK:FOLDER ...] [--rounds N] [--seconds S] [--writers W]
                          [--rate MBS] [--buffered] [--rest S]

Example: ab.py /var/tmp/iosched.json nvme0n1:/var/tmp/iosched nvme1n1:/mnt/D/.iosched
"""
import argparse
import json
import mmap
import os
import random
import signal
import subprocess
import sys
import time

BLOCK = 4096
MIB = 1 << 20
# The reader's file: large enough that its reads land all over the disk's cache and its flash.
READ_FILE_BYTES = 2 << 30
# Each writer rewrites its own file of this size from the start, so the test never fills the disk.
WRITE_FILE_BYTES = 4 << 30
SCHEDULERS = ("kyber", "adios")
# The reader's file is written in chunks of this many bytes of random data.
RANDOM_CHUNK_BYTES = 64 << 20
# /sys/block/DISK/stat counts sectors of 512 bytes, whatever the disk's own sector size.
SECTOR_BYTES = 512
# Seconds the writers run before a window is measured, so they have reached a steady rate.
SETTLE_SECONDS = 3
# The latency percentiles a window reports.
P99, P999 = 0.99, 0.999


def scheduler_path(disk):
    """The sysfs file that selects the I/O scheduler of `disk` (a name such as nvme0n1)."""
    return f"/sys/block/{disk}/queue/scheduler"


def current_scheduler(disk):
    """The scheduler `disk` uses now: the bracketed name in its sysfs file."""
    text = open(scheduler_path(disk)).read()
    return text[text.index("[") + 1:text.index("]")]


def set_scheduler(disk, name):
    """Selects scheduler `name` for `disk` and checks that the kernel took it."""
    with open(scheduler_path(disk), "w") as handle:
        handle.write(name)
    if current_scheduler(disk) != name:
        raise RuntimeError(f"{disk} did not take {name}")


def prepare(folder):
    """Makes the test folder with btrfs compression off (so direct I/O stays direct) and writes the reader's file of
    random data. Returns the reader's file path."""
    os.makedirs(folder, exist_ok=True)
    subprocess.run(["chattr", "+m", folder], check=False)
    path = os.path.join(folder, "read.bin")
    if not os.path.exists(path) or os.path.getsize(path) != READ_FILE_BYTES:
        chunk = os.urandom(RANDOM_CHUNK_BYTES)
        with open(path, "wb") as handle:
            for _ in range(READ_FILE_BYTES // len(chunk)):
                handle.write(chunk)
            handle.flush()
            os.fsync(handle.fileno())
    return path


def writer(target, rate, buffered):
    """Rewrites `target` from the start in 1 MiB blocks until SIGTERM; runs in a child process. `rate` is its limit
    in bytes per second (0 for none); `buffered` writes through the page cache instead of directly."""
    signal.signal(signal.SIGTERM, lambda *_: os._exit(0))
    flags = os.O_WRONLY | os.O_CREAT | (0 if buffered else os.O_DIRECT)
    fd = os.open(target, flags, 0o600)
    block = mmap.mmap(-1, MIB)  # page aligned, as O_DIRECT needs; zeros, which btrfs stores as they are with +m
    offset, written, start = 0, 0, time.monotonic()
    while True:
        os.pwrite(fd, block, offset)
        offset = (offset + MIB) % WRITE_FILE_BYTES
        written += MIB
        if rate:
            ahead = written / rate - (time.monotonic() - start)
            if ahead > 0:
                time.sleep(ahead)


def start_writers(folder, count, rate, buffered):
    """Starts `count` writer processes, each in its own process group with its own file. Returns their pids."""
    pids = []
    for i in range(count):
        pid = os.fork()
        if pid == 0:
            os.setsid()
            writer(os.path.join(folder, f"write{i}.bin"), rate, buffered)
        pids.append(pid)
    return pids


def stop_writers(pids):
    """Stops the writers and their dd children, and reaps them."""
    for pid in pids:
        try:
            os.killpg(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    for pid in pids:
        os.waitpid(pid, 0)


def disk_written(disk):
    """Bytes written to `disk` since boot, from /sys/block/DISK/stat (sector 512 B)."""
    return int(open(f"/sys/block/{disk}/stat").read().split()[6]) * SECTOR_BYTES


def time_reads(path, seconds):
    """Times 4 KiB direct random reads of `path` for `seconds`, one at a time. Returns the latencies in µs."""
    fd = os.open(path, os.O_RDONLY | os.O_DIRECT)
    buffer = mmap.mmap(-1, BLOCK)  # page aligned, as O_DIRECT needs
    blocks = os.path.getsize(path) // BLOCK
    latencies = []
    end = time.monotonic() + seconds
    try:
        while time.monotonic() < end:
            offset = random.randrange(blocks) * BLOCK
            start = time.perf_counter_ns()
            os.preadv(fd, [buffer], offset)
            latencies.append((time.perf_counter_ns() - start) / 1000)
    finally:
        os.close(fd)
    return latencies


def percentile(sorted_values, share):
    """The value below which `share` (0 to 1) of `sorted_values` falls."""
    return sorted_values[min(len(sorted_values) - 1, int(share * len(sorted_values)))]


def window(disk, folder, path, scheduler, writers, seconds, load):
    """One measured window: scheduler set, writers running (none for a quiet window), reads timed. `load` holds the
    writers' rate, buffered and rest settings. Returns the window's result as a dict."""
    time.sleep(load.rest)
    set_scheduler(disk, scheduler)
    running = start_writers(folder, writers, load.rate * 1e6, load.buffered) if writers else []
    try:
        time.sleep(SETTLE_SECONDS)
        written = disk_written(disk)
        started = time.monotonic()
        latencies = sorted(time_reads(path, seconds))
        elapsed = time.monotonic() - started
        write_mbs = (disk_written(disk) - written) / elapsed / 1e6
    finally:
        stop_writers(running)
    return {"disk": disk, "scheduler": scheduler, "writers": writers, "reads": len(latencies),
            "reads_per_s": round(len(latencies) / elapsed), "p50_us": round(percentile(latencies, 0.5)),
            "p99_us": round(percentile(latencies, P99)), "p999_us": round(percentile(latencies, P999)),
            "max_us": round(latencies[-1]), "write_mbs": round(write_mbs)}


def main():
    """Runs the windows for every disk, prints and saves the results, and restores every scheduler."""
    parser = argparse.ArgumentParser()
    parser.add_argument("out")
    parser.add_argument("targets", nargs="+")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--writers", type=int, default=4)
    parser.add_argument("--rate", type=float, default=0, help="MB/s per writer, 0 for flat out")
    parser.add_argument("--buffered", action="store_true", help="write through the page cache")
    parser.add_argument("--rest", type=float, default=0, help="quiet seconds before each window")
    args = parser.parse_args()
    targets = [target.split(":", 1) for target in args.targets]
    before = {disk: current_scheduler(disk) for disk, _ in targets}
    results = []
    try:
        for disk, folder in targets:
            path = prepare(folder)
            plan = [(name, 0) for name in SCHEDULERS]
            for round_index in range(args.rounds):
                # The order flips each round, so neither scheduler always goes first.
                order = SCHEDULERS if round_index % 2 == 0 else SCHEDULERS[::-1]
                plan += [(name, args.writers) for name in order]
            for scheduler, writers in plan:
                result = window(disk, folder, path, scheduler, writers, args.seconds, args)
                results.append(result)
                print(json.dumps(result), flush=True)
    finally:
        for disk, name in before.items():
            set_scheduler(disk, name)
        for _, folder in targets:
            subprocess.run(["rm", "-rf", folder], check=False)
        with open(args.out, "w") as handle:
            settings = {"rounds": args.rounds, "seconds": args.seconds, "writers": args.writers, "rate_mbs": args.rate,
                        "buffered": args.buffered, "rest_s": args.rest}
            json.dump({"before": before, "settings": settings, "results": results}, handle, indent=1)
        owner = os.environ.get("SUDO_UID")
        if owner:
            os.chown(args.out, int(owner), int(os.environ.get("SUDO_GID", owner)))
    print("schedulers back to:", before, file=sys.stderr)


if __name__ == "__main__":
    main()
