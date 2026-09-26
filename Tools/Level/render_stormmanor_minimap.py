"""Storm Manor minimap from the layout JSON: a stylised ground-floor plan PNG (no text) + the regions JSON the HUD reads.

    python Tools/Level/render_stormmanor_minimap.py [--size 2048] [--preview]

Same outputs and conventions as render_minimap.py (north = -Y up, x east -> right, square world_min..world_max, cm):
  Art/Textures/Map/T_KG_Map_Stormmanor.png
  Art/Textures/Map/KG_MapRegions_Stormmanor.json
Tools/Unreal/kg_make_minimap.build_minimap(".../stormmanor_layout.json", do_render=False) imports both into AKGMapInfo
(the builder calls it). Regions: the island (layer 0), gardens / boathouse (2), ground-floor rooms (3), the cellar and
upper-floor rooms as map labels (-1: regions are 2D, so stacked rooms must not fight over the location toast).
"""
import argparse
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stormmanor_geo as G  # noqa: E402

OUT_DIR = os.path.join(G.ROOT, "Art", "Textures", "Map")
KEY = "Stormmanor"            # kg_make_minimap.map_key("stormmanor_layout.json")
X0, Y0, SPAN = -61.0, -52.0, 120.0          # SPRINT-040: the north block + wings reach y -44
SEA, SEA2, ROCK, ROCK2 = (38, 66, 92), (52, 86, 114), (104, 98, 90), (84, 79, 73)
GROUND = {"courtyard": (150, 140, 118), "service_yard": (120, 104, 82), "graveyard": (86, 112, 70),
          "cliff_path": (112, 106, 96), "greenhouse": (150, 190, 170), "boathouse": (132, 110, 84)}
WING = {"hall": (236, 196, 120), "west": (224, 150, 110), "east": (140, 176, 222), "service": (178, 170, 158),
        "cellar": (170, 146, 120), "guest": (222, 160, 176), "master": (130, 192, 178), "upper": (178, 160, 220),
        "grounds": (150, 190, 130), "north": (214, 186, 140), "deep": (150, 130, 110)}
