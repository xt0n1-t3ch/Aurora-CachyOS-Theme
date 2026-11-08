# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Checks that machine code runs on every x86-64 CPU: it holds no instruction newer than the x86-64 baseline (SSE2),
and it is not marked as needing a newer CPU.

    python3 -I scripts/cpu_baseline.py PATH...             each PATH a file or a folder; exits 1 and names every
                                                           function that needs a newer CPU and every file marked so,
                                                           2 when objdump is missing
    python3 -I scripts/cpu_baseline.py --relabel PATH...   marks each file whose code is baseline as needing only
                                                           the baseline; exits 1 when a marked file has newer code

A function built for a newer instruction set on purpose is allowed: a program calls it only after asking the CPU,
and its name says which set it is built for (memchr's `find_avx2`, sha2's `x86_sha::compress`). So the check reads
the files before they are stripped (packaging/aur/PKGBUILD runs it in check()); a file without symbols may hold no
newer instruction at all. Code built for the build machine's CPU (gcc -march=native, rustc -C target-cpu=native)
has newer instructions in nearly every function, and stops with SIGILL on an older CPU.

The mark is the x86 ISA level a file says it needs (GNU_PROPERTY_X86_ISA_1_NEEDED in its GNU property note). glibc's
loader reads it before any code runs and stops with "CPU ISA level is lower than required" on a CPU below it
(sysdeps/x86/dl-prop.h, dl_isa_level_check). GNU ld and mold copy the mark of every object they link, and CachyOS's
x86-64-v3 glibc marks its program start object (/usr/lib/Scrt1.o) as needing x86-64-v3, so every program linked there
is marked so whatever its code holds. --relabel puts the mark back to what the code needs, after reading the code.
"""
import os
import re
import shutil
import struct
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
# ELF program header types and the GNU property that holds the needed ISA level (elf.h, binutils' include/elf/common.h).
PT_NOTE = 4
PT_GNU_PROPERTY = 0x6474E553
NT_GNU_PROPERTY_TYPE_0 = 5
GNU_PROPERTY_X86_ISA_1_NEEDED = 0xC0008002
# The ELF header's class and byte order (e_ident[EI_CLASS], e_ident[EI_DATA]): 64-bit, little-endian.
IDENT = slice(4, 6)
ELF64_LSB = b"\x02\x01"
# Where an ELF64 header keeps e_phoff, and e_phentsize followed by e_phnum.
E_PHOFF = 0x20
E_PHENTSIZE = 0x36
# A program header: p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align.
PHDR = "<IIQQQQQQ"
# A note's header (namesz, descsz, type) and a property's (pr_type, pr_datasz), in bytes.
NOTE_HEADER = 12
PROPERTY_HEADER = 8
# Notes and properties are padded to 8 bytes in a 64-bit file, notes to 4 in a segment aligned to 4 (gABI).
ALIGN_64 = 8
ALIGN_32 = 4
# The size of the ISA_1_NEEDED property's data, one 32-bit mask.
MASK_SIZE = 4
# The level bits of that property, lowest first: GNU_PROPERTY_X86_ISA_1_BASELINE, _V2, _V3 and _V4.
BASELINE = 1
LEVELS = ["x86-64-baseline", "x86-64-v2", "x86-64-v3", "x86-64-v4"]


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


def needed_marks(path):
    """Returns (file offset, levels) for each ISA_1_NEEDED property of a 64-bit little-endian ELF file, levels being
    the property's bit mask. A file without program headers (an object file) or without the note has none."""
    with open(path, "rb") as file:
        data = file.read()
    if data[IDENT] != ELF64_LSB:
        return []
    phoff, = struct.unpack_from("<Q", data, E_PHOFF)
    phentsize, phnum = struct.unpack_from("<HH", data, E_PHENTSIZE)
    segments = [struct.unpack_from(PHDR, data, phoff + i * phentsize) for i in range(phnum)] if phoff else []
    # glibc reads PT_GNU_PROPERTY when it is there and the PT_NOTE segments otherwise; both cover the same bytes.
    property_segments = [s for s in segments if s[0] == PT_GNU_PROPERTY] or [s for s in segments if s[0] == PT_NOTE]
    return [(offset + PROPERTY_HEADER, struct.unpack_from("<I", data, offset + PROPERTY_HEADER)[0])
            for segment in property_segments
            for desc, size in gnu_property_notes(data, segment)
            for offset, pr_type, pr_datasz in properties(data, desc, size)
            if pr_type == GNU_PROPERTY_X86_ISA_1_NEEDED and pr_datasz == MASK_SIZE]


