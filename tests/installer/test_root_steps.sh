#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# installer/root-steps.sh, the part of the install that runs as root, takes paths as arguments and never as shell
# code: a home folder or checkout whose path holds a quote, $(...) or ; is linked as written, and nothing in it runs.
# The steps run here as the current user, linking inside a temporary folder; install.sh runs them through sudo.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}

# A path built to break out of quotes in a shell string.
odd="$work/it's \$(touch $work/ran-subst); touch $work/ran-semicolon; '"
mkdir -p "$odd"
touch "$odd/plugin.so"
bash "$root/installer/root-steps.sh" link "$odd/plugin.so" "$work/link.so"; code=$?
check "the steps succeed" "$code" 0
check "the link points at the path as written" "$(readlink "$work/link.so")" "$odd/plugin.so"
check "nothing in the path ran" "$(find "$work" -maxdepth 1 -name 'ran-*' | wc -l)" 0

bash "$root/installer/root-steps.sh" format-disk > /dev/null 2>&1; code=$?
check "an unknown step is refused" "$code" 2
bash "$root/installer/root-steps.sh" link "$odd/plugin.so" > /dev/null 2>&1; code=$?
check "a step missing an argument is refused" "$code" 2

check "install.sh hands root no shell text" "$(grep -c 'as_root sh -c\|as_root bash -c' "$root/install.sh")" 0
exit "$failures"
