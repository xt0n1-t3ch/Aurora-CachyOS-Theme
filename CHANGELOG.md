# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The version covers what you install and use: the commands `./install.sh` and `./uninstall.sh` and their options,
the ids and folders the theme writes to your home folder (`org.aurora.*`, `~/.local/share/aurora`), and the settings
it changes. A change that needs a fresh install, or that an earlier backup cannot undo, is a major version.

## [Unreleased]

### Added

- File Explorer's address field shows the path as segments you can click, as Windows' File Explorer does; a click
  beside them, Ctrl+L or Alt+D still opens it for typing. In both the address field and the path bar, the arrow after
  a segment lists the folders inside it, the ellipsis of a long path lists the folders it hides, a middle click or
  Ctrl+click opens a segment in a new tab, and a right-click offers Open, Open in New Tab, Open Terminal Here, Copy
  Path and Get Info.
- Files dropped on a segment move, copy or link there (it asks which), or get the tag of a tag's shelf. A drag held on
  a segment opens that folder, so you can go on deeper. Dropping a folder into itself, or files into the folder they
  are already in, is refused. A segment dragged out carries its folder, to the sidebar, a tab or a terminal.
- Screen readers read File Explorer's path bar as one link per folder, which they can press.

### Changed

- `aurora` looks for a newer Aurora when its menu opens, unless it looked in the last 5 minutes, so a new release
  shows up right away instead of after the next daily check.
- `aurora update` sets up Aurora's repository by itself when it needs it: when pacman.conf's `[aurora]` section is
  missing or points elsewhere, or pacman does not trust Aurora's key, it says so and fixes it with your password, so
  pacman always sees new versions. `aurora status` shows the repository's state.

### Fixed

- The menu no longer offers an older Aurora after you updated through pacman or paru. On 1.3.1 it still said
  "Aurora 1.3.0 is available".
- `aurora update` checks that pacman really installed the new version, and says how to retry when it did not.
- In File Explorer, the Move, Copy or Link menu of files dropped on the sidebar opens where they were dropped, not off
  to the side by the sidebar's offset in the window.

## [1.3.1] - 2026-11-08

### Fixed

- File Explorer and Quick Menu start on processors without AVX2, such as Intel Pentium, Celeron and Atom chips. The
  package's programs were marked as needing a newer processor, so the system refused to start them with "CPU ISA
  level is lower than required", and Quick Menu's key only showed "Launching KRunner failed".
- Building Aurora from its sources no longer stops at the pointing hand on a system without `vulkan-headers`, which
  KWin's build files ask for; the installer now installs it.

## [1.3.0] - 2026-11-07

### Changed

- The notifications popover in the menu bar looks like macOS Tahoe's Notification Center. Each notification sits on
  its own rounded glass card with the app's icon large at the left, a bold title with its time and the message below,
  and a picture in rounded corners at the right. Clear appears over a card's top-right corner on hover. The app's
  settings and Minimize moved to the menu of a right-click or the Menu key. Do not disturb reads as a settings row
  with its switch at the right, and the label also turns it on. An empty popover shows a bell with "No Notifications".
  A long message shows its first four lines and opens in full with a click or Show More, and older notifications
  show their date.
- Aurora's Mission Center is one sheet of dark glass like File Explorer: KWin blurs what is behind it, the device
  sidebar floats as the clearer glass, the pages switch in one glass capsule, the window buttons sit in another, and a
  window that is not in front turns opaque. The graphs are tinted cards, the Performance details sit on a glass card,
  and the Services filters are chips. The device sidebar now stays on Apps and Services too, when the window is wide
  enough for every column, and a click on a device opens its Performance view. The search field is always in the
  header bar, and typing on the Performance page searches the apps. Busy values in Apps and Services are tinted
  warmer as they rise, as in the Windows 11 Task Manager; Preferences and `aurora mission-center settings` turn the
  tint off.
