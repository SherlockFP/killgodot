"""SPRINT-025: before/after contact sheet of the HUD shots (Saved/Screenshots/HUD) and the chore panel shots
(Saved/UIShots). Usage: python Tools/Unreal/kg_contact_sheet.py [1280x720|1920x1080] -> Saved/Screenshots/HUD/contact_<WxH>.png
"""
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HUD = os.path.join(ROOT, "Saved", "Screenshots", "HUD")
NAMES = ["near", "mid", "far", "edge", "bigmap", "fullmap", "demo2", "demo3"]


def sheet(size):
    w, h = (int(v) for v in size.split("x"))
    tw = 640
    th = int(tw * h / w)
    cols = 4
    rows = 2 * ((len(NAMES) + cols - 1) // cols)
    pad = 10
    head = 28
    out = Image.new("RGB", (cols * (tw + pad) + pad, rows * (th + pad + head) + pad), (18, 13, 23))
    d = ImageDraw.Draw(out)
    y = pad
    for tag in ("before", "after"):
        for i, name in enumerate(NAMES):
            r, c = divmod(i, cols)
            x = pad + c * (tw + pad)
            yy = y + r * (th + pad + head)
            path = os.path.join(HUD, f"{tag}_{size}_{name}.png")
            d.text((x, yy), f"{tag} · {name} · {size}", fill=(246, 231, 200))
            if os.path.exists(path):
                im = Image.open(path).convert("RGB").resize((tw, th), Image.LANCZOS)
                out.paste(im, (x, yy + head))
            else:
                d.rectangle([x, yy + head, x + tw, yy + head + th], outline=(120, 90, 80))
                d.text((x + 8, yy + head + 8), "missing", fill=(200, 100, 100))
        y += ((len(NAMES) + cols - 1) // cols) * (th + pad + head)
    dst = os.path.join(HUD, f"contact_{size}.png")
    out.save(dst)
    print(dst)


if __name__ == "__main__":
    for s in (sys.argv[1:] or ["1280x720", "1920x1080"]):
        sheet(s)
