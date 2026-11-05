#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Draws the menu bar's status icons in the spirit of SF Symbols into the Aurora icon theme.

They have even 1.5 px strokes, rounded caps and a filled speaker and bell, so the bar does not mix Breeze's thin
16 px pixel art with the base icons'. No Apple artwork: every path is drawn here.

The icons are symbolic: Plasma recolours ColorScheme-Text to the panel's text colour. Writes status/symbolic and
actions/symbolic (the Quick Menu magnifier) and adds both folders to index.theme once.

    status_glyphs.py THEME_DIR [--sheet out.png]

--sheet also renders every icon at 16 and 32 px into one PNG, to review them by eye (needs rsvg-convert and Pillow).
"""
import math
import pathlib
import subprocess
import sys

# Plasma replaces the color of the ColorScheme-Text class with the panel's text color; white is the default.
STYLE = ('<defs><style id="current-color-scheme" type="text/css">.ColorScheme-Text { color:#ffffff; }</style></defs>')


def svg(body):
    """An icon file: `body` inside a 16 px symbolic SVG whose color follows the panel's text."""
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">' + STYLE
            + '<g class="ColorScheme-Text" fill="currentColor" stroke="currentColor">' + body + "</g></svg>\n")


def line(d, width=1.5):
    """A stroked path `d` with rounded caps and joins."""
    return f'<path d="{d}" fill="none" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>'


def shape(d, rule="nonzero"):
    """A filled path `d` without a stroke; `rule` is its SVG fill rule."""
    return f'<path d="{d}" stroke="none" fill-rule="{rule}"/>'


def f(v):
    """A number with at most three decimals and no trailing zeros."""
    return f"{v:.3f}".rstrip("0").rstrip(".")


def arc(cx, cy, r, a0, a1):
    """Arc from angle a0 to a1 in degrees (0 = up, clockwise)."""
    def p(a):
        t = math.radians(a)
        return cx + r * math.sin(t), cy - r * math.cos(t)
    (x0, y0), (x1, y1) = p(a0), p(a1)
    large = 1 if (a1 - a0) % 360 > 180 else 0
    return f"M{f(x0)} {f(y0)} A{f(r)} {f(r)} 0 {large} 1 {f(x1)} {f(y1)}"


SPEAKER = shape("M1.9 5.9 H4 L7.1 3.2 Q7.9 2.6 7.9 3.6 V12.4 Q7.9 13.4 7.1 12.8 L4 10.1 H1.9 "
                "Q1.2 10.1 1.2 9.4 V6.6 Q1.2 5.9 1.9 5.9 Z")


def waves(count):
    """The sound waves beside the speaker: `count` arcs, the inner one first."""
    return "".join(line(arc(8.1, 8, r, 50, 130), 1.4) for r in (2.6, 4.8, 7.0)[:count])


def ethernet(limited=False):
    """The wired network icon: two angle brackets around three dots."""
    body = line("M5.2 4.4 L1.9 8 L5.2 11.6 M10.8 4.4 L14.1 8 L10.8 11.6", 1.5)
    body += "".join(f'<circle cx="{x}" cy="8" r="0.95" stroke="none"/>' for x in (5.9, 8, 10.1))
    return body


def play_circle(inner):
    """A circle with `inner` drawn in it, the frame of the media state icons."""
    return f'<circle cx="8" cy="8" r="6.5" fill="none" stroke-width="1.4"/>' + inner


BELL = shape("M8 1.6 C5.6 1.6 4.2 3.5 4.2 5.8 V8.6 L2.9 10.6 Q2.5 11.4 3.4 11.4 H12.6 Q13.5 11.4 13.1 10.6 "
             "L11.8 8.6 V5.8 C11.8 3.5 10.4 1.6 8 1.6 Z") + shape("M6.2 12.4 H9.8 A1.8 1.8 0 0 1 6.2 12.4 Z")