- Mission Center's Apps and Services pages sit on a glass card with the column headings inside, the Apps and
  Processes headings no longer show as a grey selected bar, Stop, Force Stop and Details are separate glass pills, and
  busy cells get a rounded tint, blue for CPU and GPU and amber for memory. The device sidebar now fits beside every
  column from a 1220 px wide window. On Performance, each reading is a glass tile, two to a row, with the details on
  a card below, lined up with the graphs instead of starting under an empty gap. The selected device in the sidebar is
  glass instead of solid blue.
  Its header is File Explorer's toolbar: the window buttons sit in the same capsule with the same glyphs and at the
  same distance from the window's edges, every control has the same height on one line, and the sidebar and edit
  buttons sit at the sidebar's top left as in File Explorer, where the sidebar button stays when the sidebar is
  hidden. In a narrow window, or while the sidebar is being edited, the search field narrows so the page tabs keep
  their full names. The light style gets the same glass as the dark one instead of the old look.
  Its glass is now File Explorer's: the same tint over a real blur, the sidebar the clearer glass, and the same soft
  shadow drawn by KWin; the window resizes from just inside its edges. The devices in the sidebar, the tables and the
  graphs keep File Explorer's spacing from the edges instead of touching them.
  Its right-click menus, the main menu and tooltips are File Explorer's blurred glass with its row sizes, blue
  highlight, check marks and soft shadow, and submenus such as Send Signal open beside the menu instead of replacing
  it. Dialogs use File Explorer's dialog color, and Preferences is wide enough for its four page names. A drive's
  partitions sit on a glass card under its details, with File Explorer's capacity bars that turn amber past 90 % used
  and red past 95 %, and a network's addresses join the rest of its details on their card.
- The menu bar stays see-through over a maximized window, so its popups (notifications, sound, network) stay glass
  like File Explorer instead of turning solid gray.

### Fixed

- The menu bar's status icons drawn by Aurora (volume, network, media, the bell and the magnifier) show correctly in
  GTK apps. GTK drew their outlines filled, so Mission Center's search button showed a solid dot instead of a
  magnifier.
- Table headings in GTK apps line up with their columns again. Aurora's GTK theme padded them like list
  headers, which moved each heading to the right of its values and cut off the last one, such as Mission Center's
  GPU Memory.
- The window buttons of GTK apps are as sharp as File Explorer's. Their glyphs sat between two rows of pixels, so
  they looked dimmer and thicker.
- Mission Center's Performance details no longer run past the window's right edge in a narrow window.

## [1.2.0] - 2026-11-07

### Added

- Mission Center is part of Aurora. `aurora mission-center on` puts Aurora's build in place of the distribution's:
  each app's GPU, memory and network use counted exactly, a lean background service, fan readings that match the
  hardware and a color per device. It comes from Aurora's repository or is built on your PC; `off` puts the
  distribution's back. Every change saves a snapshot of the build and the settings first, and
  `aurora mission-center rollback` puts one back.
- `aurora mission-center settings` changes the refresh, the charts, the look, the graphs and the sidebar, and `--set`
  reaches any Mission Center setting, checked before anything changes. `aurora status` and `aurora settings` show it.

### Changed

- `aurora update` installs a new Aurora only in a way its parts keep working. It first reads which KDE Plasma, KWin,
  Qt and KDecoration the new package was built for, then updates it with the rest of the system when that is what a
  system update brings, on its own when it is built for the system as it is, and not at all when it fits neither,
  saying which versions differ. With no new Aurora out it changes nothing, where it used to update the whole system.
- `aurora update` installs the package from Aurora's repository with pacman instead of the AUR helper, so AUR packages
  are no longer rebuilt along with it.
- An inactive File Explorer window is opaque, as a macOS window is when it is not in front, so nothing behind it shows
  through, blurred or not. Its sidebar selection turns gray with it, like the selections in the file views.

### Fixed