def padded(start, at, align):
    """`at` moved forward to the next multiple of `align` bytes from `start`."""
    return start + (at - start + align - 1) // align * align


def gnu_property_notes(data, segment):
    """Yields (offset, size) of the descriptor of each GNU property note in a note segment. The descriptor and the
    next note start at the segment's alignment, 8 bytes for property notes in a 64-bit file (gABI, "Note Section")."""
    _, _, offset, _, _, size, _, align = segment
    pad = ALIGN_64 if align == ALIGN_64 else ALIGN_32
    at, end = offset, offset + size
    while at + NOTE_HEADER <= end:
        namesz, descsz, kind = struct.unpack_from("<III", data, at)
        name = data[at + NOTE_HEADER:at + NOTE_HEADER + namesz]
        desc = padded(offset, at + NOTE_HEADER + namesz, pad)
        if kind == NT_GNU_PROPERTY_TYPE_0 and name == b"GNU\0":
            yield desc, descsz
        at = padded(offset, desc + descsz, pad)


def properties(data, desc, size):
    """Yields (offset, type, data size) of each property in a GNU property note's descriptor; each property's data is
    padded to 8 bytes in a 64-bit file."""
    at, end = desc, desc + size
    while at + PROPERTY_HEADER <= end:
        pr_type, pr_datasz = struct.unpack_from("<II", data, at)
        yield at, pr_type, pr_datasz
        at = padded(desc, at + PROPERTY_HEADER + pr_datasz, ALIGN_64)


def newest(levels):
    """The name of the newest level in an ISA_1_NEEDED bit mask."""
    return LEVELS[min(levels.bit_length(), len(LEVELS)) - 1]


def relabel(path):
    """Marks a file whose code holds nothing newer than the baseline as needing only the baseline, in place. Returns
    the functions that kept a marked file from being relabelled (none when it was relabelled or had no mark)."""
    marks = [(offset, levels) for offset, levels in needed_marks(path) if levels & ~BASELINE]
    if not marks:
        return []
    found = newer_functions(path)
    if found:
        return found
    with open(path, "r+b") as file:
        for offset, _ in marks:
            file.seek(offset)
            file.write(struct.pack("<I", BASELINE))
    return []


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


def report(path, found):
    """Prints the functions of `path` that need a newer CPU, the first SHOWN of them."""
    print(f"{path}: {len(found)} {'function needs' if len(found) == 1 else 'functions need'} a newer CPU than "
          "the x86-64 baseline")
    for function, mnemonic, needs in found[:SHOWN]:
        print(f"  {function} uses {mnemonic} ({needs})")
    if len(found) > SHOWN:
        print(f"  and {len(found) - SHOWN} more")


def check(path):
    """Prints what keeps `path` from starting on a baseline CPU; returns whether anything does."""
    found = newer_functions(path)
    marked = [levels for _, levels in needed_marks(path) if levels & ~BASELINE]
    if found:
        report(path, found)
    if marked:
        print(f"{path}: marked as needing {newest(max(marked))}, so glibc's loader refuses to start it on an older "
              "CPU (\"CPU ISA level is lower than required\")")
    return bool(found or marked)


def relabel_and_report(path):
    """Relabels `path` and prints what kept it marked; returns whether anything did."""
    found = relabel(path)
    if found:
        report(path, found)
    return bool(found)


def count(files):
    """`files` with its noun, "1 file" or "3 files"."""
    return f"{files} {'file' if files == 1 else 'files'}"


def main():
    args = sys.argv[1:]
    relabelling = args[:1] == ["--relabel"]
    paths = args[1:] if relabelling else args
    if not paths:
        sys.exit(__doc__)
    if not shutil.which("objdump"):
        print("objdump is not installed (binutils)", file=sys.stderr)
        return 2
    step = relabel_and_report if relabelling else check
    results = [step(path) for path in elf_files(paths)]
    failed = sum(results)
    if failed and relabelling:
        print(f"{failed} of {len(results)} files stay marked: their code needs a newer CPU than the x86-64 baseline")
    elif failed:
        print(f"{failed} of {len(results)} files would not start on an older x86-64 CPU")
    elif relabelling:
        print(f"every file is marked for what its code needs ({count(len(results))})")
    else:
        print(f"every file runs on the x86-64 baseline ({count(len(results))})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
