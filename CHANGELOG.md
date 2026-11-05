# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The version covers what you install and use: the commands `./install.sh` and `./uninstall.sh` and their options,
the ids and folders the theme writes to your home folder (`org.aurora.*`, `~/.local/share/aurora`), and the settings
it changes. A change that needs a fresh install, or that an earlier backup cannot undo, is a major version.

## [Unreleased]

## [1.0.0] - 2026-11-05

The first release: a macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6, installed with one command and one
question, and removed with one command.

### Added

- One command installs Aurora, in fish, bash or zsh: `curl -fsSL .../get.sh | bash` shows Aurora's wordmark, asks
  for your password once, adds Aurora's signed pacman repository, installs the aurora-theme package and opens
  `aurora`, whose only question is the menu bar's logo. The package comes with its parts built, so nothing is
  compiled on your machine, and every `sudo pacman -Syu` brings the new versions.
- Update notices: once a day (a user timer that catches up after the PC was off), Aurora looks for a newer version and
  offers it in a notification with Update now and Skip this version; `aurora` also names it above its menu.
  `aurora check-updates` looks right away.
- `aurora settings`: how often to look for updates (every day, every week or never; `--updates` sets it from a
  script) and the menu bar logo, so the install itself keeps its one question.
- Everything Aurora installs is in this repository: its themes, widgets and parts. Nothing is downloaded from other
  projects.
- `./install.sh` installs from the source in one go: the packages it needs, the eight compiled parts built for your
  Plasma, KWin and Qt, a backup of every setting it changes, and the look. Options: `--performance`, `--rebuild`,
  `--reset-panels`, `--check`, `--yes`.
- `./uninstall.sh` and `aurora uninstall` put back every saved setting and remove everything the theme added.
- The `aurora` command: the install with its one question, each step with its progress and a log, and `update`,
  `rebuild`, `logo`, `settings`, `check-updates`, `status` and `uninstall`. On the package, `aurora update` upgrades the
  whole system with it (`-Syu` through paru, yay or pacman), so there is never a partial upgrade.
- A license: Aurora's own code is free software under the GNU GPL version 3, with additional terms (NOTICE.md) that
  keep the author's credit in every version. `aurora license` shows them.
- File Explorer: a Finder-style file manager on Dolphin's engine, opened by Win+E, drawing every app's Open and Save
  panels and answering "Show in folder". Drag a row of its sidebar onto another of the same section to put Favorites
  and Locations in your own order; the order is kept.
- Window buttons: one glass capsule with minimize, maximize and close in Windows order, the same in Qt, GTK and
  libadwaita apps.
- Menu bar: the system menu under CachyOS's black-and-white logo (Arch Linux, Apple and Kali Linux are in its
  settings, each also in its own colors where it has them), the active app's name, the system tray with Windows 11
  drag and drop, Quick Menu, the Control Center and a macOS clock, in light text over the wallpaper.
- Quick Menu on the Windows key: macOS Tahoe's Applications view, and Plasma's search when you type. Drag an app onto
  another of its section to reorder the section; the order is kept. Right-click an app, or press the Menu key, to
  pin it to the top (a Pinned section above Suggestions) or to the Dock, remove it from Suggestions, edit it or
  uninstall it. Under Suggestions it also lists the app's recent files. Its rail, as in Windows 11's Start: your
  picture and name open Lock, Switch User and Log Out; the power button opens Sleep, Hibernate where the machine
  can, Restart and Shut Down.
- Control Center with Tahoe's floating glass modules, and notifications as macOS banners in the bottom-right corner.
- Dock with your pinned apps, Downloads and Trash, which steps aside for windows and comes back with one touch of the
  bottom edge.
- The pointing hand over everything clickable, KWin's window buttons included, and Aurora's cursors.
- Info Center > Energy: live CPU and GPU watts, a two-minute chart and the power profiles.
- Dawn Bird, Aurora's own wallpaper: a bird of folded glass rising at first light, in 5K, offered in the wallpaper
  picker.
- Aurora's colors, Plasma style, Kvantum and GTK themes, and icons with every app's own logo; the free Inter and
  JetBrains Mono fonts, or SF Pro and SF Mono when you installed them.
- `--performance`: the measured scx_lavd scheduler, memory and zram tuning.
- After a Plasma, KWin or Qt update, the parts built for the old version step aside on their own until the package
  brings them built for the new one; `aurora rebuild` builds them right away.

[Unreleased]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/tag/v1.0.0
