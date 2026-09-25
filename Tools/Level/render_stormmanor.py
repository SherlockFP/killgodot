"""Render the Storm Manor layout JSON as a floor-by-floor plan PNG (map 2; companion of render_layout.py, which
draws Morrowmere v2 and stays untouched).

    python Tools/Level/render_stormmanor.py [layout.json] [out.png]

Default: Tools/Level/stormmanor_layout.json -> Docs/Level/StormManor.png. Four plan panels at one scale (ground floor
+ grounds, first floor, cellar + sea level, attic + tower top), each drawn only from the JSON: rooms coloured by
wing, walls, doors, windows (rattling ones in red), stairs with their direction, secret passages, chore spots
(numbered like the chore table), hiding spots, chokepoints, the meeting table and the spawn ring. The right column
holds the legend, the chore table with the times computed by validate_stormmanor.py, and the Region Gate table.
"""
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.patheffects as pe  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.patches import Circle, FancyArrowPatch, Patch, Polygon as MPoly, Rectangle  # noqa: E402
from shapely.geometry import Polygon  # noqa: E402
from shapely.ops import unary_union  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import validate_stormmanor as V  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
X0, X1, Y0, Y1 = -56.0, 54.0, -28.0, 53.0

INK, INK2, PAPER = "#1f1d1a", "#5b5750", "#f5f1e8"
SEA, SEA2, ROCK = "#3d6f8e", "#5a8aa6", "#8c8378"
WING = {
    "hall": "#f2c46b", "west": "#e8906a", "east": "#86b2e0", "service": "#b9b2a6", "cellar": "#b89f86",
    "guest": "#e59ab0", "master": "#7cc4b4", "upper": "#b3a0dc", "grounds": "#a9cf8c",
}
RISK = {"güvenli": "#2e9d57", "orta": "#e0a21a", "tehlikeli": "#d23b3b"}
CHORE, HIDE, SECRET, WIN, WIN_RATTLE, STAIR = "#f08a00", "#1f8a4c", "#7a2fbf", "#2aa7d8", "#e0314b", "#40372f"
HALO = [pe.withStroke(linewidth=3.2, foreground="white")]
HALO2 = [pe.withStroke(linewidth=2.2, foreground="white")]

PANELS = [("F0", "ZEMİN KAT + BAHÇE  (z 0)"), ("F1", "BİRİNCİ KAT  (z +3)"),
          ("C", "MAHZEN + DENİZ SEVİYESİ  (z −3)"), ("F2", "ÇATI KATI + KULE  (z +6 / +9)")]


def edges(poly):
    n = len(poly)
    for i in range(n):
        yield poly[i], poly[(i + 1) % n]


def wall_axis(poly, p):
    """'v' when p lies on a vertical edge of poly, 'h' on a horizontal one."""
    for a, b in edges(poly):
        if abs(a[0] - b[0]) < 1e-6 and abs(p[0] - a[0]) < 0.06 and min(a[1], b[1]) - 0.06 <= p[1] <= max(a[1], b[1]) + 0.06:
            return "v"
        if abs(a[1] - b[1]) < 1e-6 and abs(p[1] - a[1]) < 0.06 and min(a[0], b[0]) - 0.06 <= p[0] <= max(a[0], b[0]) + 0.06:
            return "h"
    return "h"


def seg(ax, p, axis, length, **kw):
    h = length / 2.0
    if axis == "v":
        ax.plot([p[0], p[0]], [p[1] - h, p[1] + h], solid_capstyle="butt", **kw)
    else:
        ax.plot([p[0] - h, p[0] + h], [p[1], p[1]], solid_capstyle="butt", **kw)


def floors_of(fid):
    return ["F2", "F3"] if fid == "F2" else [fid]


