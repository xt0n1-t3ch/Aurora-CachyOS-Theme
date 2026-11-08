#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/cpu_baseline.py, the package's check that its machine code runs on every x86-64 CPU, tried on libraries gcc
# builds here: baseline code passes, code built for a newer CPU fails and names the function, a function built for
# AVX2 on purpose (a program picks it after asking the CPU) passes, and a stripped file built for a newer CPU fails.
# Then the ISA level a program is marked with: glibc's loader refuses a program marked as needing x86-64-v3 on an older
# CPU, so the check fails it, and --relabel marks baseline code as baseline but leaves a program with newer code alone.
set -u
root=$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}
# Runs the check on its arguments and prints its exit status; its output goes to $work/out.
run() {
    python3 -I "$root/scripts/cpu_baseline.py" "$@" > "$work/out" 2>&1
    echo $?
}

if ! command -v gcc > /dev/null || ! command -v objdump > /dev/null; then
    echo "SKIP gcc and objdump (base-devel) are not installed"
    exit 0
fi
# Floating-point code: built for a CPU with AVX, gcc writes it with VEX instructions (vmulsd, vaddsd).
cat > "$work/scale.c" << 'EOF'
double scale(double x, double y) { return x * y + 1.0; }
EOF
# A name that only holds a set's letters inside a word (sub-mi-t) says nothing about it.
cat > "$work/submit.c" << 'EOF'
double submit(double x, double y) { return x * y + 1.0; }
EOF
cat > "$work/picked.c" << 'EOF'
__attribute__((target("avx2"))) double scale_avx2(double x, double y) { return x * y + 1.0; }
double scale(double x, double y) { return x * y + 1.0; }
EOF
# C++ whose symbols hold templates and lambdas: objdump names them after branches and calls, braces and all.
cat > "$work/lambdas.cpp" << 'EOF'
#include <algorithm>
#include <functional>
#include <vector>
std::function<int(int)> add(int k) { return [k](int x) { return x + k; }; }
void order(std::vector<int> &v) { std::stable_sort(v.begin(), v.end(), [](int a, int b) { return a > b; }); }
EOF
# Integer code that newer CPUs run with one scalar instruction: popcnt (x86-64-v2) and shlx (BMI2, x86-64-v3). The
# baseline build of a popcount would call libgcc's __popcountdi2, which a toolchain built for a newer CPU (CachyOS's
# x86-64-v3 gcc) links in with popcnt: the check rightly fails that, so the baseline case shifts only.
cat > "$work/bits.c" << 'EOF'
int bits(unsigned x) { return __builtin_popcount(x); }
EOF
# crc32 (SSE4.2, x86-64-v2), which objdump writes with a size suffix only for a memory operand.
cat > "$work/crc.c" << 'EOF'
unsigned crc(unsigned sum, unsigned char byte) { return __builtin_ia32_crc32qi(sum, byte); }
unsigned crc_at(unsigned sum, const unsigned *word) { return __builtin_ia32_crc32si(sum, *word); }
EOF
# A 16-byte compare-and-swap: objdump writes the prefix first, lock cmpxchg16b (x86-64-v2).
cat > "$work/swap.c" << 'EOF'
int swap(__int128 *p, __int128 old, __int128 new_) { return __sync_bool_compare_and_swap(p, old, new_); }
EOF
# rdrand, an extension outside the levels.
cat > "$work/random.c" << 'EOF'
int random32(unsigned *out) { return __builtin_ia32_rdrand32_step(out); }
EOF
cat > "$work/shift.c" << 'EOF'
unsigned long shift(unsigned long x, int n) { return x << n; }
EOF
gcc -O2 -shared -fPIC -o "$work/baseline.so" "$work/scale.c"
gcc -O2 -shared -fPIC -o "$work/shift.so" "$work/shift.c"
gcc -O2 -shared -fPIC -mpopcnt -o "$work/popcnt.so" "$work/bits.c"
gcc -O2 -shared -fPIC -mbmi2 -o "$work/bmi2.so" "$work/shift.c"
gcc -O2 -shared -fPIC -msse4.2 -o "$work/crc.so" "$work/crc.c"
gcc -O2 -shared -fPIC -mcx16 -o "$work/swap.so" "$work/swap.c"
gcc -O2 -shared -fPIC -mrdrnd -o "$work/random.so" "$work/random.c"
g++ -O2 -shared -fPIC -o "$work/lambdas.so" "$work/lambdas.cpp"
gcc -O2 -shared -fPIC -mavx2 -o "$work/newer.so" "$work/scale.c"
gcc -O2 -shared -fPIC -o "$work/picked.so" "$work/picked.c"
gcc -O2 -shared -fPIC -mavx2 -o "$work/submit.so" "$work/submit.c"
cp "$work/newer.so" "$work/stripped.so" && strip --strip-unneeded "$work/stripped.so"
mkdir "$work/folder" && cp "$work/baseline.so" "$work/picked.so" "$work/folder/" && echo text > "$work/folder/notes.txt"

