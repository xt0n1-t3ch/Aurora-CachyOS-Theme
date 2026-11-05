[Aurora](../README.md) / [Documentation](index.md) / Architecture

# How the theme hooks into the desktop

The theme never edits a file that a package owns. Each part either loads ahead of the stock component with the same
id, is started instead of it through a user-level systemd drop-in, or is a setting that `installer/install.py`
writes after saving the old value. Root is needed for two plugin links, and for the tuning only with `--performance`.

## Session start

`~/.config/plasma-workspace/env/aurora.sh` (from [`installer/session/aurora.sh`](../installer/session/aurora.sh))
is sourced by Plasma before KWin and plasmashell start. It:

- puts `~/.local/lib/qt6/plugins` first in `QT_PLUGIN_PATH` and `~/.local/lib/qt6/qml` first in `QML_IMPORT_PATH`;
- sets `PLASMA_INTEGRATION_USE_PORTAL=1` and `GTK_USE_PORTAL=1`, so Open and Save panels come from File Explorer;
- compares what each plugin was built for with the installed packages, and renames a plugin built for other versions
  to `*.paused`, so Plasma and KWin load their stock part instead of one built for other libraries.

| In `~/.local/share/aurora/` | Holds | Guards |
| --- | --- | --- |
| `system-tray-built-for` | plasma-workspace and qt6-base versions | the system tray |
| `notifications-built-for` | plasma-workspace and qt6-base versions | the notifications applet |
| `window-buttons-built-for` | kdecoration major.minor | the window decoration |
| `quick-menu-built-for` | plasma-workspace and qt6-base versions | Quick Menu (checked by its launcher at every start) |

`installer/build.sh` writes `build/<part>/built-for`; the install copies it beside the installed part, and
`~/.local/share/aurora/rebuild.sh` builds and installs again ([update.md](update.md)).

## Plugins that load ahead of the stock ones

Same plugin id, found first through `QT_PLUGIN_PATH` (KPluginMetaData prefers the first match):

| Installed file | Replaces or adds |
| --- | --- |
| `~/.local/lib/qt6/plugins/plasma/applets/org.kde.plasma.systemtray.so` | the system tray (drag and drop, hidden items, click fixes) |
| `~/.local/lib/qt6/plugins/plasma/applets/org.kde.plasma.notifications.so` | the notifications applet (macOS banners) |
| `~/.local/lib/qt6/plugins/plasma/kcms/kinfocenter/kcm_energyinfo.so` | Info Center > Energy |
| `~/.local/lib/qt6/plugins/org.kde.kdecoration3/org.aurora.decoration.so` | adds the window decoration, chosen in kwinrc |
| `~/.local/lib/qt6/plugins/kwin/plugins/aurora-pointer.so` | adds a KWin plugin: the pointing hand over window buttons |

KWin starts before the login script sets the plugin path, so the decoration and the pointer plugin are also linked
into `/usr/lib/qt6/plugins/` ([install.md](install.md)).

The QML module `org.aurora.glass` (`BlurBehind`, `PointerHints`) lives in `~/.local/lib/qt6/qml/org/aurora/glass`.
plasmashell finds it through `~/.config/systemd/user/plasma-plasmashell.service.d/aurora.conf`, Quick Menu through
its own drop-in.

## Processes started instead of the stock ones

| Override | Effect |
| --- | --- |
| `~/.config/systemd/user/plasma-krunner.service.d/aurora.conf` | `plasma-krunner.service` runs `~/.local/lib/aurora/quick-menu/krunner-launch`, which starts Quick Menu when it was built for the installed Plasma and Qt, and `/usr/bin/krunner` otherwise |
| `~/.config/systemd/user/plasma-{kwin_wayland,plasmashell}.service.d/90-aurora-noswap.conf` | `MemorySwapMax=0`: the compositor and the shell never go to swap |
| `~/.local/share/dbus-1/services/org.freedesktop.FileManager1.service` | "Show in folder" opens File Explorer (`--service`) |
| `~/.local/share/dbus-1/services/org.freedesktop.impl.portal.desktop.aurora.service`, `~/.local/share/xdg-desktop-portal/portals/aurora.portal`, `~/.config/xdg-desktop-portal/kde-portals.conf` (`FileChooser=aurora;kde`) | Open and Save panels are File Explorer windows (`--portal`) |
| `~/.config/environment.d/90-aurora-portal.conf` | the portal environment for apps that systemd starts |

