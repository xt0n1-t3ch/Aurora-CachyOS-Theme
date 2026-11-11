[Aurora](../README.md) / [Documentation](index.md) / Components

# The components

One section per folder of [`components/`](../components), then the themes. Each says what the part does, where its
source is, what is installed and how it is tested. Build commands are in [build.md](build.md), test commands in
[../tests/index.md](../tests/index.md).

## File Explorer (`components/file-explorer`)

A Finder-style file manager in C++ and Qt Widgets on top of Dolphin's KParts view. A sidebar with Favorites, drives
and color Tags (drives can be renamed inline, kept in `file-explorerrc [Drive Names]`; SF-style symbols; a capacity bar
with a tooltip, amber above 90 % and red above 95 %; a folder the user added unfolds into its subfolders and each
section folds away, kept in `file-explorerrc [Sidebar Tree]`), tabs, a toolbar with an address field
(`fe::AddressField`) that shows the path as clickable segments and takes a typed path on a click beside them, Ctrl+L or
Alt+D (rules in `core/address`: `~`, relative paths, suggestions, shell Tab) and the Group, Share
(Purpose), Edit Tags and More menus, a row of file actions (`fe::ActionBar`), Quick Look (`fe::QuickLook`, Space;
rules in `core/quicklook`), Split View (`fe::TabPage`, F3), a path bar that names the selected file and shows the item
count, Undo (Ctrl+Z) from KIO's undo history, which Dolphin's view records its operations in, a filter row
(`fe::FilterBar`, Ctrl+Alt+F or More > Filter by Name) that keeps the items whose names contain what is typed, case
ignored, with the count of matches, and ends when the tab shows another folder or search, a Rename sheet for several
items (`fe::BatchRenameSheet`, F2 with more than one item selected; rules in `core/batchrename`: replace text or a
regular expression, add text, a name with an index, a counter or each item's date, a change of case; extensions kept
the way KIO keeps them) that shows every new name, marks the ones that cannot be taken and why, plans again against
the folders when Rename is pressed, and runs one KIO batch job that one Undo takes back, path segments in both bars (`fe::CrumbStrip`, rules in `core/crumbs`; below), Finder menus with shortcuts written the Windows way (`fe::shortcutLabel`: "Ctrl+Alt+C", "Alt+Enter", "Win+E"),
and the pointing hand on every clickable widget.

Group offers the Finder's choices (None, Name, Kind, Date Modified, Date Created, Size, Tags) through Dolphin's own
actions: `group_by_none` and `group_<role>` since Dolphin 26.08, which groups apart from the sort order and remembers
the choice per folder, and `show_in_groups` with the sort column before it. The toolbar's fields draw one capsule
focus ring (`style::paintFocusRing`); `FinderStyle` drops the `QFocusFrame` Breeze adds around a field focused by Tab
or a shortcut. The window is glass with KWin's blur while it is active and opaque while it is not, as macOS materials
are when their window is not key, so nothing behind shows through it whether KWin blurs or not; selections turn gray
with it.

