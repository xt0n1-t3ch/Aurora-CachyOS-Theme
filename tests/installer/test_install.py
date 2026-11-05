# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""The installer keeps the panels a user already has, and its backup can take back every file it adds.

    python3 -I tests/installer/test_install.py      (after installer/build.sh; changes nothing outside a temp folder)
"""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INSTALL = os.path.join(ROOT, "installer/install.py")
spec = importlib.util.spec_from_file_location("install", INSTALL)
install = importlib.util.module_from_spec(spec)
spec.loader.exec_module(install)


def read(path):
    """Return the text of the file at path."""
    with open(path) as f:
        return f.read()


def tree(home, skip):
    """Every path under home, relative to it, except those under skip."""
    paths = set()
    for directory, dirs, files in os.walk(home):
        for name in dirs + files:
            path = os.path.relpath(os.path.join(directory, name), home)
            if not path.startswith(skip):
                paths.add(path)
    return paths


class Panels(unittest.TestCase):
    """install.has_theme_panels tells a desktop that has the theme's menu bar from a stock one."""

    def test_a_desktop_that_has_the_menu_bar_keeps_its_panels(self):
        applets = "[Containments][2][Applets][31]\nimmutability=1\nplugin=org.aurora.clock\n"
        self.assertTrue(install.has_theme_panels(applets))

    def test_a_stock_desktop_gets_the_menu_bar_and_the_dock(self):
        applets = "[Containments][2][Applets][31]\nimmutability=1\nplugin=org.kde.plasma.digitalclock\n"
        self.assertFalse(install.has_theme_panels(applets))

    def test_the_clock_named_in_another_key_is_not_a_menu_bar(self):
        applets = "[Containments][2][Applets][31][Configuration]\nnote=org.aurora.clock\n"
        self.assertFalse(install.has_theme_panels(applets))


class Fonts(unittest.TestCase):
    """Aurora ships only free fonts, and keeps Apple's when the user installed them."""

    def test_without_apples_fonts_the_desktop_uses_inter(self):
        self.assertEqual(install.ui_font({"Inter", "Noto Sans"}), "Inter")

    def test_sf_pro_installed_by_the_user_is_kept(self):
        self.assertEqual(install.ui_font({"SF Pro Text", "Inter"}), "SF Pro Text")

    def test_konsole_uses_jetbrains_mono_unless_sf_mono_is_installed(self):
        self.assertEqual(install.mono_font({"Inter"}), "JetBrainsMono Nerd Font")
        self.assertEqual(install.mono_font({"SFMono Nerd Font", "JetBrainsMono Nerd Font"}), "SFMono Nerd Font")

    def test_kde_writes_the_font_with_its_size_and_weight(self):
        self.assertEqual(install.kde_font("Inter", 10, 600), "Inter,10,-1,5,600,0,0,0,0,0,0,0,0,0,0,1")

    def test_the_repository_carries_no_apple_font(self):
        tracked = subprocess.run(["git", "-C", ROOT, "ls-files"], capture_output=True, text=True, check=True).stdout
        self.assertFalse([p for p in tracked.splitlines()
                          if "SFMono" in p or "SF-Mono" in p or "SF-Pro" in p or "San-Francisco" in p])