def draw_panel(ax, L, fid, title, res, chore_no):
    RM = {r["id"]: r for r in L["rooms"]}
    fl = floors_of(fid)
    ax.set_xlim(X0, X1)
    ax.set_ylim(Y1, Y0)          # +y south is down, like the UE top view
    ax.set_aspect("equal")
    ax.set_facecolor(SEA)
    ax.set_xticks([])
    ax.set_yticks([])
    for s in ax.spines.values():
        s.set_color(INK)
        s.set_linewidth(1.5)
    # sea swell lines + the rock under the whole manor
    for k in range(-30, 60, 6):
        ax.plot([X0, X1], [k + 1.5, k - 1.0], color=SEA2, lw=0.8, alpha=0.5, zorder=0)
    rock = unary_union([Polygon(r["poly"]) for r in L["rooms"] if r["floor"] in ("F0", "C")]).buffer(3.5, join_style=2)
    for g in getattr(rock, "geoms", [rock]):
        ax.add_patch(MPoly(list(g.exterior.coords), closed=True, fc=ROCK, ec="#6d655c", lw=1.2, zorder=1))
    # ghost of the ground floor under the upper floors
    if fid in ("F1", "F2", "C"):
        base = unary_union([Polygon(r["poly"]) for r in L["rooms"] if r["floor"] == "F0" and r["kind"] != "grounds"])
        for g in getattr(base, "geoms", [base]):
            ax.add_patch(MPoly(list(g.exterior.coords), closed=True, fc="#d8d2c6", ec="#9d968a", lw=1.0, ls="--",
                               zorder=2))
        if fid == "C":
            gr = [r for r in L["rooms"] if r["floor"] == "F0" and r["kind"] == "grounds"]
            for r in gr:
                ax.add_patch(MPoly(r["poly"], closed=True, fc="#b7ad9f", ec="#9d968a", lw=0.8, ls=":", zorder=2))
    # rooms
    for r in L["rooms"]:
        if r["floor"] not in fl:
            continue
        outdoor = r["kind"] in ("grounds", "outdoor")
        fc = WING.get(r["wing"], "#ccc")
        z = 3 if r["floor"] != "F3" else 6
        ax.add_patch(MPoly(r["poly"], closed=True, fc=fc, ec=INK, lw=0.6 if outdoor else 2.4, zorder=z,
                           hatch="...." if outdoor else ("////" if r["kind"] == "circulation" else None),
                           alpha=0.9 if r["floor"] == "F3" else 1.0))
        if r["floor"] == "F3":
            ax.text(30, -25.2, "↑ Kule Tepesi F3 (z +9)", ha="center", va="bottom", fontsize=8.5, color=INK,
                    path_effects=HALO2, zorder=20)
    if fid == "F1":
        v = RM["great_hall"]["void"]
        ax.add_patch(MPoly(v, closed=True, fc="#fbf3d9", ec="#8a7a55", lw=1.4, ls=(0, (4, 2)), zorder=3))
        ax.text(0, -8.5, "SALON BOŞLUĞU\n(çift kat yüksekliği,\nkorkuluktan aşağı bakılır)", ha="center",
                va="center", fontsize=8, color="#6b5d3a", style="italic", zorder=12)
    # doors
    for d in L["doors"]:
        if RM[d["a"]]["floor"] not in fl:
            continue
        axis = wall_axis(RM[d["a"]]["poly"], d["at"])
        seg(ax, d["at"], axis, d["width"] - 0.3, color="#fdfaf2", lw=5.5, zorder=8)
        col = "#8a5a2b" if d["kind"] not in ("gate",) else "#333"
        seg(ax, d["at"], axis, d["width"] - 0.3, color=col, lw=1.2, zorder=9)
    # windows
    for w in L["windows"]:
        if w["floor"] not in fl:
            continue
        col = WIN_RATTLE if w.get("rattle") else WIN
        seg(ax, w["at"], w["axis"], 1.4 if w["kind"] == "exterior" else 4.0, color=col, lw=3.2, zorder=7)
    # stairs
    FZ = {f["id"]: f["z"] for f in L["floors"]}
    for s in L["stairs"]:
        lo, up = RM[s["lower"]]["floor"], RM[s["upper"]]["floor"]
        b, t = s["bottom"], s["top"]
        if lo in fl:
            dx, dy = t[0] - b[0], t[1] - b[1]
            n = math.hypot(dx, dy)
            if s["kind"] in ("spiral", "ladder") or n < 0.1:
                c = Circle(b, 1.2 if s["kind"] == "spiral" else 0.8, fc="#efe6d2", ec=STAIR, lw=1.6, zorder=10)
                ax.add_patch(c)
                ax.text(b[0], b[1], "⟳" if s["kind"] == "spiral" else "H", ha="center", va="center", fontsize=9,
                        color=STAIR, zorder=11)
            else:
                ux, uy = dx / n, dy / n
                px, py = -uy * s["width"] / 2, ux * s["width"] / 2
                quad = [(b[0] + px, b[1] + py), (t[0] + px, t[1] + py), (t[0] - px, t[1] - py), (b[0] - px, b[1] - py)]
                ax.add_patch(MPoly(quad, closed=True, fc="#efe6d2", ec=STAIR, lw=1.4, zorder=10))
                for k in range(1, int(n / 0.6)):
                    q = (b[0] + ux * k * 0.6, b[1] + uy * k * 0.6)
                    ax.plot([q[0] + px, q[0] - px], [q[1] + py, q[1] - py], color=STAIR, lw=0.5, zorder=10)
                ax.add_patch(FancyArrowPatch(b, t, arrowstyle="-|>", mutation_scale=11, color=STAIR, lw=1.3,
                                             zorder=11))
            lab = next(f for f in L["floors"] if f["id"] == up)["id"]
            ax.text(t[0], t[1] - 1.3 if s["kind"] != "outdoor" else t[1] + 1.8, f"↑{lab}", ha="center",
                    va="center", fontsize=7.5, color=STAIR, weight="bold", path_effects=HALO2, zorder=12)
        if up in fl and s["kind"] not in ("spiral", "ladder"):
            ax.add_patch(Circle(t, 0.9, fc="none", ec=STAIR, lw=1.2, ls="--", zorder=10))
            ax.text(t[0], t[1] + 1.6, f"↓{lo}", ha="center", va="center", fontsize=7.5, color=STAIR, weight="bold",
                    path_effects=HALO2, zorder=12)
    # secret passages
    for s in L["secrets"]:
        ends = [(k, s[k], s["at_" + k]) for k in ("a", "b")]
        here = [(k, rid, p) for k, rid, p in ends if RM[rid]["floor"] in fl]
        if len(here) == 2 and RM[s["a"]]["floor"] == RM[s["b"]]["floor"]:
            ax.plot([here[0][2][0], here[1][2][0]], [here[0][2][1], here[1][2][1]], color=SECRET, lw=1.6,
                    ls=(0, (2, 1.5)), zorder=13)
        for k, rid, p in here:
            other = s["b"] if k == "a" else s["a"]
            ax.plot(*p, marker="*", ms=14, mfc=SECRET, mec="white", mew=1.0, zorder=14)
            of = RM[other]["floor"]
            txt = s["id"] if of == RM[rid]["floor"] else f"{s['id']}→{of}"
            ax.text(p[0] + 0.9, p[1] - 1.0, txt, fontsize=8, color=SECRET, weight="bold", path_effects=HALO2,
                    zorder=15)
    # hides
    for r in L["rooms"]:
        if r["floor"] in fl:
            for h in r.get("hides", []):
                ax.plot(*h["at"], marker="v", ms=7, mfc=HIDE, mec="white", mew=0.8, zorder=13)
    # chore anchors (numbered by chore)
    for a in L["anchors"]:
        if RM[a["room"]]["floor"] not in fl:
            continue
        nums = chore_no.get(a["id"], [])
        if not nums:
            continue
        ax.add_patch(Circle(a["at"], 0.95, fc=CHORE, ec="white", lw=1.0, zorder=16))
        ax.text(a["at"][0], a["at"][1], ",".join(str(n) for n in nums), ha="center", va="center",
                fontsize=6.3 if len(nums) > 1 else 7.2, color="white", weight="bold", zorder=17)
    # meeting table + spawn ring
    if fid == "F0":
        sp = L["spawn"]
        c = sp["center"]
        ax.add_patch(Rectangle((c[0] - 1.1, c[1] - 4.5), 2.2, 9.0, fc="#8a5a2b", ec=INK, lw=1.0, zorder=12))
        for k in range(sp["count"]):
            t = 2 * math.pi * k / sp["count"]
            ax.plot(c[0] + sp["radius"] * math.cos(t), c[1] + sp["radius"] * 0.95 * math.sin(t), "o", ms=3.6,
                    mfc="#fff", mec=INK, mew=0.7, zorder=12)
        ax.text(c[0], c[1] + 0.2, "TOPLANTI\nMASASI", ha="center", va="center", rotation=90, fontsize=6.5,
                color="white", weight="bold", zorder=13)
    # chokepoints
    for cp in L.get("chokepoints", []):
        if cp["floor"] in fl:
            ax.plot(*cp["at"], marker="D", ms=8, mfc="#d23b3b", mec="white", mew=1.0, zorder=18)
    # labels
    for r in L["rooms"]:
        if r["floor"] not in fl or r["floor"] == "F3":
            continue
        pg = Polygon(r["poly"])
        if r["id"] == "gallery":
            p = (0.0, -18.0 + 0.0)
            p = (-1.0, -17.2)
        elif r["id"] == "great_hall" and fid == "F0":
            p = (-5.2, -16.0)
        elif r["id"] == "smugglers_tunnel":
            p = (-28.0, 10.0)
        else:
            p = pg.representative_point().coords[0] if not pg.centroid.within(pg) else pg.centroid.coords[0]
        w = pg.bounds[2] - pg.bounds[0]
        hgt = pg.bounds[3] - pg.bounds[1]
        big = r["counts"]
        fs = 10.5 if (big and min(w, hgt) >= 10) else (9.0 if big else 8.2)
        name = r["name_tr"]
        if rot_ok := (big and w < 14 and " " in name and len(name) > 13):
            name = "\n".join(name.rsplit(" ", 1))
        rot = 90 if (hgt > 2.2 * w and w <= 6) else 0
        if r["id"] == "smugglers_tunnel":
            rot = 90
        t = res["room_max"].get(r["id"], 0)
        sub = f"{t:.0f} sn" + (f" · N≥{r['min_n']}" if r["min_n"] > 6 else "")
        dy = 1.5 if rot_ok else 0.9
        ax.text(p[0], p[1] - (dy if rot == 0 else 0), name, ha="center", va="center", fontsize=fs,
                weight="bold" if big else "normal", style="normal" if big else "italic", color=INK, rotation=rot,
                path_effects=HALO, zorder=19)
        if rot == 0:
            ax.text(p[0] + 0.5, p[1] + dy, sub, ha="center", va="center", fontsize=7.2, color=INK2,
                    path_effects=HALO2, zorder=19)
            ax.plot(p[0] + 0.5 - len(sub) * 0.26 - 0.8, p[1] + dy, "o", ms=6, mfc=RISK[r["risk"]], mec="white",
                    mew=0.8, zorder=19)
    if fid == "F0":
        ax.text(0, 51.5, "↓ kara (Morrowmere) ~ 900 m", ha="center", va="bottom", fontsize=8.5, color="white",
                style="italic", zorder=20)
    if fid == "F0":
        ax.text(X1 - 1, Y0 + 1.2, "K ↑", ha="right", va="top", fontsize=11, color="white", weight="bold", zorder=20)
    # scale bar
    ax.plot([X0 + 3, X0 + 23], [Y1 - 3, Y1 - 3], color="white", lw=3, solid_capstyle="butt", zorder=20)
    for k in range(0, 21, 10):
        ax.plot([X0 + 3 + k] * 2, [Y1 - 3.8, Y1 - 2.2], color="white", lw=1.5, zorder=20)
    ax.text(X0 + 13, Y1 - 4.5, "20 m  (≈ 3.4 sn koşu)", ha="center", va="bottom", fontsize=8, color="white",
            zorder=20)
    ax.set_title(title, fontsize=15, weight="bold", color=INK, loc="left", pad=6)


