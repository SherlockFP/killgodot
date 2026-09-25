"""Sakura Garden + the shrine island (zone "japan", garden terrace z 8): calm, on the island axis x = 45.

  * Karesansui: a pale raked-gravel bed with a stone kerb, raked lines and three rock islands, on the lawn east of
    the pond, seen from the pavilion.
  * Stepping-stone paths: from the Garden Stair head to the pavilion round the koi pond (FeedKoi stays clear) and out
    along the Sakura Walk.
  * A bamboo fence along the Garden West Wall with bamboo clumps behind it; a hokora (wayside shrine) with offerings
    and an ema board; benches facing the pond; a tea set on the pavilion floor.
  * Paper lanterns strung between posts over the Sakura Walk and the pavilion approach (gently swaying); a koinobori
    pole with three carp streamers; fallen petals under every sakura.
  * The island: a lantern pair on the landing axis, an offering box before the pagoda, petals.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

V, N, P, PIR, WP, JP, DW, IP, KI, E = C.V, C.N, C.P, C.PIR, C.WP, C.JP, C.DW, C.IP, C.K, C.E
R = C.rng
Z = 800.0 + 2.0
GLOW = "/Game/KillGodot/Materials/M_KG_JapanGlow"


def m(xm, ym):
    return xm * C.M, ym * C.M


def paper_lantern(x, y, z, size=1.0, sway=True):
    """Round chochin: a glowing sphere with a dark cap, swaying on its cord."""
    s = 0.34 * size
    if sway:
        C.mover(E + "Sphere", x, y, z - 30.0 * size, scale=(s, s, s * 1.2), sway=R.uniform(3.0, 6.0), sway_hz=0.35,
                sub="Lanterns", material=GLOW)
    else:
        C.place(E + "Sphere", x, y, z - 30.0 * size, scale=(s, s, s * 1.2), collide=False, sub="Lanterns",
                material=GLOW, overhead=True, cull=15000.0)
    K.clutter(K.POST, x, y, z - 8.0 * size, 0.0, (0.5 * size, 0.5 * size, 0.04 * size), cull=12000.0)


def lantern_string(a, b, n, sag=40.0):
    (ax, ay, az), (bx, by, bz) = a, b
    mx, my, mz = (ax + bx) / 2, (ay + by) / 2, (az + bz) / 2 - sag
    K.rod((ax, ay, az), (mx, my, mz), 0.06)
    K.rod((mx, my, mz), (bx, by, bz), 0.06)
    for i in range(1, n + 1):
        t = i / (n + 1)
        x, y = ax + (bx - ax) * t, ay + (by - ay) * t
        z = az + (bz - az) * t - sag * (1.0 - (2.0 * t - 1.0) ** 2)
        paper_lantern(x, y, z, R.uniform(0.85, 1.1))


ROCKS = ((-150.0, -40.0, 1.0), (60.0, 70.0, 0.7), (170.0, -90.0, 0.55))


def zen_garden():
    """Raked gravel with a stone kerb, raked lines and three rock islands (walk-through except the rocks)."""
    for xm, ym in ((61.0, 21.0), (57.5, 7.5), (63.0, 12.0), (53.5, 16.0)):
        cx, cy = m(xm, ym)
        if C.free(cx, cy, 330.0) and C.slope(cx, cy, 300.0) < 20.0:
            break
    else:
        return
    yaw = 20.0
    W, D = 560.0, 380.0
    f = K.F(cx, cy, yaw=yaw)
    sand = "/Game/KillGodot/Env/KG_Village/Materials/MI_Plaster"
    C.place(E + "Plane", cx, cy, Z + 1.5, yaw=yaw, scale=(W / 100.0, D / 100.0, 1.0), collide=False, sub="Zen",
            overhead=True, cull=20000.0, material=sand)
    for side, (lx0, ly0, lx1, ly1) in enumerate(((-W / 2, -D / 2, W / 2, -D / 2), (W / 2, -D / 2, W / 2, D / 2),
                                                 (W / 2, D / 2, -W / 2, D / 2), (-W / 2, D / 2, -W / 2, -D / 2))):
        L = math.hypot(lx1 - lx0, ly1 - ly0)
        for i in range(int(L // 100)):
            t = (i + 0.5) / int(L // 100)
            x, y = f.w(lx0 + (lx1 - lx0) * t, ly0 + (ly1 - ly0) * t)
            K.clutter(N + "RockPath_Square_Small_%d" % (1 + i % 3), x, y, Z - 8.0,
                      yaw + math.degrees(math.atan2(ly1 - ly0, lx1 - lx0)), 0.8)
    for lx, ly, sc in ROCKS:
        x, y = f.w(lx, ly)
        K.solid(JP + "GardenRock", x, y, Z - 15.0, R.uniform(0, 360), sc)
        for k in range(5):
            a = R.uniform(0, 6.28)
            K.clutter(N + "Clover_1", x + 70.0 * sc * math.cos(a), y + 70.0 * sc * math.sin(a), Z, None, 0.4)
    C.claim(cx, cy, 330.0)
    C.stats["zen"] = 1


def stepping_paths():
    """Stepping stones stair head -> bridge (south-east end) and bridge (north-west end) -> pavilion (SPRINT-022:
    the route of prep_v2_placements.garden; nothing on the bridge itself)."""
    kx, ky = C.task_spots()["FeedKoi"]
    if "line:garden_route" in C.ANCHORS:
        r = C.anchor("line:garden_route")["pts"]
        routes = [r[:3], r[3:]]
    else:
        head = C.xy("garden_stair:head")
        pav = C.xy("pavilion")
        pond = C.xy("koi_pond")
        routes = [[head, (pond[0] + 380.0, pond[1] - 250.0), (pond[0] + 380.0, pond[1] + 250.0), (pav[0] + 150.0, pav[1] - 250.0)]]
    for pts in routes:
        for x, y, ux, uy, s in C.resample(pts, 75.0):
            px, py = x - uy * R.uniform(-12, 12), y + ux * R.uniform(-12, 12)
            if math.hypot(px - kx, py - ky) < 130.0 or C.lane_distance(px, py) < -10.0:
                continue
            if C.building_hit(px, py, 20.0):
                continue
            K.clutter(JP + "SteppingStone", px, py, C.ground(px, py) - 4.0, R.uniform(0, 360), R.uniform(1.0, 1.25))


def bamboo_fence():
    """Along the Garden West Wall (garden side): posts, two rails, bamboo clumps behind."""
    for name in ("garden_west_wall_1", "garden_west_wall_2", "garden_west_wall_3"):
        pts = C.anchor(f"line:{name}")["pts"]
        (ax, ay), (bx, by) = pts[0], pts[-1]
        L = math.hypot(bx - ax, by - ay)
        ux, uy = (bx - ax) / L, (by - ay) / L
        nx, ny = -uy, ux
        for sgn in (1, -1):
            tx, ty = (ax + bx) / 2 + nx * sgn * 150.0, (ay + by) / 2 + ny * sgn * 150.0
            t = C.terrace_at(tx, ty)
            if t and t["name"] == "garden":
                nx, ny = nx * sgn, ny * sgn
                break
        off = 110.0
        n = int(L // 90)
        tops = []
        for i in range(n + 1):
            s = 40.0 + (L - 80.0) * i / max(1, n)
            x, y = ax + ux * s + nx * off, ay + uy * s + ny * off
            if not C.free(x, y, 25.0, allow_lane=False, lane_gap=20.0, claims=False):
                tops.append(None)
                continue
            z = C.ground(x, y)
            K.solid(K.POST, x, y, z - 5.0, 0.0, (0.35, 0.35, 0.55))
            tops.append((x, y, z))
        for a, b in zip(tops, tops[1:]):
            if a and b:
                for h in (45.0, 125.0):
                    K.rod((a[0], a[1], a[2] + h), (b[0], b[1], b[2] + h), 0.22, cull=9000.0)
        for i in range(0, n, 3):
            s = 80.0 + (L - 160.0) * i / max(1, n)
            x, y = ax + ux * s + nx * (off - 55.0), ay + uy * s + ny * (off - 55.0)
            if C.free(x, y, 60.0, claims=False):
                K.clutter(JP + "Bamboo", x, y, C.ground(x, y) - 10.0, None, R.uniform(0.55, 0.75))


def shrine_corner():
    """Hokora on a stone plinth with a tiny torii, offerings; an ema board; benches facing the pond."""
    pond = C.xy("koi_pond")
    s = C.find(*m(57.0, 5.0), 120.0, reach=300.0)
    if s:
        f = K.F(s[0], s[1], face=pond)
        f.solid(V + "Stairs_Exterior_Platform", 0, 0, -28.0, 0, (0.45, 0.45, 0.63))    # plinth top at +35 (offerings)
        f.solid(V + "Prop_Chimney2", 0, 20, 30.0, 0, (0.6, 0.6, 0.28))
        f.put(V + "Roof_Wooden_2x1", 0, 30, 118.0, 180.0, scale=(0.5, 0.45, 0.45), collide=False, sub="Shrine")
        f.put(JP + "Torii", 0, -140, 0, 0.0, scale=0.11, collide=False, sub="Shrine", on_ground=True)
        for lx in (-35.0, 35.0):
            f.clutter(P + "Candle_2", lx, -40, 35.0, None, 1.3)
        f.clutter(P + "Coin_Pile", 0, -35, 35.0, None, 1.4)
        f.clutter(IP + "Bread", 20, -30, 35.0, None, 0.8)
        C.light(*f.w(0, -60), f.z + 90.0, 3.0, 400.0, (255, 150, 90))
        C.claim(s[0], s[1], 130.0)
    s = C.find(*m(50.0, 6.0), 110.0, reach=250.0)
    if s:
        f = K.F(s[0], s[1], face=pond)
        for sx in (-1, 1):
            f.solid(K.POST, sx * 90.0, 0, 0, 0, (0.8, 0.8, 0.55))
        f.solid(V + "Floor_WoodDark", 0, 0, 120.0, 0, (0.95, 0.07, 0.45), 0.0, 90.0)
        f.clutter(V + "Roof_Wooden_2x1", 0, 10, 165.0, 180.0, (0.9, 0.35, 0.3))
        for k in range(12):
            col = R.choice([K.P + "Scroll_1", V + "Wall_Plaster_Straight"])
            f.clutter(V + "Wall_Plaster_Straight", -70 + (k % 6) * 28, -8, 95 + (k // 6) * 30, 0.0, (0.09, 0.02, 0.07),
                      0.0, R.uniform(-8, 8))
        C.claim(s[0], s[1], 110.0)
    for xm, ym in ((40.8, 16.0), (49.0, 22.8)):
        s = C.find(*m(xm, ym), 110.0, reach=180.0)
        if s:
            K.bench(s[0], s[1], K.face_dir(s[0], s[1], *pond))


def pavilion_tea():
    pv = C.anchor("pavilion")
    f = K.F(pv["x"], pv["y"], yaw=pv["yaw"], z=Z + 4.0)
    f.clutter(KI + "Square_Table", 0, 30, 30.0, 0.0, (0.9, 0.9, 0.45), on_ground=False)
    for k, (lx, ly) in enumerate(((-12, 20), (12, 38), (0, 30))):
        f.clutter(KI + ("Cup" if k < 2 else "Jar"), lx, ly, 30.0 + 30.0, None, 1.4, on_ground=False)
    for lx, ly in ((-70.0, 30.0), (70.0, 30.0), (0.0, 100.0)):
        f.clutter(IP + "Rug_Round", lx, ly, 3.0, None, (0.28, 0.28, 3.0), on_ground=False)


def lantern_strings():
    walk = C.lane("sakura_walk")
    L = C.polyline_len(walk)
    posts = []
    for s in (200.0, L * 0.35, L * 0.65, L - 200.0):
        x, y, ux, uy = C.at_len(walk, s)
        for sgn in (1, -1):
            px, py = x - uy * sgn * 190.0, y + ux * sgn * 190.0
            if C.free(px, py, 30.0, claims=False, lane_gap=10.0):
                top = K.post(px, py, 300.0, 0.7)
                posts.append((px, py, top - 10.0))
                break
    for a, b in zip(posts, posts[1:]):
        lantern_string(a, b, 4)
    # a line from the pavilion towards the garden torii
    gt = C.xy("garden_torii")
    pv = C.xy("pavilion")
    ends = []
    for (x, y) in ((gt[0] + 260.0, gt[1] + 150.0), (pv[0] + 260.0, pv[1] - 180.0)):
        s = C.find(x, y, 30.0, reach=150.0, claims=False)
        if s:
            ends.append((s[0], s[1], K.post(s[0], s[1], 290.0, 0.7) - 10.0))
    if len(ends) == 2:
        lantern_string(ends[0], ends[1], 5, sag=30.0)
    C.light(pv[0] + 200.0, pv[1] - 100.0, Z + 250.0, 4.0, 600.0, (255, 150, 110))


def koinobori():
    s = C.find(*m(61.0, 14.0), 50.0, reach=300.0)
    if not s:
        return
    x, y = s
    z = C.ground(x, y)
    K.post(x, y, 760.0, 0.9)
    wind = 60.0
    for k, (h, sc) in enumerate(((700.0, 5.0), (600.0, 4.2), (500.0, 3.5))):
        dx, dy = C.fwd(wind, 90.0 * sc / 5.0)
        C.mover(JP + "Koi", x + dx, y + dy, z + h, yaw=wind + 180.0, roll=90.0, scale=sc, sway=8.0, sway_hz=0.6,
                sub="Koinobori")
    C.claim(x, y, 50.0)


def petals():
    trees = [(x, y, r) for x, y, r, f, mname in C.existing() if f == "V2/Garden" and "Sakura" in mname]
    trees.append((*C.xy("giant_sakura"), 400.0))
    C.claim(*C.xy("giant_sakura"), 140.0)
    for tx, ty, r in trees:
        for _ in range(26):
            a, d = R.uniform(0, 6.283), R.uniform(80.0, min(r, 420.0))
            x, y = tx + d * math.cos(a), ty + d * math.sin(a)
            if C.zone_of(x, y) != "japan" or C.building_hit(x, y, 20.0):
                continue
            K.clutter(N + "Petal_%d" % R.randint(1, 5), x, y, C.ground(x, y) - 1.0, None, R.uniform(0.5, 0.9), cull=6000.0)


def island():
    """Lantern pair on the landing axis, an offering box before the pagoda (the plateau at z 5.55)."""
    px, py = C.xy("pagoda")
    zt = 555.0
    for sx in (-1, 1):
        K.solid(JP + "ToroLantern", px + sx * 240.0, py - 520.0, zt, 0.0, 0.8)
    f = K.F(px, py - 380.0, phi=-90.0, z=zt)
    f.solid(P + "Crate_Wooden", 0, 0, 0, 0, (0.9, 0.55, 0.55))
    f.clutter(P + "Coin_Pile_2", 0, 0, 52.0, None, 1.5, on_ground=False)
    f.clutter(P + "Coin_Pile", 15, 8, 52.0, None, 1.5, on_ground=False)
    for _ in range(30):
        a, d = R.uniform(0, 6.283), R.uniform(200.0, 600.0)
        K.clutter(N + "Petal_%d" % R.randint(1, 5), px + d * math.cos(a), py + d * math.sin(a), zt - 1.0, None, 0.7)
    C.light(px, py - 520.0, zt + 120.0, 5.0, 600.0, (255, 150, 90))


def dress():
    K.reset()
    for name, fn in (("zen", zen_garden), ("paths", stepping_paths), ("bamboo", bamboo_fence), ("shrine", shrine_corner),
                     ("tea", pavilion_tea), ("lanterns", lantern_strings), ("koinobori", koinobori), ("petals", petals),
                     ("island", island)):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
