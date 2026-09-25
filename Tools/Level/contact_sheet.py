"""Contact sheet of capture-tour renders: python Tools/Level/contact_sheet.py out.png [cols] [width] shot shot ...
Shots are names under Saved/Screenshots/V2 (without .png). Labels in the top-left corner of each tile.
A name with a folder ("V2_before/hb_mole") is read from Saved/Screenshots/<folder>/ and labelled "<name> (before)" for
V2_before, so before/after pairs sit side by side: ... 2 800 V2_before/hb_mole hb_mole V2_before/quay quay"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = "D:/Kill Godot"


def sheet(out, names, cols=3, tile_w=640):
    ims = []
    for n in names:
        if "/" in n:
            folder, base = n.rsplit("/", 1)
            p = os.path.join(ROOT, "Saved", "Screenshots", folder, base + ".png")
            label = f"{base} (before)" if folder.endswith("_before") else f"{base} ({folder})"
        else:
            p = os.path.join(ROOT, "Saved", "Screenshots", "V2", n + ".png")
            label = n
        if os.path.exists(p):
            ims.append((label, Image.open(p).convert("RGB")))
    if not ims:
        return None
    tw = tile_w
    th = int(tw * ims[0][1].height / ims[0][1].width)
    rows = (len(ims) + cols - 1) // cols
    S = Image.new("RGB", (cols * tw, rows * th), (20, 20, 20))
    d = ImageDraw.Draw(S)
    try:
        font = ImageFont.truetype("arial.ttf", max(12, tw // 40))
    except OSError:
        font = ImageFont.load_default()
    for k, (n, im) in enumerate(ims):
        x, y = (k % cols) * tw, (k // cols) * th
        S.paste(im.resize((tw, th), Image.LANCZOS), (x, y))
        w = d.textlength(n, font=font)
        d.rectangle([x, y, x + w + 12, y + font.size + 8], fill=(0, 0, 0))
        d.text((x + 6, y + 3), n, fill=(255, 220, 90), font=font)
    S.save(out)
    return out


if __name__ == "__main__":
    out = sys.argv[1]
    cols = int(sys.argv[2])
    tw = int(sys.argv[3])
    print(sheet(out, sys.argv[4:], cols, tw))
