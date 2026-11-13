#!/usr/bin/env python3
"""Draws the HOME menu's "Exit Game" button label (replacing "Wii Menu") and
writes it as a GX I4 texture in a C array, tools/hbm-exit-label.inc, which
tls.cpp writes over the label in the menu's archive once it is loaded.

The original is white text, 22 px cap height, centred at x 120 on a 240x64
I4 texture; this matches it with FreeSans. Needs Pillow."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

TEXT = sys.argv[1] if len(sys.argv) > 1 else "Exit Game"
FONT = "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
W, H, S, CAP, TOP = 240, 64, 4, 22, 21

def font(size): return ImageFont.truetype(FONT, size)

im = Image.new("L", (W * S, H * S), 0)
d = ImageDraw.Draw(im)
e = d.textbbox((0, 0), "E", font=font(30 * S))
f = font(round(30 * S * CAP * S / (e[3] - e[1])))
bb, e = d.textbbox((0, 0), TEXT, font=f), d.textbbox((0, 0), "E", font=f)
d.text((W * S / 2 - (bb[2] - bb[0]) / 2 - bb[0], TOP * S - e[1]), TEXT, fill=255, font=f)
im = im.resize((W, H), Image.LANCZOS)
px = im.load()

out = bytearray()                                  # I4: 8x8 tiles, 2 pixels a byte, high nibble first
for by in range(0, H, 8):
    for bx in range(0, W, 8):
        for y in range(8):
            for x in range(0, 8, 2):
                a, b = ((px[bx + x + i, by + y] + 8) // 17 for i in (0, 1))
                out.append(min(a, 15) << 4 | min(b, 15))

lines = ["    " + ",".join("0x%02X" % v for v in out[i:i + 24]) + "," for i in range(0, len(out), 24)]
Path(__file__).with_name("hbm-exit-label.inc").write_text(
    '// "%s": the HOME menu\'s "Wii Menu" button label redrawn, %dx%d GX I4 (tools/hbm-label.py).\n' % (TEXT, W, H)
    + "\n".join(lines) + "\n")
