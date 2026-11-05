[Aurora](../README.md) / [Documentation](index.md) / Components

# The components

One section per folder of [`components/`](../components), then the themes. Each says what the part does, where its
source is, what is installed and how it is tested. Build commands are in [build.md](build.md), test commands in
[../tests/index.md](../tests/index.md).

## File Explorer (`components/file-explorer`)

A Finder-style file manager in C++ and Qt Widgets on top of Dolphin's KParts view. A sidebar with Favorites and drives
(drives can be renamed inline, kept in `file-explorerrc [Drive Names]`; SF-style symbols; a capacity bar with a tooltip,
amber above 90 % and red above 95 %), tabs, a toolbar, a status line, Finder menus with shortcuts written the Windows way
(`fe::shortcutLabel`: "Ctrl+Alt+C", "Alt+Enter", "Win+E"), and the pointing hand on every clickable widget. It also
serves `org.freedesktop.FileManager1` (`--service`: "Show in folder" selects the file) and the FileChooser portal
backend (`--portal`: Open and Save panels with a chooser bar; it quits after three idle minutes). Installed as
`~/.local/bin/file-explorer`; Win+E launches it. Tests: `fe_tests` (67). Sandbox: `explorer.sh`, `explorer-menus.sh`,
`explorer-drives.sh`, `show-in-folder.sh`, `open-save.sh`.

## Window buttons (`components/window-buttons`)

The KDecoration3 plugin `org.aurora.decoration` ("Aurora" in System Settings): one glass capsule with
minimize, maximize and close in Windows order, the same size as the GTK and File Explorer buttons. `deco-preview`
renders it offscreen. Tests: `decoration_tests` (11). KWin loads it through a root link (it starts before the session's
plugin path is set); the login script pauses it when KDecoration changes.

## System tray (`components/system-tray`)

Plasma 6.8.2's tray applet with `system-tray.patch`: Windows 11 drag and drop (reorder, drop on the arrow to hide),
working clicks on app icons, SF-like status icons (from `themes/icons/status_glyphs.py`). The items behind the arrow
are set by `themes/panels/tray_hide.py` while plasmashell is stopped. Tests: `tests/tst_draglayering.qml` (5).
Sandbox: `tray-click.sh`, `tray-drag.sh`, `tray-show.sh`.

## Notifications (`components/notifications`)

Plasma 6.8.2's notifications applet with `notifications.patch`: macOS banners (356 px wide, a radius of about 22, the
Control Center's glass shader, the app icon leading, a bold title, "now" in the trailing corner, four lines of body, a
close button on hover over the top-left corner), in the bottom-right corner (`PopupPosition=BottomRight`), 8 px apart.
The window asks for a theme frame that does not exist (`aurora/no-frame`), so Plasma draws no frame of its own, and
an `inputRect` mask lets clicks through the shadow margin. The project needs `LANGUAGES C` for the generated Wayland
protocol code; `plasma-shell.xml` is kept in `vendor/protocols`. Sandbox: `notifications.sh`.

## Quick Menu (`components/quick-menu`)