WALL = (40, 34, 30)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=2048)
    ap.add_argument("--preview", action="store_true")
    a = ap.parse_args()
    n = a.size
    ss = 2
    W = n * ss
    L = G.layout()
    grid = G.Grid(L)

    def P(x, y):
        return ((x - X0) / SPAN * W, (y - Y0) / SPAN * W)

    img = Image.new("RGB", (W, W), SEA)
    d = ImageDraw.Draw(img)
    for k in range(-60, 140, 5):                        # swell lines
        d.line([P(X0, k), P(X0 + SPAN, k - 6)], fill=SEA2, width=max(2, W // 900))
    isl = grid.island(2)
    for (i, j) in isl:
        d.rectangle([P(i * 2.0, j * 2.0), P(i * 2.0 + 2.0, j * 2.0 + 2.0)], fill=ROCK)
    for (i, j) in grid.island(1):
        d.rectangle([P(i * 2.0 - 0.3, j * 2.0 - 0.3), P(i * 2.0 + 2.3, j * 2.0 + 2.3)], fill=ROCK2)
    for (i, j) in isl:
        d.rectangle([P(i * 2.0 + 0.2, j * 2.0 + 0.2), P(i * 2.0 + 1.8, j * 2.0 + 1.8)], fill=ROCK)
    R = G.rooms(L)
    # the cellar-level parts that show from above (boathouse, tunnel mouth) first, then the ground floor on top
    for r in L["rooms"]:
        if r["floor"] == "C" and r["id"] in ("boathouse", "smugglers_tunnel"):
            col = GROUND.get(r["id"], WING.get(r["wing"]))
            d.polygon([P(*p) for p in r["poly"]], fill=col, outline=WALL)
    for r in L["rooms"]:
        if r["floor"] != "F0":
            continue
        col = GROUND.get(r["id"]) if r["kind"] in ("grounds",) or r["id"] in GROUND else WING.get(r["wing"], (200, 200, 200))
        d.polygon([P(*p) for p in r["poly"]], fill=col)
    # walls of the ground floor's enclosed rooms
    lw = max(3, W // 350)
    for r in L["rooms"]:
        if r["floor"] != "F0" or not G.enclosed(r):
            continue
        pts = [P(*p) for p in r["poly"]] + [P(*r["poly"][0])]
        d.line(pts, fill=WALL, width=lw)
    # doors: gaps in the walls
    for dd in L["doors"]:
        if R[dd["a"]]["floor"] != "F0":
            continue
        x, y = dd["at"]
        hw = dd["width"] / 2.0 - 0.2
        vertical = any(abs(x - p[0]) < 0.01 for p in R[dd["a"]]["poly"]) and not any(abs(y - p[1]) < 0.01 for p in R[dd["a"]]["poly"])
        cx0 = R[dd["a"]]
        a_cells = set(G.cells_of(cx0["poly"]))
        vert = (int((x - 1) // 2), int(y // 2)) in a_cells or (int((x + 1) // 2), int(y // 2)) in a_cells
        vert = vert and abs(x / 2.0 - round(x / 2.0)) < 0.01
        if vert:
            d.line([P(x, y - hw), P(x, y + hw)], fill=(236, 226, 206), width=lw + 2)
        else:
            d.line([P(x - hw, y), P(x + hw, y)], fill=(236, 226, 206), width=lw + 2)
    # stairs: hatched rectangles
    for s in L["stairs"]:
        u, cells, rect = G.stair_geom(s)
        if R[s["lower"]]["floor"] not in ("F0", "C") and R[s["upper"]]["floor"] != "F0":
            continue
        d.rectangle([P(rect[0], rect[1]), P(rect[2], rect[3])], fill=(214, 204, 186), outline=WALL, width=2)
        if abs(u[0]) > 0.5:
            for k in range(7):
                xx = rect[0] + (rect[2] - rect[0]) * k / 6.0
                d.line([P(xx, rect[1]), P(xx, rect[3])], fill=WALL, width=2)
        else:
            for k in range(7):
                yy = rect[1] + (rect[3] - rect[1]) * k / 6.0
                d.line([P(rect[0], yy), P(rect[2], yy)], fill=WALL, width=2)
    # the long table in the Great Hall and the fountain
    d.rectangle([P(-0.6, -14.7), P(0.6, -3.3)], fill=(120, 80, 50), outline=WALL)
    fx, fy = 16.0, 21.0
    d.ellipse([P(fx - 1.7, fy - 1.7), P(fx + 1.7, fy + 1.7)], fill=(96, 150, 190), outline=WALL, width=3)
    img = img.filter(ImageFilter.SMOOTH).resize((n, n), Image.LANCZOS)
    os.makedirs(OUT_DIR, exist_ok=True)
    png = os.path.join(OUT_DIR, f"T_KG_Map_{KEY}.png")
    img.save(png, optimize=True)

    def region(rid, name, kind, layer, toast, poly, priority, icon="None"):
        xs = [p[0] for p in poly]
        ys = [p[1] for p in poly]
        c = ((min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0)
        return {"id": rid, "name": name, "kind": kind, "layer": layer, "toast": toast,
                "center": [round(c[0] * 100.0, 1), round(c[1] * 100.0, 1)], "radius": 0.0,
                "polygon": [[round(x * 100.0, 1), round(y * 100.0, 1)] for x, y in poly],
                "label": [round(c[0] * 100.0, 1), round(c[1] * 100.0, 1)], "icon": icon, "priority": priority}

    regions = []
    island = [(-58.0, -48.0), (58.0, -48.0), (58.0, 56.0), (-58.0, 56.0)]
    regions.append(region("island", "Storm Manor", "district", 0, True, island, 10))
    for r in L["rooms"]:
        name = r["name_en"]
        if r.get("secret"):
            continue            # SPRINT-040: secret rooms are never on the map (the HUD draws discovered passages)
        if r["floor"] == "F0" and G.enclosed(r) and r["id"] not in ("greenhouse",):
            regions.append(region(r["id"], name, "building", 3, True, r["poly"], 60))
        elif r["floor"] == "F0" or r["id"] == "boathouse":
            regions.append(region(r["id"], name, "place", 2, True, r["poly"], 50))
        else:
            tag = {"C2": "lower vaults", "C": "cellar", "F1": "1st floor", "F2": "attic", "F3": "tower top"}[r["floor"]]
            regions.append(region(r["id"], f"{name} ({tag})", "poi", -1, False, r["poly"], 30))
    data = {"_doc": "Generated by Tools/Level/render_stormmanor_minimap.py - do not edit. Units: cm (UE). Image: north "
                    "(-Y) up, u = (x - world_min.x) / (world_max.x - world_min.x), v likewise with y.",
            "key": KEY, "layout": "Tools/Level/stormmanor_layout.json", "png": os.path.relpath(png, G.ROOT),
            "size": n, "world_min": [X0 * 100.0, Y0 * 100.0], "world_max": [(X0 + SPAN) * 100.0, (Y0 + SPAN) * 100.0],
            "regions": regions}
    rj = os.path.join(OUT_DIR, f"KG_MapRegions_{KEY}.json")
    json.dump(data, open(rj, "w", encoding="utf-8"), indent=1)
    print(f"KG_MINIMAP {png} {n}px regions {len(regions)} -> {rj}")


if __name__ == "__main__":
    main()
