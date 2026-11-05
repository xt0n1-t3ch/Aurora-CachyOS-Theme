# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""Draws the README's cover, section headings and install steps, and the social preview GitHub shows when someone shares
the repository's link, all on Dawn Bird, Aurora's own wallpaper and the one in every Aurora picture.

Each picture is a 2x JPEG of a slice of the wallpaper, darkened until white text on it reaches a contrast of at least
7:1 (WCAG's enhanced level), with the text set in Inter (the inter-font package). It is wrapped in an SVG that rounds its corners, so the
pictures keep their shape on GitHub's light and dark pages alike. Run it after changing a heading's words:

    python3 -I assets/readme/make.py
"""
import base64
import io
import os
import subprocess
from xml.sax.saxutils import escape

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

# Sizes in CSS pixels; the JPEGs are drawn at twice that for sharp text on high-density screens.
WIDTH = 880
SCALE = 2
RADIUS = 18
CONTRAST = 7.0
# GitHub's recommended social preview size, in real pixels.
PREVIEW = (1280, 640)
WHITE = (255, 255, 255)
# Dawn Bird's own colors: the glass wings' ice and the light trail's ember, for the secondary text and the steps' path.
ICE = (222, 226, 255)
EMBER = (246, 160, 82)

# Each section's title, the line under it, and where on the wallpaper its slice comes from (fractions of the width and
# height, the slice's centre), so neighbouring headings show different parts of the image.
HEADINGS = {
    "overview": ("Overview", "What Aurora is and what it changes on your desktop", (0.62, 0.30)),
    "showcase": ("Showcase", "The desktop, Quick Menu, Control Center, the menu and notifications", (0.35, 0.75)),
    "features": ("Features", "Every part, what it does and where its code lives", (0.78, 0.18)),
    "requirements": ("Requirements", "What your system needs first", (0.20, 0.40)),
    "installation": ("Installation", "One command, and the guide does the rest", (0.55, 0.62)),
    "keyboard-shortcuts": ("Keyboard shortcuts", "The keys you know from Windows", (0.70, 0.45)),
    "updating": ("Updating", "New releases, and rebuilding after a Plasma update", (0.30, 0.20)),
    "performance-tuning": ("Performance tuning", "Optional scheduler, memory and zram settings, all measured", (0.82, 0.70)),
    "dependencies": ("Dependencies", "The projects and packages Aurora is built on", (0.45, 0.85)),
    "removal": ("Removal", "Putting your desktop back the way it was", (0.15, 0.80)),
    "project-structure": ("Project structure", "Where everything lives in this repository", (0.66, 0.08)),
    "development": ("Development", "Building, testing and the sandbox", (0.40, 0.48)),
    "faq": ("FAQ", "Questions people ask", (0.88, 0.35)),
    "license": ("License", "Who owns what", (0.25, 0.60)),
}

STEPS = [
    ("Packages", "Installs what your system lacks with pacman, from installer/dependencies.txt."),
    ("Themes", "Checks Aurora's own themes in themes/: nothing is downloaded."),
    ("Build", "Uses the parts the package already built; builds them again only for a newer Plasma."),
    ("Backup", "Saves every setting it is about to change, then copies everything into your home folder."),
    ("System", "Links the window buttons and the pointing hand where KWin finds them."),
    ("Look", "Applies the colors, fonts, icons, cursor and keys, then puts up the menu bar and the Dock."),
]


def font(face, size):
    """Inter `face` (for example "Display-Bold": Inter Display, bold) at `size` pixels, laid out with kerning (Raqm).
    fontconfig says which file and which face of the collection holds it."""
    optical, weight = face.split("-")
    family = "Inter Display" if optical == "Display" else "Inter"
    style = weight.replace("Semibold", "SemiBold")
    found = subprocess.run(["fc-match", "-f", "%{file}\n%{index}\n%{family[0]}\n%{style[0]}",
                            f"{family}:style={style}"], capture_output=True, text=True, check=True).stdout.split("\n")
    if found[2:4] != [family, style]:
        raise SystemExit(f"{family} {style} is not installed (package inter-font)")
    return ImageFont.truetype(found[0], size, index=int(found[1]), layout_engine=ImageFont.Layout.RAQM)


def dawn_bird():
    """Dawn Bird, Aurora's own wallpaper, at its largest size."""
    return Image.open(os.path.join(ROOT, "themes/wallpapers/Aurora/contents/images/5120x2880.jpg")).convert("RGB")


def luminance(rgb):
    """WCAG relative luminance of an sRGB colour given as 0-255 values."""
    def linear(c):
        c /= 255
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (linear(c) for c in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def worst_contrast(image, box):
    """White text's contrast against the brightest 2 % of `box` in `image`, the part that limits legibility."""
    pixels = sorted(luminance(p) for p in image.crop(box).resize((64, 16)).get_flattened_data())
    bright = pixels[int(len(pixels) * 0.98)]
    return 1.05 / (bright + 0.05)


def slice_of(wallpaper, size, centre, blur):
    """A `size` (2x pixels) slice of the wallpaper around `centre` (fractions), softened by `blur`."""
    width, height = size
    scale = max(width / wallpaper.width, height / wallpaper.height) * 1.6
    big = wallpaper.resize((round(wallpaper.width * scale), round(wallpaper.height * scale)), Image.LANCZOS)
    left = min(max(0, round(big.width * centre[0] - width / 2)), big.width - width)
    top = min(max(0, round(big.height * centre[1] - height / 2)), big.height - height)
    return big.crop((left, top, left + width, top + height)).filter(ImageFilter.GaussianBlur(blur))


def darkened(image, box):
    """`image` darkened, most behind `box`, until white text there reaches CONTRAST."""
    width, height = image.size
    cx, cy = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
    # A soft oval of shade behind the text, so the slice's colour still shows at the edges.
    shade = Image.new("L", image.size, 0)
    ImageDraw.Draw(shade).ellipse((cx - width * 0.62, cy - height * 1.1, cx + width * 0.62, cy + height * 1.1), fill=255)
    shade = shade.filter(ImageFilter.GaussianBlur(height * 0.35))
    for step in range(30):
        amount = 0.30 + step * 0.025
        base = Image.eval(image, lambda v, a=amount: v * (1 - a * 0.75))
        result = Image.composite(Image.eval(image, lambda v, a=amount: v * (1 - a)), base, shade)
        if worst_contrast(result, box) >= CONTRAST:
            return result
    return result


def text_box(draw, lines, centre_x, top):
    """The bounding box of `lines` ((text, font, gap after) tuples) set centred at `centre_x` from `top`."""
    boxes = []
    y = top
    for text, face, gap in lines:
        left, upper, right, lower = draw.textbbox((0, 0), text, font=face)
        boxes.append((centre_x - (right - left) / 2, y + upper, centre_x + (right - left) / 2, y + lower))
        y += lower + gap
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def set_lines(draw, lines, centre_x, top, colours):
    """Draws `lines` centred at `centre_x` from `top`, each in its colour."""
    y = top
    for (text, face, gap), colour in zip(lines, colours):
        left, _, right, lower = draw.textbbox((0, 0), text, font=face)
        draw.text((centre_x - (right - left) / 2 - left, y), text, font=face, fill=colour)
        y += lower + gap


def svg(image, alt):
    """`image` (2x) as an SVG with rounded corners and a hairline edge, like a macOS glass panel."""
    buffer = io.BytesIO()
    image.save(buffer, "JPEG", quality=90, optimize=True, progressive=True)
    data = base64.b64encode(buffer.getvalue()).decode()
    width, height = image.width // SCALE, image.height // SCALE
    label = escape(alt, {'"': "&quot;"})
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
            f'role="img" aria-label="{label}">\n<title>{label}</title>\n'
            f'<defs><clipPath id="r"><rect width="{width}" height="{height}" rx="{RADIUS}"/></clipPath></defs>\n'
            f'<image width="{width}" height="{height}" href="data:image/jpeg;base64,{data}" clip-path="url(#r)"/>\n'
            f'<rect x="0.5" y="0.5" width="{width - 1}" height="{height - 1}" rx="{RADIUS - 0.5}" fill="none" '
            f'stroke="#ffffff" stroke-opacity="0.14"/>\n</svg>\n')


def write(name, content):
    with open(os.path.join(HERE, name), "w") as f:
        f.write(content)
    print(f"{name}: {len(content) // 1024} KB")


def heading(wallpaper, title, line, centre):
    """A section heading: the title large, the line under it, on a slice of the wallpaper."""
    size = (WIDTH * SCALE, 128 * SCALE)
    lines = [(title, font("Display-Bold", 46 * SCALE), 9 * SCALE), (line, font("Text-Regular", 17 * SCALE), 0)]
    image = slice_of(wallpaper, size, centre, 5 * SCALE)
    draw = ImageDraw.Draw(image)
    box = text_box(draw, lines, size[0] / 2, 0)
    top = (size[1] - (box[3] - box[1])) / 2 - box[1]
    box = text_box(draw, lines, size[0] / 2, top)
    image = darkened(image, box)
    # Measured on the background alone: once the text is drawn, its own white would be the brightest part.
    contrast = worst_contrast(image, box)
    set_lines(ImageDraw.Draw(image), lines, size[0] / 2, top, [WHITE, ICE])
    return image, contrast


def cover(wallpaper, start=0.0, span=1.0, top_at=0.0):
    """The cover: the bird on the right, the name and what it is on the left, and who made it. The slice starts at
    `start` of the wallpaper's width and `top_at` of its height, and is `span` of its width wide."""
    size = (WIDTH * SCALE, 380 * SCALE)
    # The slice that holds the whole bird, sharp: this is the picture people remember.
    scale = size[0] / (wallpaper.width * span)
    big = wallpaper.resize((round(wallpaper.width * scale), round(wallpaper.height * scale)), Image.LANCZOS)
    left = round(big.width * start)
    top = round(big.height * top_at)
    image = big.crop((left, top, left + size[0], top + size[1]))
    x = 56 * SCALE
    name = font("Display-Bold", 104 * SCALE)
    tagline = font("Display-Regular", 25 * SCALE)
    credit = font("Text-Medium", 15 * SCALE)
    draw = ImageDraw.Draw(image)
    rows = [("Aurora", name, WHITE, 26 * SCALE),
            ("A macOS Tahoe-inspired desktop", tagline, WHITE, 6 * SCALE),
            ("for CachyOS and KDE Plasma 6", tagline, ICE, 34 * SCALE),
            ("Developed by xt0n1", credit, EMBER, 0)]
    # Each row starts where the one above ends, plus its gap; `origins` are where each row is drawn from.
    origins, boxes = [], []
    y = 92 * SCALE
    for text, face, _, gap in rows:
        bbox = draw.textbbox((x, y), text, font=face)
        origins.append(y)
        boxes.append(bbox)
        y = bbox[3] + gap
    text_area = (x, boxes[0][1], max(b[2] for b in boxes), boxes[-1][3])
    # Shade only the left part, behind the words, so the crystals keep their light.
    shade = Image.new("L", size, 0)
    ImageDraw.Draw(shade).rectangle((0, 0, text_area[2] + 40 * SCALE, size[1]), fill=255)
    shade = shade.filter(ImageFilter.GaussianBlur(70 * SCALE))
    for step in range(30):
        amount = 0.25 + step * 0.025
        dark = Image.eval(image, lambda v, a=amount: v * (1 - a))
        result = Image.composite(dark, image, shade)
        if worst_contrast(result, text_area) >= CONTRAST:
            break
    contrast = worst_contrast(result, text_area)
    draw = ImageDraw.Draw(result)
    for (text, face, colour, _), origin in zip(rows, origins):
        draw.text((x, origin), text, font=face, fill=colour)
    return result, contrast


def through(points, samples=24):
    """A smooth line through `points` (a Catmull-Rom spline), as a list of points to draw."""
    padded = [points[0]] + points + [points[-1]]
    line = []
    for i in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[i - 1:i + 3]
        for s in range(samples):
            t = s / samples
            line.append(tuple(0.5 * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t * t
                                     + (3 * b - a - 3 * c + d) * t ** 3) for a, b, c, d in zip(p0, p1, p2, p3)))
    return line + [points[-1]]


