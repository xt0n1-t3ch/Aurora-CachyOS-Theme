# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Checks that machine code runs on every x86-64 CPU: it holds no instruction newer than the x86-64 baseline (SSE2).

    python3 -I scripts/cpu_baseline.py PATH...   each PATH a file or a folder; exits 1 and names every function that
                                                 needs a newer CPU, 2 when objdump is missing

A function built for a newer instruction set on purpose is allowed: a program calls it only after asking the CPU,
and its name says which set it is built for (memchr's `find_avx2`, sha2's `x86_sha::compress`). So the check reads
the files before they are stripped (packaging/aur/PKGBUILD runs it in check()); a file without symbols may hold no
newer instruction at all. Code built for the build machine's CPU (gcc -march=native, rustc -C target-cpu=native)
has newer instructions in nearly every function, and stops with SIGILL on an older CPU.
"""
import os
import re
import shutil
import subprocess
import sys

# The instructions each level adds over the one before, by objdump's AT&T mnemonics (x86-64 psABI, "Micro-
# architecture levels"). Every VEX or EVEX instruction (a mnemonic starting with v) is x86-64-v3 or newer; AVX-512's
# mask instructions (kmovw) come only with EVEX ones, so they need no rule of their own.
V2 = set(
    "popcnt cmpxchg16b addsubps addsubpd haddps haddpd hsubps hsubpd lddqu movddup movshdup movsldup pabsb pabsw "
    "pabsd palignr phaddw phaddd phaddsw phsubw phsubd phsubsw pmaddubsw pmulhrsw pshufb psignb psignw psignd "
    "pblendvb pblendw blendps blendpd blendvps blendvpd dpps dppd insertps extractps mpsadbw packusdw pcmpeqq pextrb "
    "pextrd pextrq phminposuw pinsrb pinsrd pinsrq pmaxsb pmaxsd pmaxud pmaxuw pminsb pminsd pminud pminuw pmovsxbw "
    "pmovsxbd pmovsxbq pmovsxwd pmovsxwq pmovsxdq pmovzxbw pmovzxbd pmovzxbq pmovzxwd pmovzxwq pmovzxdq pmuldq pmulld "
    "ptest roundps roundpd roundss roundsd movntdqa pcmpestri pcmpestrm pcmpistri pcmpistrm pcmpgtq crc32 crc32b "
    "crc32w crc32l crc32q".split()
)
# tzcnt is not here: a CPU without BMI1 runs it as bsf, and compilers use it for the baseline on purpose.
V3 = set("andn bextr blsi blsmsk blsr bzhi mulx pdep pext rorx sarx shlx shrx lzcnt movbe".split())
# Extensions outside the levels that a baseline CPU lacks too.
EXTENSIONS = set(
    "aesenc aesenclast aesdec aesdeclast aesimc aeskeygenassist pclmulqdq sha1rnds4 sha1nexte sha1msg1 sha1msg2 "
    "sha256rnds2 sha256msg1 sha256msg2 rdrand rdseed adcx adox extrq insertq movntss movntsd".split()
)
# Prefixes objdump writes before the mnemonic.
PREFIXES = set("lock rep repz repnz repe repne notrack bnd data16 addr32 cs ds es fs gs ss xacquire xrelease".split())
# A function name that says the instruction set it is built for: as a word in snake_case or a path (find_avx2,
# x86_sha), or in capitals inside CamelCase (SlimAVX2).
PICKED = re.compile(r"(?<![A-Za-z])(?i:avx\w*|sse3|ssse3|sse4\w*|sha_?ni|x86_sha|bmi[12]?|popcnt|lzcnt|pclmul\w*|"
                    r"aes_?ni|fma|f16c)|AVX|SSSE3|SSE4|BMI|PCLMUL|AESNI")
FUNCTION = re.compile(r"^[0-9a-f]+ <(?P<name>.+)>:$")
INSTRUCTION = re.compile(r"^\s*[0-9a-f]+:\s+(?P<text>\S.*)$")
# How many functions a failing file lists before it only counts the rest.
SHOWN = 5
# The first bytes of every ELF file.
ELF_MAGIC = b"\x7fELF"


def level(text):
    """Returns the mnemonic of one disassembled instruction and the CPU it needs (None for the baseline). Only the
    mnemonic counts: the operands follow it, then objdump's notes, whose C++ symbols may hold anything."""
    words = text.split()
    while len(words) > 1 and words[0] in PREFIXES:
        words.pop(0)
    mnemonic = words[0]
    if mnemonic.startswith("v"):
        return mnemonic, "x86-64-v3 or newer"
    if mnemonic in V3:
        return mnemonic, "x86-64-v3"
    if mnemonic in V2:
        return mnemonic, "x86-64-v2"
    if mnemonic in EXTENSIONS:
        return mnemonic, "an extension"
    return mnemonic, None


def newer_functions(path):
    """Returns (function, mnemonic, level) for each function of an ELF file that needs a newer CPU without saying so
    in its name. Code outside any symbol counts as a function named after its section, like <.text>."""
    listing = subprocess.run(["objdump", "-d", "-C", "--no-show-raw-insn", path],
                             capture_output=True, text=True, check=True).stdout
    found, function, reported = [], "", False
    for line in listing.splitlines():
        header = FUNCTION.match(line)
        if header:
            function, reported = header["name"], False
            continue
        instruction = INSTRUCTION.match(line)
        if not instruction or reported:
            continue
        mnemonic, needs = level(instruction["text"])
        if needs and not PICKED.search(function):
            found.append((function, mnemonic, needs))
            reported = True
    return found


def is_elf(path):
    """Whether `path` is a regular file that starts with the ELF magic."""
    if os.path.islink(path) or not os.path.isfile(path):
        return False
    with open(path, "rb") as file:
        return file.read(len(ELF_MAGIC)) == ELF_MAGIC


def elf_files(paths):
    """Every ELF file among `paths`, folders searched to the bottom, in a stable order. Links are skipped: what they
    point to is checked where it lives."""
    for path in paths:
        if os.path.isdir(path):
            for folder, _, names in sorted(os.walk(path)):
                yield from (os.path.join(folder, name) for name in sorted(names) if is_elf(os.path.join(folder, name)))
        elif is_elf(path):
            yield path


def main():
    paths = sys.argv[1:]
    if not paths:
        sys.exit(__doc__)
    if not shutil.which("objdump"):
        print("objdump is not installed (binutils)", file=sys.stderr)
        return 2
    files, failed = 0, 0
    for path in elf_files(paths):
        files += 1
        found = newer_functions(path)
        if not found:
            continue
        failed += 1
        print(f"{path}: {len(found)} {'function needs' if len(found) == 1 else 'functions need'} a newer CPU than "
              "the x86-64 baseline")
        for function, mnemonic, needs in found[:SHOWN]:
            print(f"  {function} uses {mnemonic} ({needs})")
        if len(found) > SHOWN:
            print(f"  and {len(found) - SHOWN} more")
    if failed:
        print(f"{failed} of {files} files would stop with SIGILL on an older x86-64 CPU")
        return 1
    print(f"every file runs on the x86-64 baseline ({files} {'file' if files == 1 else 'files'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
