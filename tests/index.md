[Aurora](../README.md) / Tests

# Tests

`scripts/check.sh` builds every part and runs every suite below (about 25 seconds after a build;
`--no-build` skips the build). Counts are those of 2026-11-04 (qmltestrunner counts include its two setup cases).
While working, run the suites of what you changed, once, after the change is written; then the sandbox scenario of the
changed flow.

```bash
export QT_QPA_PLATFORM=offscreen q=/usr/lib/qt6/bin/qmltestrunner B=$PWD/build
```

## Installer

| What it proves | Command | Result |
| --- | --- | --- |
| The install keeps a menu bar that is already there; its backup lists every file it adds (install, then delete `created.txt`, gives back the same home); a second backup keeps the first; it installs the `aurora` command and offers Aurora's wallpaper; it copies no font, uses Inter and JetBrains Mono unless SF Pro or SF Mono is installed, and the repository carries no Apple font; an install from before the Quick Menu's name moves its files, its saved order and the menu bar button over; old menu bar widgets give their place to Aurora's own; the package's prebuilt parts export without object files; installed from the package, an older `aurora` in `~/.local/bin` goes so the package's runs; the update notices' timer runs the installed aurora daily, catches up after the PC was off, is enabled once, and the backup lists it; a running Plasma stops before old widgets get Aurora's ids (it would write the old ones back) and nothing to move leaves it alone; GTK apps get Plasma's cursor; animations are smooth (1) unless aurora.conf chooses fast or instant, and only its [look] group counts; every loaded KWin effect reloads so the new speed reaches the running desktop; Aurora Motion is installed and turned on while KWin's Scale, Squash and Magic Lamp step aside | `python3 -I tests/installer/test_install.py` | 31 pass |
| `install.sh --help` and `--version`, refuses unknown options, `--check` writes nothing and names the build folder (the user's cache for a read-only copy, as the aurora-theme package's); `uninstall.sh` without an install fails clearly | `tests/installer/test_cli.sh` | 12 pass |
| `get.sh`, README.md's one command: checks the downloaded key's fingerprint before pacman trusts it, adds `[aurora]` to pacman.conf once, upgrades with `pacman -Syu --needed aurora-theme`, then opens the package's `aurora install` (`/usr/bin`, ahead of an older copy on PATH); a second run only upgrades and opens it; another key changes nothing; README.md's command, `curl -fsSL .../get.sh | bash`, runs in fish, zsh and bash, which find only the stand-in sudo (a real one failing would count toward pam_faillock's lockout); Aurora's wordmark comes first, the password is asked once before anything changes, and aurora does not draw the wordmark again; it names the system (Wayland, Plasma 6.8.2), numbers the four steps with a tick and a time each, ends with the installed version, refuses Plasma 5 before any sudo, uses no middle dots, and points a `[aurora]` section with another address back at Aurora's | `tests/installer/test_get.sh` | 38 pass |
| The root part of the install (`installer/root-steps.sh`) takes paths as arguments: a path with a quote, `$(...)` or `;` is linked as written and runs nothing; unknown steps are refused; `install.sh` hands root no shell text | `tests/installer/test_root_steps.sh` | 6 pass |
| The Plasma parts build from `vendor/` for its version and from that version's own sources for any other; the `aurora` command builds when Rust is installed and is skipped without it or in a copy without `cli/` (the aurora-theme package's) | `tests/installer/test_build.sh` | 6 pass |
| The login script keeps a plugin only while it was built for the installed versions | `tests/installer/test_login_script.sh` | 7 pass |
| `installer/dependencies.txt` installs every package the build and the installer use (CMake packages from `build/*/CMakeCache.txt`, commands, Python modules), and names itself every package whose QML modules the widgets and parts import | `tests/installer/test_dependencies.sh` | pass (66 CMake packages, 18 commands, 46 QML modules) |

## Releases

