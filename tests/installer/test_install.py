# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""The installer keeps the panels a user already has, and its backup can take back every file it adds.

    python3 -I tests/installer/test_install.py      (after installer/build.sh; changes nothing outside a temp folder)
"""
import importlib.util
import json
import os
import re
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


class LayoutScript(unittest.TestCase):
    """install.layout_script fills every placeholder of the panel script, and stops the install when one is left."""

    def panels_folder(self, layout):
        """A themes folder whose panels/layout.js is `layout`, rendered by the real render_layout.sh."""
        themes = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, themes)
        os.mkdir(os.path.join(themes, "panels"))
        shutil.copy(os.path.join(ROOT, "themes/panels/render_layout.sh"), os.path.join(themes, "panels"))
        with open(os.path.join(themes, "panels/layout.js"), "w") as f:
            f.write(layout)
        return themes

    def test_the_shipped_layout_renders_with_the_home_folder_and_the_gaps_filled(self):
        script = install.layout_script("/home/steven")
        self.assertIn('localPath: "/home/steven/.local/share/applications/quick-menu.desktop"', script)
        self.assertIn('{url: "file:///home/steven/Downloads"', script)
        self.assertEqual(re.findall(r"length: (\S+?)\}", script), ["2", "11", "1"])
        self.assertEqual(re.findall(r"@[A-Z_]+@", script), [])

    def test_at_signs_in_comments_and_addresses_are_not_placeholders(self):
        layout = "// render_layout.sh fills the @...@ placeholders; ask kde@kde.org.\nvar home = \"@HOME@\";\n"
        with unittest.mock.patch.object(install, "THEMES", self.panels_folder(layout)):
            script = install.layout_script("/home/steven")
        self.assertIn('var home = "/home/steven";', script)

    def test_a_placeholder_the_renderer_does_not_know_stops_the_install(self):
        with unittest.mock.patch.object(install, "THEMES", self.panels_folder("var gap = @GAP_NEW@;\n")):
            with self.assertRaises(SystemExit) as stop:
                install.layout_script("/home/steven")
        self.assertIn("@GAP_NEW@", str(stop.exception.code))

    def test_a_renderer_that_prints_nothing_stops_the_install(self):
        themes = self.panels_folder("")
        os.remove(os.path.join(themes, "panels/layout.js"))  # sed fails and prints nothing
        with unittest.mock.patch.object(install, "THEMES", themes):
            with self.assertRaises(SystemExit):
                install.layout_script("/home/steven")


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

    def test_a_running_plasma_stops_before_the_ids_change_and_says_it_must_start_again(self):
        # Plasma keeps its widgets in memory and writes them back when it quits, so a change made while it runs
        # would be undone at logout, with the old widgets' files gone.
        with tempfile.TemporaryDirectory() as home:
            applets = os.path.join(home, install.APPLETSRC)
            os.makedirs(os.path.dirname(applets))
            with open(applets, "w") as f:
                f.write("[Containments][2][Applets][30]\nplugin=com.github.chrtall.kppleMenu\n")
            seen = []
            record = lambda *cmd, check=True: seen.append((cmd, read(applets)))
            with unittest.mock.patch.object(install, "session", record), \
                    unittest.mock.patch.object(install, "plasmashell_running", lambda: True):
                restart = install.migrate_widgets_live(home)
            self.assertTrue(restart)
            self.assertEqual(seen[0][0], ("systemctl", "--user", "stop", "plasma-plasmashell.service"))
            self.assertIn("kppleMenu", seen[0][1], "Plasma stopped before the file changed")
            self.assertIn("plugin=org.aurora.logomenu\n", read(applets))

    def test_nothing_to_move_leaves_a_running_plasma_alone(self):
        with tempfile.TemporaryDirectory() as home:
            seen = []
            with unittest.mock.patch.object(install, "session", lambda *cmd, check=True: seen.append(cmd)), \
                    unittest.mock.patch.object(install, "plasmashell_running", lambda: True):
                self.assertFalse(install.migrate_widgets_live(home))
            self.assertEqual(seen, [])


class HomeCli(unittest.TestCase):
    """A source install put aurora in ~/.local/bin, which comes before /usr/bin on PATH. Installed from the package,
    whose aurora is in /usr/bin, that older copy goes, so `aurora` runs the package's."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.home, self.build = os.path.join(self.tmp.name, "home"), os.path.join(self.tmp.name, "prebuilt")
        self.system = os.path.join(self.tmp.name, "usr/bin/aurora")
        self.copy = os.path.join(self.home, install.CLI[1])
        for path in (self.copy, self.system):
            os.makedirs(os.path.dirname(path))
            open(path, "w").close()
        os.makedirs(self.build)

    def tearDown(self):
        self.tmp.cleanup()

    def test_the_package_s_aurora_takes_over_from_an_older_home_copy(self):
        install.retire_home_cli(self.home, self.build, self.system)
        self.assertFalse(os.path.lexists(self.copy))

    def test_a_build_with_its_own_aurora_keeps_the_home_copy(self):
        os.makedirs(os.path.dirname(os.path.join(self.build, install.CLI[0])))
        open(os.path.join(self.build, install.CLI[0]), "w").close()
        install.retire_home_cli(self.home, self.build, self.system)
        self.assertTrue(os.path.exists(self.copy))

    def test_without_the_package_the_home_copy_stays(self):
        os.remove(self.system)
        install.retire_home_cli(self.home, self.build, self.system)
        self.assertTrue(os.path.exists(self.copy))