class QuickMenuMigration(unittest.TestCase):
    """An install from before the Quick Menu had its name (it was called Spotlight) moves over on the next install."""

    def test_the_old_spotlight_files_go_and_the_menu_bar_button_follows(self):
        with tempfile.TemporaryDirectory() as home:
            old = [".local/lib/aurora/spotlight/krunner-launch", ".local/share/aurora/spotlight-built-for",
                   ".local/share/applications/spotlight.desktop"]
            for path in old:
                os.makedirs(os.path.dirname(os.path.join(home, path)), exist_ok=True)
                open(os.path.join(home, path), "w").close()
            applets = os.path.join(home, install.APPLETSRC)
            os.makedirs(os.path.dirname(applets))
            with open(applets, "w") as f:
                f.write(f"[Containments][2][Applets][40][Configuration][General]\n"
                        f"url=file://{home}/.local/share/applications/spotlight.desktop\n")
            os.makedirs(os.path.join(home, ".config/aurora"))
            with open(os.path.join(home, ".config/aurora/spotlight.conf"), "w") as f:
                f.write('[Applications]\npinned=["applications:steam.desktop"]\n')
            install.migrate_from_spotlight(home)
            self.assertFalse([p for p in old if os.path.lexists(os.path.join(home, p))])
            # The dragged order and the pins move with the name.
            self.assertIn("steam.desktop", read(os.path.join(home, ".config/aurora/quick-menu.conf")))
            self.assertFalse(os.path.exists(os.path.join(home, ".config/aurora/spotlight.conf")))
            self.assertFalse(os.path.exists(os.path.join(home, ".local/lib/aurora/spotlight")))
            self.assertIn(f"url=file://{home}/.local/share/applications/quick-menu.desktop", read(applets))

    def test_a_new_install_has_nothing_to_move(self):
        with tempfile.TemporaryDirectory() as home:
            install.migrate_from_spotlight(home)
            self.assertEqual(tree(home, "nothing"), set())


class NativeWidgets(unittest.TestCase):
    """An install whose menu bar still has a widget Aurora now draws itself gets Aurora's own one on the next install."""

    def test_the_old_widget_gives_its_place_to_aurora_s_own(self):
        with tempfile.TemporaryDirectory() as home:
            applets = os.path.join(home, install.APPLETSRC)
            os.makedirs(os.path.dirname(applets))
            with open(applets, "w") as f:
                f.write("[Containments][2][Applets][30]\nimmutability=1\nplugin=luisbocanegra.panel.colorizer\n"
                        "[Containments][2][Applets][31]\nimmutability=1\nplugin=com.github.antroids.application-title-bar\n"
                        "[Containments][2][Applets][32]\nimmutability=1\nplugin=org.aurora.clock\n")
            old = os.path.join(home, ".local/share/plasma/plasmoids/com.github.antroids.application-title-bar")
            os.makedirs(os.path.join(old, "contents"))
            install.migrate_to_native_widgets(home)
            text = read(applets)
            self.assertIn("[Containments][2][Applets][31]\nimmutability=1\nplugin=org.aurora.apptitle\n", text)
            self.assertIn("[Containments][2][Applets][30]\nimmutability=1\nplugin=org.aurora.menubarstyle\n", text)
            self.assertIn("plugin=org.aurora.clock\n", text)
            self.assertNotIn("antroids", text)
            self.assertNotIn("luisbocanegra", text)
            self.assertFalse(os.path.exists(old))

    def test_a_new_install_has_nothing_to_move(self):
        with tempfile.TemporaryDirectory() as home:
            install.migrate_to_native_widgets(home)
            self.assertEqual(tree(home, "nothing"), set())


