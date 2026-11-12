#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Every check of the project, as tests/index.md lists them, in one command: builds the parts, then runs the component
# suites, the installer and release tests, the changelog format and the Plasma patches. Exits non-zero when any fails.
# The sandbox scenarios need a nested Plasma session and stay manual (tests/index.md).
#
#   scripts/check.sh            build, then every check
#   scripts/check.sh --no-build  the checks only, on the existing build
set -uo pipefail
ROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/..")
cd "$ROOT" || exit 1
# The QML suites run without a display.
export QT_QPA_PLATFORM=offscreen
QML=/usr/lib/qt6/bin/qmltestrunner
# The default build folder, where installer/build.sh puts the parts.
B="${BUILD_DIR:-$ROOT/build}"
failed=()
# The one failure tests/index.md documents: the Kali menu's settings editor drag under offscreen fails on the untouched
# upstream package too.
KNOWN_FAILURES="test_mouse_drag_reorders"

# Runs one suite: run NAME COMMAND.... Prints pass, known or FAIL with the last summary line of its output, and adds
# the suite to `failed` when it fails for any reason but KNOWN_FAILURES.
run() {
    local name=$1 log
    shift
    log=$(mktemp)
    if "$@" > "$log" 2>&1; then
        printf '\033[32mpass\033[0m  %-26s %s\n' "$name" "$(grep -E 'Totals|PASS|^OK|checked|test result|^ *[0-9]+ pass$' "$log" | tail -1)"
    else
        local unknown
        unknown=$(grep -E '^FAIL' "$log" | grep -v -E "$KNOWN_FAILURES" || true)
        if [ -z "$unknown" ] && grep -q -E "$KNOWN_FAILURES" "$log"; then
            printf '\033[33mknown\033[0m %-26s %s\n' "$name" "$(grep Totals "$log" | tail -1) (known: $KNOWN_FAILURES)"
        else
            printf '\033[31mFAIL\033[0m  %-26s\n' "$name"
            grep -E '^FAIL|Error|error' "$log" | head -5 | sed 's/^/      /'
            failed+=("$name")
        fi
    fi
    rm -f "$log"
}
# Runs one qmltestrunner file from DIR: qml DIR TEST [IMPORT]. IMPORT is an extra QML import path.
qml() {
    (cd "$1" && "$QML" ${3:+-import "$3"} -input "$2")
}

if [ "${1:-}" != "--no-build" ]; then
    installer/build.sh > /dev/null || failed+=("build (see build/*.build.log)")
fi
run "file explorer" "$B/file-explorer/fe_tests"
run "file explorer finder" "$B/file-explorer/fe_finder_tests"
QT_QPA_PLATFORM=offscreen run "file explorer widgets" "$B/file-explorer/fe_widget_tests"
run "file explorer git" "$B/file-explorer/fe_git_tests"
QT_QPA_PLATFORM=offscreen run "file explorer git sheets" "$B/file-explorer/fe_git_widget_tests"
run "window buttons" "$B/window-buttons/decoration_tests"
run "energy page" "$B/energy-page/test_power"
run "glass blur" qml components/glass tests/tst_blurbehind.qml "$B/glass/qml"
run "pointer hints" qml components/glass tests/tst_pointerhints.qml "$B/glass/qml"
run "clock" qml components/clock tests/tst_clock.qml
run "menu bar app name" qml components/app-title tests/tst_apptitle.qml
run "control center" qml components/control-center tests/tst_cc.qml "$B/glass/qml"
run "tray drag layering" qml components/system-tray/tests tst_draglayering.qml
run "kali menu" qml components/kali-menu tests/tst_menu.qml
run "menu logos" qml components/kali-menu tests/tst_logos.qml
run "quick-menu logic" qml components/quick-menu tests/tst_quickmenu.qml
run "quick-menu launcher" components/quick-menu/tests/test_launch.sh
run "window motion" bun test components/motion/tests
run "tag emblems" tests/installer/test_tag_emblems.sh
run "status icons" tests/installer/test_status_glyphs.sh
run "plasma patches" tests/release/test_patches.sh
run "installer" python3 -I tests/installer/test_install.py
run "GTK theme" python3 -I tests/installer/test_gtk_theme.py
run "install.sh options" tests/installer/test_cli.sh
run "one-command install" tests/installer/test_get.sh
run "root steps" tests/installer/test_root_steps.sh
run "build sources" tests/installer/test_build.sh
run "login script" tests/installer/test_login_script.sh
run "dependencies" tests/installer/test_dependencies.sh
run "changelog" python3 -I tests/release/test_changelog.py
run "release" tests/release/test_release.sh
run "AUR package" tests/release/test_pkgbuild.sh
run "CPU baseline" tests/release/test_cpu_baseline.sh
run "older processor plan" tests/release/test_old_cpu.sh
run "aurora update fit" tests/release/test_built_for.sh
run "pacman repository" tests/release/test_pacman_repo.sh
run "license headers" python3 -I scripts/license_headers.py --check
run "llms.txt" python3 -I scripts/llms.py --check
run "GitHub metadata" python3 -I scripts/github_meta.py --check
if command -v shellcheck > /dev/null; then
    run "shell lint" scripts/lint-shell.sh
fi
# The aurora command and File Explorer's search helper are Rust and optional, so their tests run only where cargo is
# installed. The helper's root tests run in a virtual machine (components/file-explorer/search/tests/vm.sh).
if command -v cargo > /dev/null; then
    run "search helper" cargo test --quiet --locked --manifest-path components/file-explorer/search/Cargo.toml
    run "aurora command" cargo test --quiet --locked --manifest-path cli/Cargo.toml
fi

if [ ${#failed[@]} -gt 0 ]; then
    printf '\n\033[31m%d failed:\033[0m %s\n' "${#failed[@]}" "${failed[*]}"
    exit 1
fi
printf '\n\033[32mevery check passed\033[0m\n'
