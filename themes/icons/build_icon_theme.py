# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Build the Aurora icon theme: the base icons (Aurora-base) for the system, and for every other app the
logo its own package ships. The base redraws many app logos, and KDE's icon loader also matches a dash
prefix of a missing name in the base ("claude-desktop" finds the base's Anthropic "claude"), so each
shadowed logo is linked here under its exact name, ahead of the base in the lookup chain.

    build_icon_theme.py THEME_DIR BASE_PARENT_DIR
"""
import collections
import glob
import os
import subprocess
import sys

theme_dir, base_parent = sys.argv[1], sys.argv[2]
# Package groups of KDE software. Their logos keep the base's drawing; only other packages' logos are linked.
KDE_GROUPS = ["kf6", "plasma", "kde-applications", "kde-system", "kde-utilities", "kde-graphics", "kde-multimedia", "kde-network"]
kde_packages = set(subprocess.run(["pacman", "-Qqg", *KDE_GROUPS], capture_output=True, text=True).stdout.split())

# The icon names the base draws: its file names without the extension.
base_names = set()
for theme in ("Aurora-base", "Aurora-base-dark"):
    for f in glob.glob(f"{base_parent}/{theme}/**/*", recursive=True):
        if f.endswith((".svg", ".png")):
            base_names.add(os.path.splitext(os.path.basename(f))[0])

def shadowed(name):
    """Whether the base has an icon for `name` or for a dash-separated prefix of it, as KDE's icon loader matches."""
    probe = name
    while probe:
        if probe in base_names:
            return True
        probe = probe.rsplit("-", 1)[0] if "-" in probe else ""
    return False

# Every system app icon by name: name -> [(size folder such as 48x48 or scalable, path)].
variants = collections.defaultdict(list)
for path in glob.glob("/usr/share/icons/hicolor/*/apps/*"):
    if path.endswith((".png", ".svg")):
        size_dir = path.split("/")[5]
        variants[os.path.splitext(os.path.basename(path))[0]].append((size_dir, path))

# The package that owns each logo, found from its first file.
first_paths = {name: v[0][1] for name, v in variants.items()}
owners = {}
for line in subprocess.run(["pacman", "-Qo", *first_paths.values()], capture_output=True, text=True).stdout.splitlines():
    path, _, rest = line.partition(" is owned by ")
    owners[path] = rest.split()[0]

# Link every size of each shadowed logo under its exact name, ahead of the base in the theme's lookup chain.
linked, dirs = [], set()
for name, entries in sorted(variants.items()):
    if owners.get(first_paths[name]) in kde_packages or not shadowed(name):
        continue
    for size_dir, path in entries:
        target_dir = os.path.join(theme_dir, "apps", size_dir)
        os.makedirs(target_dir, exist_ok=True)
        link = os.path.join(target_dir, os.path.basename(path))
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(path, link)
        dirs.add(size_dir)
    linked.append(name)

def section(size_dir):
    """The index.theme section of the apps folder `size_dir`."""
    if size_dir == "scalable":
        return f"[apps/scalable]\nSize=128\nMinSize=8\nMaxSize=1024\nType=Scalable\nContext=Applications\n"
    if size_dir == "symbolic":
        return f"[apps/symbolic]\nSize=16\nMinSize=8\nMaxSize=512\nType=Scalable\nContext=Applications\n"
    base = size_dir.split("@")[0]
    size = int(base.split("x")[0])
    scale = f"Scale={size_dir.split('@')[1]}\n" if "@" in size_dir else ""
    return f"[apps/{size_dir}]\nSize={size}\n{scale}Type=Threshold\nContext=Applications\n"

# The theme's own system icons (Settings, Trash, Downloads) live in places/scalable and apps/scalable.
os.makedirs(os.path.join(theme_dir, "places", "scalable"), exist_ok=True)
os.makedirs(os.path.join(theme_dir, "apps", "scalable"), exist_ok=True)
dirs.add("scalable")
directories = sorted(f"apps/{d}" for d in dirs) + ["places/scalable"]
with open(os.path.join(theme_dir, "index.theme"), "w") as f:
    f.write("[Icon Theme]\nName=Aurora Dark\nComment=Aurora's icons; every app keeps its own logo\n"
            "Inherits=Aurora-base-dark,Aurora-base,breeze-dark,hicolor\nExample=folder\n")
    f.write(f"Directories={','.join(directories)}\n\n")
    for d in sorted(dirs):
        f.write(section(d) + "\n")
    f.write("[places/scalable]\nSize=128\nMinSize=8\nMaxSize=1024\nType=Scalable\nContext=Places\n")
print(f"linked {len(linked)} app logos: {' '.join(linked)}")
