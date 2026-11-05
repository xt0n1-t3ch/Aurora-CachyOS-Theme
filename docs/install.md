[Aurora](../README.md) / [Documentation](index.md) / Install

# Installing

The package installs Aurora with its parts built (README.md, [Installation](../README.md#installation)). From the
source:

```bash
git clone https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme.git
cd Aurora-CachyOS-Theme
./install.sh
```

Log out and back in once afterwards. `./install.sh --help` lists the options; the README explains them
([Install](../README.md#installation)).

## What each step runs

| Step | Runs | Changes |
| --- | --- | --- |
| 1. Packages | `pacman -S --needed` with what `installer/dependencies.txt` lists and the system lacks, plus `installer/build-dependencies.txt` when the parts are built here | system packages (sudo) |
| 2. Themes | checks that `themes/` holds every theme | nothing |
| 3. Build | `installer/build.sh` ([build.md](build.md)); with the package, `installer/build.sh --current` checks its `prebuilt/` parts instead | `build/` (nothing with the package) |
| 4. Install | `installer/install.py backup`, then `files` (`components` with `--rebuild`) | the home folder |
| 5. System | links the window buttons and the pointing hand into `/usr/lib/qt6/plugins`; with `--performance`, the tuning files | sudo |
| 6. Look and panels | `installer/install.py look`, then `panels` (skipped with `--rebuild`) | settings, panels |

The steps of `installer/install.py` can run alone too:

| Command | Does |
| --- | --- |
| `install.py backup` | Saves every setting in `SAVED` to `~/.local/state/aurora/backups/<date>/home/`, lists in `created.txt` what does not exist yet, writes `restore.sh` and `windows_key.py` there and points `latest-backup` at it. Kept from the first install: a second run leaves it alone. |
| `install.py files` | Icon themes, cursors, color scheme, Plasma style, Kvantum, wallpapers, widgets, the built parts, desktop entries, Konsole files, the login script, the drop-ins, the D-Bus services, the portal, `rebuild.sh` and the `source` link |
| `install.py components` | Only the built parts, each beside what it was built for |
| `install.py look` | Fonts, units, colors, Plasma style, icons, cursor, Kvantum, notification corner, GTK, Konsole, decoration, Dock edge, the Windows key, Win+E, File Explorer as the default file manager |
| `install.py panels [--force]` | The menu bar and the Dock from `themes/panels/layout.js`, unless the menu bar is already there; then the tray settings and hidden items, and a plasmashell restart |
| `install.py check` | Lists what the steps need and cannot find; changes nothing |

`--no-session` installs files without telling the running Plasma session (the tests use it with a temporary HOME).

## Root steps by hand

`install.sh` runs these through sudo (with `SUDO_ASKPASS` set it uses `sudo -A`, a graphical password prompt):

```bash
P=~/.local/lib/qt6/plugins
sudo ln -sfn $P/org.kde.kdecoration3/org.aurora.decoration.so /usr/lib/qt6/plugins/org.kde.kdecoration3/
sudo ln -sfn $P/kwin/plugins/aurora-pointer.so /usr/lib/qt6/plugins/kwin/plugins/
qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin aurora-pointer
# The tuning (aurora performance on, or --performance; performance/system/, reasons in performance.md):
sudo install -Dm644 performance/system/scx_loader.toml /etc/scx_loader.toml && sudo systemctl enable --now scx_loader.service
sudo install -Dm644 performance/system/thp.conf /etc/tmpfiles.d/thp.conf
sudo install -Dm644 performance/system/99-aurora-memory.conf /etc/sysctl.d/99-aurora-memory.conf && sudo sysctl -q --system
sudo install -Dm644 performance/system/90-aurora-zram.conf /etc/systemd/zram-generator.conf.d/90-aurora-zram.conf
```

## Checking an install

- `systemctl --user show -p MainPID --value plasma-krunner.service` runs `~/.local/lib/aurora/quick-menu/krunner`.
- `python3 -I components/windows-key/windows_key.py status` names `org.kde.krunner.desktop / _launch`.
- `qdbus6 org.kde.KWin /KWin supportInformation | grep -A1 ^Decoration` shows `Plugin: org.aurora.decoration`.
- `find ~/.local/lib/qt6/plugins -name '*.paused'` prints nothing (no part stepped aside).
- With `--performance`: `cat /sys/kernel/sched_ext/root/ops` names lavd; `cat /proc/sys/vm/watermark_scale_factor` is 125.
