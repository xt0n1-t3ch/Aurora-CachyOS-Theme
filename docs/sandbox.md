[Aurora](../README.md) / [Documentation](index.md) / Sandbox

# The sandbox

`tests/sandbox/run.sh SCENARIO [TIMEOUT]` starts an isolated headless Plasma: `kwin_wayland --virtual` (2560x1440,
socket `wayland-sbx`, Xwayland) under `dbus-run-session`, with `HOME=tests/sandbox/home`, its own `XDG_*` folders and
runtime folder (`tests/sandbox/run`), and `QT_PLUGIN_PATH` and `QML_IMPORT_PATH` pointing at the sandbox's home first.
KWin runs SCENARIO as its session and exits with it. Nothing touches the real session, its settings or its D-Bus.

```bash
tests/sandbox/run.sh "$PWD/tests/sandbox/scenarios/quick-menu.sh" 200 > tests/sandbox/logs/run-quick-menu.log 2>&1
SANDBOX_SCALE=2 tests/sandbox/run.sh "$PWD/tests/sandbox/scenarios/readme.sh" 160   # the README's pictures, sharp
python3 -I assets/screenshots/make.py                                                # crops them into assets/
```

Pass the scenario's full path: KWin looks a bare name up in `PATH`. A scenario sources `prelude.sh` (starts
kactivitymanagerd and kded6, defines `shot NAME`, a Spectacle capture into `tests/sandbox/shots/NAME.png`, and
`waitfor SECONDS`), starts plasmashell or the app under test, drives it, captures, and writes PASS or FAIL lines to
`tests/sandbox/logs/<name>.txt` where it checks something. The home, captures and logs are not in git.

## Its home folder

The sandbox's home is a whole Plasma home (panels, caches, a browser profile), so it stays out of git. To give it the
theme's current files: `HOME=$PWD/tests/sandbox/home python3 -I installer/install.py files --no-session`.
`tests/sandbox/relayout.sh` rebuilds its panels from `layout.js` and copies the real tray's settings into it.

## Driving it

- `tests/helpers/fakeinput/fakeinput` speaks `org_kde_kwin_fake_input` to the nested KWin only: `move X Y`,
  `glide X Y STEPS`, `down`, `up`, `keydown CODE`, `keyup CODE` (evdev codes: 125 Win, 28 Return, 1 Esc,
  103/108/105/106 arrows), `wait MS`.
- `tests/helpers/region_changed.py BEFORE AFTER X0 Y0 X1 Y1` exits 0 when a region changed (mean difference above 6):
  the usual "did it open" check. Keep the region tight around what appears.
- `tests/helpers/fake-tray` puts tray icons in the nested tray (`tray_apps.sh`); `fake_mpris.py` plays a track.
- KWin scripts print into KWin's output, which is the run log.

## Gotchas

- Plasma's menu is `plasma-applications.menu`: export `XDG_MENU_PREFIX=plasma-` and run `kbuildsycoca6
  --noincremental` in scenarios that need the app catalog (Quick Menu, launchers).
- The sandbox's activity statistics stay empty even with a copied database, so Quick Menu's Suggestions show only live.
- KWin keeps the sandbox screen's saved scale; `readme.sh` sets the scale it needs with `kscreen-doctor` and puts 1 back.
- Wayland dismisses a popup that no real input opened: capture QWidget menus from an X11 client
  (`QT_QPA_PLATFORM=xcb`, `explorer-menus.sh`).
- An active fullscreen window covers the panel; QML `Item.left/top` are FINAL; a DropArea inside a ToolTipArea never
  gets drags; libplasma prefers an applet's compiled QML over a package on disk.
- A click outside a popup closes it: color searches for click targets must stay inside the panel.
