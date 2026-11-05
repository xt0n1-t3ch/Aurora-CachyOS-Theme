[Aurora](../README.md) / [Documentation](index.md) / Build

# Building the parts

`./install.sh` builds everything on its own. This page is for building by hand while changing a part.

## Everything

```bash
installer/build.sh                 # every part into build/<part>, with logs in build/<part>.build.log
installer/build.sh quick-menu       # only the parts named
installer/build.sh --plan          # where each Plasma part's sources come from; builds nothing
```

Packages: [`installer/dependencies.txt`](../installer/dependencies.txt) (`./install.sh --check` lists the missing ones).
Every build is CMake with Ninja in `RelWithDebInfo`, through ccache when it is installed, at `nice 10`. A finished
part gets `build/<part>/built-for`, the versions it was built against; the install copies it beside the part.

| Part | Output in `build/` | Built for |
| --- | --- | --- |
| file-explorer | `file-explorer/file-explorer`, `fe_tests` | Qt |
| window-buttons | `window-buttons/org.aurora.decoration.so`, `decoration_tests`, `deco-preview` | KDecoration major.minor |
| glass | `glass/qml/org/aurora/glass/` | Qt |
| energy-page | `energy-page/bin/plasma/kcms/kinfocenter/kcm_energyinfo.so`, `test_power` | Qt |
| pointing-hand | `pointing-hand/aurora-pointer.so` | KWin |
| system-tray | `system-tray/bin/plasma/applets/org.kde.plasma.systemtray.so` | plasma-workspace and Qt |
| notifications | `notifications/bin/plasma/applets/org.kde.plasma.notifications.so` | plasma-workspace and Qt |
| quick-menu | `quick-menu/bin/krunner` | plasma-workspace and Qt |

## The three Plasma parts

The system tray, the notifications and Quick Menu are Plasma's own code. Each component keeps three things:

- `upstream/`: Plasma's files of the version in `vendor/`, untouched;
- `vendor/`: the same files with the theme's changes, the copy that builds;
- `<part>.patch`: the difference between the two (it reproduces `vendor/` exactly from `upstream/`, plus the binary
  `glass.frag.qsb` shader, which is copied).

`build.sh` builds from `vendor/` when the installed plasma-workspace is that version. For any other version it clones
that version's tag from KDE (`git clone --depth 1 --filter=blob:none --sparse`, only the folders the part needs), applies
the patch and builds from there. If the patch no longer applies, the part is skipped and Plasma keeps its own;
[update.md](update.md) shows how to port the patch.

## One part by hand

```bash
cmake -S components/window-buttons -B build/window-buttons -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/window-buttons
python3 -I installer/install.py components     # installs every built part, beside what it was built for
```

## Test helpers

```bash
gcc -O2 tests/helpers/fakeinput/fakeinput.c tests/helpers/fakeinput/fake-input-protocol.c -lwayland-client -lm \
    -o tests/helpers/fakeinput/fakeinput
cmake -S tests/helpers/fake-tray -B tests/helpers/fake-tray/build -G Ninja && cmake --build tests/helpers/fake-tray/build
gcc -O2 performance/measure/power/burst.c -o performance/measure/power/burst
```