check "baseline code passes" "$(run "$work/baseline.so")" 0
check "baseline C++ with templates and lambdas passes" "$(run "$work/lambdas.so")" 0
check "code built for a newer CPU fails" "$(run "$work/newer.so")" 1
check "the failure names the function" "$(grep -c 'scale.*vmulsd\|scale.*vaddsd' "$work/out")" 1
check "baseline integer code passes" "$(run "$work/shift.so")" 0
check "popcnt fails" "$(run "$work/popcnt.so")" 1
check "the failure names popcnt and its level" "$(grep -c 'bits uses popcnt.*(x86-64-v2)' "$work/out")" 1
check "shlx fails" "$(run "$work/bmi2.so")" 1
check "the failure names shlx and its level" "$(grep -c 'shift uses shlx.*(x86-64-v3)' "$work/out")" 1
check "crc32 fails, with a register and with a memory operand" \
    "$(run "$work/crc.so"; grep -cE '  crc(_at)? uses crc32' "$work/out")" "$(printf '1\n2')"
check "an instruction after a prefix fails" "$(run "$work/swap.so"; grep -c 'swap uses cmpxchg16b' "$work/out")" "$(printf '1\n1')"
check "an extension fails" "$(run "$work/random.so"; grep -c 'random32 uses rdrand (an extension)' "$work/out")" \
    "$(printf '1\n1')"
check "a function built for AVX2 on purpose passes" "$(run "$work/picked.so")" 0
check "a name with a set's letters inside a word is no excuse" "$(run "$work/submit.so")" 1
check "a stripped file built for a newer CPU fails" "$(run "$work/stripped.so")" 1
# A link may point out of the folder (to a system library); what it points to is not the folder's business.
ln -s "$work/newer.so" "$work/folder/system.so"
check "a folder is checked file by file, skipping links and what is not machine code" "$(run "$work/folder")" 0
cp "$work/newer.so" "$work/folder/"
check "one newer file fails the folder" "$(run "$work/folder")" 1

# The marked level, as readelf prints it (empty without a mark).
marked() {
    readelf -n "$1" | sed -n 's/.*x86 ISA needed: //p'
}
# CachyOS's glibc links its x86-64-v3 mark into every program from Scrt1.o; -z x86-64-v3 marks the same way on any
# toolchain. glibc's loader reads the mark before any code runs (sysdeps/x86/dl-prop.h, dl_isa_level_check) and stops
# with "CPU ISA level is lower than required" on a CPU below it.
cat > "$work/main.c" << 'EOF'
int main(void) { return 0; }
EOF
gcc -O2 -fuse-ld=bfd -Wl,-z,x86-64-v3 -o "$work/marked" "$work/main.c" "$work/scale.c"
gcc -O2 -fuse-ld=bfd -mavx2 -Wl,-z,x86-64-v3 -o "$work/marked-newer" "$work/main.c" "$work/scale.c"
gcc -O2 -fuse-ld=lld -o "$work/unmarked" "$work/main.c" "$work/scale.c" 2> /dev/null ||
    gcc -O2 -fuse-ld=bfd -Wl,-z,x86-64-baseline -o "$work/unmarked" "$work/main.c" "$work/scale.c"
check "baseline code marked as needing x86-64-v3 fails" \
    "$(run "$work/marked"; grep -c 'marked as needing x86-64-v3' "$work/out")" "$(printf '1\n1')"
check "a program with no mark above the baseline passes" "$(run "$work/unmarked")" 0
check "--relabel marks baseline code as baseline" "$(run --relabel "$work/marked"; marked "$work/marked")" \
    "$(printf '0\nx86-64-baseline')"
check "the relabelled program passes" "$(run "$work/marked")" 0
check "the relabelled program still runs" "$("$work/marked"; echo $?)" 0
check "--relabel twice changes nothing more" \
    "$(sum_before=$(sha256sum < "$work/marked"); run --relabel "$work/marked" > /dev/null; [ "$(sha256sum < "$work/marked")" = "$sum_before" ] && echo same)" same
check "--relabel leaves a program with newer code marked, and fails" \
    "$(run --relabel "$work/marked-newer"; marked "$work/marked-newer")" "$(printf '1\nx86-64-baseline, x86-64-v2, x86-64-v3')"
check "--relabel leaves a program without a mark untouched" \
    "$(sum_before=$(sha256sum < "$work/unmarked"); run --relabel "$work/unmarked" > /dev/null; [ "$(sha256sum < "$work/unmarked")" = "$sum_before" ] && echo same)" same
mkdir "$work/marked-folder" && cp "$work/marked" "$work/marked-folder/" && gcc -O2 -fuse-ld=bfd -Wl,-z,x86-64-v3 \
    -o "$work/marked-folder/again" "$work/main.c"
check "--relabel walks a folder" "$(run --relabel "$work/marked-folder"; run "$work/marked-folder")" "$(printf '0\n0')"
exit "$failures"