## Shortcuts (kglobalaccel, over D-Bus)

- Win+E: Dolphin's `_launch` gives up Meta+E and `file-explorer.desktop` `_launch` takes it (`set_explorer_shortcut`
  in `installer/install.py`).
- Win alone: moved from plasmashell's "Activate Application Launcher" to `org.kde.krunner.desktop` `_launch`
  ([`components/windows-key/windows_key.py`](../components/windows-key/windows_key.py) `quick-menu`). Since Plasma 6.1
  a lone modifier is an ordinary kglobalaccel shortcut; `kwinrc [ModifierOnlyShortcuts]` is no longer read.

## Settings the install writes

Saved first (the `SAVED` list of `installer/install.py`) and put back by the uninstall: `kdeglobals`, `kwinrc`,
`plasmarc`, `kcminputrc`, `dolphinrc`, `konsolerc`, the Plasma panel file, `plasmashellrc`, `kglobalshortcutsrc`,
`mimeapps.list`, `plasmanotifyrc`, `xdg-desktop-portal/kde-portals.conf`, Kvantum, GTK 2/3/4, xsettingsd,
`konsolestaterc`. The notable keys: kwinrc decoration `org.aurora.decoration` with `ButtonsOnRight=IAX`,
`[Windows] ElectricBorderPushbackPixels=0` (one touch of the bottom edge brings the Dock back),
`plasmanotifyrc [Notifications] PopupPosition=BottomRight`, the icon theme `Aurora-dark`, the Konsole
profile `Aurora`, and the panel layout from [`themes/panels/layout.js`](../themes/panels/layout.js).

## Files the install adds to the home folder

The `ADDED` list of `installer/install.py`, removed again by the uninstall through the backup's `created.txt`: the
base icon themes (`Aurora-base`, `Aurora-base-dark`), the `Aurora-dark` icon theme, the `Aurora` cursors (and
`~/.icons/Aurora`), the theme's own app icons, Aurora's color scheme, Plasma style, Kvantum theme and GTK theme, the
Dawn Bird wallpaper, the six
widgets, File Explorer and its desktop entry, the Quick Menu entry, the Konsole profile and toolbars, the plugins and the
glass module above, the login script, the systemd drop-ins, the D-Bus services and the portal, and two state folders:
`~/.local/share/aurora` (what each part was built for, `rebuild.sh`, and `source`, a link to the checkout) and
`~/.local/lib/aurora` (Quick Menu and its launcher).

The backups live in `~/.local/state/aurora/backups/`, with `latest-backup` naming the one the uninstall uses;
they outlive an uninstall.

## Outside the home folder (root)

| File | Purpose | Undo |
| --- | --- | --- |
| `/usr/lib/qt6/plugins/org.kde.kdecoration3/org.aurora.decoration.so` (link) | KWin loads the decoration before the session environment | `./uninstall.sh` |
| `/usr/lib/qt6/plugins/kwin/plugins/aurora-pointer.so` (link) | the KWin pointer plugin | `./uninstall.sh` |
| `/etc/scx_loader.toml`, and `scx_loader.service` enabled | scx_lavd in Auto mode at boot (`--performance`) | `./uninstall.sh` |
| `/etc/tmpfiles.d/thp.conf` | THP defrag `defer` (`--performance`) | `./uninstall.sh` |
| `/etc/sysctl.d/99-aurora-memory.conf`, `/etc/systemd/zram-generator.conf.d/90-aurora-zram.conf` | `vm.watermark_scale_factor=125`, zram lz4 (`--performance`) | `./uninstall.sh` |

Why each tuning value: [performance.md](performance.md).
