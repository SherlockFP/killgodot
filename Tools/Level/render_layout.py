"""Render the Morrowmere v2 layout JSON as a town-plan PNG (top-down, sea at the bottom, as in the editor top view).

    python Tools/Level/render_layout.py [layout.json] [out.png]

Default: Tools/Level/morrowmere_layout_v2.json -> Docs/Level/Morrowmere_v2.png. Everything is drawn from the JSON
(terraces coloured by height, water, walls, lanes, stairs, ramps, buildings by kind, chores, sightlines), plus a
section along the Processional Axis and the chore travel times computed by validate_layout.py.
"""
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.patheffects as pe  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap  # noqa: E402
from matplotlib.patches import Circle, FancyArrowPatch, Patch, Polygon as MPoly  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.path import Path  # noqa: E402
from shapely.geometry import LineString  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import validate_layout as V  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
X0, X1, Y0, Y1 = -132.0, 134.0, -122.0, 172.0
RES = 0.5

INK = "#1f1d1a"
INK2 = "#55524c"
PAPER = "#f6f2ea"
SEA_DEEP, SEA_SHALLOW, BASIN = "#2d6f96", "#5fa6c6", "#79bcd4"
NATURAL = "#cfd8b6"
LANE, LANE_EDGE, SQUARE = "#efe4cf", "#9c8a6a", "#e6d3ae"
HOME, CIVIC, INFILL, TOWER = "#eb6834", "#2a78d6", "#bfb29c", "#4a3aa7"
WALL, CLIFF = "#3b3530", "#6b5d52"
CHORE = "#eda100"
SIGHT = "#c2185b"
HEIGHT = LinearSegmentedColormap.from_list("height", ["#f3ecc9", "#e2e6b2", "#c4d99a", "#9fc584", "#7bab70", "#5f9063"])
ZMIN, ZMAX = 0.0, 18.0
halo = [pe.withStroke(linewidth=3.2, foreground=PAPER)]
halo_s = [pe.withStroke(linewidth=2.2, foreground=PAPER)]


def xy(pts):
    return [p[0] for p in pts], [p[1] for p in pts]


def ground_raster(W, L):
    xs = np.arange(X0, X1, RES) + RES / 2
    ys = np.arange(Y0, Y1, RES) + RES / 2
    gx, gy = np.meshgrid(xs, ys)
    pts = np.column_stack([gx.ravel(), gy.ravel()])
    z = np.full(len(pts), np.nan)
    kind = np.zeros(len(pts), dtype=np.int8)      # 0 sea, 1 natural land, 2 terrace
    land = Path(L["coast"]["land_polygon"]).contains_points(pts)
    kind[land] = 1
    for t, g in W.terraces:
        m = Path(t["polygon"]).contains_points(pts) & np.isnan(z)
        if t.get("z") is not None:
            z[m] = t["z"]
        else:
            pr = t["z_profile"]
            s = (pts[m, 0] - pr["origin"][0]) * pr["dir"][0] + (pts[m, 1] - pr["origin"][1]) * pr["dir"][1]
            st = np.array(pr["stops"])
            z[m] = np.interp(s, st[:, 0], st[:, 1])
        kind[m] = 2
    for mnd in L.get("mounds", []):
        d = np.hypot(pts[:, 0] - mnd["center"][0], pts[:, 1] - mnd["center"][1])
        k = np.clip(1 - d / mnd["radius"], 0, 1)
        k = k * k * (3 - 2 * k)
        z = np.where(kind == 2, z + mnd["height"] * k, z)
    # sea shading by distance from the basin centre / coast
    sea = kind == 0
    c = L["basin"]["center"]
    dbas = np.hypot(pts[:, 0] - c[0], pts[:, 1] - c[1])
    depth = np.clip((pts[:, 1] - 60.0) / 110.0, 0, 1)
    return xs, ys, z.reshape(gx.shape), kind.reshape(gx.shape), sea.reshape(gx.shape), \
        (dbas.reshape(gx.shape) < L["basin"]["radius"]), depth.reshape(gx.shape)