class UpdateTimer(unittest.TestCase):
    """The login timer that runs `aurora check-updates --notify`: once a day, catching up after the PC was off, with
    the aurora the install found; enabled in the home folder, where the backup lists it."""

    def test_the_timer_runs_the_installed_aurora_daily_and_is_enabled(self):
        with tempfile.TemporaryDirectory() as home:
            install.update_timer(home, "/usr/bin/aurora")
            units = os.path.join(home, ".config/systemd/user")
            service = read(os.path.join(units, "aurora-updates.service"))
            self.assertIn("ExecStart=/usr/bin/aurora check-updates --notify\n", service)
            # The Update button opens Konsole, which must outlive this one-shot service.
            self.assertIn("KillMode=process\n", service)
            timer = read(os.path.join(units, "aurora-updates.timer"))
            self.assertIn("OnCalendar=daily\n", timer)
            self.assertIn("Persistent=true\n", timer)
            wants = os.path.join(units, "timers.target.wants/aurora-updates.timer")
            self.assertEqual(os.readlink(wants), os.path.join(units, "aurora-updates.timer"))
            for path in tree(home, "nothing"):
                if os.path.isfile(os.path.join(home, path)) or os.path.islink(os.path.join(home, path)):
                    self.assertTrue(any(path == a or path.startswith(a + "/") for a in install.ADDED), path)

    def test_a_second_install_leaves_one_enabled_timer(self):
        with tempfile.TemporaryDirectory() as home:
            install.update_timer(home, "/usr/bin/aurora")
            install.update_timer(home, "/home/someone/.local/bin/aurora")
            units = os.path.join(home, ".config/systemd/user")
            self.assertIn("ExecStart=/home/someone/.local/bin/aurora check-updates --notify\n",
                          read(os.path.join(units, "aurora-updates.service")))
            self.assertEqual(os.listdir(os.path.join(units, "timers.target.wants")), ["aurora-updates.timer"])


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