The address field and the path bar show the path as segments (`fe::CrumbStrip`, which each bar feeds its events;
the layout, the hit test and the drop rules are pure functions in `core/crumbs`, tested). The address field shows them
at rest without icons, as Windows' address bar does, and opens for typing on a click beside them, Ctrl+L or Alt+D (its
focus policy is Tab, so a click on a segment opens the folder instead of the field). A click opens a segment and a
middle click or Ctrl+click opens it in a background tab; the chevron after a segment lists its visible folders (250 at
most, the next one of the path in bold, then Show All); a path too long for its bar folds the segments after the
first into an ellipsis, outermost first, which lists them, and the folder on screen keeps the rest of the room; a
right-click offers Open, Open in New Tab, Open Terminal Here, Copy Path and Get Info. Files dropped on a segment go
through `KIO::drop`, as on the sidebar, so KIO asks Move, Copy or Link; a tag's shelf tags them instead. Refused: a
folder into itself or a folder inside it, files into the folder they are in, Recents, a search, Network, the shelf
of all tags, a folder that is not writable, and the selected file the path bar can end with. A drag resting 700 ms on
a segment opens it (Finder's spring-loaded folders), except the folder on screen. A segment dragged out carries its
URL. The tooltip of a segment is its whole path. Screen readers see the path bar as a group of links, one per segment,
which they can press (`crumbaccessible.cpp`); the address field is a line edit whose text is the whole path. KIO opens
its Move, Copy or Link menu at the drop's position mapped from the widget given to `KJobWidgets::setWindow`
(`src/widgets/dropjob.cpp`, v6.31.0, line 671), so a drop names the widget whose coordinates the drop has: the bar,
or the sidebar's viewport.

Search answers from aurora-search's index of every local disk when it runs ([search.md](search.md)): a KIO worker
(`src/plugins/searchworker.cpp`, `aurora-search:` URLs) lists the helper's matches into Dolphin's view, so they have
icons, columns, drag and drop and menus; `fe::SearchService` keeps one connection while a window is in use, which
keeps the helper and its live updates running, and lets go after three idle minutes; a scope row under the toolbar
(`fe::SearchScopeBar`) chooses This PC or the folder, kept in `file-explorerrc [Search] Everywhere`; hidden names are
matched only while the view shows hidden files. A folder the index does not hold (a network share, a phone) is
searched in place with Dolphin's filenamesearch, as before.

Color tags (`tagging.cpp`, rules in `core/tags`) live in the file's `user.xdg.tags` attribute through KFileMetaData, so
Dolphin and Baloo read them too. Each tag's sidebar row opens its shelf, `~/.local/share/file-explorer/tags/<Tag>`, a
folder of links to the tagged files that File Explorer keeps in step and tidies as it opens, so tags work without
Baloo's index. The `aurora-tags` KIO overlay plugin (`src/plugins`, installed in
`~/.local/lib/qt6/plugins/kf6/overlayicon`) draws a tagged file's colors as dots on its icon, in File Explorer and in
Dolphin, with the 63 emblems `themes/icons/tag_emblems.py` adds to the icon theme.

It also serves `org.freedesktop.FileManager1` (`--service`: "Show in folder" selects the file) and the FileChooser
portal backend (`--portal`: Open and Save panels with a chooser bar; it quits after three idle minutes). Installed as
`~/.local/bin/file-explorer`; Win+E launches it. Folders without a view of their own show Finder's columns (Name,
Modified, Size, Type) from `~/.local/share/file-explorer/view_properties/global/.directory`. Tests: `fe_tests` (79),
`fe_finder_tests` (54), `fe_widget_tests` (17, offscreen: clicks, drops, a drag resting on a segment, the address
field at rest, accessibility, the filter row over a real Dolphin view, the glass behind a field inside a tab, the
Rename sheet renaming a real folder and one Undo taking it back, F2 on two items opening it), `tests/installer/test_tag_emblems.sh` (5). Sandbox: `explorer.sh`, `explorer-crumbs.sh` (the path
segments: hover, both menus, a drop in the accent, a segment opening under a drag, KIO's menu at the pointer on a
segment and on the sidebar, a refused drop, Ctrl+L, Ctrl+click and middle click), `explorer-finder.sh` (the
mockup's scene: tags, the unfolded sidebar, the menu, a typed path, Quick Look, Split View, a tag's shelf, the Group
menu and the folder grouped by Kind from it, and the window inactive behind another),
`explorer-menus.sh`, `explorer-drives.sh`, `explorer-search.sh` (search from the index: whole home, scope row, hidden
names), `explorer-undo.sh` (Delete, then Ctrl+Z brings the file back; FAIL on the code before Undo), `explorer-filter.sh`
(Ctrl+Alt+F, a word hides the rest, no match empties the view, Escape brings everything back), `explorer-rename.sh`
(Ctrl+A, F2, find and replace, Enter, then Ctrl+Z; 3 PASS), `show-in-folder.sh`, `open-save.sh`.

## Mission Center (`components/mission-center`)

Mission Center, the system monitor, built from upstream main with Aurora's patches: each app's GPU and memory counted
exactly (memory as PSS; the sum came within 0.4 % of the real total on my own machine), magpie kept lean (3 % of one
core with the window open and 0.6 % hidden, against 13 % for Mission Center 1.2.0), fans read the way the hardware
reports them, each app's network use from nethogs, a color per device in the sidebar, and smoother graphs. The folder
holds the package's recipe: the `PKGBUILD` (package `mission-center-improved`, in place of the distribution's
`mission-center`), one patch per upstream repository, a pacman hook that gives nethogs the two capabilities it needs
to count traffic without root, and an install script that takes them away with the package. `make_patches.sh` writes
the patches from the four upstream clones the work is done in, which stay next to it and out of git.