def steps(wallpaper):
    """The six install steps down one curved path, like the sweep across the wallpaper, each named beside its number."""
    size = (WIDTH * SCALE, 600 * SCALE)
    image = slice_of(wallpaper, size, (0.30, 0.78), 3 * SCALE)
    title = font("Display-Bold", 23 * SCALE)
    body = font("Text-Regular", 16 * SCALE)
    text_x = 168 * SCALE
    rows = [70 + i * 92 for i in range(len(STEPS))]
    image = darkened(image, (text_x, rows[0] * SCALE - 30 * SCALE, size[0] - 40 * SCALE, rows[-1] * SCALE + 30 * SCALE))
    # The path enters at the top and leaves at the bottom, swaying a little, and is drawn four times larger and scaled
    # down, which smooths its edges.
    big = SCALE * 2
    sway = [100, 118, 104, 84, 96, 116]
    nodes = [(x * big, y * big) for x, y in zip(sway, rows)]
    over = Image.new("RGBA", (size[0] * 2, size[1] * 2), (0, 0, 0, 0))
    pen = ImageDraw.Draw(over)
    path = [(112 * big, 0)] + nodes + [(104 * big, size[1] * 2)]
    pen.line(through(path), fill=EMBER + (235,), width=3 * big, joint="curve")
    radius = 22 * big
    number = font("Display-Semibold", 21 * big)
    for index, (nx, ny) in enumerate(nodes, start=1):
        pen.ellipse((nx - radius, ny - radius, nx + radius, ny + radius), fill=(32, 26, 56, 255), outline=EMBER + (255,),
                    width=3 * big)
        left, top, right, bottom = pen.textbbox((0, 0), str(index), font=number)
        pen.text((nx - (right - left) / 2 - left, ny - (bottom - top) / 2 - top), str(index), font=number, fill=WHITE)
    over = over.resize(size, Image.LANCZOS)
    image.paste(over, (0, 0), over)
    draw = ImageDraw.Draw(image)
    for (name, line), row in zip(STEPS, rows):
        y = row * SCALE
        draw.text((text_x, y - 4 * SCALE), name, font=title, fill=WHITE, anchor="ls")
        draw.text((text_x, y + 22 * SCALE), line, font=body, fill=ICE, anchor="ls")
    return image