| What it proves | Command | Result |
| --- | --- | --- |
| CHANGELOG.md is in the Keep a Changelog 2.0.0 format with SemVer versions; a release dates [Unreleased] and relinks; VERSION names the latest release | `python3 -I tests/release/test_changelog.py` | 15 pass |
| `scripts/release.sh` refuses a dirty tree, a non-SemVer version and an empty [Unreleased]; dates CITATION.cff; commits, tags, and builds an archive, the notes and verifying checksums; publishes nothing without `--publish` | `tests/release/test_release.sh` | 23 pass |
| Each Plasma patch on a fresh `upstream/` gives `vendor/` | `tests/release/test_patches.sh` | 3 pass |
| Every file of Aurora's own code carries the license header (author, GPL-3.0-only, NOTICE.md's additional terms) | `python3 -I scripts/license_headers.py --check` | pass (236 files) |
| llms.txt follows llmstxt.org's format, every link in it names a file of the repository, and llms.txt and llms-full.txt match the docs they are written from (no HTML left in llms-full.txt) | `python3 -I scripts/llms.py --check` | pass (15 links) |
| The repository's description and topics fit GitHub's limits (20 topics of lowercase letters, digits and hyphens, no repeats; 350 characters) | `python3 -I scripts/github_meta.py --check` | pass (20 topics) |
| The social preview is 1280x640, under 1 MB, with its text at 7:1 or more (`make.py` refuses to write it otherwise) | `python3 -I assets/readme/make.py` | pass (73 KB, 12.4:1) |
| Every shell script Aurora owns, and the `restore.sh` that `install.py` writes, has no ShellCheck finding at its strictest level (style), with `.shellcheckrc` | `scripts/lint-shell.sh` | pass (73 scripts) |
| The package depends on `installer/dependencies.txt` plus what the `aurora` command needs (its update notices too: checkupdates, notify-send, Konsole), builds with `installer/build-dependencies.txt`, its functions leave makepkg's variables alone, and `.SRCINFO` matches the PKGBUILD | `tests/release/test_pkgbuild.sh` | 4 pass |
| `scripts/pacman-repo.sh` builds a signed package and a signed `aurora` database of real files with the key's public half, refuses a key whose secret half is missing, and pacman (under fakeroot, in a throwaway root) installs the package from it and refuses a tampered database | `tests/release/test_pacman_repo.sh` | 14 pass |

## Components

