# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Draws Aurora's system icons: Settings, Downloads and Trash (empty and full), and the Kali dragon when given.

They use only paths and gradients, because QtSvg (Plasma's icon renderer) ignores filters, clip paths and nested
<svg>.

    draw_system_icons.py OUT_DIR [KALI_SVG]
"""
import math
import os
import sys

OUT = sys.argv[1]
NS = 'xmlns="http://www.w3.org/2000/svg"'


def squircle(x, y, s, r=0.2237):
    """The SVG path of a rounded square of side `s` at (`x`, `y`); `r` is the corner size as a fraction of `s`."""
    k = s * r
    c = k * 0.553
    return (f"M{x+k},{y} H{x+s-k} C{x+s-k+c},{y} {x+s},{y+k-c} {x+s},{y+k} V{y+s-k} "
            f"C{x+s},{y+s-k+c} {x+s-k+c},{y+s} {x+s-k},{y+s} H{x+k} C{x+k-c},{y+s} {x},{y+s-k+c} {x},{y+s-k} "
            f"V{y+k} C{x},{y+k-c} {x+k-c},{y} {x+k},{y} Z")


def gear(cx, cy, r_base, r_tip, teeth, base_half, tip_half):
    """A gear outline: straight-flanked teeth joined by arcs of the base circle."""
    parts = []
    for i in range(teeth):
        a = 2 * math.pi * i / teeth - math.pi / 2
        pts = [(r_base, a - base_half), (r_tip, a - tip_half), (r_tip, a + tip_half), (r_base, a + base_half)]
        xy = [(cx + r * math.cos(t), cy + r * math.sin(t)) for r, t in pts]
        parts.append(("M" if i == 0 else "L") + f"{xy[0][0]:.1f},{xy[0][1]:.1f}")
        parts.append(f"L{xy[1][0]:.1f},{xy[1][1]:.1f} L{xy[2][0]:.1f},{xy[2][1]:.1f} L{xy[3][0]:.1f},{xy[3][1]:.1f}")
        nxt = 2 * math.pi * (i + 1) / teeth - math.pi / 2 - base_half
        parts.append(f"A{r_base},{r_base} 0 0 1 {cx + r_base * math.cos(nxt):.1f},{cy + r_base * math.sin(nxt):.1f}")
    return " ".join(parts) + " Z"


def settings():
    """The Settings icon: a gear on a dark rounded square, as SVG text."""
    sq = squircle(100, 100, 824)
    g = gear(512, 520, 268, 322, 12, math.radians(11.5), math.radians(7.5))
    return f'''<svg {NS} viewBox="0 0 1024 1024">
 <defs>
  <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4c4c54"/><stop offset=".55" stop-color="#232328"/><stop offset="1" stop-color="#141417"/></linearGradient>
  <radialGradient id="glow" cx="512" cy="530" r="430" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#2f6bff" stop-opacity=".62"/><stop offset=".45" stop-color="#5e5ce6" stop-opacity=".22"/><stop offset="1" stop-color="#5e5ce6" stop-opacity="0"/></radialGradient>
  <linearGradient id="sheen" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".2"/><stop offset=".42" stop-color="#fff" stop-opacity="0"/></linearGradient>
  <radialGradient id="drop" cx="512" cy="560" r="360" gradientUnits="userSpaceOnUse"><stop offset=".72" stop-color="#000" stop-opacity=".5"/><stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient>
  <linearGradient id="metal" x1="300" y1="200" x2="740" y2="860" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ffffff"/><stop offset=".45" stop-color="#dcdce2"/><stop offset="1" stop-color="#8e8e96"/></linearGradient>
  <linearGradient id="well" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#16161a"/><stop offset="1" stop-color="#3c3c43"/></linearGradient>
  <linearGradient id="ring" x1="380" y1="390" x2="650" y2="660" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#7fdcff"/><stop offset=".5" stop-color="#0a84ff"/><stop offset="1" stop-color="#6e5bff"/></linearGradient>
  <linearGradient id="hub" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#a5a5ad"/></linearGradient>
 </defs>
 <path d="{sq}" fill="url(#bg)"/>
 <path d="{sq}" fill="url(#glow)"/>
 <path d="{sq}" fill="url(#sheen)"/>
 <circle cx="512" cy="548" r="360" fill="url(#drop)"/>
 <path d="{g}" fill="url(#metal)" stroke="#ffffff" stroke-opacity=".7" stroke-width="3" stroke-linejoin="round"/>
 <circle cx="512" cy="520" r="196" fill="url(#well)" stroke="#000" stroke-opacity=".28" stroke-width="5"/>
 <circle cx="512" cy="520" r="132" fill="none" stroke="url(#ring)" stroke-width="58" stroke-opacity=".22"/>
 <circle cx="512" cy="520" r="132" fill="none" stroke="url(#ring)" stroke-width="30"/>
 <circle cx="512" cy="520" r="72" fill="url(#hub)"/>
 <circle cx="512" cy="520" r="24" fill="#2c2c30"/>
 <path d="{squircle(101.5, 101.5, 821)}" fill="none" stroke="#fff" stroke-opacity=".16" stroke-width="3"/>
</svg>'''


def downloads():
    """The Downloads icon: a folder with a download arrow badge, as SVG text."""
    return f'''<svg {NS} viewBox="0 0 1024 1024">
 <defs>
  <radialGradient id="ground" cx="512" cy="852" r="380" gradientTransform="matrix(1 0 0 .12 0 750)" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#000" stop-opacity=".45"/><stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient>
  <linearGradient id="back" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f0b429"/><stop offset="1" stop-color="#c98a0e"/></linearGradient>
  <linearGradient id="front" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffdd6e"/><stop offset=".6" stop-color="#fcc443"/><stop offset="1" stop-color="#f2ac1f"/></linearGradient>
  <linearGradient id="lip" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#fff" stop-opacity=".75"/><stop offset="1" stop-color="#fff" stop-opacity=".1"/></linearGradient>
  <linearGradient id="badge" x1="600" y1="560" x2="860" y2="880" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#5ac8ff"/><stop offset=".5" stop-color="#0a84ff"/><stop offset="1" stop-color="#0057d9"/></linearGradient>
  <linearGradient id="badgeSheen" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".45"/><stop offset=".5" stop-color="#fff" stop-opacity="0"/></linearGradient>
 </defs>
 <ellipse cx="512" cy="852" rx="380" ry="46" fill="url(#ground)"/>
 <path d="M150,270 Q150,214 206,214 H404 Q436,214 456,238 L494,284 H818 Q874,284 874,340 V760 H150 Z" fill="url(#back)"/>
 <path d="M150,382 Q150,334 198,334 H826 Q874,334 874,382 V782 Q874,838 818,838 H206 Q150,838 150,782 Z" fill="url(#front)"/>
 <path d="M188,348 H836" stroke="url(#lip)" stroke-width="10" stroke-linecap="round"/>
 <circle cx="742" cy="742" r="168" fill="#1c1c1e" fill-opacity=".35"/>
 <circle cx="736" cy="734" r="156" fill="url(#badge)" stroke="#ffffff" stroke-width="12"/>
 <path d="M592,700 A146,146 0 0 1 880,700 Q736,660 592,700 Z" fill="url(#badgeSheen)"/>
 <path d="M736,640 V812 M670,748 L736,814 L802,748" fill="none" stroke="#fff" stroke-width="34" stroke-linecap="round" stroke-linejoin="round"/>
</svg>'''


def trash(full):
    """The Trash icon, a glass bin as SVG text; with `full` it holds paper."""
    ribs = []
    for i in range(1, 7):
        t = i / 7
        xt, xb = 262 + t * 500, 300 + t * 424
        ribs.append(f'<path d="M{xt:.1f},392 L{xb:.1f},858" stroke="#fff" stroke-opacity=".34" stroke-width="15" stroke-linecap="round"/>')
        ribs.append(f'<path d="M{xt+9:.1f},396 L{xb+8:.1f},858" stroke="#0b1a2e" stroke-opacity=".10" stroke-width="7" stroke-linecap="round"/>')
    paper = ""
    if full:
        paper = '''
 <path d="M300,330 C290,250 380,210 440,236 C470,190 560,196 580,250 C640,214 730,240 724,330 Z" fill="url(#paper)"/>
 <path d="M346,300 C380,262 420,288 452,262 M520,250 C548,286 600,262 640,292 M470,318 C500,296 540,318 566,300" fill="none" stroke="#8e8e96" stroke-opacity=".55" stroke-width="7" stroke-linecap="round"/>
 <ellipse cx="420" cy="560" rx="96" ry="78" fill="#fff" fill-opacity=".22"/>
 <ellipse cx="600" cy="640" rx="112" ry="84" fill="#fff" fill-opacity=".18"/>
 <ellipse cx="470" cy="760" rx="120" ry="70" fill="#fff" fill-opacity=".16"/>'''
    return f'''<svg {NS} viewBox="0 0 1024 1024">
 <defs>
  <radialGradient id="ground" cx="512" cy="924" r="330" gradientTransform="matrix(1 0 0 .13 0 804)" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#000" stop-opacity=".5"/><stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient>
  <linearGradient id="glass" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e8f3ff" stop-opacity=".46"/><stop offset=".55" stop-color="#bcd8ff" stop-opacity=".2"/><stop offset="1" stop-color="#e8f3ff" stop-opacity=".34"/></linearGradient>
  <linearGradient id="gloss" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#fff" stop-opacity=".55"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>
  <linearGradient id="rim" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".92"/><stop offset="1" stop-color="#d6e6ff" stop-opacity=".5"/></linearGradient>
  <linearGradient id="paper" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#c7c7cc"/></linearGradient>
 </defs>
 <ellipse cx="512" cy="924" rx="330" ry="40" fill="url(#ground)"/>{paper}
 <path d="M256,352 H768 L730,866 Q726,912 680,912 H344 Q298,912 294,866 Z" fill="url(#glass)" stroke="#fff" stroke-opacity=".62" stroke-width="7" stroke-linejoin="round"/>
 {"".join(ribs)}
 <path d="M286,384 L318,880" stroke="url(#gloss)" stroke-width="26" stroke-linecap="round"/>
 <rect x="226" y="300" width="572" height="64" rx="32" fill="url(#rim)" stroke="#fff" stroke-opacity=".95" stroke-width="4"/>
 <rect x="256" y="318" width="512" height="30" rx="15" fill="#0b1626" fill-opacity=".32"/>
</svg>'''


def kali_dragon(source_svg):
    """The Kali dragon for the menu bar, with its strokes thickened so it reads at 18 px, in the menu bar's text
    color (Plasma's ColorScheme-Text class, as menu_logos.py draws the other logos)."""
    text = open(source_svg).read()
    text = text.replace('<path fill="#ffffff"', '<path class="ColorScheme-Text" fill="currentColor" stroke="currentColor" '
                        'stroke-width="9" stroke-linejoin="round"', 1)
    style = '<defs><style id="current-color-scheme" type="text/css">.ColorScheme-Text{color:#fcfcfc}</style></defs>'
    return text.replace(">", ">" + style, 1)


os.makedirs(OUT, exist_ok=True)
files = {"settings.svg": settings(), "downloads.svg": downloads(),
         "user-trash.svg": trash(False), "user-trash-full.svg": trash(True)}
if len(sys.argv) > 2:
    files["aurora-logo-kali-symbolic.svg"] = kali_dragon(sys.argv[2])
for name, svg in files.items():
    with open(os.path.join(OUT, name), "w") as f:
        f.write(svg)
print("wrote", ", ".join(files))