The look matches File Explorer's glass (`resources/ui/aurora.css` for the window, `aurora-header.css` for the header
bars, `aurora-pages.css` for the pages and `aurora-popups.css` for what opens over them, in light and dark style
alike, as Aurora's GTK theme is dark in both),
measured over the same wallpaper as File Explorer in the sandbox. `src/glass.rs` paints File Explorer's tint,
rgba(24, 24, 27, 0.63), over the window body but for the sidebar's card, which stays the clearer glass on its own
(`paint_tint`, as File Explorer's `glassCutout`, drawn as the card's shadow with no blur so no path is rasterized);
GTK's own window background stays clear, since the GTK theme fills it at 96 %. KWin blurs behind it, and the window
turns the blur off while it is not in front and opaque, as File Explorer does: `glass.rs` asks through
ext-background-effect-v1 itself, once per size or state (KWin 6.8 removed Plasma's `org_kde_kwin_blur`, which it
still uses where the new protocol is missing). A surface may hold one effect object and GTK 4.24 makes one for every
surface, so `main` keeps GDK off the protocol with `GDK_WAYLAND_DISABLE` (tested). GTK's own way, CSS
`backdrop-filter`, built copy, paste and blur nodes into every frame and walked the whole frame for the region: with
animated graphs the window used 66 % of a core instead of 20 % ([performance.md](performance.md)). KWin 6.8 places
the region from the window's edge rather than the surface's, so GTK's client-side shadow put the blur 68 by 50 px
off; the window is
undecorated instead, KWin draws File Explorer's shadow (`src/shadow.rs`, `org_kde_kwin_shadow`, tests on the image and
its tiles) and the window resizes from a 6 px band inside its edges (`src/edges.rs`, tested). `glass.rs` loads the
four sheets one step above the user's sheet (`~/.config/gtk-4.0/gtk.css`): a desktop theme loaded there outranks every
app's own sheet, and its generic row and button rules turned the selected row grey and the selected device solid
blue. The header row is File Explorer's
toolbar, measured on both apps' captures: 34 px capsules 15 px below the window's top, the window buttons in File
Explorer's 120 by 34 px capsule 22 px in from the right edge with the same glyphs, round buttons of 34 px, 18 px
symbols, File Explorer's fills and rim, and a search field of 230 px that narrows to 150 px when the header is short of
room (below 1220 scaled pixels, or while the sidebar is edited) so the page switcher keeps its labels. The sidebar's
header is File Explorer's too: its sidebar button (File Explorer's symbol) and the Edit button at the top left, the
edit tools at the other end while editing; with the sidebar hidden, the header bar's sidebar button takes the same
place, and a window in the background keeps its header as it is but for the window buttons' glyphs. The device sidebar
is the clearer glass, 6 px from the window's edges; each device's pill keeps 8 px from the card's sides and its graph
10 px from the pill's edge, as File Explorer's rows do. The tables and the Performance graphs start 25 px from the
sidebar and end 16 px above the window's bottom edge and, like the details column, 22 px from its right edge, under
the window buttons. The sidebar stays on every page; on Apps and Services it shows from 1220
scaled pixels, where every column still fits (measured: the Apps table needs 871 px with a process tree ten levels
deep, the sidebar and margins 328).
Apps and Services put their table on a glass card with the column headings inside; section headings (Apps,
Processes) never show as selected; Stop, Force Stop, Details, Start and Restart are separate glass pills. A heading
keeps no room for the sort arrow until its column is sorted, reading columns are 64 px at least (75 upstream) and each
tree level steps in 16 px (22 in the theme), which together let the table fit beside the sidebar. Busy cells get a
rounded heat tint (`src/table_view/columns/heat.rs`, five steps per unit, accent blue for CPU and GPU shares and amber
for memory and throughput, tested; setting `apps-page-heat-tint`). On Performance, `src/performance_page/tiles.rs`
turns each device's readings into glass tiles (two to a row where the layout allows) with the facts on one card below,
lined up with the top of the graphs; it reads the shape of each device's layout file, so every device gets the same
look (tests on what counts as a reading, the facts' span and the pairing). A network's addresses are facts on the same
card. A drive's partitions sit on a card of their own under the facts and as wide as theirs, under File Explorer's
section title (11 px, semibold, secondary); each mount point shows a folder symbol, and each usage bar is File
Explorer's capacity bar, blue, amber past 90 % used and red past 95 % (its thresholds, tested). The details column
takes about a third of the page and is never narrower than its tiles; in a narrow window the graphs give way instead.
The search field is always in the header bar. Graph corners stay at the stock 7 px: at 10 and 12 GTK 4.24's
GPU renderer logged clip errors.
Menus are File Explorer's (`aurora-popups.css`, from its `finderstyle.cpp`): glass 11 px round and at least 180 px
wide, 24 px rows with the text 14 px in, an accent pill under the pointer or the keyboard but none on the first row
when a menu opens with the mouse, an 18 px column with a check mark on the chosen choice, secondary shortcuts and
chevrons, and hairlines between sections. Submenus open beside their menu (`flags: nested`) instead of sliding in, and
no menu has a tail. Tooltips are the same glass, 8 px round. KWin blurs behind each of them and draws their shadow:
`src/popups.rs` gives every popover and tooltip, as it maps, its own blur, fitted to its size as GTK lays it out, and
File Explorer's menu shadow, those GTK makes on its own included (submenus, the search field's menu), through an
emission hook on GtkWidget's `map`. Over a checkerboard in the sandbox, a shadow drawn by GTK widened the menu's surface and KWin put the blur off
by the shadow's reach, as it did for the window. Dialogs are opaque in File Explorer's dialog color, rgb(30, 30, 32),
with the window's rim, and their grouped rows sit on one card; Preferences is 820 px wide so its four page names fit
(at 640 the switcher, which gives every page the room of the longest name, Performance Page, cut them short).
The sandbox (`tools/sandbox`) captures over Aurora's wallpaper, or another picture with `MC_BACKDROP`, with the host's
icons, fonts, icon theme, font setting and whole `~/.config/gtk-4.0`, at any width (`MC_WIDTH`) and style
(`MC_STYLE`), and can click a spot first (`click+X+Y`), leave the pointer over one (`hover+X+Y`) or play any input
(`input+WORDS`, fakeinput's words, such as a right-click and a move into the menu it opens).

`aurora mission-center on` installs it from Aurora's repository when pacman finds it there, otherwise it builds it
here with makepkg (about ten minutes; the build tools come and go with `--syncdeps --rmdeps`); `off` puts the
distribution's back. Every change first saves a snapshot in `~/.local/state/aurora/mission-center/snapshots/<time>`
(which build was installed, and `dconf dump` of the settings), and every build Aurora installs is kept in
`~/.local/state/aurora/mission-center/packages`, so `rollback [NAME]` puts both back; `snapshots` lists them. pacman's
own snapshots (snap-pac) cover the system side. `settings` offers the refresh, the points on each chart, the look,
smooth and sliding graphs, one row per app, kernel time, fans and batteries, and resets the sidebar's order and the
device colors; `--set KEY=VALUE` and `--reset KEY` reach every key of `io.missioncenter.MissionCenter`, each value
checked against `gsettings range` before anything changes. Tests: the CLI's `mission` tests and the contract test on
the recipe ([../tests/index.md](../tests/index.md)).

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
close button on hover over the top-left corner, the button shared with the popover in `components/BannerButton.qml`),
in the bottom-right corner (`PopupPosition=BottomRight`), 8 px apart. The popover (`FullRepresentation.qml`) keeps
Plasma's history and every control, drawn as macOS Tahoe's Notification Center: each notification is a glass platter
inset from the edges with the banner's layout (`DelegatePopup`, dates past 22 hours), a long body folded to four
lines that a click or Show More opens, Clear on hover over its top-right corner (the time fades under it), and the
app's settings, Minimize and Close in a right-click or Menu-key menu; a group is its app's icon and name above its
platters; Do not disturb is a row with the switch trailing; the empty
state is a bell with "No Notifications". SwipeDelegate moves its background with the swipe and ignores insets, so the
platter is inset inside a full-width background item.
The window asks for a theme frame that does not exist (`aurora/no-frame`), so Plasma draws no frame of its own, and
an `inputRect` mask lets clicks through the shadow margin. The project needs `LANGUAGES C` for the generated Wayland
protocol code; `plasma-shell.xml` is kept in `vendor/protocols`. Sandbox: `notifications.sh`,
`notifications-history.sh` (the popover with notifications, on hover, empty with `SCEN_TAG=empty`, a group with
`SCEN_TAG=group`; Plasma keeps an expired notification only when it names its desktop entry).

