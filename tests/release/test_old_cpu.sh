#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# scripts/old_cpu.py, which starts a package's programs and loads its libraries on an emulated older processor, with
# --plan: what it would run for a package installed in a chroot folder. A program runs with the arguments of the runs
# table, a library is loaded the way Plasma loads a plugin, an object file and what is not machine code are skipped,
# and a program missing from the table fails before anything runs. The runs themselves need root and the clean chroot;
# scripts/clean-build.sh does them on every release build.
set -u
root=$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}
if ! command -v gcc > /dev/null || ! command -v bsdtar > /dev/null; then
    echo "SKIP gcc and bsdtar (base-devel, libarchive) are not installed"
    exit 0
fi

# A chroot folder with a package installed: a program, a library, an object file and a text file.
copy="$work/copy"
mkdir -p "$copy/usr/bin" "$copy/usr/lib/demo" "$copy/usr/share/demo"
echo 'int main(void) { return 0; }' > "$work/main.c"
echo 'int answer(void) { return 42; }' > "$work/lib.c"
gcc -O2 -o "$copy/usr/bin/demo" "$work/main.c"
gcc -O2 -shared -fPIC -o "$copy/usr/lib/demo/plugin.so" "$work/lib.c"
gcc -O2 -c -o "$copy/usr/lib/demo/part.o" "$work/lib.c"
echo text > "$copy/usr/share/demo/notes.txt"
# The package lists the same files, as pacman's packages do, with its metadata at the top.
mkdir "$work/pkg" && cp -a "$copy/usr" "$work/pkg/" && echo "pkgname = demo" > "$work/pkg/.PKGINFO"
(cd "$work/pkg" && bsdtar -cf "$work/demo.pkg.tar" .PKGINFO usr)
echo '/usr/bin/demo --version' > "$work/runs.txt"

plan() {
    python3 -I "$root/scripts/old_cpu.py" --plan --runs "$1" "$copy" "$work/demo.pkg.tar" > "$work/out" 2>&1
    echo $?
}
check "a package whose programs are all in the table is planned" "$(plan "$work/runs.txt")" 0
check "the program runs with the table's arguments" "$(grep -c '^run /usr/bin/demo --version$' "$work/out")" 1
check "the library is loaded" "$(grep -c '^load /usr/lib/demo/plugin.so$' "$work/out")" 1
check "an object file and a text file are skipped" "$(grep -cE 'part\.o|notes\.txt' "$work/out")" 0
check "the plan names the emulated processor" "$(grep -c '^on Opteron_G1' "$work/out")" 1
: > "$work/empty.txt"
check "a program missing from the table fails" "$(plan "$work/empty.txt")" 1
check "the failure names the program" "$(grep -c '/usr/bin/demo has no line in' "$work/out")" 1
check "nothing runs when a program is missing" "$(grep -c '^run \|^load ' "$work/out")" 0
check "the repository's table names programs only by absolute path" \
    "$(grep -v '^#' "$root/packaging/repo/old-cpu-runs.txt" | grep -v '^$' | grep -vc '^/')" 0
exit "$failures"
