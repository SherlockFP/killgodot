"""Render the underground map layer (Tools/Level/underground_layout.py) for the HUD minimap / full map (M).

    python Tools/Level/render_underground_map.py [--size 2048] [--preview]

Same world bounds and orientation as the village map (Art/Textures/Map/KG_MapRegions_Morrowmere_v2.json, north (-Y)
up), so the two layers line up: the village is drawn underneath as a dim, cold ghost for orientation, the cellar,
tunnel and catacombs on top as lit stone plans (warm brick, rough rock, cool crypt stone), walls inked, the crypt
stair hatched, the well shaft as a ring, the vault gate as a gold bar, niches as bone ticks. No text: AKGHUD draws
the labels and icons live from the regions.

Outputs:
  Art/Textures/Map/T_KG_Map_Underground_v2.png
  Art/Textures/Map/KG_MapRegions_Underground_v2.json   {world_min, world_max (cm), regions, volumes} -> AKGUndergroundInfo
  --preview: Saved/Minimap/preview_Underground_v2.png (labels drawn, cropped to the underground)
"""
import argparse
import json
import os
import sys

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import underground_layout as U  # noqa: E402

MAP_DIR = os.path.join(ROOT, "Art", "Textures", "Map")
SURFACE_PNG = os.path.join(MAP_DIR, "T_KG_Map_Morrowmere_v2.png")
SURFACE_JSON = os.path.join(MAP_DIR, "KG_MapRegions_Morrowmere_v2.json")
KEY = "Underground_v2"
SS = 2

