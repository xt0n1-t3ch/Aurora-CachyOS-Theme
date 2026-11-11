[Aurora](../README.md) / [Documentation](index.md) / Decisions

# Design decisions

The rules I set while building Aurora, and why. They decide every design question.

## Look and behaviour

- As beautiful as macOS Tahoe, with the habits of Windows, and no regression against the stock desktop (Dolphin's
  places, the tray's behaviour). Improvements are welcome, for example Windows 11 style drag and drop in the tray.
- Every design change is shown in real screenshots before it is applied.
- Window buttons: one minimalist glass capsule rather than traffic lights, in Windows order (minimize, maximize,
  close), the same size in Qt, GTK, libadwaita and the window decoration.
- Real app logos, and unique Settings, Downloads and Trash icons.
- Apps with their own design keep it (Edge, for one). The File Explorer design is for system surfaces: the Control
  Center, Open and Save panels, Win+E and the shell.
- No global menu in the top bar.
- The top bar and the Control Center reach macOS Tahoe's quality: smooth text, SF Symbols-like icons, macOS's order,
  and the date and time spaced as on macOS.
- Notifications are macOS banners, in the bottom-right corner where CachyOS puts them.
- Shortcut hints in menus are written the Windows way ("Ctrl+C"), never with Mac symbols.
- The Windows key alone opens Quick Menu, as Start does on Windows, drawn like Tahoe's Applications view. Its apps can
  be dragged within their category and keep that order. A right-click opens Open, the app's own actions, Pin to Top (a
  Pinned section above Suggestions, like Start's pinned apps), Pin to Dock, Remove from Suggestions, Edit Application
  and Uninstall; the Menu key and Shift+F10 open it from the keyboard.
- Quick Menu ends in a rail, as Windows 11's Start does: the account's picture and name open Lock, Switch User and Log
  Out; the power button opens Sleep, Hibernate (where the machine can), Restart and Shut Down. Plasma's session
  manager does the work, with its own confirmation.
- Power and Battery stays visible in the tray, where the power profile is switched.
- The pointing hand on everything clickable; the black minimalist macOS cursor elsewhere, GTK apps included.
- The clock follows the region's 12- or 24-hour format.
- The README's pictures show Orion first under Developer Tools and in the Dock.

## File Explorer's search index

- 2026-11-11, Tony: search must be instant, never stale, and cost nothing while it is not used, even with a File
  Explorer window open all day. Neither a resident indexer (Everything's way, always running) nor an index built
  when the window opens (a full read of the disks every time) meets that. The helper starts on demand from its
  socket, catches up from btrfs transaction ids or folder times instead of reading everything, follows changes with
  fanotify only while File Explorer is in use, and leaves when idle ([search.md](search.md)).
- The index lives in the helper's memory while it runs, not in a mapped file: entries link to each other and change
  in place, which a read-only mapping cannot hold, and the memory goes back when the helper leaves.
- `plocate`'s nightly `updatedb` stays: other programs use `locate`, and turning it off is the user's choice.

## Fonts

- Aurora ships only free fonts: Inter for the interface and JetBrains Mono with Nerd Fonts' symbols for Konsole.
  Apple's SF Pro and SF Mono are free to download but not to redistribute: their license allows them only for mock-ups
  of interfaces for Apple's systems. When SF Pro or SF Mono is installed (the AUR's `apple-fonts`, for one), the
  install uses it instead; Inter is the closest free match, a little wider, with the same shapes and weights.

## Way of working

- Research before deciding; measure before tuning, and say what is inferred.
- No hardcoding of one case where a general rule exists.
- Everything stays instant under load.
- Validation is local; GitHub holds the repository and its releases.

## Naming and the repository

- The theme is called **Aurora**, written in plain words everywhere: no code names. Ids and paths say `aurora`
  (`org.aurora.decoration`, `org.aurora.glass`, `aurora-pointer`, `~/.local/share/aurora`).
- The repository is organized by category: `components/`, `cli/`, `themes/`, `installer/`, `packaging/`,
  `performance/`, `tests/`, `docs/`, `assets/`. Everything Aurora installs lives in this repository: no submodules and
  nothing downloaded from other projects. One command installs everything (`./install.sh`), one
  undoes it (`./uninstall.sh`), and the README lists every library and package it uses.
- Aurora is a macOS Tahoe-inspired theme, never presented as a copy.
- Pages and code read like a person wrote them: no AI-looking icons, badges, slogans or phrasing.
- Nothing personal goes into the repository: no saved desktop settings, no private screenshots, no keys.
- Every showcase picture (the README, the social preview) is taken on Dawn Bird, Aurora's own wallpaper in
  `themes/wallpapers/Aurora`, chosen among three original concepts. It was generated for Aurora with an image model,
  scaled to 5120x2880 with Real-ESRGAN (realesrgan-x4plus) and given a fine grain against banding. The sandbox
  scenarios set it through `tests/helpers/showcase_wallpaper.sh`. The install offers it in the wallpaper picker and
  leaves the wallpaper alone.

## Menu bar logo

- The menu at the left of the menu bar shows CachyOS's black-and-white logo by default, so it matches the rest of the
  bar. It is cscs's symbolic version of the official mark, which follows the bar's text color.
- The menu's settings offer Arch Linux, Apple and Kali Linux too, and the CachyOS and Arch Linux logos in their own
  colors; any other icon still works.

## Installing and releasing

- `./install.sh` stays the one implementation of the install. The `aurora` command (Rust, `cli/`) is a guided front
  end over it: it asks for the logo and the tuning, shows each step with a spinner, logs the output, and also updates,
  rebuilds, changes the logo, shows the status and uninstalls.
- Distribution: the package `aurora-theme`, built from the GitHub release archive and served from Aurora's own signed
  pacman repository in the GitHub release `repo` (`sudo pacman -Syu aurora-theme`, then `aurora`). The AUR closed new
  accounts in 2026 after waves of malicious packages, so a repository I host is the route that works today; the same
  PKGBUILD goes to the AUR when it reopens. `bunx` was considered and dropped: it needs an npm package and puts a JavaScript runtime in front of a KDE
  theme.
- Versions follow SemVer, the changelog Keep a Changelog 2.0.0, commits Conventional Commits; `scripts/release.sh`
  cuts a release and publishes only with `--publish`.
- Everything a package ships prebuilt targets the x86-64 baseline, never the processor of the PC that builds it: the
  package is built once and installed on PCs older than mine. The speed a newer instruction set would add to a theme
  is not worth a desktop that stops with "Illegal instruction" (issue #11). Libraries that pick a faster path after
  asking the processor (memchr's AVX2 search) keep doing so.

## License

- Aurora must always credit its author. Aurora's own code is GPL-3.0-only with additional terms under its section 7
  (NOTICE.md): (b) keep the author attribution in the source and in the Appropriate Legal Notices of every
  interactive interface, (c) mark modified versions as such, (d) no use of the author's name to promote them.
  Section 5(d) already makes every modified version keep showing those notices, and `aurora license` plus the banner
  are where Aurora shows them.
- GPL rather than Apache-2.0 or MIT: those let a modified version keep the credit only in a NOTICE or license file
  and close its source; the GPL keeps every version open and, with 7(b), keeps the name in the interface. The
  Attribution Assurance License forces a splash screen on every launch, which no GPL code may be combined with, and
  Aurora is built on KDE's GPL code. "Only" rather than "or later" keeps the additional terms tied to the version
  they were written for.
- Every file of Aurora's own code carries a three-line header; `scripts/license_headers.py` adds and checks it.
