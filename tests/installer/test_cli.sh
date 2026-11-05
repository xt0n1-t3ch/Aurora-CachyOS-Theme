#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# install.sh explains itself, refuses options it does not know, and --check changes nothing: it runs here against an
# empty home folder and must leave it empty.
set -u
here=$(dirname "$(readlink -f "$0")")
root=$(readlink -f "$here/../..")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
# check NAME GOT WANT: prints PASS when GOT equals WANT and FAIL otherwise; a FAIL counts as a failure.
check() {
    if [ "$2" = "$3" ]; then echo "PASS $1"; else echo "FAIL $1: got '$2', want '$3'"; failures=$((failures + 1)); fi
}

out=$("$root/install.sh" --help 2>&1); code=$?
check "--help succeeds" "$code" 0
check "--help shows the usage" "$(grep -c '^Usage: ./install.sh' <<< "$out")" 1

check "--version prints the version in VERSION" "$("$root/install.sh" --version)" "Aurora $(cat "$root/VERSION")"

"$root/install.sh" --no-such-option > /dev/null 2>&1; code=$?
check "an unknown option is refused" "$code" 2

"$root/uninstall.sh" --help > /dev/null 2>&1; code=$?
check "uninstall.sh --help succeeds" "$code" 0

mkdir -p "$work/home"
HOME="$work/home" "$root/install.sh" --check > "$work/check.log" 2>&1; code=$?
check "--check finishes (0: ready, 1: something missing)" "$([ "$code" -le 1 ] && echo ok || echo "exit $code")" ok
check "--check writes nothing into the home folder" "$(find "$work/home" -mindepth 1 | wc -l)" 0

check "--check names the build folder next to the files" "$(grep -c "^build folder: $root/build$" "$work/check.log")" 1

# The aurora-theme package's copy is read-only: its parts are built in the user's cache.
mkdir -p "$work/packaged/installer"
cp "$root/install.sh" "$root/VERSION" "$work/packaged/"
cp "$root/installer/dependencies.txt" "$root/installer/build-dependencies.txt" "$root/installer/install.py" \
    "$work/packaged/installer/"
chmod -R a-w "$work/packaged"
HOME="$work/home" XDG_CACHE_HOME="$work/cache" "$work/packaged/install.sh" --check > "$work/packaged.log" 2>&1
chmod -R u+w "$work/packaged"
check "a read-only copy builds in the user's cache" "$(grep -c "^build folder: $work/cache/aurora/build$" "$work/packaged.log")" 1
# The copy has no build and no themes: --check lists them instead of failing on the first one.
check "--check lists what a copy lacks without a traceback" \
    "$([ "$(grep -c '^missing ' "$work/packaged.log")" -gt 0 ] && ! grep -q Traceback "$work/packaged.log" && echo yes)" yes
check "--check names a build folder outside the copy by its full path" "$(grep -c '^missing \.\./' "$work/packaged.log")" 0

# The aurora-theme package's copy comes with its parts built (prebuilt/): it builds nothing, so it asks for no build
# tools. A pacman on PATH reports Plasma 6 and every package as missing, to show what each copy asks for.
mkdir -p "$work/fakebin" "$work/prebuilt-copy/installer" "$work/prebuilt-copy/prebuilt"
cat > "$work/fakebin/pacman" <<'FAKE'
#!/bin/sh
case "$1" in
    -Q) echo "plasma-workspace 6.8.2-1" ;;
    -T) shift; printf '%s\n' "$@" ;;
esac
FAKE
chmod +x "$work/fakebin/pacman"
cp "$root/install.sh" "$root/VERSION" "$work/prebuilt-copy/"
cp "$root/installer/dependencies.txt" "$root/installer/build-dependencies.txt" "$root/installer/install.py" \
    "$root/installer/build.sh" "$work/prebuilt-copy/installer/"
PATH="$work/fakebin:$PATH" HOME="$work/home" "$work/prebuilt-copy/install.sh" --check > "$work/prebuilt.log" 2>&1
check "the package's copy installs from its prebuilt parts" \
    "$(grep -c "^build folder: $work/prebuilt-copy/prebuilt$" "$work/prebuilt.log")" 1
check "the package's copy asks for no build tools" "$(grep '^packages to install:' "$work/prebuilt.log" | grep -c -w cmake)" 0
PATH="$work/fakebin:$PATH" HOME="$work/home" "$root/install.sh" --check > "$work/checkout.log" 2>&1
check "a checkout asks for the build tools too" "$(grep '^packages to install:' "$work/checkout.log" | grep -c -w cmake)" 1

HOME="$work/home" "$root/uninstall.sh" > "$work/uninstall.log" 2>&1; code=$?
check "uninstall.sh without an install says so and fails" "$code" 1

exit "$failures"