def social_preview(wallpaper):
    """The picture of a shared link: the name and what it is on the left, the real desktop on the right.
    GitHub asks for 1280x640 under 1 MB; it is drawn at twice that and scaled down."""
    size = (PREVIEW[0] * SCALE, PREVIEW[1] * SCALE)
    # A calm stretch of the wallpaper's purple, so the screenshot, which shows the sweep and the crystals, stands out.
    image = slice_of(wallpaper, size, (0.12, 0.30), 6 * SCALE)
    # The desktop as a screen with rounded corners, a hairline edge and a soft shadow under it.
    shot = Image.open(os.path.join(ROOT, "assets/screenshots/desktop.jpg")).convert("RGB")
    shot_width = 700 * SCALE
    shot = shot.resize((shot_width, round(shot.height * shot_width / shot.width)), Image.LANCZOS)
    x, y = size[0] - shot.width - 56 * SCALE, (size[1] - shot.height) // 2
    corners = Image.new("L", shot.size, 0)
    ImageDraw.Draw(corners).rounded_rectangle((0, 0, shot.width - 1, shot.height - 1), 12 * SCALE, fill=255)
    rows = [("Aurora", font("Display-Bold", 92 * SCALE), WHITE, 22 * SCALE),
            ("A macOS Tahoe-inspired desktop", font("Display-Regular", 25 * SCALE), WHITE, 6 * SCALE),
            ("for CachyOS and KDE Plasma 6", font("Display-Regular", 25 * SCALE), ICE, 0)]
    draw = ImageDraw.Draw(image)
    text_x = 60 * SCALE
    heights, boxes = [], []
    for text, face, _, gap in rows:
        bbox = draw.textbbox((0, 0), text, font=face)
        heights.append((bbox, gap))
    total = sum(b[3] - b[1] + gap for b, gap in heights)
    origins, top = [], (size[1] - total) / 2
    for bbox, gap in heights:
        origins.append(top - bbox[1])
        boxes.append((text_x + bbox[0], top, text_x + bbox[2], top + bbox[3] - bbox[1]))
        top += bbox[3] - bbox[1] + gap
    text_area = (text_x, boxes[0][1], max(b[2] for b in boxes), boxes[-1][3])
    # Shade the text's side only, as on the cover, until the words reach CONTRAST.
    shade = Image.new("L", size, 0)
    ImageDraw.Draw(shade).rectangle((0, 0, x - 20 * SCALE, size[1]), fill=255)
    shade = shade.filter(ImageFilter.GaussianBlur(60 * SCALE))
    for step in range(30):
        amount = 0.25 + step * 0.025
        result = Image.composite(Image.eval(image, lambda v, a=amount: v * (1 - a)), image, shade)
        if worst_contrast(result, text_area) >= CONTRAST:
            break
    contrast = worst_contrast(result, text_area)
    shadow = Image.new("L", size, 0)
    ImageDraw.Draw(shadow).rounded_rectangle((x, y + 14 * SCALE, x + shot.width, y + shot.height + 14 * SCALE),
                                             12 * SCALE, fill=150)
    shadow = shadow.filter(ImageFilter.GaussianBlur(22 * SCALE))
    result = Image.composite(Image.new("RGB", size, (8, 6, 20)), result, shadow)
    result.paste(shot, (x, y), corners)
    edge = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(edge).rounded_rectangle((x, y, x + shot.width - 1, y + shot.height - 1), 12 * SCALE,
                                           outline=(255, 255, 255, 60), width=SCALE)
    result.paste(edge, (0, 0), edge)
    draw = ImageDraw.Draw(result)
    for (text, face, colour, _), origin in zip(rows, origins):
        draw.text((text_x, origin), text, font=face, fill=colour)
    return result.resize(PREVIEW, Image.LANCZOS), contrast


