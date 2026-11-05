# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Draws Aurora's icon: a macOS squircle with an aurora over Lake Tahoe's mountains.

Writes assets/logo.svg and assets/logo.png (512 px, rsvg-convert). The README shows it above the name, which GitHub
draws as text.

    python3 -I assets/make_art.py
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))


def squircle(x, y, s, r=0.2237):
    """The SVG path of a rounded square of side `s` at (`x`, `y`); `r` is the corner size as a fraction of `s`."""
    k = s * r
    c = k * 0.553
    return (f"M{x+k:.2f},{y:.2f} H{x+s-k:.2f} C{x+s-k+c:.2f},{y:.2f} {x+s:.2f},{y+k-c:.2f} {x+s:.2f},{y+k:.2f} "
            f"V{y+s-k:.2f} C{x+s:.2f},{y+s-k+c:.2f} {x+s-k+c:.2f},{y+s:.2f} {x+s-k:.2f},{y+s:.2f} H{x+k:.2f} "
            f"C{x+k-c:.2f},{y+s:.2f} {x:.2f},{y+s-k+c:.2f} {x:.2f},{y+s-k:.2f} V{y+k:.2f} C{x:.2f},{y+k-c:.2f} "
            f"{x+k-c:.2f},{y:.2f} {x+k:.2f},{y:.2f} Z")


def logo():
    """The icon as SVG text, on a 140 unit square."""
    shape = squircle(4, 4, 132)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="140" height="140" viewBox="0 0 140 140">
  <defs>
    <clipPath id="c"><path d="{shape}"/></clipPath>
    <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#070b26"/><stop offset="0.55" stop-color="#1b1f5e"/><stop offset="0.72" stop-color="#4b3a8f"/>
    </linearGradient>
    <linearGradient id="ribbon1" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#4ff2b0" stop-opacity="0"/><stop offset="0.3" stop-color="#4ff2b0"/>
      <stop offset="0.65" stop-color="#38bdf8"/><stop offset="1" stop-color="#a78bfa" stop-opacity="0.2"/>
    </linearGradient>
    <linearGradient id="ribbon2" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#a78bfa" stop-opacity="0.1"/><stop offset="0.5" stop-color="#c084fc"/>
      <stop offset="1" stop-color="#f472b6" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="far" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3a3488"/><stop offset="1" stop-color="#1d1a52"/></linearGradient>
    <linearGradient id="near" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#14123d"/><stop offset="1" stop-color="#090822"/></linearGradient>
    <linearGradient id="lake" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2b5fd9"/><stop offset="1" stop-color="#0b1240"/></linearGradient>
    <linearGradient id="shine" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff" stop-opacity="0.22"/><stop offset="0.45" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
    <filter id="soft" x="-20%" y="-50%" width="140%" height="200%"><feGaussianBlur stdDeviation="2.6"/></filter>
  </defs>
  <g clip-path="url(#c)">
    <rect width="140" height="140" fill="url(#sky)"/>
    <g fill="#ffffff" opacity="0.8"><circle cx="24" cy="18" r="0.9"/><circle cx="58" cy="12" r="0.7"/><circle cx="112" cy="20" r="1"/><circle cx="92" cy="9" r="0.6"/><circle cx="16" cy="40" r="0.6"/></g>
    <path d="M-6,62 C20,30 44,58 70,36 C92,18 112,40 146,22 L146,40 C112,58 92,36 70,54 C44,76 20,48 -6,80 Z" fill="url(#ribbon1)" opacity="0.85" filter="url(#soft)"/>
    <path d="M-6,46 C24,24 52,46 80,28 C102,14 124,30 146,16 L146,26 C124,40 102,24 80,38 C52,56 24,34 -6,56 Z" fill="url(#ribbon2)" opacity="0.7" filter="url(#soft)"/>
    <path d="M0,88 L22,64 L36,76 L58,50 L80,74 L96,60 L118,82 L140,70 L140,100 L0,100 Z" fill="url(#far)"/>
    <path d="M0,98 L18,84 L34,94 L54,78 L76,96 L100,82 L122,96 L140,90 L140,104 L0,104 Z" fill="url(#near)"/>
    <rect y="102" width="140" height="38" fill="url(#lake)"/>
    <path d="M14,112 C40,108 54,116 80,111 S118,108 128,112" fill="none" stroke="#4ff2b0" stroke-opacity="0.45" stroke-width="2.2" stroke-linecap="round" filter="url(#soft)"/>
    <path d="M34,122 H70 M84,122 H108" stroke="#93c5fd" stroke-opacity="0.5" stroke-width="2" stroke-linecap="round"/>
    <path d="{shape}" fill="url(#shine)"/>
  </g>
  <path d="{shape}" fill="none" stroke="#ffffff" stroke-opacity="0.22" stroke-width="1.5"/>
</svg>
'''


def main():
    """Writes logo.svg beside this script and renders logo.png from it."""
    svg = os.path.join(HERE, "logo.svg")
    with open(svg, "w") as f:
        f.write(logo())
    subprocess.run(["rsvg-convert", "-w", "512", svg, "-o", os.path.join(HERE, "logo.png")], check=True)
    print("logo.svg, logo.png")


if __name__ == "__main__":
    main()
