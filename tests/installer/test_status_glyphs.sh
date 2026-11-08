#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Checks themes/icons/status_glyphs.py: it writes the menu bar's 19 symbolic icons with filled shapes only, since GTK 4
# fills every shape of a symbolic icon and drops strokes, and GTK draws the magnifier as a ring, not a dot. The GTK
# check runs when PyGObject can open the session's display, and says when it is skipped.
#
#   tests/installer/test_status_glyphs.sh
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
pass=0
fail=0
# Prints PASS or FAIL for a check. Arguments: the check's name, then the command that must succeed.
check() {
    local name=$1
    shift
    if "$@"; then
        echo "PASS: $name"
        pass=$((pass + 1))
    else
        echo "FAIL: $name"
        fail=$((fail + 1))
    fi
}

theme=$(mktemp -d)
trap 'rm -rf "$theme"' EXIT
printf '[Icon Theme]\nName=Test\nDirectories=\n' > "$theme/index.theme"
python3 -I "$ROOT/themes/icons/status_glyphs.py" "$theme" > /dev/null
search="$theme/actions/symbolic/system-search-symbolic.svg"

check "it writes the 19 icons" test "$(find "$theme" -name "*-symbolic.svg" | wc -l)" -eq 19
check "no icon draws with a stroke" test -z "$(grep -l -E 'stroke-width|stroke="currentColor"' -r "$theme" --include='*.svg')"
check "every icon parses as XML" python3 -I -c '
import sys, pathlib, xml.etree.ElementTree as ET
for path in pathlib.Path(sys.argv[1]).rglob("*.svg"):
    ET.parse(path)' "$theme"

# GTK's own symbolic rendering of the magnifier: the middle of the lens is empty and the handle has ink.
gtk=$(python3 -I - "$search" 2>/dev/null <<'EOF'
import sys
import gi
gi.require_version("Gtk", "4.0")
gi.require_version("Gsk", "4.0")
from gi.repository import Gdk, Gio, Graphene, Gsk, Gtk
if not Gtk.init_check():
    print("skip")
    sys.exit()
paintable = Gtk.IconPaintable.new_for_file(Gio.File.new_for_path(sys.argv[1]), 64, 1)
snapshot = Gtk.Snapshot()
white = Gdk.RGBA()
white.parse("white")
paintable.snapshot_symbolic(snapshot, 64, 64, [white] * 4)
renderer = Gsk.CairoRenderer()
renderer.realize_for_display(Gdk.Display.get_default())
texture = renderer.render_texture(snapshot.to_node(), Graphene.Rect().init(0, 0, 64, 64))
renderer.unrealize()
pixels = texture.save_to_png_bytes().get_data()
from io import BytesIO
from PIL import Image
image = Image.open(BytesIO(pixels)).convert("RGBA")
# The lens centre is (6.8, 6.8) and the handle ends at (14, 14) in 16 px units; 64 px is 4 to 1.
print("ring" if image.getpixel((27, 27))[3] < 40 and image.getpixel((50, 50))[3] > 200 else "dot")
EOF
)
case "$gtk" in
    ring|dot) check "GTK draws the magnifier as a ring with a handle" test "$gtk" = ring ;;
    *) echo "SKIP: GTK cannot open a display here" ;;
esac

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