## Quick Menu (`components/quick-menu`)

KRunner from plasma-workspace 6.8.2 (`upstream/`, `vendor/`, `quick-menu.patch`) drawn as macOS Tahoe's Spotlight: one
frameless glass window floating a quarter of the way down the active screen. With nothing typed it shows the
Applications view: Suggestions (the apps used most, from Plasma's activity statistics through Kicker's
`RecentUsageModel`) and Plasma's menu in Tahoe's sections (`quickmenu.js`: categories keyed by their `.directory`
icon, merged, de-duplicated, sorted), one row of five per section until "Show More". The grid button beside ⋯ in the search
row opens All Applications: every app once under its first letter, accents filed under the plain letter and digits and signs under "#" first
(`alphabeticalSections()`, like Windows' All apps), under a heading drawn like a section's with Back where a
section has Show More; the button, filled while the list shows, also goes back. The arrow keys walk the grid, the
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
`tests/tst_quickmenu.qml` (43), `tests/test_launch.sh` (5). Sandbox: `quick-menu.sh`, `quick-menu-all.sh`, `quick-menu-drag.sh`,
`quick-menu-app-menu.sh`.

## The Windows key (`components/windows-key`)

`windows_key.py quick-menu|stock|status` moves `Qt::Key_Meta` (16777250) between plasmashell's "Activate Application
Launcher" (which keeps Alt+F1) and KRunner's `_launch` over kglobalaccel's D-Bus API, leaving every other key alone. Win
alone opens Quick Menu, the next press closes it, Win+key combinations are untouched. Sandbox: `windows-key.sh` (the
first check reproduces the old bug, then four PASS).

## Control Center (`components/control-center`)

The widget `org.aurora.controlcenter`, Tahoe's floating glass modules: Wi-Fi or Ethernet, Bluetooth, Focus, sound with a device panel, the player,
Night Light, screen mirroring, settings and screenshots. The blur comes from `org.aurora.glass`. Tests: `tests/tst_cc.qml` (20). Sandbox: `control-center*.sh`.

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
Control Center, clock; translucent, because an adaptive bar turns opaque over a maximized window and its popups with
it) and the Dock (task manager with the pinned apps carried over, divider, Downloads, Trash).
`render_layout.sh` fills its placeholders: the home folder and the three gaps that keep the right-hand icons 46 px
apart, as the tray's own icons are. A placeholder is a name in capitals between two `@`; the installer stops when
one is left, and any other `@` is the script's own text. `patch_tray.py` copies the tray settings from the backup; `tray_hide.py` puts
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
  libadwaita, with File Explorer's glyphs written half a pixel off the icon's middle, so each 1.4 px stroke falls on
  one row of pixels as File Explorer and the decoration draw it; the GTK 4 sheet's copy of libadwaita's list header
  padding is narrowed to lists, because the user sheet loads above every app's own and would otherwise pad table
  headings that an app sets itself).
