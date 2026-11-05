#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# krunner-launch.sh picks Quick Menu only when it was built for the plasma-workspace and Qt that are
# installed and its binary is there; anything else starts the stock KRunner, so a Plasma update never leaves the
# desktop without search. Fake pacman and fake binaries in a temp dir; each case prints which one ran and its args.
# Run it directly; it exits with the number of failed cases.
set -u
here=$(dirname "$(readlink -f "$0")")
launch="$here/../krunner-launch.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/bin" "$work/share"
# A fake pacman that answers only for the two packages the launcher asks about.
cat > "$work/bin/pacman" <<'EOF'
#!/bin/sh
case "$2" in
    plasma-workspace) echo "plasma-workspace 6.8.2-1.1" ;;
    qt6-base) echo "qt6-base 6.12.0-1" ;;
esac
EOF
# Fake Quick Menu and stock KRunner binaries that print their own name and arguments.
for name in quick-menu stock; do
    printf '#!/bin/sh\necho %s "$@"\n' "$name" > "$work/bin/$name"
done
chmod +x "$work/bin/"*
failures=0
# run: starts the launcher with every path pointed into the temp dir; prints the name and arguments of the program it
# started.
run() {
    PATH="$work/bin:$PATH" QUICK_MENU_BIN="$work/bin/quick-menu" STOCK_KRUNNER="$work/bin/stock" \
        QUICK_MENU_STAMP="$work/share/quick-menu-built-for" "$launch" --daemon
}
# expect DESCRIPTION WANTED: compares the output of run with WANTED; prints PASS or FAIL with DESCRIPTION and counts
# the failures.
expect() {
    local got
    got=$(run)
    if [ "$got" = "$2" ]; then echo "PASS $1"; else echo "FAIL $1: got '$got', want '$2'"; failures=$((failures + 1)); fi
}

# The stamp reads "<plasma-workspace version> <qt6-base version>", as the fake pacman reports them.
echo "6.8.2 6.12.0" > "$work/share/quick-menu-built-for"
expect "built for the installed Plasma and Qt: Quick Menu, with the arguments" "quick-menu --daemon"

echo "6.8.1 6.12.0" > "$work/share/quick-menu-built-for"
expect "built for an older Plasma: the stock KRunner" "stock --daemon"

echo "6.8.2 6.11.2" > "$work/share/quick-menu-built-for"
expect "built for an older Qt: the stock KRunner" "stock --daemon"

rm -f "$work/share/quick-menu-built-for"
expect "never built: the stock KRunner" "stock --daemon"

echo "6.8.2 6.12.0" > "$work/share/quick-menu-built-for"
rm -f "$work/bin/quick-menu"
expect "binary missing: the stock KRunner" "stock --daemon"

exit "$failures"