| Part | Command | Result |
| --- | --- | --- |
| File Explorer core | `build/file-explorer/fe_tests` | 78 pass |
| File Explorer's Finder features (typed paths and their suggestions and Tab, color tags and their emblems, the tag shelf's names and path, the sidebar's folder tree, Quick Look's kinds and when Space opens it) | `build/file-explorer/fe_finder_tests` | 33 pass |
| Window buttons | `build/window-buttons/decoration_tests` | 11 pass |
| Energy page | `build/energy-page/test_power` | 10 pass |
| Glass blur | `(cd components/glass && $q -import $B/glass/qml -input tests/tst_blurbehind.qml)` | 9 pass |
| Pointer hints | `(cd components/glass && $q -import $B/glass/qml -input tests/tst_pointerhints.qml)` | 13 pass |
| Clock | `(cd components/clock && $q -input tests/tst_clock.qml)` | 9 pass |
| Menu bar app name | `(cd components/app-title && $q -input tests/tst_apptitle.qml)` | 4 pass |
| Control Center (grid, module texts, a pill's title clear of its rounded end, its entrance at the desktop's speed) | `(cd components/control-center && $q -import $B/glass/qml -input tests/tst_cc.qml)` | 20 pass |
| Window motion (open, close, minimize into the Dock icon and back, Quick Menu's close, windows it leaves alone) | `bun test components/motion/tests` | 6 pass |
| The color tag emblems: 63 of them, their folder registered once, File Explorer's colors | `tests/installer/test_tag_emblems.sh` | 5 pass |
| Tray drag layering | `(cd components/system-tray/tests && $q -input tst_draglayering.qml)` | 5 pass |
| Kali menu | `(cd components/kali-menu && $q -input tests/tst_menu.qml)` | 31 pass, 1 known fail: `test_mouse_drag_reorders` (the settings editor's drag under offscreen; it fails on the untouched upstream package too) |
| Quick Menu logic (sections, keys, dragged order, the app menu, pins, the Dock script, the account and power rail, its entrance) | `(cd components/quick-menu && $q -input tests/tst_quickmenu.qml)` | 40 pass |
| Quick Menu launcher | `components/quick-menu/tests/test_launch.sh` | 5 pass |
| Menu bar logos (the logo table, colors, ids) | `(cd components/kali-menu && $q -input tests/tst_logos.qml)` | 8 pass |
| The `aurora` command: logos, install steps, releases (SemVer tags, HTTPS links, checksums, archive members), sources, logs, the banner (not twice after get.sh); update notices (daily, weekly or never with a few hours of slack, settings and record round trips, checkupdates output recorded on CachyOS, a skipped version, the notice's title, text and buttons, and that it stays until answered; the package release `aurora update` compares; animation speeds read back, default to smooth and are steps of Plasma's slider, the same table as install.py's); sudo's password dialog behind a wrapper that keeps its warnings out of the terminal; the package's program installs the package's copy over an older installed one; contract tests against `tools.js`, `install.sh` and `release.sh` | `cargo test --locked --manifest-path cli/Cargo.toml` | 63 + 3 + 8 pass |
| Plasma patches | each `<part>.patch` on a fresh copy of `upstream/` (plus the `.qsb`) reproduces `vendor/` ([../docs/update.md](../docs/update.md)) | 3 reproduce |

## Sandbox scenarios

`tests/sandbox/run.sh "$PWD/tests/sandbox/scenarios/NAME.sh" TIMEOUT > tests/sandbox/logs/run-NAME.log 2>&1`
([../docs/sandbox.md](../docs/sandbox.md)). Checking scenarios write PASS or FAIL lines to `tests/sandbox/logs/`; the
others capture `tests/sandbox/shots/` to compare by eye.

| Scenario | Checks or captures |
| --- | --- |
| `windows-key.sh` | Win alone opens Quick Menu, again closes it, Win+E does not, Alt+Space does (4 PASS after the old bug's check) |
| `dock-keeps-pins.sh` | the layout keeps the apps pinned in the task manager it replaces, File Explorer first (PASS) |
| `quick-menu.sh` | Quick Menu: the Applications view, arrow keys, Show More, results, over a white window |
| `quick-menu-drag.sh` | dragging ProtonPlus onto Faugus in Games puts it first, saved, and kept after KRunner restarts (PASS) |
| `quick-menu-rail.sh` | the rail: the account's picture and name; Restart, Shut Down, Log Out and Lock each reach Plasma's session services, stood in for by `tests/helpers/fake_session.py` so no real call can restart or lock the machine (5 PASS) |
| `cli.sh` | the `aurora` command in Konsole with Aurora's profile on Dawn Bird: its menu under the banner and the credit, then `aurora status` (captures) |
| `quick-menu-app-menu.sh` | the app menu: right-click, Pin to Top and the Pinned section, the Menu key (not the field's text menu), Unpin from Dock, an opened app in Suggestions and Remove from Suggestions, no QML warnings (8 PASS); labels found with `tests/helpers/find_text.py` |
| `readme.sh` | the README's pictures (run with `SANDBOX_SCALE=2`, then `assets/screenshots/make.py`) |
| `kali-menu.sh` | the Kali menu with PC labels |
| `explorer.sh`, `explorer-menus.sh`, `explorer-drives.sh`, `explorer-sidebar-order.sh`, `show-in-folder.sh`, `open-save.sh` | File Explorer, its menus (X11 client), its drives, dragging sidebar rows into a new order, "Show in folder", Open and Save |
| `notifications.sh` | banners in the bottom-right corner, hover buttons |
| `tray-click.sh`, `tray-drag.sh`, `tray-show.sh` | tray clicks, drag to reorder and hide, bring back |
| `control-center.sh`, `control-center-panels.sh`, `control-center-bluetooth.sh`, `control-center-shortcut.sh` | the Control Center, its panels, reopening, its shortcut |
| `dock.sh`, `dock-edge.sh` | the Dock steps aside for a maximized window; one touch of the edge brings it back |
| `pointing-hand.sh`, `pointing-hand-kwin.sh`, `pointing-hand-cost.sh` | the pointing hand in the shell and on window buttons, and its cost |
| `energy-page.sh` | Info Center > Energy after 12 s of samples |
| `menu-bar.sh`, `setup-panels.sh` | the menu bar as installed; the panels from `layout.js` |
| `window-buttons.sh`, `gtk-apps.sh` | the same capsule in KWin-drawn, GTK and libadwaita windows |

## On the live desktop

- `python3 -I components/windows-key/windows_key.py status`; `systemctl --user show -p MainPID --value plasma-krunner.service`.
- `performance/measure/power/ab.sh 3 20` (Balanced against Performance); `performance/measure/probe.py` (launch times).