STYLE = {   # floor top, floor edge (sRGB)
    "crypt": ((150, 158, 180), (112, 118, 140)),
    "stair": ((178, 184, 202), (120, 126, 148)),
    "cellar": ((196, 150, 104), (150, 108, 72)),
    "mine": ((160, 126, 90), (118, 90, 62)),
    "shaft": ((110, 120, 140), (70, 78, 96)),
}
INK = (22, 18, 30, 255)
BONE = (236, 226, 200, 255)
GOLD = (255, 190, 70, 255)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=2048)
    ap.add_argument("--preview", action="store_true")
    a = ap.parse_args()
    ref = json.load(open(SURFACE_JSON, encoding="utf-8"))
    wmin, wmax = ref["world_min"], ref["world_max"]   # cm
    n = a.size
    N = n * SS

    def px(x_m, y_m):
        return ((x_m * 100.0 - wmin[0]) / (wmax[0] - wmin[0]) * N, (y_m * 100.0 - wmin[1]) / (wmax[1] - wmin[1]) * N)

    # The village as a cold ghost (orientation only).
    if os.path.exists(SURFACE_PNG):
        base = Image.open(SURFACE_PNG).convert("RGB").resize((N, N), Image.BILINEAR)
        base = ImageEnhance.Color(base).enhance(0.15)
        base = ImageEnhance.Brightness(base).enhance(0.26)
        tint = Image.new("RGB", (N, N), (14, 16, 30))
        img = Image.blend(base, tint, 0.35).convert("RGBA")
    else:
        img = Image.new("RGBA", (N, N), (16, 18, 28, 255))
    d = ImageDraw.Draw(img)
    cell_px = px(U.X0 + U.CELL, 0)[0] - px(U.X0, 0)[0]

    # Soft dark halo around the whole underground so it reads against the ghost.
    halo = Image.new("L", (N, N), 0)
    hd = ImageDraw.Draw(halo)
    for i, j, c in U.cells():
        x0, y0 = px(U.X0 + U.CELL * i, U.Y0 + U.CELL * j)
        hd.rectangle([x0 - cell_px * 0.6, y0 - cell_px * 0.6, x0 + cell_px * 1.6, y0 + cell_px * 1.6], fill=255)
    halo = halo.filter(ImageFilter.GaussianBlur(cell_px * 0.5))
    img = Image.composite(Image.new("RGBA", (N, N), (6, 6, 12, 255)), img, halo.point(lambda v: int(v * 0.8)))
    d = ImageDraw.Draw(img)

    # Floors per cell (slight per-cell variation so it reads as paving), then inked walls on the union boundary.
    for i, j, c in U.cells():
        style = U.CELLS[c]["style"]
        top, edge = STYLE[style]
        k = ((i * 7 + j * 13) % 5 - 2) * 4
        col = tuple(max(0, min(255, v + k)) for v in top) + (255,)
        x0, y0 = px(U.X0 + U.CELL * i, U.Y0 + U.CELL * j)
        x1, y1 = px(U.X0 + U.CELL * (i + 1), U.Y0 + U.CELL * (j + 1))
        d.rectangle([x0, y0, x1, y1], fill=col)
        if style in ("crypt", "cellar"):
            # paving joints
            for t in (0.5,):
                d.line([x0, y0 + (y1 - y0) * t, x1, y0 + (y1 - y0) * t], fill=edge + (255,), width=max(1, int(cell_px * 0.04)))
                d.line([x0 + (x1 - x0) * t, y0, x0 + (x1 - x0) * t, y1], fill=edge + (255,), width=max(1, int(cell_px * 0.04)))
        if style == "mine":
            for t in (0.2, 0.55, 0.8):
                d.ellipse([x0 + (x1 - x0) * t - 2 * SS, y0 + (y1 - y0) * (1 - t) - 2 * SS,
                           x0 + (x1 - x0) * t + 2 * SS, y0 + (y1 - y0) * (1 - t) + 2 * SS], fill=edge + (255,))
    # Stair hatching (treads across the rise).
    for (i, j) in U.STAIR["cells"]:
        x0, y0 = px(U.X0 + U.CELL * i, U.Y0 + U.CELL * j)
        x1, y1 = px(U.X0 + U.CELL * (i + 1), U.Y0 + U.CELL * (j + 1))
        for t in range(6):
            y = y0 + (y1 - y0) * (t + 0.5) / 6
            d.line([x0 + 2 * SS, y, x1 - 2 * SS, y], fill=(90, 96, 118, 255), width=max(1, int(cell_px * 0.05)))
    # Niche ticks along crypt corridor walls (bones in the walls).
    for i, j, c in U.cells("C"):
        x0, y0 = px(U.X0 + U.CELL * i, U.Y0 + U.CELL * j)
        x1, y1 = px(U.X0 + U.CELL * (i + 1), U.Y0 + U.CELL * (j + 1))
        for di, dj, side in ((0, -1, "N"), (0, 1, "S"), (-1, 0, "W"), (1, 0, "E")):
            if U.cell_at(i + di, j + dj):
                continue
            for t in (0.3, 0.7):
                if side in "NS":
                    x = x0 + (x1 - x0) * t
                    y = y0 if side == "N" else y1
                    d.rectangle([x - cell_px * 0.12, y - cell_px * 0.08, x + cell_px * 0.12, y + cell_px * 0.08], fill=BONE)
                else:
                    y = y0 + (y1 - y0) * t
                    x = x0 if side == "W" else x1
                    d.rectangle([x - cell_px * 0.08, y - cell_px * 0.12, x + cell_px * 0.08, y + cell_px * 0.12], fill=BONE)
    # Walls: every open-cell edge that faces rock.
    w = max(2, int(cell_px * 0.16))
    for i, j, c in U.cells():
        x0, y0 = px(U.X0 + U.CELL * i, U.Y0 + U.CELL * j)
        x1, y1 = px(U.X0 + U.CELL * (i + 1), U.Y0 + U.CELL * (j + 1))
        if not U.cell_at(i, j - 1):
            d.line([x0 - w / 2, y0, x1 + w / 2, y0], fill=INK, width=w)
        if not U.cell_at(i, j + 1):
            d.line([x0 - w / 2, y1, x1 + w / 2, y1], fill=INK, width=w)
        if not U.cell_at(i - 1, j):
            d.line([x0, y0 - w / 2, x0, y1 + w / 2], fill=INK, width=w)
        if not U.cell_at(i + 1, j):
            d.line([x1, y0 - w / 2, x1, y1 + w / 2], fill=INK, width=w)
    # The well shaft: a stone ring with a ladder.
    sx, sy = px(*U.WELL[:2])
    r = cell_px * 0.5
    d.ellipse([sx - r, sy - r, sx + r, sy + r], outline=INK, width=w)
    d.ellipse([sx - r * 0.72, sy - r * 0.72, sx + r * 0.72, sy + r * 0.72], fill=(40, 44, 60, 255))
    for t in (-0.3, 0.0, 0.3):
        d.line([sx + r * 0.2, sy + r * t, sx + r * 0.55, sy + r * t], fill=(200, 160, 110, 255), width=max(1, int(cell_px * 0.05)))
    # The vault gate: a gold bar on the edge.
    gi, gj = U.GATE["cell"]
    gx0, gy = px(U.X0 + U.CELL * gi, U.Y0 + U.CELL * gj)
    gx1, _ = px(U.X0 + U.CELL * (gi + 1), U.Y0 + U.CELL * gj)
    d.line([gx0 + w, gy, gx1 - w, gy], fill=GOLD, width=max(2, int(w * 0.9)))
    for t in range(1, 5):
        x = gx0 + (gx1 - gx0) * t / 5
        d.line([x, gy - w, x, gy + w], fill=GOLD, width=max(1, int(w * 0.35)))

    out = img.resize((n, n), Image.LANCZOS)
    os.makedirs(MAP_DIR, exist_ok=True)
    png = os.path.join(MAP_DIR, f"T_KG_Map_{KEY}.png")
    out.convert("RGB").save(png, optimize=True)
    data = {
        "_doc": "Generated by Tools/Level/render_underground_map.py - do not edit. Units: cm (UE).",
        "key": KEY, "png": os.path.relpath(png, ROOT).replace("\\", "/"), "size": n,
        "world_min": wmin, "world_max": wmax,
        "regions": U.regions(),
        "volumes": U.volumes(),
    }
    rj = os.path.join(MAP_DIR, f"KG_MapRegions_{KEY}.json")
    json.dump(data, open(rj, "w", encoding="utf-8"), indent=1)
    print(f"KG_UNDERMAP {png} {n}px, {len(data['regions'])} regions, {len(data['volumes'])} volumes -> {rj}")
    if a.preview:
        allc = [(i, j) for i, j, _ in U.cells()]
        xs = [px(U.X0 + U.CELL * i, 0)[0] / SS for i, j in allc]
        ys = [px(0, U.Y0 + U.CELL * j)[1] / SS for i, j in allc]
        pad = 60
        box = (int(min(xs)) - pad, int(min(ys)) - pad, int(max(xs)) + pad + 20, int(max(ys)) + pad + 20)
        pv = out.crop(box).resize(((box[2] - box[0]) * 3, (box[3] - box[1]) * 3), Image.NEAREST).convert("RGB")
        pd = ImageDraw.Draw(pv)
        try:
            font = ImageFont.truetype("arial.ttf", 22)
        except OSError:
            font = ImageFont.load_default()
        for r in data["regions"]:
            if r["layer"] < 0 or r["id"] == "underground":
                continue
            lx = ((r["label"][0] - wmin[0]) / (wmax[0] - wmin[0]) * n - box[0]) * 3
            ly = ((r["label"][1] - wmin[1]) / (wmax[1] - wmin[1]) * n - box[1]) * 3
            pd.text((lx, ly), r["name"], fill=(255, 244, 222), font=font, anchor="mm", stroke_width=3, stroke_fill=(20, 14, 28))
        path = os.path.join(ROOT, "Saved", "Minimap", f"preview_{KEY}.png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        pv.save(path)
        print(f"KG_UNDERMAP preview {path}")


if __name__ == "__main__":
    main()