class Animations(unittest.TestCase):
    """How fast the desktop animates comes from `aurora settings` ([look] animations in aurora.conf): smooth by
    default, a step of Plasma's own speed slider (0.75), never the near-instant values that make it feel abrupt."""

    def factor(self, conf):
        with tempfile.TemporaryDirectory() as home:
            if conf is not None:
                os.makedirs(os.path.join(home, ".config/aurora"))
                with open(os.path.join(home, ".config/aurora/aurora.conf"), "w") as f:
                    f.write(conf)
            return install.animation_factor(home)

    def test_a_new_install_animates_at_plasma_s_designed_speed(self):
        # 0.75 left window open and close at 150 ms and the minimize at 187 ms: shorter than macOS, felt abrupt.
        self.assertEqual(self.factor(None), "1")

    def test_the_chosen_speed_is_used(self):
        self.assertEqual(self.factor("[updates]\ncheck=daily\n\n[look]\nanimations=fast\n"), "0.5")
        self.assertEqual(self.factor("[look]\nanimations = instant\n"), "0")

    def test_aurora_motion_takes_over_opening_closing_and_minimizing(self):
        # KWin's Scale closes on InCubic to 80 %, which looks like the window is swallowed; Aurora Motion replaces it
        # and the two minimize effects, which would otherwise animate the same windows twice.
        effects = dict(install.WINDOW_EFFECTS)
        self.assertEqual(effects["org.aurora.motion"], "true")
        for stock in ("scale", "squash", "magiclamp"):
            self.assertEqual(effects[stock], "false", stock)

    def test_aurora_motion_is_installed_where_kwin_finds_it_under_its_own_id(self):
        source, target = install.MOTION
        with open(os.path.join(source, "metadata.json")) as f:
            metadata = json.load(f)
        self.assertEqual(target, f".local/share/kwin/effects/{metadata['KPlugin']['Id']}")
        self.assertEqual(metadata["X-Plasma-API"], "javascript")
        self.assertTrue(os.path.isfile(os.path.join(source, "contents", metadata["X-Plasma-MainScript"])))
        self.assertIn(target, install.ADDED)

    def test_the_speed_is_announced_under_the_name_every_app_listens_to(self):
        # KConfig announces a change under the file's name: with a full path the signal goes to /home/.../kdeglobals,
        # and KWin, Plasma and the apps listen to /kdeglobals (KConfigWatcher), so none of them heard it.
        command = install.speed_command("1")
        self.assertEqual(command[:2], ["kwriteconfig6", "--notify"])
        self.assertEqual(command[command.index("--file") + 1], "kdeglobals")
        self.assertEqual(command[-2:], ["AnimationDurationFactor", "1"])

    def test_window_effects_take_macos_timing(self):
        # KWin 6.8.2 bases: scale 200 ms, magic lamp 250 ms (linear), sliding popups 200 ms. macOS opens a window in
        # about 300 ms and the genie takes about 450 ms; each value is still scaled by the animation speed.
        timings = {(group, key): value for group, key, value in install.EFFECT_TIMINGS}
        self.assertEqual(timings[("Effect-slidingpopups", "SlideInTime")], "250")
        self.assertEqual(timings[("Effect-slidingpopups", "SlideOutTime")], "250")

    # Recorded from `qdbus6 org.kde.KWin /KWin supportInformation`, KWin 6.8.2, 2026-11-05 (Effect Settings part).
    SUPPORT = "Effect Settings:\n----------------\nslidingpopups\n-------------\nslideInDuration: {}\nslideOutDuration: {}\n"

    def test_the_witness_duration_is_read_from_kwin_s_support_information(self):
        self.assertEqual(install.slide_in_duration(self.SUPPORT.format(187, 187)), 187)
        self.assertIsNone(install.slide_in_duration("Loaded Effects:\nblur\n"))

    def test_effects_reload_once_kwin_has_the_new_speed(self):
        # KWin applies a new speed on its own timer (Workspace::reconfigure, 200 ms), so the effects reload only once
        # the witness effect reports SlideInTime x speed; reloading earlier keeps the old timing (seen live: 187 ms).
        answers = iter([self.SUPPORT.format(187, 187), self.SUPPORT.format(250, 250)])
        seen = []

        def output(*cmd):
            if cmd[-1] == "loadedEffects" or cmd[-1].endswith("loadedEffects"):
                return "scale\nmagiclamp\nslidingpopups\n"
            return next(answers)
        with unittest.mock.patch.object(install, "output", output), \
                unittest.mock.patch.object(install, "session", lambda *cmd, check=True: seen.append(cmd)), \
                unittest.mock.patch.object(install.time, "sleep", lambda seconds: None):
            self.assertTrue(install.reload_effect_timing("1"))
        reloads = [cmd[-1] for cmd in seen if cmd[3] == "org.kde.kwin.Effects.reconfigureEffect"]
        self.assertEqual(reloads[-3:], ["scale", "magiclamp", "slidingpopups"], "every effect reloads at the end")

    def test_a_kwin_that_never_reports_the_speed_still_gets_its_effects_reloaded(self):
        seen = []
        with unittest.mock.patch.object(install, "output", lambda *cmd: "scale\n" if "loadedEffects" in cmd[-1] else ""), \
                unittest.mock.patch.object(install, "session", lambda *cmd, check=True: seen.append(cmd)), \
                unittest.mock.patch.object(install.time, "sleep", lambda seconds: None):
            self.assertFalse(install.reload_effect_timing("1"))
        self.assertEqual(seen[-1][-1], "scale")

    def test_an_unknown_speed_or_another_group_keeps_smooth(self):
        self.assertEqual(self.factor("[look]\nanimations=warp\n"), "1")
        self.assertEqual(self.factor("[updates]\nanimations=instant\n"), "1")


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
