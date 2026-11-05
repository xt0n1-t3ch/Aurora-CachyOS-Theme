# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""The logos the menu bar's menu can show: CachyOS (the default), Arch Linux, Apple and Kali Linux.

Each has a symbolic version that takes the menu bar's text color (white on Aurora's dark bar), and CachyOS and Arch
Linux also have one in their own colors. Writes the Arch Linux, Apple and Kali files into themes/icons/apps and
checks that the two CachyOS files are the ones described below.

    python3 -I themes/icons/menu_logos.py

Sources:
- aurora-logo-cachyos-symbolic.svg: "CachyOS symbolic vector logo" by cscs, start-here-cachyos.svg from
  https://gitlab.com/macelinux/icons/standalone (shared for any use on the CachyOS forum,
  https://discuss.cachyos.org/t/cachyos-symbolic-vector-logo/10151), drawn from the official logo. It is the
  default because its black-and-white drawing matches the menu bar (see docs/decisions.md, Menu bar logo).
- aurora-logo-cachyos.svg: CachyOS's official logo by ptr1337 (Wikimedia Commons File:CachyOS_Logo.svg, GPL; the
  same file CachyOS ships as org.cachyos.hello.svg).
- Arch Linux and Apple: Simple Icons 16.36.0 (https://simpleicons.org, CC0-1.0), on a 24-unit grid. The Arch outline
  leaves out the "TM" Simple Icons draws beside it. The marks stay their owners' trademarks.
- Kali's dragon: the one draw_system_icons.py thickens for the menu bar.
"""
import hashlib
import os

APPS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "apps")

ARCH = ("M11.39.605C10.376 3.092 9.764 4.72 8.635 7.132c.693.734 1.543 1.589 2.923 2.554-1.484-.61-2.496-1.224-3.252-1.86"
        "C6.86 10.842 4.596 15.138 0 23.395c3.612-2.085 6.412-3.37 9.021-3.862a6.61 6.61 0 01-.171-1.547l.003-.115"
        "c.058-2.315 1.261-4.095 2.687-3.973 1.426.12 2.534 2.096 2.478 4.409a6.52 6.52 0 01-.146 1.243"
        "c2.58.505 5.352 1.787 8.914 3.844-.702-1.293-1.33-2.459-1.929-3.57-.943-.73-1.926-1.682-3.933-2.713"
        " 1.38.359 2.367.772 3.137 1.234-6.09-11.334-6.582-12.84-8.67-17.74z")
APPLE = ("M12.152 6.896c-.948 0-2.415-1.078-3.96-1.04-2.04.027-3.91 1.183-4.961 3.014-2.117 3.675-.546 9.103 1.519 12.09"
         " 1.013 1.454 2.208 3.09 3.792 3.039 1.52-.065 2.09-.987 3.935-.987 1.831 0 2.35.987 3.96.948 1.637-.026"
         " 2.676-1.48 3.676-2.948 1.156-1.688 1.636-3.325 1.662-3.415-.039-.013-3.182-1.221-3.22-4.857-.026-3.04"
         " 2.48-4.494 2.597-4.559-1.429-2.09-3.623-2.324-4.39-2.376-2-.156-3.675 1.09-4.61 1.09zM15.53 3.83"
         "c.843-1.012 1.4-2.427 1.245-3.83-1.207.052-2.662.805-3.532 1.818-.78.896-1.454 2.338-1.273 3.714"
         " 1.338.104 2.715-.688 3.559-1.701")

# Plasma's convention for symbolic icons: the shape is drawn in currentColor, and the ColorScheme-Text class gives
# it the text color of wherever it is shown (white on the dark menu bar, dark on a light one).
STYLE = '<defs><style id="current-color-scheme" type="text/css">.ColorScheme-Text{color:#fcfcfc}</style></defs>'
VIEWBOX = "-1 -1 26 26"

# The two CachyOS files are kept as published; these are their SHA-1 sums.
CACHYOS_FILES = {
    "aurora-logo-cachyos-symbolic.svg": "6fe74c7d75bfffcfe49571f798da3eded604c2af",
    "aurora-logo-cachyos.svg": "26efb35d088e00b73ce9834fa69438b2b6d909c8",
}


def symbolic(path):
    """A symbolic logo as SVG text: the shape `path` in currentColor, with the style that gives it the text color."""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{VIEWBOX}">{STYLE}'
            f'<path class="ColorScheme-Text" fill="currentColor" d="{path}"/></svg>\n')


def kali_symbolic(text):
    """The Kali dragon `text` with its white fill and stroke switched to the menu bar's text color, like the others."""
    text = text.replace('fill="#ffffff" stroke="#ffffff"',
                        'class="ColorScheme-Text" fill="currentColor" stroke="currentColor"')
    if STYLE not in text:
        text = text.replace(">", ">" + STYLE, 1)
    return text


def main():
    """Writes the generated logos into themes/icons/apps and checks the two CachyOS files against their sums."""
    files = {
        "aurora-logo-arch-symbolic.svg": symbolic(ARCH),
        "aurora-logo-arch.svg": f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{VIEWBOX}"><path fill="#1793d1" d="{ARCH}"/></svg>\n',
        "aurora-logo-apple-symbolic.svg": symbolic(APPLE),
    }
    kali = os.path.join(APPS, "aurora-logo-kali-symbolic.svg")
    with open(kali) as f:
        files["aurora-logo-kali-symbolic.svg"] = kali_symbolic(f.read())
    for name, text in files.items():
        with open(os.path.join(APPS, name), "w") as f:
            f.write(text)
    for name, sha1 in CACHYOS_FILES.items():
        with open(os.path.join(APPS, name), "rb") as f:
            got = hashlib.sha1(f.read()).hexdigest()
        print(f"{name}: {'as published' if got == sha1 else 'CHANGED, sha1 ' + got}")
    print("wrote", ", ".join(files))


if __name__ == "__main__":
    main()
