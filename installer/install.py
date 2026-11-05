#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Installs Aurora into the home folder, from this checkout after installer/build.sh.

install.sh runs the steps in this order; each one can run alone. Nothing outside the home folder changes
(install.sh does the root steps).

    install.py backup       save every setting the install changes and write restore.sh beside the copies (kept
                            from the first install: it holds the desktop as it was before the theme)
    install.py files        install the themes, fonts, icons, cursors, widgets, plugins, File Explorer and Quick Menu
    install.py components   install only the built parts and what they were built for (install.sh --rebuild)
    install.py look         apply colors, Plasma style, icons, cursor, fonts, Kvantum, GTK, Konsole, KWin and keys
    install.py panels       put up the menu bar and the Dock, unless they are there already (--force rebuilds them)
    install.py check        list what the steps need and cannot find; changes nothing

--no-session writes the files without telling the running Plasma session (the tests use it with a temporary HOME).
"""
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.environ.get("BUILD_DIR", os.path.join(ROOT, "build"))
HOME = os.path.expanduser("~")
SHARE = os.path.join(HOME, ".local/share")
STATE = os.path.join(SHARE, "aurora")                # stamps, rebuild.sh and the link to this checkout
BACKUPS = os.path.join(HOME, ".local/state/aurora")  # the backups outlive an uninstall
LATEST = os.path.join(BACKUPS, "latest-backup")
LIB = os.path.join(HOME, ".local/lib/aurora")
PLUGINS = ".local/lib/qt6/plugins"
APPLETSRC = ".config/plasma-org.kde.plasma.desktop-appletsrc"
SESSION_FILES = os.path.join(ROOT, "installer/session")
THEMES = os.path.join(ROOT, "themes")

# The built parts: the file each one leaves in build/<part>, and where it goes under the home folder.
BUILT = [
    ("file-explorer", "file-explorer", ".local/bin/file-explorer"),
    ("window-buttons", "org.aurora.decoration.so", f"{PLUGINS}/org.kde.kdecoration3/org.aurora.decoration.so"),
    ("pointing-hand", "aurora-pointer.so", f"{PLUGINS}/kwin/plugins/aurora-pointer.so"),
    ("energy-page", "bin/plasma/kcms/kinfocenter/kcm_energyinfo.so", f"{PLUGINS}/plasma/kcms/kinfocenter/kcm_energyinfo.so"),
    ("system-tray", "bin/plasma/applets/org.kde.plasma.systemtray.so", f"{PLUGINS}/plasma/applets/org.kde.plasma.systemtray.so"),
    ("notifications", "bin/plasma/applets/org.kde.plasma.notifications.so",
     f"{PLUGINS}/plasma/applets/org.kde.plasma.notifications.so"),
    ("quick-menu", "bin/krunner", ".local/lib/aurora/quick-menu/krunner"),
]
# The glass QML module (blur behind the Control Center, Quick Menu and the banners; the pointing hand in the shell).
GLASS = ("glass", "qml/org/aurora/glass", ".local/lib/qt6/qml/org/aurora/glass")
# The aurora command, built when Rust is installed (build.sh's cli part). It is not tied to a Plasma version.
CLI = ("cli/aurora", ".local/bin/aurora")
# The Plasma widgets installed for the user.
PLASMOIDS = [os.path.join(ROOT, "components", p, "package") for p in ("kali-menu", "control-center", "clock", "dock-divider", "app-title", "menu-bar-style")]
# The base of Aurora's icons: two hidden themes, kept built in themes/icons/base, that Aurora-dark inherits.
BASE_ICONS = ["Aurora-base", "Aurora-base-dark"]
# Aurora's own wallpapers (themes/wallpapers), offered in the wallpaper picker; the install does not
# change the wallpaper.
WALLPAPERS = ["Aurora"]
# Aurora's color scheme (themes/colors) and Plasma style (themes/plasma), both drawn by their own scripts.
COLORS = "Aurora"
STYLE = "Aurora"
# The Qt apps' widget style (themes/kvantum), drawn by Kvantum.
KVANTUM = "Aurora"
# GTK apps' theme (themes/gtk/Aurora.tar.xz), with Aurora's window buttons added by gtk_theme.py.
GTK = "Aurora"
ICON_THEME = "Aurora-dark"  # the base icons with every app's own logo and SF-like status icons
CURSORS = "Aurora"                    # themes/cursors: the pointer, with four aliases Plasma asks for
# Konsole's files: each one in themes/konsole, and the folder under the home folder it goes to.
KONSOLE = {"Aurora.profile": ".local/share/konsole", "Aurora.colorscheme": ".local/share/konsole",
           "konsoleui.rc": ".local/share/kxmlgui5/konsole", "sessionui.rc": ".local/share/kxmlgui5/konsole"}
# The fonts: Inter and JetBrains Mono (free, from the inter-font and ttf-jetbrains-mono-nerd packages), or Apple's SF
# Pro and SF Mono when the user installed them. Aurora ships no Apple font: their license allows no redistribution.
UI_FONTS = ("SF Pro Text", "Inter")
MONO_FONTS = ("SFMono Nerd Font", "JetBrainsMono Nerd Font")
# Widgets the Control Center covers go behind the tray's arrow, as macOS keeps them in Control Center. Power and
# Battery stays in the bar: it switches the power profile.
TRAY_HIDDEN = ["org.kde.plasma.brightness", "org.kde.plasma.clipboard", "org.kde.plasma.devicenotifier", "org.kde.plasma.bluetooth"]
# The D-Bus names File Explorer answers, each with the option that starts it for that name and the comment written
# into its service file.
DBUS_SERVICES = {"org.freedesktop.FileManager1": ("--service", '"Show in folder" from browsers, Steam and Plasma opens File Explorer.'),
                 "org.freedesktop.impl.portal.desktop.aurora": ("--portal", "Open and Save panels drawn by File Explorer.")}
# systemd drop-ins: the file in installer/session, and where each copy goes.
DROP_INS = {"plasma-krunner.conf": [".config/systemd/user/plasma-krunner.service.d/aurora.conf"],
            "plasmashell.conf": [".config/systemd/user/plasma-plasmashell.service.d/aurora.conf"],
            # The compositor and the shell never go to swap: switching apps faulted swapped KWin pages back in.
            "noswap.conf": [".config/systemd/user/plasma-kwin_wayland.service.d/90-aurora-noswap.conf",
                            ".config/systemd/user/plasma-plasmashell.service.d/90-aurora-noswap.conf"]}


def plasmoid_id(package):
    """The plasmoid id from the package's metadata.json.

    A package that is not there (an incomplete copy) gives its folder's name, so the module still loads
    and the check step can list what is missing.
    """
    try:
        with open(os.path.join(package, "metadata.json")) as f:
            return json.load(f)["KPlugin"]["Id"]
    except FileNotFoundError:
        return os.path.basename(os.path.dirname(package))


def own_app_icons():
    """The file names of the app logos in themes/icons/apps, sorted; none when the folder is not there."""
    folder = os.path.join(THEMES, "icons/apps")
    return sorted(os.listdir(folder)) if os.path.isdir(folder) else []


# Settings the install rewrites: saved first, put back by restore.sh.
SAVED = [".config/kdeglobals", ".config/kwinrc", ".config/plasmarc", ".config/kcminputrc", ".config/dolphinrc",
         ".config/konsolerc", APPLETSRC, ".config/plasmashellrc", ".config/kglobalshortcutsrc", ".config/mimeapps.list",
         ".config/plasmanotifyrc", ".config/xdg-desktop-portal/kde-portals.conf",
         ".config/Kvantum/kvantum.kvconfig", ".config/gtk-3.0", ".config/gtk-4.0", ".config/xsettingsd", ".config/gtkrc",
         ".config/gtkrc-2.0", ".gtkrc-2.0", ".local/state/konsolestaterc"]
# What the install adds: restore.sh removes each one that did not exist before.
ADDED = [*[f".local/share/icons/{t}" for t in (*BASE_ICONS, ICON_THEME, CURSORS)],
         f".icons/{CURSORS}", *[f".local/share/icons/hicolor/scalable/apps/{i}" for i in own_app_icons()],
         f".local/share/color-schemes/{COLORS}.colors", f".local/share/plasma/desktoptheme/{STYLE}",
         f".config/Kvantum/{KVANTUM}", f".local/share/themes/{GTK}",
         *[f".local/share/wallpapers/{w}" for w in WALLPAPERS],
         *[f".local/share/plasma/plasmoids/{plasmoid_id(p)}" for p in PLASMOIDS],
         *[target for _, _, target in BUILT], GLASS[2], CLI[1], ".local/lib/aurora", ".local/share/aurora",
         ".local/share/applications/file-explorer.desktop", ".local/share/applications/quick-menu.desktop",
         *[os.path.join(d, name) for name, d in KONSOLE.items()],
         ".config/plasma-workspace/env/aurora.sh", *[t for targets in DROP_INS.values() for t in targets],
         *[f".local/share/dbus-1/services/{name}.service" for name in DBUS_SERVICES],
         ".local/share/xdg-desktop-portal/portals/aurora.portal", ".config/environment.d/90-aurora-portal.conf",
         ".config/aurora"]  # the order the apps were dragged into in Quick Menu

RESTORE = r"""#!/bin/bash
# Puts the desktop back the way it was before Aurora, from the copies saved next to this script.
set -u
# The backup folder, which holds this script.
B="$(dirname "$(readlink -f "$0")")"
# Succeeds only for a relative path without "..", so a restore never touches anything outside the home folder.
safe() { case "$1" in ""|/*|*..*) return 1 ;; esac; }
# Runs a command as root: with the graphical password prompt when SUDO_ASKPASS is set, in the terminal otherwise.
as_root() { if [ -n "${SUDO_ASKPASS:-}" ]; then sudo -A "$@"; else sudo "$@"; fi; }
echo "Stopping plasmashell"
systemctl --user stop plasma-plasmashell.service
echo "Removing what the theme added"
while IFS= read -r path; do safe "$path" && rm -rf "${HOME:?}/$path"; done < "$B/created.txt"
echo "Putting the saved settings back"
while IFS= read -r path; do
    safe "$path" || continue
    rm -rf "${HOME:?}/$path"
    mkdir -p "$(dirname "$HOME/$path")"
    cp -a "$B/home/$path" "$HOME/$path"
done < "$B/saved.txt"
# When the backup holds no copy of plasmanotifyrc, delete the banner position the theme set, so Plasma's default
# returns.
grep -qx ".config/plasmanotifyrc" "$B/saved.txt" ||
    kwriteconfig6 --file "$HOME/.config/plasmanotifyrc" --group Notifications --key PopupPosition --delete
dconf load /org/gnome/desktop/interface/ < "$B/dconf-interface.ini"
systemctl --user unset-environment QT_PLUGIN_PATH QML_IMPORT_PATH PLASMA_INTEGRATION_USE_PORTAL GTK_USE_PORTAL
# kglobalacceld keeps shortcuts in memory and writes them back over kglobalshortcutsrc: give Win+E back to Dolphin.
busctl --user call org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel setShortcut asaiu 4 file-explorer.desktop _launch \
    "File Explorer" "Launch File Explorer" 0 6 > /dev/null 2>&1
busctl --user call org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel setShortcut asaiu 4 org.kde.dolphin.desktop _launch \
    Dolphin "Launch Dolphin" 1 268435525 6 > /dev/null 2>&1
# The Windows key goes back to Plasma's "Activate Application Launcher" (the theme gives it to Quick Menu).
python3 -I "$B/windows_key.py" stock > /dev/null 2>&1
# The FileManager1 and FileChooser services are gone with their files; let D-Bus and the portal forget them.
busctl --user call org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus ReloadConfig > /dev/null 2>&1
systemctl --user daemon-reload
# KRunner comes back as Plasma ships it once the Quick Menu drop-in is gone.
systemctl --user try-restart plasma-krunner.service > /dev/null 2>&1
systemctl --user restart xdg-desktop-portal.service > /dev/null 2>&1
# KWin starts before the login script, so the window buttons and the pointing hand were also linked into the
# system plugin folders (root).
links=(/usr/lib/qt6/plugins/org.kde.kdecoration3/org.aurora.decoration.so /usr/lib/qt6/plugins/kwin/plugins/aurora-pointer.so)
for link in "${links[@]}"; do
    if [ -L "$link" ]; then
        echo "Removing the links in the system plugin folders (asks for your password)"
        as_root rm -f -- "${links[@]}"
        break
    fi
done
# The optional performance tuning (install.sh --performance) goes back to CachyOS's defaults.
tuning=(/etc/scx_loader.toml /etc/tmpfiles.d/thp.conf /etc/sysctl.d/99-aurora-memory.conf
        /etc/systemd/zram-generator.conf.d/90-aurora-zram.conf)
for file in "${tuning[@]}"; do
    if [ -f "$file" ]; then
        echo "Putting CachyOS's scheduler and memory settings back (asks for your password)"
        # One sudo call; the files reach the fixed script as its arguments.
        as_root sh -c 'rm -f -- "$@"; systemctl disable --now scx_loader.service
            echo defer+madvise > /sys/kernel/mm/transparent_hugepage/defrag; sysctl -q --system' sh "${tuning[@]}"
        break
    fi
done
# Put the old GTK theme back, then tell the running apps that the colors, fonts, style, icons and cursor changed.
qdbus6 org.kde.kded6 /modules/gtkconfig org.kde.GtkConfig.setGtkTheme "$(cat "$B/old-gtk-theme")" > /dev/null 2>&1
for change in 0 1 2 4 5; do
    dbus-send --session --type=signal /KGlobalSettings org.kde.KGlobalSettings.notifyChange "int32:$change" int32:0
done
# KWin rereads its settings, and the application cache is refreshed.
qdbus6 org.kde.KWin /KWin reconfigure > /dev/null 2>&1
kbuildsycoca6 > /dev/null 2>&1
echo "Starting plasmashell"
systemctl --user start plasma-plasmashell.service
echo "Done. Log out and back in so every app and KWin load the old look again."
"""

# False with --no-session: the steps then only write files and never contact the running session.
TALK_TO_SESSION = True


def installed_families():
    """The font families fontconfig knows, by every name each one goes by."""
    listed = subprocess.run(["fc-list", ":", "family"], capture_output=True, text=True, check=False).stdout
    return {name.strip() for line in listed.splitlines() for name in line.split(",") if name.strip()}


def ui_font(families):
    """The interface font: SF Pro Text when the user installed it, else Inter."""
    return next((f for f in UI_FONTS[:-1] if f in families), UI_FONTS[-1])


def mono_font(families):
    """Konsole's font: SF Mono with Nerd Fonts' symbols when the user installed it, else JetBrains Mono's."""
    return next((f for f in MONO_FONTS[:-1] if f in families), MONO_FONTS[-1])


def kde_font(family, size, weight):
    """`family` at `size` points and `weight` in the string format KDE writes to kdeglobals."""
    return f"{family},{size},-1,5,{weight},0,0,0,0,0,0,0,0,0,0,1"


def run(*cmd, check=True, quiet=False):
    """Prints and runs a command and returns the finished process.

    With `check`, a failure exits the install. `quiet` captures the output instead of showing it, and the exit
    message then carries the command's stderr.
    """
    print("+", " ".join(cmd), flush=True)
    result = subprocess.run(cmd, text=True, capture_output=quiet)
    if check and result.returncode != 0:
        sys.exit(f"failed ({result.returncode}): {' '.join(cmd)}\n{result.stderr if quiet else ''}")
    return result


def session(*cmd, check=True):
    """A command that tells the running Plasma session about a change; skipped with --no-session."""
    if TALK_TO_SESSION:
        return run(*cmd, check=check, quiet=True)
    return None


def output(*cmd):
    """The stripped stdout of a command; its exit status is ignored."""
    return subprocess.run(cmd, text=True, capture_output=True).stdout.strip()


def kwrite(file, groups, key, value=None):
    """Sets `key`, nested in `groups`, in a config file under the home folder; a `value` of None deletes the key."""
    args = ["kwriteconfig6", "--file", os.path.join(HOME, file)]
    for group in groups:
        args += ["--group", group]
    args += ["--key", key]
    run(*(args + (["--delete"] if value is None else [value])), quiet=True)


def kread(file, group, key):
    """The value of `key` in `group` of a config file under the home folder."""
    return output("kreadconfig6", "--file", os.path.join(HOME, file), "--group", group, "--key", key)


def replace_tree(source, target):
    """Replaces `target` (a file, a link or a folder) with a copy of the folder `source`; links inside stay links."""
    if os.path.lexists(target):
        shutil.rmtree(target) if os.path.isdir(target) and not os.path.islink(target) else os.remove(target)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    shutil.copytree(source, target, symlinks=True)


def install_file(source, target, mode=0o644):
    """Copies `source` to `target` with `mode`, creating the folders above it."""
    os.makedirs(os.path.dirname(target), exist_ok=True)
    # A copy beside the target, then a rename: writing into a running binary (File Explorer) fails with ETXTBSY.
    staged = target + ".installing"
    shutil.copy2(source, staged)
    os.chmod(staged, mode)
    os.replace(staged, target)


def topmost_missing(path):
    """The highest folder of path that does not exist yet: removing it removes everything the install made there."""
    parts = path.split("/")
    for i in range(1, len(parts) + 1):
        if not os.path.lexists(os.path.join(HOME, *parts[:i])):
            return "/".join(parts[:i])
    return None


def has_theme_panels(applets):
    """Whether a Plasma panel file already holds the theme's menu bar (its clock is the theme's own widget)."""
    return re.search(r"^plugin=org\.aurora\.clock$", applets, re.MULTILINE) is not None


def backup():
    """Saves what the install changes and writes restore.sh beside the copies, then records the folder in LATEST.

    Does nothing when a backup exists, so the first one keeps the desktop from before the theme.
    """
    if os.path.exists(LATEST):
        print(f"backup kept: {open(LATEST).read().strip()} (the desktop from before the theme)")
        return
    created = []
    for path in SAVED + ADDED:
        missing = topmost_missing(path)
        if missing and missing not in created:
            created.append(missing)
    directory = os.path.join(BACKUPS, "backups", datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    os.makedirs(os.path.join(directory, "home"))
    saved = []
    for path in SAVED:
        source = os.path.join(HOME, path)
        if os.path.lexists(source):
            target = os.path.join(directory, "home", path)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if os.path.isdir(source) and not os.path.islink(source):
                shutil.copytree(source, target, symlinks=True)
            else:
                shutil.copy2(source, target, follow_symlinks=False)
            saved.append(path)
    with open(os.path.join(directory, "saved.txt"), "w") as f:
        f.write("\n".join(saved) + "\n")
    with open(os.path.join(directory, "created.txt"), "w") as f:
        f.write("\n".join(created) + "\n")
    with open(os.path.join(directory, "dconf-interface.ini"), "w") as f:
        f.write((output("dconf", "dump", "/org/gnome/desktop/interface/") if TALK_TO_SESSION else "") + "\n")
    with open(os.path.join(directory, "old-gtk-theme"), "w") as f:
        gtk = output("qdbus6", "org.kde.kded6", "/modules/gtkconfig", "org.kde.GtkConfig.gtkTheme") if TALK_TO_SESSION else ""
        f.write(gtk + "\n")
    before = {"colorScheme": kread(".config/kdeglobals", "General", "ColorScheme"),
              "plasmaTheme": kread(".config/plasmarc", "Theme", "name"),
              "icons": kread(".config/kdeglobals", "Icons", "Theme"),
              "cursor": kread(".config/kcminputrc", "Mouse", "cursorTheme"),
              "versions": {p: output("pacman", "-Q", p) for p in ("plasma-workspace", "qt6-base", "kdecoration", "kwin")}}
    with open(os.path.join(directory, "before.json"), "w") as f:
        json.dump(before, f, indent=2)
    with open(os.path.join(directory, "restore.sh"), "w") as f:
        f.write(RESTORE)
    os.chmod(os.path.join(directory, "restore.sh"), 0o755)
    # The restore gives the Windows key back with this script; the checkout may be gone by then.
    shutil.copy2(os.path.join(ROOT, "components/windows-key/windows_key.py"), directory)
    with open(LATEST, "w") as f:
        f.write(directory + "\n")
    print(f"backup: {directory} ({len(saved)} saved, {len(created)} to remove on restore)")


def stage_icons(build=BUILD):
    """Unpacks the base icon themes from themes/icons/base into the build folder's icons/ once; the install copies
    them from there."""
    staged = os.path.join(build, "icons")
    if not all(os.path.isdir(os.path.join(staged, t)) for t in BASE_ICONS):
        os.makedirs(staged, exist_ok=True)
        with tarfile.open(os.path.join(THEMES, "icons/base/Aurora-base.tar.xz")) as archive:
            archive.extractall(staged, filter="data")
    return staged


def gtk_release(build=BUILD):
    """The GTK theme, unpacked once into the build folder's gtk/."""
    target = os.path.join(build, "gtk", GTK)
    if not os.path.isdir(target):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with tarfile.open(os.path.join(THEMES, "gtk", f"{GTK}.tar.xz")) as archive:
            archive.extractall(os.path.dirname(target), filter="data")
    return target


def migrate_from_spotlight(home):
    """Moves an install made while the Quick Menu was called Spotlight over to the new name: its launcher folder, its
    build stamp and its desktop entry go, its saved order and pins move, and the menu bar's button points at the Quick
    Menu's entry."""
    for path in (".local/lib/aurora/spotlight", ".local/share/aurora/spotlight-built-for",
                 ".local/share/applications/spotlight.desktop"):
        old = os.path.join(home, path)
        if os.path.isdir(old) and not os.path.islink(old):
            shutil.rmtree(old)
        elif os.path.lexists(old):
            os.remove(old)
    # The dragged order and the pins keep their place under the new name; a file the Quick Menu already wrote wins.
    order = os.path.join(home, ".config/aurora/spotlight.conf")
    if os.path.exists(order) and not os.path.exists(os.path.join(home, ".config/aurora/quick-menu.conf")):
        os.rename(order, os.path.join(home, ".config/aurora/quick-menu.conf"))
    applets = os.path.join(home, APPLETSRC)
    if os.path.exists(applets):
        with open(applets) as f:
            text = f.read()
        moved = text.replace("/applications/spotlight.desktop", "/applications/quick-menu.desktop")
        if moved != text:
            with open(applets, "w") as f:
                f.write(moved)


# Widget ids of earlier Aurora versions, and the id each widget has now; the panels keep their settings.
NATIVE_WIDGETS = {"com.github.antroids.application-title-bar": "org.aurora.apptitle",
                  "luisbocanegra.panel.colorizer": "org.aurora.menubarstyle",
                  "com.github.chrtall.kppleMenu": "org.aurora.logomenu",
                  "com.github.mkeays501.macoscontrolcentre": "org.aurora.controlcenter"}


def migrate_to_native_widgets(home):
    """Gives each widget of NATIVE_WIDGETS in the panels the plugin id of Aurora's own one, in place, and removes the
    old widget's files."""
    applets = os.path.join(home, APPLETSRC)
    if os.path.exists(applets):
        with open(applets) as f:
            text = f.read()
        moved = text
        for old, new in NATIVE_WIDGETS.items():
            moved = moved.replace(f"\nplugin={old}\n", f"\nplugin={new}\n")
        if moved != text:
            with open(applets, "w") as f:
                f.write(moved)
    for old in NATIVE_WIDGETS:
        folder = os.path.join(home, ".local/share/plasma/plasmoids", old)
        if os.path.isdir(folder):
            shutil.rmtree(folder)


def components():
    """The built parts, each beside what it was built for; a part the build skipped keeps what is installed."""
    migrate_from_spotlight(HOME)
    migrate_to_native_widgets(HOME)
    os.makedirs(STATE, exist_ok=True)
    for part, built, target in BUILT:
        stamp = os.path.join(BUILD, part, "built-for")
        if not os.path.exists(stamp):
            print(f"{part}: not built, skipped")
            continue
        destination = os.path.join(HOME, target)
        install_file(os.path.join(BUILD, part, built), destination, 0o755)
        if os.path.lexists(destination + ".paused"):
            os.remove(destination + ".paused")
        shutil.copy2(stamp, os.path.join(STATE, f"{part}-built-for"))
    part, built, target = GLASS
    if os.path.exists(os.path.join(BUILD, part, "built-for")):
        replace_tree(os.path.join(BUILD, part, built), os.path.join(HOME, target))
    built, target = CLI
    if os.path.exists(os.path.join(BUILD, built)):
        install_file(os.path.join(BUILD, built), os.path.join(HOME, target), 0o755)
    # Quick Menu starts through its launcher, which falls back to the stock KRunner when the stamp no longer matches.
    install_file(os.path.join(ROOT, "components/quick-menu/krunner-launch.sh"), os.path.join(LIB, "quick-menu/krunner-launch"), 0o755)
    session("systemctl", "--user", "try-restart", "plasma-krunner.service", check=False)
    # KWin loads its plugins at start; this loads the pointing hand now (it needs the root link, see install.sh).
    session("qdbus6", "org.kde.KWin", "/Plugins", "org.kde.KWin.Plugins.LoadPlugin", "aurora-pointer", check=False)


def files():
    """Installs the icons, themes, widgets, built parts, launchers, D-Bus services and session files.

    Everything goes into the home folder; the running session is told afterwards. The fonts come from packages.
    """
    icons = os.path.join(SHARE, "icons")
    staged = stage_icons()
    for theme in BASE_ICONS:
        replace_tree(os.path.join(staged, theme), os.path.join(icons, theme))
    replace_tree(os.path.join(THEMES, "cursors", CURSORS), os.path.join(icons, CURSORS))
    # Plasma's cursor tools and Xcursor look in ~/.icons, not ~/.local/share/icons (cursortheme thememodel.cpp,
    # searchPaths); a link there makes the cursors visible to both.
    link = os.path.join(HOME, ".icons", CURSORS)
    os.makedirs(os.path.dirname(link), exist_ok=True)
    if os.path.lexists(link):
        os.remove(link)
    os.symlink(os.path.join(icons, CURSORS), link)
    for icon in own_app_icons():
        install_file(os.path.join(THEMES, "icons/apps", icon), os.path.join(icons, "hicolor/scalable/apps", icon))
    theme_dir = os.path.join(icons, ICON_THEME)
    if os.path.lexists(theme_dir):
        shutil.rmtree(theme_dir)
    run(sys.executable, "-I", os.path.join(THEMES, "icons/build_icon_theme.py"), theme_dir, icons, quiet=True)
    # The menu bar's status icons in the spirit of SF Symbols (volume, network, media, notifications, Quick Menu).
    run(sys.executable, "-I", os.path.join(THEMES, "icons/status_glyphs.py"), theme_dir, quiet=True)
    system = os.path.join(THEMES, "icons/system")
    for name in ("preferences-system.svg", "systemsettings.svg"):
        install_file(os.path.join(system, "settings.svg"), os.path.join(theme_dir, "apps/scalable", name))
    for name in ("user-trash.svg", "user-trash-full.svg"):
        install_file(os.path.join(system, name), os.path.join(theme_dir, "places/scalable", name))
    install_file(os.path.join(system, "downloads.svg"), os.path.join(theme_dir, "places/scalable/aurora-downloads.svg"))

    install_file(os.path.join(THEMES, "colors", f"{COLORS}.colors"), os.path.join(SHARE, "color-schemes", f"{COLORS}.colors"))
    replace_tree(os.path.join(THEMES, "plasma", STYLE), os.path.join(SHARE, "plasma/desktoptheme", STYLE))
    replace_tree(os.path.join(THEMES, "kvantum", KVANTUM), os.path.join(HOME, ".config/Kvantum", KVANTUM))
    for wallpaper in WALLPAPERS:
        replace_tree(os.path.join(ROOT, "themes/wallpapers", wallpaper), os.path.join(SHARE, "wallpapers", wallpaper))
    for package in PLASMOIDS:
        replace_tree(package, os.path.join(SHARE, "plasma/plasmoids", plasmoid_id(package)))

    components()
    explorer = os.path.join(HOME, ".local/bin/file-explorer")
    applications = os.path.join(SHARE, "applications")
    os.makedirs(applications, exist_ok=True)
    with open(os.path.join(ROOT, "installer/desktop/file-explorer.desktop")) as f:
        entry = f.read().replace("@FILE_EXPLORER@", explorer)
    with open(os.path.join(applications, "file-explorer.desktop"), "w") as f:
        f.write(entry)
    install_file(os.path.join(ROOT, "installer/desktop/quick-menu.desktop"), os.path.join(applications, "quick-menu.desktop"))
    for name, directory in KONSOLE.items():
        install_file(os.path.join(THEMES, "konsole", name), os.path.join(HOME, directory, name))
    # The profile names JetBrains Mono; SF Mono takes its place when the user installed it.
    profile = os.path.join(HOME, KONSOLE["Aurora.profile"], "Aurora.profile")
    with open(profile) as f:
        text = f.read()
    with open(profile, "w") as f:
        f.write(text.replace(f"Font={MONO_FONTS[-1]},", f"Font={mono_font(installed_families())},"))

    install_file(os.path.join(SESSION_FILES, "aurora.sh"), os.path.join(HOME, ".config/plasma-workspace/env/aurora.sh"))
    for name, targets in DROP_INS.items():
        for target in targets:
            install_file(os.path.join(SESSION_FILES, name), os.path.join(HOME, target))
    # "Show in folder" (org.freedesktop.FileManager1) and the Open and Save panels (the FileChooser portal) are File
    # Explorer windows. dbus-broker prefers ~/.local/share/dbus-1/services over Dolphin's system file for the name.
    services = os.path.join(SHARE, "dbus-1/services")
    os.makedirs(services, exist_ok=True)
    for name, (mode, comment) in DBUS_SERVICES.items():
        with open(os.path.join(services, f"{name}.service"), "w") as f:
            f.write(f"# Aurora: {comment}\n[D-BUS Service]\nName={name}\nExec={explorer} {mode}\n")
    install_file(os.path.join(SESSION_FILES, "portal/aurora.portal"), os.path.join(SHARE, "xdg-desktop-portal/portals/aurora.portal"))
    install_file(os.path.join(SESSION_FILES, "portal/kde-portals.conf"), os.path.join(HOME, ".config/xdg-desktop-portal/kde-portals.conf"))
    install_file(os.path.join(SESSION_FILES, "portal/90-aurora-portal.conf"),
                 os.path.join(HOME, ".config/environment.d/90-aurora-portal.conf"))
    # After a system update, rebuild.sh builds the parts again from this checkout.
    install_file(os.path.join(SESSION_FILES, "rebuild.sh"), os.path.join(STATE, "rebuild.sh"), 0o755)
    source = os.path.join(STATE, "source")
    if os.path.lexists(source):
        os.remove(source)
    os.symlink(ROOT, source)

    session("busctl", "--user", "call", "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ReloadConfig")
    session("systemctl", "--user", "daemon-reload")
    session("systemctl", "--user", "try-restart", "plasma-krunner.service", check=False)
    session("kbuildsycoca6")
    print("files installed")


def look():
    """Applies the look: fonts, colors, Plasma style, icons, cursor, GTK, Konsole and window buttons.

    Also sets the Windows key, the default file manager and Win+E. Exits when a theme is not applied.
    """
    font = ui_font(installed_families())
    for key in ("font", "menuFont", "toolBarFont"):
        kwrite(".config/kdeglobals", ["General"], key, kde_font(font, 10, 400))
    kwrite(".config/kdeglobals", ["General"], "smallestReadableFont", kde_font(font, 8, 400))
    kwrite(".config/kdeglobals", ["WM"], "activeFont", kde_font(font, 10, 600))
    # macOS writes sizes in decimal units; KFormat reads this for every KDE app (kformatprivate.cpp).
    kwrite(".config/kdeglobals", ["Locale"], "BinaryUnitDialect", "2")
    # Finder's list view: 16 px icons and thumbnails.
    kwrite(".config/dolphinrc", ["DetailsMode"], "IconSize", "16")
    kwrite(".config/dolphinrc", ["DetailsMode"], "PreviewSize", "16")
    kwrite(".config/Kvantum/kvantum.kvconfig", ["General"], "theme", KVANTUM)
    # Banners rise from the bottom-right corner, where CachyOS puts notifications.
    kwrite(".config/plasmanotifyrc", ["Notifications"], "PopupPosition", "BottomRight")

    # Each tool refuses with an error when its theme is already the current one, so a second run reads the
    # result back instead of trusting the exit code.
    run("plasma-apply-colorscheme", COLORS, check=False)
    run("plasma-apply-desktoptheme", STYLE, check=False)
    run("/usr/lib/plasma-changeicons", ICON_THEME, check=False)
    run("plasma-apply-cursortheme", "--size", "24", CURSORS, check=False)
    applied = {"colors": kread(".config/kdeglobals", "General", "ColorScheme"), "style": kread(".config/plasmarc", "Theme", "name"),
               "icons": kread(".config/kdeglobals", "Icons", "Theme"), "cursor": kread(".config/kcminputrc", "Mouse", "cursorTheme")}
    wanted = {"colors": COLORS, "style": STYLE, "icons": ICON_THEME, "cursor": CURSORS}
    if applied != wanted:
        sys.exit(f"themes not applied: {applied}")
    for change in ("1", "2"):  # fonts, widget style
        run("dbus-send", "--session", "--type=signal", "/KGlobalSettings", "org.kde.KGlobalSettings.notifyChange",
            f"int32:{change}", "int32:0")

    run("qdbus6", "org.kde.kded6", "/modules/gtkconfig", "org.kde.GtkConfig.setGtkTheme", GTK, quiet=True)
    run(sys.executable, "-I", os.path.join(THEMES, "gtk/gtk_theme.py"), HOME, gtk_release(), os.path.join(SHARE, "icons", ICON_THEME),
        font)

    kwrite(".config/konsolerc", ["Desktop Entry"], "DefaultProfile", "Aurora.profile")
    # Konsole restores its saved toolbar layout over the rc files that hide the toolbars (kmainwindow.cpp).
    kwrite(".local/state/konsolestaterc", ["MainWindow"], "State")

    for key, value in (("library", "org.aurora.decoration"), ("theme", ""), ("ButtonsOnLeft", ""), ("ButtonsOnRight", "IAX"),
                       ("BorderSize", "None"), ("BorderSizeAuto", "false")):
        kwrite(".config/kwinrc", ["org.kde.kdecoration2"], key, value)
    # The Windows key opens Quick Menu, as Command-Space does on a Mac. Plasma 6.1 moved lone-modifier shortcuts into
    # kglobalaccel, so the old kwinrc [ModifierOnlyShortcuts] key is dead and goes.
    kwrite(".config/kwinrc", ["ModifierOnlyShortcuts"], "Meta")
    run(sys.executable, "-I", os.path.join(ROOT, "components/windows-key/windows_key.py"), "quick-menu", quiet=True)
    # One touch of the bottom edge brings the Dock back, as on macOS; KWin's default 1 px pushback wants a second push.
    kwrite(".config/kwinrc", ["Windows"], "ElectricBorderPushbackPixels", "0")
    run("qdbus6", "org.kde.KWin", "/KWin", "reconfigure", check=False, quiet=True)
    set_default_file_manager()
    set_explorer_shortcut()
    print("look applied")


def set_explorer_shortcut():
    """Win+E opens File Explorer, as on Windows: Dolphin gives up Meta+E and File Explorer's launch action takes it."""
    meta_e = "268435525"  # Qt::META | Qt::Key_E
    accel = ["busctl", "--user", "call", "org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel"]
    run(*accel, "setShortcut", "asaiu", "4", "org.kde.dolphin.desktop", "_launch", "Dolphin", "Launch Dolphin", "0", "6", quiet=True)
    run(*accel, "doRegister", "as", "4", "file-explorer.desktop", "_launch", "File Explorer", "Launch File Explorer", quiet=True)
    run(*accel, "setShortcut", "asaiu", "4", "file-explorer.desktop", "_launch", "File Explorer", "Launch File Explorer", "1", meta_e,
        "6", quiet=True)
    owner = output("qdbus6", "org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel.action", meta_e)
    if not owner.startswith("file-explorer.desktop"):
        sys.exit(f"Win+E still belongs to {owner.splitlines()[:1]}")


def set_default_file_manager():
    """Makes File Explorer the handler of inode/directory in mimeapps.list, as a default and as an association."""
    path = os.path.join(HOME, ".config/mimeapps.list")
    lines = open(path).read().splitlines() if os.path.exists(path) else []
    for section, value in (("[Default Applications]", "file-explorer.desktop"), ("[Added Associations]", "file-explorer.desktop;")):
        if section not in lines:
            lines += ["", section]
        start = lines.index(section) + 1
        end = next((i for i in range(start, len(lines)) if lines[i].startswith("[")), len(lines))
        entry = [i for i in range(start, end) if lines[i].startswith("inode/directory=")]
        if entry:
            lines[entry[0]] = f"inode/directory={value}"
        else:
            lines.insert(start, f"inode/directory={value}")
    with open(path, "w") as f:
        f.write("\n".join(lines).strip("\n") + "\n")
    run("kbuildsycoca6", quiet=True)


def wait_for(condition, seconds):
    """Polls `condition` every 0.5 s for up to `seconds`; returns whether it became true."""
    deadline = time.time() + seconds
    while time.time() < deadline:
        if condition():
            return True
        time.sleep(0.5)
    return False


def plasmashell_running():
    """Whether the current user has a plasmashell process."""
    return subprocess.run(["pgrep", "-x", "-u", str(os.getuid()), "plasmashell"], capture_output=True).returncode == 0


def panels(force=False):
    """Puts up the menu bar and the Dock from themes/panels/layout.js and writes the tray settings.

    The tray settings are written while plasmashell is stopped. Does nothing when the panels are already there,
    unless `force` is set. Needs the backup: the tray settings come from the copy of the panel file it holds.
    """
    applets = os.path.join(HOME, APPLETSRC)
    if not force and os.path.exists(applets) and has_theme_panels(open(applets).read()):
        print("panels kept: the menu bar and the Dock are already there (install.py panels --force rebuilds them)")
        return
    backup_dir = open(LATEST).read().strip()
    script = output(os.path.join(THEMES, "panels/render_layout.sh"), HOME)
    assert "@" not in script.replace("@kde.org", ""), "unfilled placeholder in layout.js"
    result = run("qdbus6", "org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", script, quiet=True)
    print(result.stdout.strip())
    # Plasma writes the new panels to disk when it quits; the unit restarts plasmashell only after a failure
    # (Restart=on-failure), so a clean quit leaves it stopped while the tray settings are written.
    time.sleep(8)
    run("qdbus6", "org.kde.plasmashell", "/MainApplication", "org.qtproject.Qt.QCoreApplication.quit", check=False, quiet=True)
    if not wait_for(lambda: not plasmashell_running(), 30):
        run("systemctl", "--user", "stop", "plasma-plasmashell.service")
    run("systemctl", "--user", "stop", "plasma-plasmashell.service")
    if not wait_for(lambda: not plasmashell_running(), 30):
        sys.exit("plasmashell did not stop")
    run(sys.executable, "-I", os.path.join(THEMES, "panels/patch_tray.py"), applets, os.path.join(backup_dir, "home", APPLETSRC))
    run(sys.executable, "-I", os.path.join(THEMES, "panels/tray_hide.py"), applets, ",".join(TRAY_HIDDEN))
    current = output("systemctl", "--user", "show-environment")
    plugin_path = next((line.split("=", 1)[1] for line in current.splitlines() if line.startswith("QT_PLUGIN_PATH=")), "")
    plugins = os.path.join(HOME, PLUGINS)
    if plugins not in plugin_path.split(":"):
        run("systemctl", "--user", "set-environment", "QT_PLUGIN_PATH=" + ":".join(p for p in (plugins, plugin_path) if p))
    run("systemctl", "--user", "start", "plasma-plasmashell.service")
    if not wait_for(plasmashell_running, 30):
        sys.exit("plasmashell did not start")
    print("panels rebuilt")


def export(destination):
    """Copies what the install takes from the build folder into destination, which then serves as a build folder
    itself: each built part with its stamp, the glass module, and the icons and GTK theme unpacked. The aurora-theme
    package ships it as /usr/share/aurora/prebuilt, so installing Aurora builds nothing."""
    for part, built, _ in BUILT:
        stamp = os.path.join(BUILD, part, "built-for")
        if not os.path.exists(stamp):
            sys.exit(f"{part} is not built: run installer/build.sh first")
        install_file(os.path.join(BUILD, part, built), os.path.join(destination, part, built), 0o755)
        install_file(stamp, os.path.join(destination, part, "built-for"))
    part, built, _ = GLASS
    replace_tree(os.path.join(BUILD, part, built), os.path.join(destination, part, built))
    install_file(os.path.join(BUILD, part, "built-for"), os.path.join(destination, part, "built-for"))
    stage_icons(destination)
    gtk_release(destination)
    print(f"exported the built parts to {destination}")


def check():
    """Everything the steps copy from, so a missing build shows up before anything is touched."""
    sources = [os.path.join(BUILD, part, built) for part, built, _ in BUILT] + [os.path.join(BUILD, GLASS[0], GLASS[1])]
    sources += [os.path.join(THEMES, "icons/base/Aurora-base.tar.xz"), os.path.join(THEMES, "kvantum", KVANTUM),
                os.path.join(THEMES, "gtk", f"{GTK}.tar.xz"), os.path.join(THEMES, "cursors", CURSORS),
                os.path.join(THEMES, "plasma", STYLE), os.path.join(THEMES, "colors", f"{COLORS}.colors"),
                os.path.join(THEMES, "panels/layout.js")]
    sources += [os.path.join(p, "metadata.json") for p in PLASMOIDS]
    missing = [s for s in sources if not os.path.exists(s)]
    for source in missing:
        # Paths inside the copy are shown from its top; the build folder can be elsewhere (the user's cache).
        inside = os.path.commonpath([source, ROOT]) == ROOT
        print(f"missing {os.path.relpath(source, ROOT) if inside else source}")
    if missing:
        print("build the parts with installer/build.sh")
        sys.exit(1)
    print(f"all {len(sources)} sources present")


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--no-session" in args:
        TALK_TO_SESSION = False
        args.remove("--no-session")
    force = "--force" in args
    if force:
        args.remove("--force")
    steps = {"backup": backup, "files": files, "components": components, "look": look,
             "panels": lambda: panels(force), "check": check}
    if len(args) == 2 and args[0] == "export":
        export(args[1])
    elif len(args) != 1 or args[0] not in steps:
        sys.exit(__doc__)
    else:
        steps[args[0]]()