- Aurora runs on every 64-bit x86 PC. The package was built for the processor of the PC that built it, so on
  a PC with an older processor `aurora` stopped with "Illegal instruction" right after the install, and its parts
  would have stopped KWin and the panel the same way
  ([#11](https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/issues/11)). It is now built for any x86-64 processor,
  and the package refuses to build when anything in it needs a newer one.
- File Explorer's Group menu works again with Dolphin 26.08, which replaced the action it used: None, Name, Kind, Date
  Modified, Date Created, Size and Tags, with the current choice checked and kept per folder.
- File Explorer's address and search fields show one focus ring. Breeze no longer adds its own square-cornered frame
  around them when Ctrl+L, Alt+D or Tab focuses them.

## [1.1.1] - 2026-11-06

### Fixed

- The install no longer stops at its last step, "Look and panels", with `unfilled placeholder in layout.js` on a
  desktop that does not have Aurora's menu bar yet: a comment in the panel script was taken for a placeholder.
- Publishing a new version uploads the package before the database that lists it, so `pacman -Syu` never reads an
  `[aurora]` database that names a package not online yet.

## [1.1.0] - 2026-11-05

### Added

- File Explorer takes typed paths: click the path in the toolbar, or press Ctrl+L or Alt+D, and type. Subfolders drop
  down as you type, Tab completes like a shell, `~` is your home folder, Enter goes there (a file opens its folder with
  the file selected) and Esc puts the path back. A path that leads nowhere rings the field red.
- File Explorer's row of file actions under the toolbar: new folder, new file, Move to Trash, rename, duplicate, Quick
  Look, cut, copy, paste, Open Terminal Here and Split View.
- Quick Look: Space shows the selected file without opening it (pictures whole, text files, and a large preview with
  the kind, size and date for the rest); Space or Esc closes it, Enter opens the file.
- Split View (F3): two folders side by side in one tab; the toolbar and the path bar follow the one you work in.
- Finder's color tags: tag files from the context menu's color dots, the Tags submenu, the Edit Tags button or by
  dropping them on a tag in the sidebar. Tagged files show their colors as dots on their icons, in File Explorer and in
  Dolphin, and each tag in the sidebar lists its files. Tags are the standard `user.xdg.tags` attribute that Dolphin
  and Baloo also read.
- The sidebar's folders unfold: a folder you added shows a chevron that opens its subfolders, and Favorites,
  Locations and Tags fold away under their titles. Both are remembered.
- Quick Menu's All Applications: the grid button beside ⋯ lists every app once under its first letter, like Windows'
  All apps list, under a heading that reads like a section's, with Back where a section has Show More.
- The toolbar's Group menu (by name, kind, date modified or size), a Share menu, and a More menu with hidden files,
  previews, sort, Get Info and Add to Sidebar.
- Windows open, close, minimize and restore with Aurora Motion, a KWin effect in macOS's manner: a short scale and
  fade instead of the close that looked like the window was swallowed, and a minimize into its Dock icon. Quick Menu
  rises and fades in, the Control Center drops in, and File Explorer's hovers fade and its wheel scroll glides.
- `aurora settings` chooses the animation speed, smooth, fast or instant (`--animations` from a script), and applies
  it at once to the running desktop.
- `aurora performance on` and `off` turn the measured tuning on or back to CachyOS's settings at any time; alone it
  shows the four settings and whether they are in place.
- `aurora backups` lists the backups of your desktop from before Aurora, and `aurora backups restore` puts one back.
- `aurora icons` changes the icon theme to Aurora's or any other installed one, at once.
- `aurora settings` and the `aurora` menu reach every one of these.

### Changed

- The README's install, update, tuning and removal sections use the `aurora` command; the manual steps moved into
  folded sections.
- File Explorer's context menu follows Finder's order: Open, Open With, Open in New Tab; Rename, Duplicate, Compress
  to .zip, Quick Look; Cut, Copy, Copy Path, Paste; Move to Trash; the tags; Get Info, Quick Actions (the service
  menus) and Open Terminal Here, which opens a file's folder too.
- File Explorer's list shows Finder's columns in its order (Name, Modified, Size, Type) with 22 px icons, and the path
  bar at the bottom names the selected file after its folder, beside the item count and free space. The toggles of
  the old status row moved to the More menu and the View menu.
- Animations run at Plasma's designed speed (smooth, 1) instead of near-instant values.

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

[Unreleased]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.3.1...HEAD
[1.3.1]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/tag/v1.0.0