def rgb(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def draw(L, out):
    W = V.World(L)
    err, warn, info, times = V.validate(L, quiet=True)
    fig = plt.figure(figsize=(26, 19.5), dpi=100, facecolor=PAPER)
    ax = fig.add_axes([0.015, 0.03, 0.60, 0.92])
    ax.set_facecolor(PAPER)

    # ---------------------------------------------------------------- ground raster
    xs, ys, z, kind, sea, inbasin, depth = ground_raster(W, L)
    img = np.zeros(z.shape + (3,))
    deep, shallow, bas = rgb(SEA_DEEP), rgb(SEA_SHALLOW), rgb(BASIN)
    img[sea] = (shallow * (1 - depth[sea][:, None]) + deep * depth[sea][:, None])
    bmask = sea & inbasin
    img[bmask] = bas
    img[kind == 1] = rgb(NATURAL)
    tz = (np.clip(z, ZMIN, ZMAX) - ZMIN) / (ZMAX - ZMIN)
    t_mask = kind == 2
    img[t_mask] = HEIGHT(tz[t_mask])[:, :3]
    ax.imshow(img, origin="lower", extent=(X0, X1, Y0, Y1), interpolation="bilinear", zorder=0)
    # contour every 1 m on sloped terraces, heavier every 3 m
    zc = np.where(t_mask, z, np.nan)
    ax.contour(xs, ys, zc, levels=np.arange(1, 19, 1), colors="#2f4a33", linewidths=0.35, alpha=0.35, zorder=1)

    # coast
    cx, cy = xy(L["coast"]["land_polygon"])
    ax.plot(cx + [cx[0]], cy + [cy[0]], color="#e9dfb9", lw=3.0, zorder=2, solid_capstyle="round")
    ax.plot(cx + [cx[0]], cy + [cy[0]], color="#4d6b72", lw=0.8, zorder=2)

    # terrace outlines + labels
    for t, g in W.terraces:
        px, py = xy(t["polygon"])
        ax.plot(px + [px[0]], py + [py[0]], color="#2f4a33", lw=0.6, alpha=0.5, zorder=2)

    # fields, graveyard
    for f in L.get("fields", []):
        if "apple" in f.get("crop", ""):
            ax.add_patch(MPoly(f["polygon"], closed=True, facecolor="#cfe0a8", edgecolor="#7c6a45", lw=1.4, zorder=3))
            continue
        ax.add_patch(MPoly(f["polygon"], closed=True, facecolor="#e8d58a", edgecolor="#7c6a45", lw=1.4, hatch="||",
                           alpha=0.75, zorder=3))
    gy = L["landmarks"]["graveyard"]
    ax.add_patch(MPoly(gy["polygon"], closed=True, facecolor="#9fb08e", edgecolor="#4d4a40", lw=1.4, hatch="++", zorder=3))
    pen = L["landmarks"].get("pen")
    if pen:
        ax.add_patch(MPoly(pen["polygon"], closed=True, facecolor="#d8c49a", edgecolor="#7c6a45", lw=1.0, zorder=3))

    # stream + ponds + koi spill
    st = L["stream"]
    sx, sy = xy(st["points"])
    ax.plot(sx, sy, color="#2f7fb0", lw=st["width"] * 2.4 + 1.5, solid_capstyle="round", zorder=4)
    ax.plot(sx, sy, color="#7cc3e0", lw=st["width"] * 2.4 - 1.0, solid_capstyle="round", zorder=4)
    for p in st.get("ponds", []):
        ax.add_patch(Circle(p["center"], p["radius"], facecolor="#7cc3e0", edgecolor="#2f7fb0", lw=1.5, zorder=4))
    kx, ky = xy(L["koi_spill"]["points"])
    ax.plot(kx, ky, color="#7cc3e0", lw=2.2, zorder=4, ls=(0, (2, 1)))

    # mole
    mole = L["mole"]
    mline = LineString(mole["points"])
    mpoly = mline.buffer(mole["width"] / 2.0, cap_style=1)
    ax.add_patch(MPoly(list(mpoly.exterior.coords), closed=True, facecolor="#9a948a", edgecolor="#4d4a44", lw=1.0, zorder=5))
    mw = mline.buffer(mole["walk_width"] / 2.0, cap_style=2)
    ax.add_patch(MPoly(list(mw.exterior.coords), closed=True, facecolor="#d9d2c3", edgecolor="none", zorder=5))

    # walled back gardens: what is left of the town terraces after streets, squares and buildings
    from shapely.ops import unary_union as _uu
    town = _uu([g for t, g in W.terraces if t["name"] in ("quay", "heart", "upper", "garden")])
    taken = _uu([g.buffer(0.4) for _, k, g, _, _ in W.walk if k != "yard"] + [r.buffer(0.3) for r in W.rects.values()])
    gard = town.difference(taken)
    for q in ([gard] if gard.geom_type == "Polygon" else list(gard.geoms)):
        if q.area < 6.0:
            continue
        ax.add_patch(MPoly(list(q.exterior.coords), closed=True, facecolor="#b5cf8f", edgecolor="#8a9a6a", lw=0.5,
                           hatch="....", alpha=0.55, zorder=3))
    for f in L.get("fields", []):
        if "apple" in f.get("crop", ""):
            fp = Path(f["polygon"])
            for gx_ in np.arange(-130, 130, 5.0):
                for gy_ in np.arange(-130, 40, 5.0):
                    if fp.contains_point((gx_, gy_)):
                        ax.add_patch(Circle((gx_, gy_), 1.5, facecolor="#6f9a4e", edgecolor="#3f5f2c", lw=0.4, zorder=4))

    # ---------------------------------------------------------------- walkways
    for name, kindw, g, line, m in W.walk:
        if kindw in ("yard",):
            continue
        polys = [g] if g.geom_type == "Polygon" else list(g.geoms)
        for q in polys:
            if kindw == "square":
                fc, ec, zo = SQUARE, LANE_EDGE, 6
            elif kindw.startswith("lane:pier"):
                fc, ec, zo = "#a8784c", "#5a3d24", 6
            elif kindw.startswith("lane:mole"):
                continue
            elif kindw == "stair":
                fc, ec, zo = "#ddd3c1", "#6d6254", 7
            elif kindw == "ramp":
                fc, ec, zo = "#e7d9bd", "#8b7a5c", 6
            else:
                fc, ec, zo = LANE, LANE_EDGE, 6
                if m.get("surface") == "dirt":
                    fc = "#e6d2ab"
                if m.get("surface") == "gravel":
                    fc = "#ece6da"
            ax.add_patch(MPoly(list(q.exterior.coords), closed=True, facecolor=fc, edgecolor=ec, lw=0.7, zorder=zo))
    # stair risers + up arrows
    for s in L["stairs"]:
        a, b = np.array(s["from"]), np.array(s["to"])
        up = a if s["z0"] > s["z1"] else b
        dn = b if s["z0"] > s["z1"] else a
        d = (up - dn)
        Ls = np.linalg.norm(d)
        u = d / Ls
        n = np.array([-u[1], u[0]])
        risers = s["risers"]
        land = (Ls - risers * V.RISER_RUN) / max(1, s["landings"]) if s["landings"] else 0.0
        flights = s["landings"] + 1
        per = risers // flights
        pos, k = 0.0, 0
        for f in range(flights):
            nsteps = per + (1 if f < risers % flights else 0)
            for r in range(nsteps * 5):
                t = pos + (r + 0.5) * V.RISER_RUN / 5
                p = dn + u * t
                ax.plot(*zip(p - n * s["width"] / 2, p + n * s["width"] / 2), color="#6d6254", lw=0.45, zorder=8)
            pos += nsteps * V.RISER_RUN + land
        ax.add_patch(FancyArrowPatch(tuple(dn + u * 0.8), tuple(up - u * 0.8), arrowstyle="-|>", mutation_scale=9,
                                     color="#3b3530", lw=0.9, zorder=9))
    for r in L["ramps"]:
        line = LineString(r["points"])
        lo_first = r["z0"] < r["z1"]
        for k in range(1, int(line.length / 4.0)):
            p = line.interpolate(k * 4.0)
            q = line.interpolate(k * 4.0 + (0.9 if lo_first else -0.9))
            ax.annotate("", xy=(q.x, q.y), xytext=(p.x, p.y),
                        arrowprops=dict(arrowstyle="-|>", color="#8b7a5c", lw=0.8, mutation_scale=7), zorder=8)

    # bridges
    for b in L["bridges"]:
        a = np.array(b["at"])
        d = np.array(b["along"]) / np.linalg.norm(b["along"])
        n = np.array([-d[1], d[0]])
        hs, hw = b["span"] / 2, b["width"] / 2
        corners = [a - d * hs - n * hw, a + d * hs - n * hw, a + d * hs + n * hw, a - d * hs + n * hw]
        stone = "stone" in b["name"]
        ax.add_patch(MPoly(corners, closed=True, facecolor="#b8ad9c" if stone else "#a8784c", edgecolor=INK, lw=1.0,
                           zorder=9, hatch=".." if "ford" in b["name"] else None))

    # retaining walls + cliffs
    for wl in L["retaining_walls"]:
        if wl["top_z"] == wl["bottom_z"]:
            wx, wy = xy(wl["points"])
            ax.plot(wx, wy, color=WALL, lw=0.9, ls=(0, (3, 2)), zorder=9)
            continue
        wx, wy = xy(wl["points"])
        lw = 2.2 + 1.2 * (wl.get("courses") or 1)
        ax.plot(wx, wy, color=WALL, lw=lw, solid_capstyle="butt", zorder=9)
        if wl.get("parapet"):
            ax.plot(wx, wy, color="#d8cfc0", lw=0.8, zorder=9)
    qx, qy = xy(L["quay"]["edge"])
    ax.plot(qx, qy, color=WALL, lw=4.2, zorder=9)
    ax.plot(qx, qy, color="#d8cfc0", lw=0.9, zorder=9)
    for c in L["cliffs"]:
        cx_, cy_ = xy(c["points"])
        ax.plot(cx_, cy_, color=CLIFF, lw=2.2, zorder=9,
                path_effects=[pe.withTickedStroke(spacing=5, angle=-60, length=1.1, linewidth=1.4)])

    # ---------------------------------------------------------------- buildings
    for bid, cat, b in W.buildings:
        r = W.rects[bid]
        if cat == "home":
            fc, ec = HOME, "#6e2a10"
        elif cat == "infill":
            fc, ec = INFILL, "#5f5546"
        elif cat == "tower":
            fc, ec = TOWER, "#1d1450"
        else:
            fc, ec = CIVIC, "#0e3566"
        hatch = "////" if b.get("open") else None
        ax.add_patch(MPoly(list(r.exterior.coords), closed=True, facecolor=fc, edgecolor=ec, lw=0.9, zorder=10, hatch=hatch))
        if cat in ("home", "civic", "tower") and not b.get("open"):
            dp = np.array(V.door(b, 0.0))
            f = math.radians(b["face_deg"])
            u = np.array([math.cos(f), math.sin(f)])
            n = np.array([-u[1], u[0]])
            tri = [dp + u * 1.6, dp + n * 0.9, dp - n * 0.9]
            ax.add_patch(MPoly(tri, closed=True, facecolor="white", edgecolor=ec, lw=0.6, zorder=11))
        if cat == "home":
            ax.text(*b["at"], b["id"][1:].lstrip("0"), ha="center", va="center", fontsize=6.5, color="white",
                    fontweight="bold", zorder=12)
        if cat == "tower":
            ax.add_patch(Circle(b["at"], 5.5, facecolor="none", edgecolor=TOWER, lw=1.0, ls=(0, (2, 2)), zorder=11))

    for j in L.get("street_joins", []):
        jx, jy = xy(j["points"])
        if j["kind"] == "arch":
            ax.plot(jx, jy, color="#5f5546", lw=4.2, zorder=10, solid_capstyle="butt")
            ax.plot(jx, jy, color=LANE, lw=2.0, zorder=10, solid_capstyle="butt")
        else:
            ax.plot(jx, jy, color="#5f5546", lw=1.6, zorder=10)

    # ---------------------------------------------------------------- landmarks (points)
    lm = L["landmarks"]
    ax.add_patch(Circle(lm["fountain"]["at"], 1.9, facecolor="#e8f4fa", edgecolor="#2f7fb0", lw=1.4, zorder=12))
    ax.add_patch(Circle(lm["koi_pond"]["at"], lm["koi_pond"]["radius"], facecolor="#7cc3e0", edgecolor="#2f7fb0", lw=1.0, zorder=11))
    ax.add_patch(Circle(lm["island"]["at"], lm["island"]["radius"], facecolor="#b9cf8e", edgecolor="#e9dfb9", lw=4, zorder=5))
    ax.add_patch(MPoly([(lm["pagoda"]["at"][0] - 3.5, lm["pagoda"]["at"][1] - 3.5), (lm["pagoda"]["at"][0] + 3.5, lm["pagoda"]["at"][1] - 3.5),
                        (lm["pagoda"]["at"][0] + 3.5, lm["pagoda"]["at"][1] + 3.5), (lm["pagoda"]["at"][0] - 3.5, lm["pagoda"]["at"][1] + 3.5)],
                       closed=True, facecolor="#d83a2e", edgecolor=INK, lw=1.0, zorder=12))
    for key in ("sea_torii", "garden_torii"):
        p = lm[key]["at"]
        ax.plot([p[0] - 2.4, p[0] + 2.4], [p[1], p[1]], color="#d83a2e", lw=3.2, zorder=12, solid_capstyle="butt")
    ax.plot(*lm["dead_tree"]["at"], marker=(7, 1, 0), ms=11, color="#2b2420", zorder=12)
    ax.plot(*lm["old_oak"]["at"], marker="o", ms=10, color="#3d7a3a", mec=INK, zorder=12)
    ax.plot(*lm["giant_sakura"]["at"], marker="o", ms=14, color="#f2a7c3", mec="#a0496b", zorder=12)
    ax.plot(*lm["well"]["at"], marker="o", ms=7, color="#8ab4d8", mec=INK, zorder=12)
    ax.plot(*lm["harbour_light"]["at"], marker="*", ms=13, color="#3bb54a", mec=INK, zorder=12)
    ax.plot(*lm["waterwheel"]["at"], marker=(8, 2, 0), ms=12, color="#6b4a2a", zorder=12)
    ax.plot(*lm["crane"]["at"], marker="P", ms=8, color="#6b4a2a", zorder=12)
    g = lm["gallows"]["at"]
    ax.add_patch(MPoly([(g[0] - 1.8, g[1] - 1.5), (g[0] + 1.8, g[1] - 1.5), (g[0] + 1.8, g[1] + 1.5), (g[0] - 1.8, g[1] + 1.5)],
                       closed=True, facecolor="#8a5a2b", edgecolor=INK, lw=0.8, zorder=12))
    for b in L["boats"]:
        ax.add_patch(MPoly([(b[0] - 0.8, b[1] - 2.0), (b[0] + 0.8, b[1] - 2.0), (b[0], b[1] + 2.3)], closed=True,
                           facecolor="#8a5a2b", edgecolor=INK, lw=0.5, zorder=8))
    sp = L["spawn"]
    for k in range(sp["count"]):
        a = 2 * math.pi * k / sp["count"]
        ax.plot(sp["center"][0] + sp["radius"] * math.cos(a), sp["center"][1] + sp["radius"] * math.sin(a), "o", ms=2.6,
                color="#3bb54a", zorder=12)

    # ---------------------------------------------------------------- sightlines
    for s in L["sightlines"]:
        a, b = s["from"], s["to"]
        ax.add_patch(FancyArrowPatch(tuple(a), tuple(b), arrowstyle="-|>", mutation_scale=12, color=SIGHT, lw=1.3,
                                     ls=(0, (5, 3)), zorder=13, alpha=0.85))
        ax.plot(*a, "o", ms=5, color=SIGHT, zorder=13)
        ax.text(a[0] + 1.5, a[1] - 1.8, s["name"].split("_")[0], fontsize=9, color=SIGHT, fontweight="bold", zorder=14,
                path_effects=halo_s)

    # ---------------------------------------------------------------- chores
    tnum = {}
    for k, t in enumerate(L["tasks"]):
        tnum[t["id"]] = k + 1
        opt = t.get("optional")
        ax.add_patch(Circle(t["at"], 2.1, facecolor="white" if opt else CHORE, edgecolor=INK, lw=0.9, zorder=15))
        ax.text(*t["at"], str(k + 1), ha="center", va="center", fontsize=7.5, fontweight="bold", color=INK, zorder=16)

    # ---------------------------------------------------------------- labels
    def label(text, p, size=10, color=INK, rot=0, weight="normal", style="normal"):
        ax.text(p[0], p[1], text, fontsize=size, color=color, rotation=rot, ha="center", va="center", fontweight=weight,
                style=style, zorder=17, path_effects=halo)

    for ml in L.get("map_labels", []):
        label(ml["text"], ml["at"], ml["size"], weight="bold" if ml["size"] >= 10 else "normal",
              style="normal" if ml["size"] >= 10 else "italic")
    # street names along lanes
    names = {"quay_promenade": "Quay Promenade", "rope_walk": "Rope Walk", "market_street": "Market Street",
             "balcony_lane": "Balcony Lane", "back_lane_west": "Back Lane", "back_lane_east": "Back Lane",
             "tide_alley": "Tide Alley", "brook_path": "Brook Path", "mill_lane": "Mill Lane", "orchard_lane": "Orchard Lane",
             "headland_road": "Headland Road", "crown_walk": "Crown Walk", "woods_path": "Woods Path",
             "farm_track": "Farm Track", "sakura_walk": "Sakura Walk", "mole_walk": "Mole Walk", "long_jetty": "Long Jetty",
             "mill_bridge_way": "Old Bridge Way", "hollow_way": "Hollow Way"}
    for l in L["lanes"]:
        if l["name"] not in names:
            continue
        line = LineString(l["points"])
        frac = 0.62 if l["name"] in ("rope_walk",) else 0.5
        p = line.interpolate(line.length * frac)
        q = line.interpolate(min(line.length, line.length * frac + 2.0))
        o = line.interpolate(max(0.0, line.length * frac - 2.0))
        ang = math.degrees(math.atan2(q.y - o.y, q.x - o.x))
        if ang > 90:
            ang -= 180
        if ang < -90:
            ang += 180
        ax.text(p.x, p.y, names[l["name"]], fontsize=7.5, style="italic", color=INK2, rotation=-ang,
                rotation_mode="anchor", ha="center", va="center", zorder=17, path_effects=halo_s)
    for s in L["stairs"]:
        p = ((s["from"][0] + s["to"][0]) / 2, (s["from"][1] + s["to"][1]) / 2)
        nm = s["name"].replace("_", " ").title()
        off = {"grand_stair": (13, 0), "scala": (-10, 0), "pilgrim_stair": (-12, 0), "cat_steps": (-7, 0),
               "net_stairs": (6, -2), "chapel_steps": (-7, 0), "garden_stair": (0, -4), "cliff_stair": (7, 3)}.get(s["name"], (0, 0))
        ax.text(p[0] + off[0], p[1] + off[1], nm, fontsize=7.5, fontweight="bold", color="#3b3530", ha="center",
                va="center", zorder=17, path_effects=halo_s)
    for r in L["ramps"]:
        line = LineString(r["points"])
        p = line.interpolate(0.5, normalized=True)
        ax.text(p.x, p.y + 2.8, r["name"].replace("_", " ") + f" ({r['grade']})", fontsize=6.5, color="#6b5a3c",
                ha="center", zorder=17, path_effects=halo_s)
    for key, text, off in [("town_hall", "Town Hall", (0, 0)), ("inn", "Inn", (0, 0)), ("bakery", "Bakery", (0, 0)),
                           ("church", "Church", (0, 0)), ("bell_tower", "Bell Tower", (8.5, 0)),
                           ("clock_tower", "Clock Tower", (0, -6.5)), ("lighthouse", "Lighthouse", (0, -7.0)),
                           ("fish_market", "Fish Market", (0, 0)), ("boathouse", "Boathouse", (0, 0)),
                           ("mill_barn", "Barn", (0, 0)), ("windmill", "Windmill", (0, -7)),
                           ("smithy", "Smithy", (0, -5.5)), ("pavilion", "Pavilion", (0, 4.8)),
                           ("mausoleum", "Mausoleum", (0, -4.5))]:
        b = lm.get(key)
        if b:
            ax.text(b["at"][0] + off[0], b["at"][1] + off[1], text, fontsize=7.5, ha="center", va="center",
                    color="white" if off == (0, 0) else INK, fontweight="bold", zorder=17,
                    path_effects=None if off == (0, 0) else halo_s)
    for key, text, off in [("fountain", "Fountain", (0, 3.6)), ("gallows", "Moot stage", (0, -3.0)),
                           ("dead_tree", "Dead Tree", (4.5, 0)), ("well", "Well", (0, 2.8)), ("sea_torii", "Sea torii", (0, -2.8)),
                           ("garden_torii", "torii", (-4.0, 0)), ("harbour_light", "Harbour light", (0, 3.5)),
                           ("waterwheel", "Waterwheel", (0, 3.2)), ("graveyard", "Graveyard", (0, 0)),
                           ("koi_pond", "Koi", (4.0, 0)), ("old_oak", "Old Oak", (0, -3.0))]:
        b = lm[key]
        ax.text(b["at"][0] + off[0], b["at"][1] + off[1], text, fontsize=7, ha="center", va="center", color=INK2,
                zorder=17, path_effects=halo_s)
    for sq in L["squares"]:
        if sq["name"] in ("well_court", "pilgrim_garden", "belvedere"):
            c = np.mean(np.array(sq["polygon"]), axis=0)
            dy = {"belvedere": -2.8, "pilgrim_garden": 0.0, "well_court": -2.4}[sq["name"]]
            ax.text(c[0], c[1] + dy, sq["name"].replace("_", " ").title(), fontsize=7, ha="center", color=INK2,
                    style="italic", zorder=17, path_effects=halo_s)
    for b in L["bridges"]:
        ax.text(b["at"][0], b["at"][1] - 3.4, b["name"].replace("_", " "), fontsize=6.5, ha="center", color=INK2,
                zorder=17, path_effects=halo_s)

    # frame, grid, scale
    ax.set_xlim(X0, X1)
    ax.set_ylim(Y1, Y0)          # sea at the bottom
    ax.set_aspect("equal")
    ax.set_xticks(np.arange(-120, 131, 20))
    ax.set_yticks(np.arange(-120, 171, 20))
    ax.tick_params(labelsize=8, colors=INK2)
    ax.grid(color="white", lw=0.4, alpha=0.35, zorder=0.5)
    ax.set_xlabel("x (m, east)   UE cm = m x 100", fontsize=9, color=INK2)
    ax.set_ylabel("y (m, +y toward the sea)", fontsize=9, color=INK2)
    for sp_ in ax.spines.values():
        sp_.set_color("#8c8474")
    sbx, sby = 70.0, 164.0
    for k in range(5):
        ax.add_patch(MPoly([(sbx + k * 10, sby), (sbx + k * 10 + 10, sby), (sbx + k * 10 + 10, sby + 1.6), (sbx + k * 10, sby + 1.6)],
                           closed=True, facecolor=INK if k % 2 == 0 else "white", edgecolor=INK, lw=0.8, zorder=18))
    ax.text(sbx + 25, sby - 2.0, "50 m  =  8.6 s running", fontsize=9, ha="center", zorder=18, path_effects=halo_s)
    ax.annotate("", xy=(122, -112), xytext=(122, -100), arrowprops=dict(arrowstyle="-|>", color=INK, lw=1.5), zorder=18)
    ax.text(122, -97, "inland\n(-y)", fontsize=8, ha="center", va="top", zorder=18)

    # ---------------------------------------------------------------- side panel
    fig.text(0.635, 0.962, "MORROWMERE v2  -  The Amphitheatre Cove", fontsize=26, fontweight="bold", color=INK)
    for k, line in enumerate(["Final plan, drawn from Tools/Level/morrowmere_layout_v2.json.",
                              "One axis: bell tower > Belvedere > Scala > fountain > Grand Stair > jetty > torii > pagoda.",
                              "Terraces quay 2 / Heart 5 / Upper 8 / Crown 14 ring a round basin;",
                              "streets follow the rings, stairs and opes run down the radials."]):
        fig.text(0.635, 0.944 - k * 0.0135, line, fontsize=11, color=INK2)
    lx = fig.add_axes([0.635, 0.58, 0.17, 0.30])
    lx.axis("off")
    handles = [
        Patch(facecolor=HOME, edgecolor="#6e2a10", label="Player home (number = H id), white tip = door"),
        Patch(facecolor=CIVIC, edgecolor="#0e3566", label="Civic building (hatched = open hall/shed)"),
        Patch(facecolor=INFILL, edgecolor="#5f5546", label="Infill shell (exterior only, street wall)"),
        Patch(facecolor=TOWER, edgecolor="#1d1450", label="Climbable tower (ladder)"),
        Patch(facecolor=LANE, edgecolor=LANE_EDGE, label="Street / lane (true width)"),
        Patch(facecolor=SQUARE, edgecolor=LANE_EDGE, label="Square / court"),
        Patch(facecolor="#ddd3c1", edgecolor="#6d6254", label="Kit stair (arrow = up, hidden ramp collision)"),
        Patch(facecolor="#e7d9bd", edgecolor="#8b7a5c", label="Graded ramp (chevrons = up)"),
        Line2D([], [], color=WALL, lw=4, label="Retaining wall (thicker = 2 courses / 6 m)"),
        Line2D([], [], color=CLIFF, lw=2, path_effects=[pe.withTickedStroke(spacing=5, angle=-60, length=1.1)], label="Sea cliff"),
        Line2D([], [], color="#7cc3e0", lw=5, label="Morrow Brook / koi spill"),
        Patch(facecolor="#a8784c", edgecolor="#5a3d24", label="Timber jetty, pier, bridge"),
        Patch(facecolor="#e8d58a", edgecolor="#7c6a45", hatch="||", label="Walled field (dry-stone) / apple orchard"),
        Patch(facecolor="#b5cf8f", edgecolor="#8a9a6a", hatch="....", label="Walled back garden / yard"),
        Line2D([], [], color="#5f5546", lw=1.6, label="Garden wall + gate closing the street wall"),
        Line2D([], [], color="#5f5546", lw=4.2, label="Arch over an ope (sottoportego)"),
        Line2D([], [], marker="o", color="w", markerfacecolor=CHORE, markeredgecolor=INK, ms=10, label="Chore (white = new optional)"),
        Line2D([], [], color=SIGHT, lw=1.5, ls=(0, (5, 3)), label="Designed sightline (checked in 3D)"),
        Line2D([], [], marker="o", color="w", markerfacecolor="#3bb54a", ms=6, label="Player start ring (20)"),
    ]
    lx.legend(handles=handles, loc="upper left", fontsize=9.5, frameon=False, labelcolor=INK, handlelength=2.6)

    cb_ax = fig.add_axes([0.815, 0.87, 0.16, 0.012])
    cb = matplotlib.colorbar.ColorbarBase(cb_ax, cmap=HEIGHT, norm=matplotlib.colors.Normalize(ZMIN, ZMAX),
                                          orientation="horizontal")
    cb.set_ticks([0, 2, 5, 8, 11, 14, 18])
    cb.ax.tick_params(labelsize=8)
    cb.set_label("terrace height z (m); sea 0, basin floor -3", fontsize=9)

    # chores table
    tx = fig.add_axes([0.815, 0.40, 0.17, 0.44])
    tx.axis("off")
    tt = {tid: sec for tid, sec, _ in times}
    lines = ["CHORES  (run time from the fountain)"]
    for k, t in enumerate(L["tasks"]):
        star = "*" if t.get("optional") else " "
        lines.append(f"{k + 1:2d}{star} {t['id']:<17s}{tt.get(t['id'], float('nan')):5.1f} s  {t['district'][:12]}")
    tx.text(0, 1, "\n".join(lines), fontsize=9.2, family="monospace", va="top", color=INK)

    # metrics
    mx = fig.add_axes([0.635, 0.40, 0.17, 0.19])
    mx.axis("off")
    mtxt = ["PLAN METRICS (computed)"] + [s for s in info] + [
        f"{len(L['lanes'])} lanes, {len(L['stairs'])} stairs, {len(L['ramps'])} ramps",
        f"{len(L['retaining_walls'])} retaining-wall runs, {len(L['bridges'])} crossings",
        f"validator: {'PASS' if not err else 'FAIL (' + str(len(err)) + ' errors)'}"]
    mx.text(0, 1, "\n".join(mtxt), fontsize=10, va="top", color=INK, linespacing=1.5)

    # section along the axis
    sx_ = fig.add_axes([0.645, 0.05, 0.34, 0.30], facecolor="#eef3f6")
    a = np.array(L["axis"]["from"])
    b = np.array(L["axis"]["to"])
    total = np.linalg.norm(b - a)
    u = (b - a) / total
    ss = np.arange(-25.0, total + 20.0, 0.5)
    gz = []
    for s in ss:
        p = a + u * s
        zz = W.ground_z(tuple(p))
        if zz is None:
            zz = 18.0 if s < 0 else 0.0
        if not W.land.covers(__import__("shapely.geometry", fromlist=["Point"]).Point(*p)) and zz <= 0.0:
            isl = math.dist(p, L["landmarks"]["island"]["at"])
            zz = 5.0 * max(0.0, 1 - (isl / L["landmarks"]["island"]["radius"]) ** 4) if isl < 17 else \
                (-3.0 if math.dist(p, L["basin"]["center"]) < L["basin"]["radius"] else -1.0 - min(8.0, (p[1] - 90) / 8.0))
        gz.append(zz)
    gz = np.array(gz)
    sx_.fill_between(ss, -16, gz, color="#8fb573", zorder=2)
    sx_.fill_between(ss, gz, 0, where=gz < 0, color=SEA_SHALLOW, zorder=1)
    sx_.axhline(0, color="#2f7fb0", lw=0.8)
    # buildings the axis passes near (within 7 m laterally) as masses
    from shapely.geometry import Point as SP
    for bid, cat, bb in W.buildings:
        c = np.array(bb["at"])
        rel = c - a
        s = rel @ u
        lat = abs(rel @ np.array([-u[1], u[0]]))
        if lat < 20 and -25 <= s <= total:
            top = W.top_z(bid, bb)
            w = max(bb["size"])
            col = {"home": HOME, "infill": INFILL, "tower": TOWER}.get(cat, CIVIC)
            sx_.add_patch(MPoly([(s - w / 2, bb["z"]), (s + w / 2, bb["z"]), (s + w / 2, top), (s - w / 2, top)], closed=True,
                                facecolor=col, edgecolor=INK, lw=0.5, alpha=0.35 if lat > 7 else 0.9, zorder=3))
    for key, text in [("fountain", "fountain"), ("sea_torii", "torii"), ("pagoda", "pagoda")]:
        p = np.array(lm[key]["at"])
        s = (p - a) @ u
        top = {"fountain": 8.3, "sea_torii": 9.0, "pagoda": 19.0}[key]
        sx_.plot([s, s], [lm[key]["z"] or 0, top], color="#d83a2e" if key != "fountain" else "#2f7fb0", lw=3, zorder=4)
        sx_.text(s, top + 1.0, text, fontsize=8, ha="center")
    for s_ in L["sightlines"]:
        if s_["name"] in ("S1_postcard", "S2_pilgrim", "S3_belvedere"):
            pa, pb = np.array(s_["from"]), np.array(s_["to"])
            sa, sb = (pa - a) @ u, (pb - a) @ u
            sx_.plot([sa, sb], [s_["eye_z"], s_["target_z"]], color=SIGHT, lw=1.0, ls=(0, (4, 3)), zorder=5)
    for key, text, dz in [("bell_tower", "bell tower", 1.5), ("church", "church", 1.5), ("town_hall", "town hall", 1),
                          ("clock_tower", "clock", 1)]:
        p = np.array(lm[key]["at"])
        s = (p - a) @ u
        sx_.text(s, W.top_z(key, lm[key]) + dz, text, fontsize=8, ha="center")
    for t in L["terraces"]:
        if t.get("z") is not None and t["name"] != "garden":
            pass
    sx_.set_xlim(-25, total + 20)
    sx_.set_ylim(-16, 40)
    sx_.set_xlabel("metres along the axis from the bell tower toward the pagoda", fontsize=9)
    sx_.set_ylabel("z (m), vertical x2.5", fontsize=9)
    sx_.set_aspect(2.5)
    sx_.set_title("Section along the Processional Axis (crown 14 > upper 8 > heart 5 > quay 2 > basin -3 > island)",
                  fontsize=10.5, loc="left")
    sx_.tick_params(labelsize=8)
    sx_.grid(color="white", lw=0.6)

    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=100, facecolor=PAPER)
    print(f"wrote {out}  ({'PASS' if not err else str(len(err)) + ' validation errors'})")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    src = args[0] if args else V.DEFAULT
    dst = args[1] if len(args) > 1 else os.path.join(ROOT, "Docs", "Level", "Morrowmere_v2.png")
    draw(V.load(src), dst)