def write_preview(image, contrast):
    """Saves the social preview as a JPEG, refusing one GitHub would reject or that fails the contrast rule."""
    buffer = io.BytesIO()
    image.save(buffer, "JPEG", quality=92, optimize=True, progressive=True)
    if image.size != PREVIEW or buffer.tell() >= 1_000_000 or contrast < CONTRAST:
        raise SystemExit(f"social-preview.jpg: {image.size}, {buffer.tell()} bytes, contrast {contrast:.1f}:1; "
                         f"GitHub needs {PREVIEW} under 1 MB and the text needs {CONTRAST}:1")
    with open(os.path.join(HERE, "social-preview.jpg"), "wb") as f:
        f.write(buffer.getvalue())
    print(f"social-preview.jpg: {buffer.tell() // 1024} KB, contrast {contrast:.1f}:1")


def main():
    wallpaper = dawn_bird()
    write_preview(*social_preview(wallpaper))
    image, contrast = cover(wallpaper)
    write("cover.svg", svg(image, "Aurora: a macOS Tahoe-inspired desktop for CachyOS and KDE Plasma 6. "
                                  "Developed by xt0n1."))
    print(f"  cover contrast {contrast:.1f}:1")
    for name, (title, line, centre) in HEADINGS.items():
        image, contrast = heading(wallpaper, title, line, centre)
        write(f"heading-{name}.svg", svg(image, f"{title}: {line}"))
        print(f"  contrast {contrast:.1f}:1")
    image = steps(wallpaper)
    write("install-steps.svg", svg(image, "The install, step by step: " +
                                    "; ".join(f"{i}. {name}: {line}" for i, (name, line) in enumerate(STEPS, 1))))


if __name__ == "__main__":
    main()
