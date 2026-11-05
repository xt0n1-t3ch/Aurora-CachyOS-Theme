#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Write the Control Center glyphs: one 24 x 24 SVG per symbol and color.

The glyphs are drawn here in the spirit of SF Symbols (rounded caps and joins, even strokes), so no Apple artwork
is used.

    python3 tools/cc_glyphs.py [--sheet out.png]

Output: package/contents/ui/icons/<color>/<name>.svg. The colors are the macOS dark-appearance system colors the
Control Center uses: white glyphs on glass, blue/indigo/orange on an active white toggle. The contact sheet needs
Pillow and rsvg-convert.
"""
import math
import pathlib
import subprocess
import sys

COLOURS = {
    "light": "#ffffff",
    "dark": "#1d1d1f",
    "blue": "#0a84ff",
    "indigo": "#5e5ce6",
    "orange": "#ff9f0a",
}
STROKE = 'fill="none" stroke="{c}" stroke-linecap="round" stroke-linejoin="round"'


def stroke(d, width=2.0):
    """Return a path element that strokes d with round caps and joins."""
    return f'<path d="{d}" {STROKE} stroke-width="{width}"/>'


def fill(d, rule="nonzero"):
    """Return a path element that fills d. The color is the placeholder {c}, which write_all replaces."""
    return f'<path d="{d}" fill="{{c}}" fill-rule="{rule}"/>'


def circle(cx, cy, r, filled=True, width=2.0):
    """Return a filled circle, or a ring stroked with the given width when filled is false."""
    if filled:
        return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{{c}}"/>'
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" {STROKE} stroke-width="{width}"/>'


def f(v):
    """Format a coordinate with at most three decimals and no trailing zeros."""
    return f"{v:.3f}".rstrip("0").rstrip(".")


def arc(cx, cy, r, a0, a1):
    """Return the path of an arc from angle a0 to a1 in degrees, where 0 is up and angles run clockwise."""
    def point(a):
        t = math.radians(a)
        return cx + r * math.sin(t), cy - r * math.cos(t)
    (x0, y0), (x1, y1) = point(a0), point(a1)
    large = 1 if (a1 - a0) % 360 > 180 else 0
    return f"M{f(x0)} {f(y0)} A{f(r)} {f(r)} 0 {large} 1 {f(x1)} {f(y1)}"


def rounded_rect(x, y, w, h, r):
    """Return the closed path of a rectangle at (x, y), w by h, with corners of radius r."""
    return (f"M{f(x + r)} {f(y)} H{f(x + w - r)} A{f(r)} {f(r)} 0 0 1 {f(x + w)} {f(y + r)} V{f(y + h - r)} "
            f"A{f(r)} {f(r)} 0 0 1 {f(x + w - r)} {f(y + h)} H{f(x + r)} A{f(r)} {f(r)} 0 0 1 {f(x)} {f(y + h - r)} "
            f"V{f(y + r)} A{f(r)} {f(r)} 0 0 1 {f(x + r)} {f(y)} Z")


def rays(cx, cy, r0, r1, count=8, width=1.8, offset=0.0):
    """Return a stroked path of count rays around (cx, cy) from radius r0 to r1, the first at offset degrees."""
    parts = []
    for i in range(count):
        t = math.radians(offset + i * 360 / count)
        parts.append(f"M{f(cx + r0 * math.sin(t))} {f(cy - r0 * math.cos(t))} "
                     f"L{f(cx + r1 * math.sin(t))} {f(cy - r1 * math.cos(t))}")
    return stroke(" ".join(parts), width)


def crescent(cx, cy, r, ox, oy, r2):
    """Return the path of the moon: circle (cx, cy, r) minus circle (ox, oy, r2), as one path."""
    d = math.hypot(ox - cx, oy - cy)
    a = (r * r - r2 * r2 + d * d) / (2 * d)
    h = math.sqrt(max(r * r - a * a, 0))
    ux, uy = (ox - cx) / d, (oy - cy) / d
    mx, my = cx + a * ux, cy + a * uy
    p1 = (mx + h * uy, my - h * ux)
    p2 = (mx - h * uy, my + h * ux)
    return (f"M{f(p1[0])} {f(p1[1])} A{f(r)} {f(r)} 0 1 0 {f(p2[0])} {f(p2[1])} "
            f"A{f(r2)} {f(r2)} 0 0 1 {f(p1[0])} {f(p1[1])} Z")


def speaker():
    """Return the speaker body that the speaker glyphs share."""
    return fill("M3.2 9.4 H6.4 L11 5.3 Q12 4.5 12 5.8 V18.2 Q12 19.5 11 18.7 L6.4 14.6 H3.2 "
                "Q2.4 14.6 2.4 13.8 V10.2 Q2.4 9.4 3.2 9.4 Z")


def gear():
    """Return the gear: an outline of eight teeth around a ring."""
    teeth, outer, inner = 8, 9.6, 7.4
    points = []
    for i in range(teeth):
        base = i * 360 / teeth
        for da, r in ((-11, outer), (11, outer), (17, inner), (360 / teeth - 17, inner)):
            t = math.radians(base + da)
            points.append((12 + r * math.sin(t), 12 - r * math.cos(t)))
    d = "M" + " L".join(f"{f(x)} {f(y)}" for x, y in points) + " Z"
    return stroke(d, 1.8) + circle(12, 12, 3.1, filled=False, width=1.8)


# The SVG body of each glyph by name. "{c}" stands for the color, which write_all fills in.
GLYPHS = {
    # Network: the Wi-Fi fan, and the Ethernet port arrows of macOS's Network settings.
    "wifi": stroke(" ".join(arc(12, 19, r, -45, 45) for r in (5.0, 9.2, 13.4)), 2.4) + circle(12, 18.7, 1.9),
    "ethernet": stroke("M8.2 6.8 L3.6 12 L8.2 17.2 M15.8 6.8 L20.4 12 L15.8 17.2", 2.1)
                + circle(9.2, 12, 1.35) + circle(12, 12, 1.35) + circle(14.8, 12, 1.35),
    "bluetooth": stroke("M6.6 7.6 L17.2 16.4 L12 20.8 V3.2 L17.2 7.6 L6.6 16.4", 2.0),
    "nightlight": stroke("M3 18.4 H21 M7.2 18.4 A4.8 4.8 0 0 1 16.8 18.4", 2.0)
                  + stroke(" ".join(f"M{f(12 + 7.4 * math.sin(math.radians(a)))} {f(18.4 - 7.4 * math.cos(math.radians(a)))} "
                                    f"L{f(12 + 9.8 * math.sin(math.radians(a)))} {f(18.4 - 9.8 * math.cos(math.radians(a)))}"
                                    for a in (-90, -45, 0, 45, 90)), 1.9),
    "moon": fill(crescent(11.2, 12.6, 8.4, 16.2, 7.8, 6.6)),
    # Utility circles: Overview (the Stage Manager slot), display modes (Screen Mirroring), settings, screenshot.
    "overview": fill(rounded_rect(2.6, 6.2, 3.4, 3.2, 1.1)) + fill(rounded_rect(2.6, 10.4, 3.4, 3.2, 1.1))
                + fill(rounded_rect(2.6, 14.6, 3.4, 3.2, 1.1)) + stroke(rounded_rect(8.6, 5.2, 12.8, 13.6, 3.0), 2.0),
    "mirroring": stroke("M7 14.6 H5.6 A2.6 2.6 0 0 1 3 12 V7.4 A2.6 2.6 0 0 1 5.6 4.8 H13.4 "
                        "A2.6 2.6 0 0 1 16 7.4 V8.6", 2.0) + stroke(rounded_rect(7.6, 9.4, 13.4, 9.8, 2.6), 2.0),
    "gear": gear(),
    "screenshot": stroke("M3.2 8.2 V6 A2.8 2.8 0 0 1 6 3.2 H8.2 M15.8 3.2 H18 A2.8 2.8 0 0 1 20.8 6 V8.2 "
                         "M20.8 15.8 V18 A2.8 2.8 0 0 1 18 20.8 H15.8 M8.2 20.8 H6 A2.8 2.8 0 0 1 3.2 18 V15.8", 2.0)
                  + fill(rounded_rect(7.2, 9.0, 9.6, 7.4, 1.8) + " M12 10.6 A2.15 2.15 0 1 0 12 14.9 "
                         "A2.15 2.15 0 1 0 12 10.6 Z", "evenodd")
                  + fill(rounded_rect(9.9, 7.9, 4.2, 1.7, 0.7)),
    # Display and Sound modules.
    "sun.min": circle(12, 12, 3.2) + rays(12, 12, 5.6, 7.2, 8, 1.7),
    "sun.max": circle(12, 12, 4.0) + rays(12, 12, 6.9, 9.6, 8, 2.0),
    "speaker": speaker(),
    "speaker.wave": speaker() + stroke(arc(11.4, 12, 4.6, 45, 135), 1.9)
                    + stroke(arc(11.4, 12, 7.8, 40, 140), 1.9) + stroke(arc(11.4, 12, 11.0, 38, 142), 1.9),
    "speaker.slash": speaker() + stroke("M15.6 9.2 L21 14.8 M21 9.2 L15.6 14.8", 1.9),
    "output": fill("M7.4 21 H16.6 Q17.6 21 17 20.2 L12.8 15 Q12 14.1 11.2 15 L7 20.2 Q6.4 21 7.4 21 Z")
              + stroke(arc(12, 11.4, 8.4, 225, 495), 2.0) + stroke(arc(12, 11.4, 4.6, 230, 490), 2.0),
    # Now Playing.
    "play": fill("M8.4 4.9 L19.2 11.1 Q20.6 12 19.2 12.9 L8.4 19.1 Q6.9 19.9 6.9 18.2 V5.8 Q6.9 4.1 8.4 4.9 Z"),
    "pause": fill(rounded_rect(6.4, 5.0, 3.9, 14.0, 1.3)) + fill(rounded_rect(13.7, 5.0, 3.9, 14.0, 1.3)),
    "forward": fill("M3.6 6.9 L11.2 11.2 Q12.3 12 11.2 12.8 L3.6 17.1 Q2.4 17.8 2.4 16.4 V7.6 Q2.4 6.2 3.6 6.9 Z")
               + fill("M13 6.9 L20.6 11.2 Q21.7 12 20.6 12.8 L13 17.1 Q11.8 17.8 11.8 16.4 V7.6 Q11.8 6.2 13 6.9 Z"),
    "backward": fill("M20.4 6.9 L12.8 11.2 Q11.7 12 12.8 12.8 L20.4 17.1 Q21.6 17.8 21.6 16.4 V7.6 Q21.6 6.2 20.4 6.9 Z")
                + fill("M11 6.9 L3.4 11.2 Q2.3 12 3.4 12.8 L11 17.1 Q12.2 17.8 12.2 16.4 V7.6 Q12.2 6.2 11 6.9 Z"),
    "music": fill("M8.2 6.6 L19.8 4.2 V7.2 L8.2 9.6 Z") + stroke("M9 8 V17.2 M19 5.8 V15", 1.8)
             + '<ellipse cx="6.7" cy="17.6" rx="2.9" ry="2.2" transform="rotate(-18 6.7 17.6)" fill="{c}"/>'
             + '<ellipse cx="16.7" cy="15.4" rx="2.9" ry="2.2" transform="rotate(-18 16.7 15.4)" fill="{c}"/>',
    # Lists in the detail panels.
    "checkmark": stroke("M5.6 12.6 L10 17 L18.6 7.2", 2.3),
    "chevron.right": stroke("M9.4 5.6 L15.8 12 L9.4 18.4", 2.0),
    "headphones": stroke("M4.4 15 V12 A7.6 7.6 0 0 1 19.6 12 V15", 2.0)
                  + fill(rounded_rect(3.4, 13.2, 4.4, 7.0, 1.8)) + fill(rounded_rect(16.2, 13.2, 4.4, 7.0, 1.8)),
    "mouse": stroke(rounded_rect(7, 3.4, 10, 17.2, 5.0), 2.0) + stroke("M12 7 V10", 2.0),
    "keyboard": stroke(rounded_rect(2.6, 6.4, 18.8, 11.2, 2.4), 1.9)
                + "".join(circle(x, 10, 0.95) for x in (6.4, 9.5, 12.6, 15.7, 18.6 - 0.9))
                + stroke("M7.4 14 H16.6", 1.9),
    "phone": stroke(rounded_rect(7, 2.8, 10, 18.4, 2.6), 2.0) + stroke("M10.6 5.8 H13.4", 1.8),
    "device": stroke(rounded_rect(4, 4, 16, 16, 4), 2.0) + circle(12, 12, 2.2),
    "speaker.device": stroke(rounded_rect(5.6, 2.8, 12.8, 18.4, 3), 2.0) + circle(12, 14.2, 3.0, filled=False, width=1.8)
                      + circle(12, 7.2, 1.2),
}


def write_all(root):
    """Write every glyph in every color to root/<color>/<name>.svg."""
    for colour, value in COLOURS.items():
        folder = root / colour
        folder.mkdir(parents=True, exist_ok=True)
        for name, body in GLYPHS.items():
            svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">'
                   + body.replace("{c}", value) + "</svg>\n")
            (folder / f"{name}.svg").write_text(svg)


def sheet(root, out):
    """Save a contact sheet of the light glyphs on glass grey, at 24 and 48 px, to check the drawing by eye."""
    import tempfile
    from PIL import Image
    names = list(GLYPHS)
    cell = 64
    image = Image.new("RGBA", (cell * 8, cell * ((len(names) + 7) // 8) * 2), (58, 58, 64, 255))
    with tempfile.TemporaryDirectory() as tmp:
        for i, name in enumerate(names):
            for row, size in ((0, 24), (1, 48)):
                png = pathlib.Path(tmp) / f"{name}-{size}.png"
                subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(png),
                                str(root / "light" / f"{name}.svg")], check=True)
                glyph = Image.open(png)
                x = (i % 8) * cell + (cell - size) // 2
                y = ((i // 8) * 2 + row) * cell + (cell - size) // 2
                image.alpha_composite(glyph, (x, y))
    image.save(out)


if __name__ == "__main__":
    here = pathlib.Path(__file__).resolve().parent.parent
    icons = here / "package" / "contents" / "ui" / "icons"
    write_all(icons)
    if "--sheet" in sys.argv:
        sheet(icons, sys.argv[sys.argv.index("--sheet") + 1])
    print(f"wrote {len(GLYPHS)} glyphs x {len(COLOURS)} colours to {icons}")