- Icons: the base themes `Aurora-base` and `Aurora-base-dark`, kept built in `themes/icons/base/Aurora-base.tar.xz`
  (hidden in the icon picker), and `Aurora-dark` built at install by `themes/icons/build_icon_theme.py` (each app's
  own logo ahead of the base's redraws), `status_glyphs.py` (SF-like
  status icons, written as filled outlines because GTK 4 fills every shape of a symbolic icon and drops strokes) and the theme's Settings, Downloads and Trash icons in `themes/icons/system` (drawn by
  `draw_system_icons.py`).
- Cursors: `themes/cursors/Aurora`, with four aliases Plasma asks for.
- Fonts: none are shipped. The interface uses Inter (`inter-font`) and Konsole JetBrains Mono with Nerd Fonts' symbols
  (`ttf-jetbrains-mono-nerd`); `install.py` picks SF Pro Text and SF Mono with Nerd symbols instead when the user
  installed them (`ui_font()`, `mono_font()`), since Apple's license forbids redistributing them.
- Konsole: `themes/konsole` (the "Aurora" profile and color scheme, toolbars hidden).

## Measurement kit (`performance/measure`)

`probe.py` (time from a shortcut to its window, plus scheduler wait), `under-load.sh`, `sched/` (scheduler rounds,
`rtlat.c`, results) and `power/` (`ab.sh`, `burst.c`: Balanced against Performance). Results: [performance.md](performance.md).
