# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""find_text.py SHOT PHRASE [NTH|all]: where PHRASE is written in SHOT, read with tesseract; prints the centre x y of
its NTH occurrence (1, the topmost, by default), or of every occurrence top to bottom with "all", in the screenshot's
pixels. Prints nothing and exits 1 when it is not there.

The words of PHRASE must follow each other on one line. Each one may be part of a longer word, because tesseract
sometimes joins an icon drawn next to a word to the word ("ynpin" for "Unpin")."""
import subprocess
import sys
import tempfile

from PIL import Image, ImageOps

# The labels are about 13 px tall, light on dark glass. tesseract reads dark text on a light page best, at roughly
# twice that height, so the shot is turned to grey, inverted and enlarged first; boxes are scaled back.
SCALE = 2


def occurrences(tsv, phrase):
    """Yields the (left, top, right, bottom) box of each place `phrase` (a list of words) appears, top to bottom."""
    # tesseract's TSV columns: level page block par line word left top width height conf text.
    lines = {}
    for row in tsv.splitlines()[1:]:
        cells = row.split("\t")
        if len(cells) == 12 and cells[11].strip() and float(cells[10]) >= 0:
            lines.setdefault(tuple(cells[1:5]), []).append(cells)
    found = []
    for words in lines.values():
        for start in range(len(words) - len(phrase) + 1):
            run = words[start:start + len(phrase)]
            if all(wanted in word[11] for wanted, word in zip(phrase, run)):
                found.append((min(int(w[6]) for w in run), min(int(w[7]) for w in run),
                              max(int(w[6]) + int(w[8]) for w in run), max(int(w[7]) + int(w[9]) for w in run)))
    yield from sorted(found, key=lambda box: (box[1], box[0]))


def main():
    shot, phrase = sys.argv[1], sys.argv[2].split()
    which = sys.argv[3] if len(sys.argv) > 3 else "1"
    with tempfile.NamedTemporaryFile(suffix=".png") as page:
        image = ImageOps.invert(Image.open(shot).convert("L"))
        image.resize((image.width * SCALE, image.height * SCALE), Image.LANCZOS).save(page.name)
        tsv = subprocess.run(["tesseract", page.name, "-", "--psm", "11", "tsv"], capture_output=True, text=True,
                             check=True).stdout
    boxes = [tuple(value // SCALE for value in box) for box in occurrences(tsv, phrase)]
    chosen = boxes if which == "all" else boxes[int(which) - 1:int(which)]
    for left, top, right, bottom in chosen:
        print((left + right) // 2, (top + bottom) // 2)
    return 0 if chosen else 1


if __name__ == "__main__":
    sys.exit(main())