KRunner from plasma-workspace 6.8.2 (`upstream/`, `vendor/`, `quick-menu.patch`) drawn as macOS Tahoe's Spotlight: one
frameless glass window floating a quarter of the way down the active screen. With nothing typed it shows the
Applications view: Suggestions (the apps used most, from Plasma's activity statistics through Kicker's
`RecentUsageModel`) and Plasma's menu in Tahoe's sections (`quickmenu.js`: categories keyed by their `.directory`
icon, merged, de-duplicated, sorted), one row of five per section until "Show More". The arrow keys walk the grid, the
pointer selects only when it really moves, Return launches. An app dragged onto another of its section takes that
app's place, and the section keeps that order (`~/.config/aurora/quick-menu.conf`; Suggestions stay ordered by
use). A right-click on an app, or the Menu key or Shift+F10 on the selected one, opens its menu (`QuickMenuPopup.qml`,
built by `quickmenu.js` `menuItems()`): Open, the app's jump list, Pin to Top (a Pinned section above Suggestions,
saved in `quick-menu.conf`), Pin to Dock (the Dock's launchers, changed through Plasma's `evaluateScript` with a script
that accepts only a desktop file id), Remove from Suggestions (Kicker's `forget`), then Plasma's Edit Application and
Uninstall. Plasma's own pin actions are left out: Kicker offers them only inside a panel widget. Typing searches
everything KRunner's runners find, listed
with their actions, drag-out and section headers; Up in the empty field recalls past searches. The ⋯ menu keeps Keep
Open, Usage Help and KRunner's settings. It starts through `krunner-launch.sh` and the `plasma-krunner` drop-in
([architecture.md](architecture.md)); the menu bar's search icon calls `org.kde.krunner /App toggleDisplay`. Tests:
`tests/tst_quickmenu.qml` (34), `tests/test_launch.sh` (5). Sandbox: `quick-menu.sh`, `quick-menu-drag.sh`,
`quick-menu-app-menu.sh`.

## The Windows key (`components/windows-key`)

`windows_key.py quick-menu|stock|status` moves `Qt::Key_Meta` (16777250) between plasmashell's "Activate Application
Launcher" (which keeps Alt+F1) and KRunner's `_launch` over kglobalaccel's D-Bus API, leaving every other key alone. Win
alone opens Quick Menu, the next press closes it, Win+key combinations are untouched. Sandbox: `windows-key.sh` (the
first check reproduces the old bug, then four PASS).

## Control Center (`components/control-center`)

The widget `org.aurora.controlcenter`, Tahoe's floating glass modules: Wi-Fi or Ethernet, Bluetooth, Focus, sound with a device panel, the player,
Night Light, screen mirroring, settings and screenshots. The blur comes from `org.aurora.glass`. Tests: `tests/tst_cc.qml` (18). Sandbox: `control-center*.sh`.

## Glass and pointer hints (`components/glass`)

The C++ QML module `org.aurora.glass`. `BlurBehind` asks KWin to blur behind rounded items marked
`glassBlur: true`; `PointerHints` gives plasmashell the pointing hand over clickable items (an application event filter
with a top-down item search like Qt's own delivery, loaded once by the Control Center). Quick Menu and the banners use
the blur too. Tests: `tests/tst_blurbehind.qml` (9), `tests/tst_pointerhints.qml` (13). Sandbox: `pointing-hand.sh`,
`pointing-hand-cost.sh`.

## Pointing hand over window buttons (`components/pointing-hand`)

The KWin plugin `aurora-pointer` shows the hand over buttons that KWin draws (an `InputEventSpy` and
`Cursor::setSource`, no KWin patch). KWin loads it through a root link and refuses it after a KWin version change until
`rebuild.sh` runs. Sandbox: `pointing-hand-kwin.sh`.

## Logo menu (`components/kali-menu`)

The widget `org.aurora.logomenu`: the menu behind the menu bar's logo (CachyOS, Arch Linux, Apple or the Kali dragon),
with shortcut labels written the Windows way. The labels
are the menu's own accelerators while it is open ("Ctrl+Alt+Q" locks, "Alt+Shift+Q" logs out). Tests:
`tests/tst_menu.qml` (31 pass; `test_mouse_drag_reorders` is a known failure). Sandbox:
`kali-menu.sh`.

## Clock (`components/clock`) and Dock divider (`components/dock-divider`)

`org.aurora.clock` writes the date and time the way macOS does ("Wed Nov 4  6:24 PM", in the system's language)
with a calendar popup; tests: `tests/tst_clock.qml` (9). `org.aurora.dockdivider` is the thin line between the apps
and the stacks in the Dock.

## Info Center energy page (`components/energy-page`)

A KCM with the stock id `kcm_energyinfo`: live CPU package watts (RAPL), GPU watts and temperature (NVML through dlopen),
a two-minute chart and the power profile buttons (power-profiles-daemon over D-Bus); it samples only while visible.
Tests: `test_power` (9). Sandbox: `energy-page.sh`.

## Menu bar and Dock layout (`themes/panels`)

`layout.js` is a Plasma desktop script: the menu bar (menu bar style, logo menu, app name, spacer, tray, Quick Menu,
Control Center, clock) and the Dock (task manager with the pinned apps carried over, divider, Downloads, Trash).
`render_layout.sh` fills its placeholders: the home folder and the three gaps that keep the right-hand icons 46 px
apart, as the tray's own icons are. `patch_tray.py` copies the tray settings from the backup; `tray_hide.py` puts
Brightness, Clipboard, Disks and Bluetooth behind the arrow. Sandbox: `setup-panels.sh`, `menu-bar.sh`,
`dock-keeps-pins.sh`, `dock.sh`, `dock-edge.sh`.

## Menu bar style and app name (`components/menu-bar-style`, `components/app-title`)

`org.aurora.menubarstyle` gives the menu bar its light text, soft shadow and spacing; it stays hidden in the bar.
`org.aurora.apptitle` names the active app in bold next to the logo, and takes no room when no window has the focus.
Tests: `components/app-title/tests/tst_apptitle.qml`. Sandbox: `app-title.sh`.

## Themes, icons, fonts (`themes/`)

- Dawn Bird, Aurora's own wallpaper, a Plasma wallpaper package in `themes/wallpapers/Aurora` (5120x2880 and
  3840x2160, CC BY-SA 4.0); the README's pictures and the sandbox's showcase captures are taken on it.
- Aurora's Plasma style (`themes/plasma/Aurora`; its top panel stays see-through), color scheme
  (`themes/colors/Aurora.colors`) and Kvantum theme (`themes/kvantum/Aurora`); the GTK theme in
  `themes/gtk/Aurora.tar.xz`, adjusted by `gtk_theme.py` (the window-button capsule for GTK 3, GTK 4 and
  libadwaita).
- Icons: the base themes `Aurora-base` and `Aurora-base-dark`, kept built in `themes/icons/base/Aurora-base.tar.xz`
  (hidden in the icon picker), and `Aurora-dark` built at install by `themes/icons/build_icon_theme.py` (each app's
  own logo ahead of the base's redraws), `status_glyphs.py` (SF-like
  status icons) and the theme's Settings, Downloads and Trash icons in `themes/icons/system` (drawn by
  `draw_system_icons.py`).
- Cursors: `themes/cursors/Aurora`, with four aliases Plasma asks for.
- Fonts: none are shipped. The interface uses Inter (`inter-font`) and Konsole JetBrains Mono with Nerd Fonts' symbols
  (`ttf-jetbrains-mono-nerd`); `install.py` picks SF Pro Text and SF Mono with Nerd symbols instead when the user
  installed them (`ui_font()`, `mono_font()`), since Apple's license forbids redistributing them.
- Konsole: `themes/konsole` (the "Aurora" profile and color scheme, toolbars hidden).

## Measurement kit (`performance/measure`)

`probe.py` (time from a shortcut to its window, plus scheduler wait), `under-load.sh`, `sched/` (scheduler rounds,
`rtlat.c`, results) and `power/` (`ab.sh`, `burst.c`: Balanced against Performance). Results: [performance.md](performance.md).