def draw_info(ax, L, res, chore_no_list):
    ax.axis("off")
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    y = 0.995
    ax.text(0, y, "Lejant", fontsize=13, weight="bold", va="top", color=INK)
    y -= 0.03
    items = [
        (Patch(fc=WING["hall"], ec=INK), "Büyük Salon / giriş"), (Patch(fc=WING["west"], ec=INK), "Batı kanadı (hizmet)"),
        (Patch(fc=WING["east"], ec=INK), "Doğu kanadı (tören)"), (Patch(fc=WING["guest"], ec=INK), "Misafir katı"),
        (Patch(fc=WING["master"], ec=INK), "Efendi dairesi"), (Patch(fc=WING["upper"], ec=INK), "Çatı ve kule"),
        (Patch(fc=WING["cellar"], ec=INK), "Mahzen"), (Patch(fc=WING["grounds"], ec=INK, hatch="...."), "Bahçe / açık hava"),
        (Patch(fc="#ddd", ec=INK, hatch="////"), "Koridor (dolaşım)"),
        (Line2D([], [], color="#8a5a2b", lw=4), "Kapı"), (Line2D([], [], color=WIN, lw=4), "Pencere"),
        (Line2D([], [], color=WIN_RATTLE, lw=4), "Takırdayan pencere"),
        (Line2D([], [], marker="*", ls="", ms=13, mfc=SECRET, mec="white"), "Gizli geçit (S1–S6)"),
        (Line2D([], [], marker="o", ls="", ms=11, mfc=CHORE, mec="white"), "Görev noktası (no.)"),
        (Line2D([], [], marker="v", ls="", ms=9, mfc=HIDE, mec="white"), "Saklanma yeri"),
        (Line2D([], [], marker="D", ls="", ms=8, mfc="#d23b3b", mec="white"), "Darboğaz"),
        (Line2D([], [], marker="o", ls="", ms=7, mfc=RISK["güvenli"], mec="white"), "Risk: güvenli"),
        (Line2D([], [], marker="o", ls="", ms=7, mfc=RISK["orta"], mec="white"), "Risk: orta"),
        (Line2D([], [], marker="o", ls="", ms=7, mfc=RISK["tehlikeli"], mec="white"), "Risk: tehlikeli"),
        (Patch(fc="#efe6d2", ec=STAIR), "Merdiven (ok = yukarı)"),
    ]
    leg = ax.legend([h for h, _ in items], [t for _, t in items], loc="upper left", bbox_to_anchor=(0, y), ncol=2,
                    fontsize=9, frameon=False, handlelength=1.8, columnspacing=1.0, labelspacing=0.55)
    ax.add_artist(leg)
    y -= 0.175
    ax.text(0, y, "Oda etiketi: ad · en uzak noktadan toplantıya koşu süresi · bölge kapısı (N≥)",
            fontsize=8.5, color=INK2, va="top")
    y -= 0.035
    ax.text(0, y, "Görevler  (salondan yürüyerek + taşıma + iş, en kötü varyant)", fontsize=13, weight="bold",
            va="top", color=INK)
    y -= 0.028
    worst = {}
    for c in res["chores"]:
        worst[c["chore"]] = max(worst.get(c["chore"], 0), c["t"])
    AN = {a["id"]: a for a in L["anchors"]}
    RM = {r["id"]: r for r in L["rooms"]}
    for i, ch in enumerate(L["chores"], 1):
        v = V.variants(ch)[0]
        rooms = []
        for st in ch["steps"]:
            for a in V.resolve(st["at"], v["vars"]):
                rn = RM[AN[a]["room"]]["name_tr"]
                if not rooms or rooms[-1] != rn:
                    rooms.append(rn)
        route = " → ".join(rooms[:4]) + (" …" if len(rooms) > 4 else "")
        nv = len(ch.get("variants", []))
        ax.text(0.0, y, f"{i:2d}", fontsize=9.2, weight="bold", color="white", va="top",
                bbox=dict(boxstyle="round,pad=0.18", fc=CHORE, ec="none"))
        ax.text(0.055, y, ch["title_tr"] + (f"  ({nv} varyant)" if nv > 1 else ""), fontsize=9.2, weight="bold",
                va="top", color=INK)
        ax.text(0.99, y, f"{worst[ch['id']]:.0f} sn", fontsize=9.2, va="top", ha="right", color=INK)
        ax.text(0.055, y - 0.0135, route, fontsize=7.8, va="top", color=INK2)
        y -= 0.0282
    y -= 0.008
    ax.text(0, y, "Bölge Kapıları (az oyuncuda harita küçülür)", fontsize=13, weight="bold", va="top", color=INK)
    y -= 0.03
    ax.text(0, y, "  N    alan  oda  görev   m²/oyuncu", fontsize=9, family="monospace", va="top", color=INK2)
    y -= 0.02
    for g in res["gates"]:
        if g["N"] in (6, 8, 10, 12, 20):
            ax.text(0, y, f"{g['N']:3d}   {g['areas']:4d} {g['rooms']:4d} {g['chores']:5d}   {g['m2_per_player']:6d}",
                    fontsize=9, family="monospace", va="top", color=INK)
            y -= 0.017
    y -= 0.008
    ok = "GEÇTİ" if not res["err"] else "KALDI"
    ax.text(0, y, f"Doğrulama: {ok}  ·  toplanma p90 {res['p90']:.1f} sn, en fazla {res['tmax']:.1f} sn (sınır 30)",
            fontsize=9.5, weight="bold", va="top", color="#2e7d32" if not res["err"] else "#c62828")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    src = args[0] if args else V.DEFAULT
    out = args[1] if len(args) > 1 else os.path.join(ROOT, "Docs", "Level", "StormManor.png")
    L = V.load(src)
    res = V.analyse(L)
    chore_no = {}
    for i, ch in enumerate(L["chores"], 1):
        seen = set()
        for v in V.variants(ch):
            for st in ch["steps"]:
                for a in V.resolve(st["at"], v["vars"]):
                    if a not in seen:
                        seen.add(a)
                        chore_no.setdefault(a, []).append(i)
    for s in L["storm"]["outage"]["fix_spots"]:
        chore_no.setdefault(s, [])
    fig = plt.figure(figsize=(33, 20.5), facecolor=PAPER)
    gs = fig.add_gridspec(2, 3, width_ratios=[1, 1, 0.52], left=0.012, right=0.992, top=0.915, bottom=0.012,
                          wspace=0.035, hspace=0.07)
    pos = [(0, 0), (0, 1), (1, 0), (1, 1)]
    for (fid, title), (r, c) in zip(PANELS, pos):
        draw_panel(fig.add_subplot(gs[r, c]), L, fid, title, res, chore_no)
    draw_info(fig.add_subplot(gs[:, 2]), L, res, chore_no)
    fig.suptitle("FIRTINALI MALİKÂNE  ·  Storm Manor (harita 2)  —  kat kat plan", x=0.012, ha="left", fontsize=26,
                 weight="bold", color=INK, y=0.985)
    fig.text(0.012, 0.952, "Pozzo'nun Kuzgun Kayası'ndaki malikânesi · 24 adlandırılmış oda, 3 kat + mahzen + bahçe · "
             "toplantı: Büyük Salon · ölçek her panelde aynı · üretildi: Tools/Level/render_stormmanor.py",
             fontsize=11.5, color=INK2)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=80, facecolor=PAPER)
    print(f"wrote {out} ({'PASS' if not res['err'] else 'FAIL'})")


if __name__ == "__main__":
    main()