class Export(unittest.TestCase):
    """install.py export copies what the install takes from a build into a folder of its own: the package's prebuilt
    parts. That folder works as a build folder: each part with its stamp, the glass module, the icons and GTK theme
    already unpacked, and nothing else (no object files)."""

    def test_the_export_holds_what_the_install_copies_and_nothing_more(self):
        with tempfile.TemporaryDirectory() as tmp:
            build, out = os.path.join(tmp, "build"), os.path.join(tmp, "prebuilt")
            for part, built, _ in install.BUILT:
                os.makedirs(os.path.dirname(os.path.join(build, part, built)), exist_ok=True)
                for name in (built, "built-for", "main.cpp.o"):
                    with open(os.path.join(build, part, name), "w") as f:
                        f.write(part)
            glass = os.path.join(build, install.GLASS[0], install.GLASS[1])
            os.makedirs(glass)
            open(os.path.join(glass, "qmldir"), "w").close()
            open(os.path.join(build, install.GLASS[0], "built-for"), "w").close()
            env = dict(os.environ, BUILD_DIR=build)
            result = subprocess.run([sys.executable, "-I", INSTALL, "export", out], env=env, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            exported = tree(out, ("icons/", "gtk/"))
            want = {os.path.join(part, name) for part, built, _ in install.BUILT for name in (built, "built-for")}
            want |= {os.path.dirname(p) for p in list(want)} | {os.path.join(install.GLASS[0], install.GLASS[1], "qmldir"),
                                                                os.path.join(install.GLASS[0], "built-for")}
            parents = set()
            for path in want:
                while os.path.dirname(path):
                    path = os.path.dirname(path)
                    parents.add(path)
            self.assertEqual(exported - {"icons", "gtk"}, want | parents)
            for theme in install.BASE_ICONS:
                self.assertTrue(os.path.isfile(os.path.join(out, "icons", theme, "index.theme")), theme)
            self.assertTrue(os.path.isdir(os.path.join(out, "gtk", install.GTK, "gtk-3.0")))


class Look(unittest.TestCase):
    def test_gtk_apps_use_the_cursor_plasma_uses(self):
        # gtk_theme.py writes its settings when it runs, so its SETTINGS table is read as text.
        source = read(os.path.join(ROOT, "themes/gtk/gtk_theme.py"))
        self.assertIn(f'"gtk-cursor-theme-name": "{install.CURSORS}",', source)


class Restore(unittest.TestCase):
    """The backup the installer writes before it changes anything."""

    def test_the_backup_lists_every_file_the_install_adds(self):
        with tempfile.TemporaryDirectory() as home:
            # A home as Plasma leaves it: its settings folders exist, one setting is there.
            for folder in (".config", ".local/share", ".local/state"):
                os.makedirs(os.path.join(home, folder))
            with open(os.path.join(home, ".config/kdeglobals"), "w") as f:
                f.write("[General]\nColorScheme=BreezeDark\n")
            # The backups stay after a restore; everything else the install made goes.
            backups = os.path.join(".local", "state", "aurora")
            before = tree(home, backups)
            env = dict(os.environ, HOME=home)
            for step in ("backup", "files"):
                result = subprocess.run([sys.executable, "-I", INSTALL, step, "--no-session"], env=env, text=True,
                                        capture_output=True)
                self.assertEqual(result.returncode, 0, f"{step}: {result.stdout}{result.stderr}")
            backup = read(os.path.join(home, backups, "latest-backup")).strip()
            self.assertTrue(backup.startswith(home), backup)
            added = tree(home, backups) - before
            self.assertTrue(any(p.endswith("org.aurora.decoration.so") for p in added), "the install added nothing")
            self.assertIn(".local/share/wallpapers/Aurora/metadata.json", added, "Aurora's wallpaper is not offered")
            self.assertFalse([p for p in added if p.startswith(".local/share/fonts")], "the install copied fonts")
            profile = read(os.path.join(home, ".local/share/konsole/Aurora.profile"))
            with unittest.mock.patch.dict(os.environ, {"HOME": home}):  # the fonts that home sees, as the install did
                self.assertIn(f"Font={install.mono_font(install.installed_families())},11,", profile)
            if os.path.exists(os.path.join(ROOT, "build/cli/aurora")):
                self.assertIn(".local/bin/aurora", added, "the aurora command was not installed")
            for path in read(os.path.join(backup, "created.txt")).splitlines():  # one path per line, as restore.sh reads it
                target = os.path.join(home, path)
                if os.path.isdir(target) and not os.path.islink(target):
                    shutil.rmtree(target)
                elif os.path.lexists(target):
                    os.remove(target)
            self.assertEqual(tree(home, backups), before)

    def test_a_second_backup_keeps_the_first(self):
        with tempfile.TemporaryDirectory() as home:
            env = dict(os.environ, HOME=home)
            first = subprocess.run([sys.executable, "-I", INSTALL, "backup", "--no-session"], env=env, text=True,
                                   capture_output=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            latest = os.path.join(home, ".local/state/aurora/latest-backup")
            kept = read(latest)
            second = subprocess.run([sys.executable, "-I", INSTALL, "backup", "--no-session"], env=env, text=True,
                                    capture_output=True)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(read(latest), kept)


if __name__ == "__main__":
    unittest.main(verbosity=2)
