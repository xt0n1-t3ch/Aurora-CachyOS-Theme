#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Draws the color tag emblems into the Aurora icon theme: Finder's overlapping tag dots, one emblem for every set of
one to three of its seven colors (63 emblems).

File Explorer's tag plugin (components/file-explorer/src/plugins) names them emblem-aurora-tags-<colors>, the colors in
Finder's order, and Dolphin's views draw them on the corner of a tagged file's icon. The colors are macOS's dark system
colors; tests/installer/test_tag_emblems.sh keeps them equal to the File Explorer's (src/core/tags.cpp).

    tag_emblems.py THEME_DIR
"""
import itertools
import pathlib
import sys

# Finder's tags in its order, with macOS's dark system colors.
COLORS = [("Red", "#ff453a"), ("Orange", "#ff9f0a"), ("Yellow", "#ffd60a"), ("Green", "#30d158"), ("Blue", "#0a84ff"),
          ("Purple", "#bf5af2"), ("Gray", "#98989d")]
# The most dots an emblem shows, like Finder's.
MOST = 3
# Each dot's center and radius, by how many dots share the emblem; a dark ring parts overlapping dots.
LAYOUTS = {1: ([8.0], 6.2), 2: ([5.6, 10.4], 5.0), 3: ([4.4, 8.0, 11.6], 4.2)}
RING = "#1c1c1e"

SECTION = "\n[emblems/scalable]\nSize=16\nMinSize=8\nMaxSize=512\nType=Scalable\nContext=Emblems\n"


def emblem(colors):
    """The SVG of one emblem: a dot per (name, color) of `colors`, left to right, each over the one before."""
    centers, radius = LAYOUTS[len(colors)]
    dots = "".join(f'<circle cx="{x}" cy="8" r="{radius}" fill="{color}" stroke="{RING}" stroke-width="1.2"/>'
                   for x, (_, color) in zip(centers, colors))
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">{dots}</svg>\n'


def names():
    """Every emblem as (file name, colors), the colors of each in Finder's order."""
    for count in range(1, MOST + 1):
        for colors in itertools.combinations(COLORS, count):
            yield "emblem-aurora-tags-" + "-".join(name.lower() for name, _ in colors), colors


def install(theme):
    """Writes every emblem under `theme`/emblems/scalable and lists that folder in its index.theme once."""
    folder = theme / "emblems" / "scalable"
    folder.mkdir(parents=True, exist_ok=True)
    count = 0
    for name, colors in names():
        (folder / f"{name}.svg").write_text(emblem(colors))
        count += 1
    index = theme / "index.theme"
    text = index.read_text()
    if "[emblems/scalable]" not in text:
        text = text.replace("Directories=", "Directories=emblems/scalable,", 1) + SECTION
        index.write_text(text)
    return count


if __name__ == "__main__":
    print(f"wrote {install(pathlib.Path(sys.argv[1]))} tag emblems into {sys.argv[1]}")
