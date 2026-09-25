"""Top-down plot of a v2 dressing plan/report (Saved/KG_V2_DressPlan.json or KG_V2_DressReport.json) over the layout:
walkways, buildings + doors, reserved areas (red), placements coloured by kind. System Python (matplotlib).

    python Tools/Level/dress_plan_plot.py [plan.json] [zone ...]   -> Saved/KG_V2_DressPlan_<zone|core>.png
"""
import json
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import Polygon as MPoly, Circle  # noqa: E402

ROOT = "D:/Kill Godot"
sys.path.insert(0, f"{ROOT}/Tools/Unreal")

COL = {"actor": "#1f5fbf", "inst": "#2e9e3e", "seat": "#ff8c00", "breakable": "#8b4513", "chest": "#ffd700",
       "mover": "#9b30ff", "light": "#ffee00", "sound": "#888888"}


def _extent(C, zone):
    if zone == "core":
        return (-60 * 100, 60 * 100, -70 * 100, 75 * 100)
    if zone == "square":
        return (-8 * 100, 28 * 100, -18 * 100, 28 * 100)
    if zone == "japan":
        return (38 * 100, 70 * 100, -18 * 100, 34 * 100)
    if zone == "church":
        return (-50 * 100, 15 * 100, -75 * 100, -35 * 100)
    if zone == "harbour":
        return (-16 * 100, 50 * 100, 20 * 100, 104 * 100)
    polys = C.zone_polys(zone)
    if not polys:
        return (-13000, 13000, -13000, 13000)
    xs = [p[0] for poly in polys for p in poly]
    ys = [p[1] for poly in polys for p in poly]
    pad = 800.0
    return (min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad)


def plot(plan_path, zones=None, out_dir=f"{ROOT}/Saved"):
    import kg_dress_common_v2 as C
    plan = json.load(open(plan_path))
    rec = plan["record"]
    areas = list(zones or []) or ["core"]
    if "core" not in areas and any(z in ("square", "harbour", "streets") for z in areas):
        areas.append("core")
    outs = []
    for area in areas:
        x0, x1, y0, y1 = _extent(C, area)
        w = (x1 - x0) / 100.0
        h = (y1 - y0) / 100.0
        scale = 22.0 / max(w, h)
        fig, ax = plt.subplots(figsize=(max(6, w * scale), max(6, h * scale)), dpi=110)
        ax.set_facecolor("#e9f0dc")
        for name, poly, _bb in C._ZONES:
            ax.add_patch(MPoly(poly, closed=True, fill=False, ec="#9a9a9a", lw=0.6, ls="--"))
        for t, poly in C._TERR:
            ax.add_patch(MPoly(poly, closed=True, fc="#d8e4c4" if t.get("z") is None else "#e4dcc8", ec="#b0a58a", lw=0.4))
        for n, k, wdt, pts in C.WALKWAYS:
            xs, ys = zip(*pts)
            ax.plot(xs, ys, color="#c9b48a" if k not in ("stair", "ramp") else "#a08060", lw=max(0.5, wdt * scale * 0.72),
                    solid_capstyle="butt", alpha=0.85, zorder=1)
        ax.plot(*zip(*C._STREAM), color="#5aa0d8", lw=C._STREAM_W * scale * 0.72, alpha=0.8, zorder=1)
        for sq in C.LAYOUT["squares"]:
            ax.add_patch(MPoly(C.cm(sq["polygon"]), closed=True, fc="#e8d6b0", ec="#b09060", lw=0.5, zorder=1))
        for ax_, ay, bx, by, r, why in C.reserved():
            c = "#ff3030"
            if abs(ax_ - bx) + abs(ay - by) < 1:
                ax.add_patch(Circle((ax_, ay), r, fc=c, ec="none", alpha=0.18, zorder=2))
            else:
                ax.plot([ax_, bx], [ay, by], color=c, lw=2 * r * scale * 0.72,
                        alpha=0.18, solid_capstyle="round", zorder=2)
        for x_, y_, r_, f_, m_ in C.existing():
            if x0 < x_ < x1 and y0 < y_ < y1:
                ax.add_patch(Circle((x_, y_), r_, fc="#555555", ec="none", alpha=0.35, zorder=2))
        for f in C.buildings():
            pts = [f.world(sx * f.w / 2, sy * f.d / 2) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            ax.add_patch(MPoly(pts, closed=True, fc="#c0504d" if f.kind == "home" else "#9a8f86" if f.kind == "infill" else "#4f81bd",
                               ec="#333", lw=0.4, alpha=0.9, zorder=3))
            if f.door:
                ax.plot(*f.door, "w^", ms=3, zorder=4)
            if x0 < f.x < x1 and y0 < f.y < y1:
                ax.text(f.x, f.y, f.id if f.kind != "infill" else "", fontsize=4, ha="center", va="center", zorder=5)
        for r in rec:
            if not (x0 < r["x"] < x1 and y0 < r["y"] < y1):
                continue
            k = r["kind"]
            if k == "light":
                ax.plot(r["x"], r["y"], "*", color=COL[k], mec="#aa8800", ms=6, zorder=7)
                continue
            if k == "chest":
                ax.plot(r["x"], r["y"], "s", color=COL[k], mec="k", ms=5, zorder=7)
                continue
            if r.get("o") and k != "inst":
                ax.plot(r["x"], r["y"], ".", color=COL.get(k, "k"), ms=3, zorder=7)
                continue
            rad = max(8.0, min(r["r"], 500.0 if not r.get("o") else 60.0))
            ax.add_patch(Circle((r["x"], r["y"]), rad, fc=COL.get(k, "k"), ec="none" if k == "inst" else "k",
                                lw=0.2, alpha=0.35 if k == "inst" else 0.6, zorder=6))
        ax.set_xlim(x0, x1)
        ax.set_ylim(y1, y0)            # +y (sea) down, like the plan drawing
        ax.set_aspect("equal")
        ax.set_title(f"v2 dressing plan: {area}  ({sum(1 for r in rec if x0 < r['x'] < x1 and y0 < r['y'] < y1)} placements)")
        out = os.path.join(out_dir, f"KG_V2_DressPlan_{area}.png")
        fig.savefig(out, bbox_inches="tight")
        plt.close(fig)
        outs.append(out)
        print(f"plot -> {out}")
    return outs


if __name__ == "__main__":
    a = sys.argv[1:]
    path = a[0] if a and a[0].endswith(".json") else f"{ROOT}/Saved/KG_V2_DressPlan.json"
    zs = [z for z in a if not z.endswith(".json")]
    plot(path, zs)
