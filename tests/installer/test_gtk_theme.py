# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""GTK apps get Aurora's look from themes/gtk/gtk_theme.py: Plasma's cursor, table headings left to their app, and
window button glyphs as crisp as File Explorer's.

    python3 -I tests/installer/test_gtk_theme.py      (changes nothing outside a temp folder)
"""
import importlib.util
import os
import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GTK_THEME = os.path.join(ROOT, "themes/gtk/gtk_theme.py")
spec = importlib.util.spec_from_file_location("install", os.path.join(ROOT, "installer/install.py"))
install = importlib.util.module_from_spec(spec)
spec.loader.exec_module(install)

SVG = "{http://www.w3.org/2000/svg}"


def read(path):
    """Return the text of the file at path."""
    with open(path) as f:
        return f.read()


def install_gtk_theme(home):
    """Run gtk_theme.py into home with an empty icon theme, and return that icon theme's folder."""
    icons = os.path.join(home, "icons")
    os.makedirs(icons)
    with open(os.path.join(icons, "index.theme"), "w") as f:
        f.write("[Icon Theme]\nDirectories=\n")
    subprocess.run([sys.executable, "-I", GTK_THEME, home, install.gtk_release(), icons], check=True,
                   capture_output=True)
    return icons


def shift(group):
    """Return the (x, y) of a group's translate transform, (0, 0) when it has none."""
    match = re.fullmatch(r"translate\(([\d.]+) ([\d.]+)\)", group.get("transform", "translate(0 0)"))
    return float(match[1]), float(match[2])


class Look(unittest.TestCase):
    def test_gtk_apps_use_the_cursor_plasma_uses(self):
        # gtk_theme.py writes its settings when it runs, so its SETTINGS table is read as text.
        self.assertIn(f'"gtk-cursor-theme-name": "{install.CURSORS}",', read(GTK_THEME))

    def test_column_view_headings_keep_the_padding_their_app_gives_them(self):
        # The GTK 4 sheet loads above every app's own stylesheet, so a list rule that also matches a column view's
        # heading row overrides the app; Mission Center's last heading was drawn past its table's edge.
        with tempfile.TemporaryDirectory() as home:
            install_gtk_theme(home)
            sheet = read(os.path.join(home, ".config/gtk-4.0/gtk-Dark.css"))
        self.assertNotRegex(sheet, r"(^|[\s,}])\.rich-list > header")
        self.assertIn(":not(columnview).rich-list > header {", sheet)

    def test_window_button_glyphs_are_centered_on_a_pixel(self):
        # File Explorer and the window decoration center each glyph on a pixel's middle (crisp() in
        # components/file-explorer/src/windowcontrols.cpp and components/window-buttons/src/glass.cpp), so its 1.4 px
        # strokes cover one row of pixels at a scale of 1. GTK centers the 16 px icon on whole pixels, so a glyph
        # centered at (8, 8) straddled two rows and looked dimmer and thicker than File Explorer's.
        with tempfile.TemporaryDirectory() as home:
            icons = install_gtk_theme(home)
            glyphs = {name: ET.parse(os.path.join(icons, "actions/symbolic", f"window-{name}-symbolic.svg")).getroot()
                      for name in ("minimize", "maximize", "restore", "close")}
        shifts = {name: shift(svg.find(SVG + "g")) for name, svg in glyphs.items()}
        self.assertEqual(set(shifts.values()), {shifts["minimize"]}, "every glyph moves alike")
        dx, dy = shifts["minimize"]
        bar = glyphs["minimize"].find(f"{SVG}g/{SVG}rect")
        center = (float(bar.get("x")) + float(bar.get("width")) / 2 + dx,
                  float(bar.get("y")) + float(bar.get("height")) / 2 + dy)
        self.assertEqual(tuple(round(value, 6) for value in center), (8.5, 8.5))
        for stroke in glyphs["close"].iter(SVG + "rect"):
            turn = re.fullmatch(r"rotate\(-?45 ([\d.]+) ([\d.]+)\)", stroke.get("transform"))
            self.assertEqual((float(turn[1]) + dx, float(turn[2]) + dy), (8.5, 8.5))


if __name__ == "__main__":
    unittest.main(verbosity=2)
