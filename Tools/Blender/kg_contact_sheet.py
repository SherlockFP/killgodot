"""Tile preview renders into one labelled contact sheet (for reviewing many viewmodel frames at once).

  python Tools/Blender/kg_contact_sheet.py <out.png> <cols> <img> [<img> ...]
"""
import os
import sys

from PIL import Image, ImageDraw

out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
tw, th = 640, 360
rows = (len(files) + cols - 1) // cols
sheet = Image.new("RGB", (cols * tw, rows * th), (30, 30, 30))
draw = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).convert("RGB").resize((tw, th), Image.LANCZOS)
    x, y = (i % cols) * tw, (i // cols) * th
    sheet.paste(im, (x, y))
    label = os.path.splitext(os.path.basename(f))[0].replace("FP2_", "")
    draw.rectangle([x, y, x + 8 * len(label) + 8, y + 16], fill=(0, 0, 0))
    draw.text((x + 4, y + 2), label, fill=(255, 255, 0))
sheet.save(out)
print("sheet", out, sheet.size)
