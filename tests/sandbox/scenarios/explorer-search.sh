#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# File Explorer's search from aurora-search's index: the helper indexes the sandbox's home (run as the user, with
# `root =`, since the sandbox has no root), File Explorer types a word, the matches come from the whole home, hidden
# names left out, and the scope row limits them to the folder. Live changes need fanotify, so root: the virtual
# machine test covers them (components/file-explorer/search/tests/vm_server.rs). Captures shots/search-*.png, writes
# PASS or FAIL lines and the measured times and memory to logs/search.txt.
source "$SBX/prelude.sh"
FI="$SB/tests/helpers/fakeinput/fakeinput"
CHANGED="$SB/tests/helpers/region_changed.py"
OUT="$SBX/logs/search.txt"; : > "$OUT"
SHOTS="$SBX/shots"
DOCS="$HOME/Crumbs/Projects/Aurora/docs"
mkdir -p "$DOCS" "$HOME/Crumbs/Projects/Notes"
printf 'int main(void) { return 0; }\n' > "$DOCS/main.c"
printf 'plan\n' > "$HOME/Crumbs/Projects/Notes/aurora-plan.txt"
printf 'docs\n' > "$DOCS/aurora-notes.md"
rm -f "$HOME/Crumbs/aurora-live-note.txt"

check() {
    if python3 -I "$CHANGED" "$SHOTS/$2.png" "$SHOTS/$3.png" "$4" "$5" "$6" "$7" > /dev/null 2>&1; then
        echo "PASS $1" >> "$OUT"
    else
        echo "FAIL $1" >> "$OUT"
    fi
}
input() { "$FI" "$@" > /dev/null 2>&1; }
# type WORD: presses the keys of a lowercase word (evdev codes).
type_word() {
    local -A code=([a]=30 [b]=48 [c]=46 [d]=32 [e]=18 [f]=33 [g]=34 [h]=35 [i]=23 [j]=36 [k]=37 [l]=38 [m]=50 [n]=49
                   [o]=24 [p]=25 [q]=16 [r]=19 [s]=31 [t]=20 [u]=22 [v]=47 [w]=17 [x]=45 [y]=21 [z]=44 [-]=12)
    local word=$1 i
    for ((i = 0; i < ${#word}; i++)); do input keydown "${code[${word:i:1}]}" keyup "${code[${word:i:1}]}" wait 40; done
}

STATE="$SBX/run/search-state"
mkdir -p "$STATE"
printf 'root = %s\nexclude = %s/.cache\nexclude = %s/.local/share/baloo\nidle_seconds = 120\n' "$HOME" "$HOME" "$HOME" > "$STATE/search.conf"
export AURORA_SEARCH_SOCKET="$SBX/run/search.sock"
export QT_PLUGIN_PATH="$SB/build/file-explorer/bin${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
START=$(date +%s%N)
"$SB/build/search/aurora-search" --socket "$AURORA_SEARCH_SOCKET" --state "$STATE" --config "$STATE/search.conf" \
    > "$SBX/logs/search-helper.log" 2>&1 &
HELPER=$!
for _ in $(seq 1 100); do [ -S "$AURORA_SEARCH_SOCKET" ] && break; sleep 0.05; done
"$SB/build/search/aurora-search-query" --socket "$AURORA_SEARCH_SOCKET" --wait-ready --limit 1 aurora > /dev/null 2>&1
echo "first index of the home: $(( ($(date +%s%N) - START) / 1000000 )) ms" >> "$OUT"
for run in 1 2 3; do
    T0=$(date +%s%N)
    "$SB/build/search/aurora-search-query" --socket "$AURORA_SEARCH_SOCKET" --limit 200 aurora > /dev/null 2>&1
    echo "query 'aurora', whole home, run $run (process start included): $(( ($(date +%s%N) - T0) / 1000 )) us" >> "$OUT"
done
echo "entries: $("$SB/build/search/aurora-search-query" --socket "$AURORA_SEARCH_SOCKET" --limit 1 'regex:.' | head -1 | cut -f1)" >> "$OUT"
echo "helper memory: $(awk '/VmRSS/ {print $2, $3}' /proc/$HELPER/status)" >> "$OUT"

plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 12
"$SB/build/file-explorer/file-explorer" "$DOCS" > "$SBX/logs/fe-search.log" 2>&1 &
waitfor 8
shot search-rest

# Type into the search field: the matches of the whole home list at once.
input move 1700 309 wait 200 down wait 80 up wait 300
type_word aurora
input wait 1500
shot search-results
check results-from-the-whole-home search-rest search-results 830 500 1300 680

# The folder's capsule in the scope row: only the matches inside docs.
input move 983 406 wait 200 down wait 80 up wait 1500
shot search-folder
check the-scope-row-keeps-the-folder search-results search-folder 880 400 1010 425
IN_DOCS=$("$SB/build/search/aurora-search-query" --socket "$AURORA_SEARCH_SOCKET" --scope "$DOCS" aurora | head -1 | cut -f1)
[ "$IN_DOCS" = 1 ] && echo "PASS the-helper-finds-one-match-in-docs" >> "$OUT" || echo "FAIL the-helper-finds-one-match-in-docs ($IN_DOCS)" >> "$OUT"

echo "helper memory after: $(awk '/VmRSS/ {print $2, $3}' /proc/$HELPER/status)" >> "$OUT"
kill "$HELPER"
