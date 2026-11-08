# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Starts every program of a package and loads every library of it on an emulated x86-64 processor with nothing newer
than SSE2, inside the clean chroot the package was built and installed in.

    python3 -I scripts/old_cpu.py [--plan] [--runs FILE] CHROOT PACKAGE

CHROOT is a chroot folder of scripts/clean-build.sh with PACKAGE installed. Each program runs with the arguments of
its line in FILE (packaging/repo/old-cpu-runs.txt); a program without a line fails before anything runs. Each library
is loaded with dlopen, the way Plasma loads a plugin. Everything runs under QEMU's user mode as an AMD Opteron of
2003 (QEMU's Opteron_G1: SSE2, no SSE3, SSSE3, SSE4, AVX or BMI), through arch-nspawn and sudo, so glibc's loader,
every library's start code and the program's own run on that processor, with Arch's packages for the libraries the
user has. Exits 1 when anything fails. --plan prints what it would run and runs nothing.
"""
import os
import shlex
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNS = os.path.join(ROOT, "packaging/repo/old-cpu-runs.txt")
# QEMU's model of the AMD Opteron (K8, 2003): the x86-64 baseline and no more (target/i386/cpu.c, "Opteron_G1").
CPU = "Opteron_G1"
# Where QEMU's static user-mode emulator is on this PC and where the chroot sees it.
QEMU = "/usr/bin/qemu-x86_64-static"
QEMU_INSIDE = "/qemu"
# Seconds one program or library may take under emulation before it counts as failed.
TIMEOUT = 300
ELF_MAGIC = b"\x7fELF"
# ELF64 header fields (e_type, e_phoff, e_phentsize and e_phnum) and the program header that names an interpreter.
E_TYPE = 0x10
E_PHOFF = 0x20
E_PHENTSIZE = 0x36
PHDR = "<IIQQQQQQ"
ET_DYN = 3
PT_INTERP = 3
# dlopen's RTLD_LAZY: a plugin's symbols that only its host program defines stay unresolved, as in Plasma.
RTLD_LAZY = 1
LOAD = (
    "import ctypes, sys\n"
    "libc = ctypes.CDLL(None)\n"
    "libc.dlopen.restype = ctypes.c_void_p\n"
    "libc.dlopen.argtypes = [ctypes.c_char_p, ctypes.c_int]\n"
    "libc.dlerror.restype = ctypes.c_char_p\n"
    f"sys.exit(0 if libc.dlopen(sys.argv[1].encode(), {RTLD_LAZY}) else libc.dlerror().decode())\n"
)


def kind(path):
    """Returns "program" for an ELF file with an interpreter, "library" for another shared object, else None."""
    with open(path, "rb") as file:
        data = file.read()
    if data[:4] != ELF_MAGIC:
        return None
    e_type, = struct.unpack_from("<H", data, E_TYPE)
    phoff, = struct.unpack_from("<Q", data, E_PHOFF)
    phentsize, phnum = struct.unpack_from("<HH", data, E_PHENTSIZE)
    types = [struct.unpack_from(PHDR, data, phoff + i * phentsize)[0] for i in range(phnum)] if phoff else []
    if PT_INTERP in types:
        return "program"
    return "library" if e_type == ET_DYN else None


def members(package):
    """The paths a package installs, absolute, from `bsdtar -tf`; pacman's metadata files are left out."""
    listing = subprocess.run(["bsdtar", "-tf", package], capture_output=True, text=True, check=True).stdout
    return ["/" + name.rstrip("/") for name in listing.splitlines() if name and not name.startswith(".")]


def read_runs(path):
    """The runs table: each program's absolute path to its arguments."""
    runs = {}
    with open(path) as file:
        for line in file:
            words = shlex.split(line, comments=True)
            if words:
                runs[words[0]] = words[1:]
    return runs


def plan(chroot, package, runs):
    """Returns (steps, missing): each step ("run", path, args) or ("load", path, []), and the programs without a line
    in `runs`."""
    steps, missing = [], []
    for path in members(package):
        file = chroot + path
        if os.path.islink(file) or not os.path.isfile(file):
            continue
        found = kind(file)
        if found == "program" and path not in runs:
            missing.append(path)
        elif found == "program":
            steps.append(("run", path, runs[path]))
        elif found == "library":
            steps.append(("load", path, []))
    return steps, missing


def command(chroot, step):
    """The command that runs one step in the chroot on the emulated processor."""
    action, path, args = step
    inside = [path, *args] if action == "run" else ["/usr/bin/python3", "-c", LOAD, path]
    return ["sudo", "-n", "arch-nspawn", chroot, f"--bind-ro={QEMU}:{QEMU_INSIDE}",
            "--setenv=QT_QPA_PLATFORM=offscreen", QEMU_INSIDE, "-cpu", CPU, *inside]


def run(chroot, step):
    """Runs one step and prints pass or FAIL with the end of its output; returns whether it passed."""
    try:
        result = subprocess.run(command(chroot, step), capture_output=True, text=True, timeout=TIMEOUT)
        ok, output = result.returncode == 0, result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        ok, output = False, f"still running after {TIMEOUT} s"
    print(f"{'pass' if ok else 'FAIL'}  {step[0]} {step[1]}")
    if not ok:
        print("      " + "\n      ".join(output.strip().splitlines()[-5:]))
    return ok


def main():
    args = sys.argv[1:]
    planning = "--plan" in args
    args = [a for a in args if a != "--plan"]
    runs_file = RUNS
    if "--runs" in args:
        at = args.index("--runs")
        runs_file = args[at + 1]
        del args[at:at + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    chroot, package = os.path.abspath(args[0]), args[1]
    steps, missing = plan(chroot, package, read_runs(runs_file))
    for path in missing:
        print(f"{path} has no line in {runs_file}: add the arguments that start it and exit")
    if missing:
        return 1
    print(f"on {CPU}, {len(steps)} steps")
    if planning:
        for action, path, step_args in steps:
            print(" ".join([action, path, *step_args]))
        return 0
    failed = sum(not run(chroot, step) for step in steps)
    if failed:
        print(f"{failed} of {len(steps)} would fail on an x86-64 processor without SSE3")
        return 1
    print(f"every program starts and every library loads on {CPU}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