# The icons by folder: icon name -> SVG body.
ICONS = {
    "status": {
        "audio-volume-high-symbolic": SPEAKER + waves(3),
        "audio-volume-high-warning-symbolic": SPEAKER + waves(3),
        "audio-volume-high-danger-symbolic": SPEAKER + waves(3),
        "audio-volume-medium-symbolic": SPEAKER + waves(2),
        "audio-volume-low-symbolic": SPEAKER + waves(1),
        "audio-volume-muted-symbolic": SPEAKER + line("M10.4 5.9 L14.6 10.1 M14.6 5.9 L10.4 10.1", 1.4),
        "network-wired-activated-symbolic": ethernet(),
        "network-wired-activated-limited-symbolic": ethernet(),
        "network-wired-symbolic": ethernet(),
        "network-wired-disconnected-symbolic": ethernet() + line("M2.6 2.6 L13.4 13.4", 1.4),
        "media-playback-playing-symbolic": play_circle(shape("M6.5 5.2 L10.9 7.6 Q11.5 8 10.9 8.4 L6.5 10.8 Q5.9 11.1 5.9 10.4 V5.6 Q5.9 4.9 6.5 5.2 Z")),
        "media-playback-paused-symbolic": play_circle(shape("M5.5 5 H7.1 V11 H5.5 Z M8.9 5 H10.5 V11 H8.9 Z")),
        "media-playback-stopped-symbolic": play_circle(shape("M5.6 5.6 H10.4 V10.4 H5.6 Z")),
        "notification-active-symbolic": BELL + '<circle cx="12.6" cy="3.4" r="2.2" stroke="none"/>',
        "notification-inactive-symbolic": BELL,
        "notifications-symbolic": BELL,
        "notification-disabled-symbolic": BELL + line("M2.2 2.2 L13.8 13.8", 1.4),
        "notifications-disabled-symbolic": BELL + line("M2.2 2.2 L13.8 13.8", 1.4),
    },
    "actions": {
        "system-search-symbolic": '<circle cx="6.8" cy="6.8" r="4.6" fill="none" stroke-width="1.6"/>'
                                  + line("M10.3 10.3 L14 14", 1.8),
    },
}

# Template of the index.theme section for a symbolic icon folder.
SECTION = "\n[{context}/symbolic]\nSize=16\nMinSize=8\nMaxSize=512\nType=Scalable\nContext={title}\n"


def install(theme):
    """Writes every icon of ICONS under the `theme` folder and lists the symbolic folders in its index.theme once."""
    for context, icons in ICONS.items():
        folder = theme / context / "symbolic"
        folder.mkdir(parents=True, exist_ok=True)
        for name, body in icons.items():
            (folder / f"{name}.svg").write_text(svg(body))
    index = theme / "index.theme"
    text = index.read_text()
    for context, title in (("status", "Status"), ("actions", "Actions")):
        if f"[{context}/symbolic]" not in text:
            text = text.replace("Directories=", f"Directories={context}/symbolic,", 1)
            text += SECTION.format(context=context, title=title)
    index.write_text(text)


def sheet(theme, out):
    """Renders every icon of `theme` at 16 and 32 px into one PNG at `out`."""
    import tempfile
    from PIL import Image
    names = [(c, n) for c, icons in ICONS.items() for n in icons]
    image = Image.new("RGBA", (len(names) * 40, 80), (40, 44, 52, 255))
    with tempfile.TemporaryDirectory() as tmp:
        for i, (context, name) in enumerate(names):
            for row, size in ((0, 16), (1, 32)):
                png = pathlib.Path(tmp) / f"{name}-{size}.png"
                subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(png),
                                str(theme / context / "symbolic" / f"{name}.svg")], check=True)
                image.alpha_composite(Image.open(png), (i * 40 + (40 - size) // 2, row * 40 + (40 - size) // 2))
    image.save(out)


if __name__ == "__main__":
    target = pathlib.Path(sys.argv[1])
    install(target)
    if "--sheet" in sys.argv:
        sheet(target, sys.argv[sys.argv.index("--sheet") + 1])
    print(f"wrote {sum(len(v) for v in ICONS.values())} status icons into {target}")
