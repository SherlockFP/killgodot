"""Crown Hill (zone "church", z 14): solemn, windy, the highest ground - church, bell tower, graveyard, Belvedere.

  * The Belvedere: flagstones, a brass-and-timber telescope on its tripod at the parapet, benches looking down the
    stepped axis to the harbour and the island, planters, a lantern post - the panorama reward (S3).
  * Cypress rows along the Crown Wall either side of the Belvedere (dark vertical accents on the skyline), lavender
    beds along the church front and the Crown Walk.
  * The church forecourt: candle stands by the door (LightCandles stays clear), a notice board, benches.
  * The graveyard (the builder's stones, fence and lychgate): offerings at the stones (flowers, candles), a freshly dug
    grave with its spoil heap and shovel, yews, gnarled dead trees, the gravedigger's shed with tools, a coffin, a
    lantern and a loot chest; the mausoleum door gets candles and ivy (the catacomb teaser, its apron kept clear).
  * The bell: a bronze bell hung under the bell tower's lookout arches.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

V, N, P, PIR, WP, DV, DH, DW, IP, KI = C.V, C.N, C.P, C.PIR, C.WP, C.DV, C.DH, C.DW, C.IP, C.K
R = C.rng
Z = 1400.0 + 2.0
AXIS = C.LAYOUT["axis"]["dir_inland"]
SEA = math.degrees(math.atan2(-AXIS[1], -AXIS[0]))       # looking down the axis to the harbour


def m(xm, ym):
    return xm * C.M, ym * C.M


def belvedere():
    poly = C.square_poly("belvedere")
    K.pave(poly, math.degrees(math.atan2(AXIS[1], AXIS[0])), lambda x, y, i, j: V + "Floor_UnevenBrick", Z - 1.0,
           inset=0.0, min_corners=3)
    tx, ty = C.xy("belvedere_telescope")
    # telescope: three splayed legs, a tube pitched up towards the island
    for k in range(3):
        a = math.radians(k * 120.0 + 30.0)
        foot = (tx + 55.0 * math.cos(a), ty + 55.0 * math.sin(a), Z - 2.0)
        K.rod(foot, (tx, ty, Z + 125.0), thick=0.35, cull=12000.0, collide=False)
    dx, dy = C.fwd(SEA, 1.0)
    K.rod((tx - dx * 45.0, ty - dy * 45.0, Z + 112.0), (tx + dx * 70.0, ty + dy * 70.0, Z + 150.0), thick=0.9, cull=12000.0)
    K.clutter(P + "Chalice", tx + dx * 72.0, ty + dy * 72.0, Z + 140.0, None, 2.2, 0.0, 70.0)
    C.claim(tx, ty, 80.0)
    # benches looking out (along the parapet), planters at the corners, a lantern
    cx, cy = C.centroid(poly)
    for off in (-620.0, 620.0):
        x, y = cx + math.cos(math.radians(SEA + 90.0)) * off, cy + math.sin(math.radians(SEA + 90.0)) * off
        x, y = x + math.cos(math.radians(SEA)) * 230.0, y + math.sin(math.radians(SEA)) * 230.0
        s = C.find(x, y, 60.0, reach=150.0, allow_lane=True)
        if s:
            K.bench(s[0], s[1], SEA)
    for (px, py) in poly:
        x, y = px + (cx - px) * 0.12, py + (cy - py) * 0.12
        if C.free(x, y, 60.0, allow_lane=True):
            K.planter(x, y, K.face_dir(x, y, cx, cy))
    s = C.find(cx - math.cos(math.radians(SEA)) * 280.0, cy - math.sin(math.radians(SEA)) * 280.0, 45.0, reach=200.0,
               allow_lane=True)
    if s:
        K.lantern_post(s[0], s[1], SEA, light=7.0)


def cypress_rows():
    """Cypress every ~5.5 m on the crown side of the Crown Wall, skipping stairs and the Belvedere."""
    n = 0
    for name in ("crown_wall_1", "crown_wall_2"):
        pts = C.anchor(f"line:{name}")["pts"]
        for x, y, ux, uy, s in C.resample(pts, 550.0, start=250.0, end_trim=250.0):
            for sgn in (1, -1):
                px, py = x - uy * sgn * 190.0, y + ux * sgn * 190.0
                t = C.terrace_at(px, py)
                if t and t["name"] == "crown":
                    break
            else:
                continue
            if C.in_square(px, py) or not C.free(px, py, 70.0):
                continue
            K.cypress(px, py, R.uniform(0.95, 1.2))
            n += 1
    C.stats["cypress"] = n


def lavender(x, y, yaw, w=320.0):
    f = K.F(x, y, yaw=yaw)
    for i in range(int(w // 38)):
        lx = -w / 2 + 19 + i * 38
        for row in (-22.0, 22.0):
            f.clutter(N + R.choice(["Flower_4_Group", "Flower_4_Single"]), lx + R.uniform(-6, 6), row + R.uniform(-5, 5),
                      -6.0, None, R.uniform(0.3, 0.38))
    f.clutter(V + "Prop_ExteriorBorder_Straight1", 0.0, 45.0, -3.0, 0.0, (w / 200.0, 0.5, 1.0))
    C.claim(x, y, w * 0.5)


def church_front():
    ch = C.building("church")
    lx, ly = C.task_spots()["LightCandles"]
    for t in (-300.0, 300.0):
        x, y = ch.at("front", t, K.FACADE + 90.0)
        if C.free(x, y, 40.0, allow_lane=True) and math.hypot(x - lx, y - ly) > 250.0:
            K.solid(P + "CandleStick_Stand", x, y, Z, R.uniform(0, 360))
            C.claim(x, y, 40.0)
    for side in ("left", "right"):
        x, y = ch.at(side, -200.0, K.FACADE + 110.0)
        if C.free(x, y, 170.0):
            lavender(x, y, ch.tyaw(side) + 90.0 - 90.0, 300.0)
    x, y = ch.at("front", 0.0, K.FACADE + 90.0)
    C.light(x, y, Z + 170.0, 5.0, 600.0, (255, 160, 80))
    K.dress_buildings([b for b in K.zone_buildings("church") if b.kind in ("home", "infill")], 0.6, 0.9, 0.3)
    # lavender along the Crown Walk's uphill verge
    pts = C.lane("crown_walk")
    for x, y, ux, uy, s in C.resample(pts, 900.0, start=400.0, end_trim=400.0):
        for sgn in (1, -1):
            px, py = x - uy * sgn * 260.0, y + ux * sgn * 260.0
            if C.free(px, py, 170.0) and C.slope(px, py, 150.0) < 60.0:
                lavender(px, py, math.degrees(math.atan2(uy, ux)) + 90.0, 300.0)
                break


def graveyard():
    g = C.anchor("graveyard")
    poly = C.cm(C.LAYOUT["landmarks"]["graveyard"]["polygon"])
    gx, gy = C.task_spots()["TendGraves"]
    stones = [(x, y) for x, y, r, f, mname in C.existing() if f == "V2/Graveyard" and "Gravestone" in mname]
    # offerings at the foot of the stones (flowers, candles, a wreath)
    for k, (x, y) in enumerate(stones):
        if R.random() < 0.45:
            continue
        ox, oy = x + R.uniform(-25, 25), y - 55.0
        if math.hypot(ox - gx, oy - gy) < 200.0:
            continue
        roll = R.random()
        if roll < 0.4:
            K.clutter(N + "Petal_%d" % R.randint(1, 5), ox, oy, Z - 2.0, None, 0.45)
        elif roll < 0.7:
            K.clutter(P + "Candle_2", ox, oy, Z, None, 1.4)
            K.clutter(P + "Candle_1", ox + 12.0, oy + 5.0, Z, None, 1.4)
        else:
            K.clutter(P + "Vase_4", ox, oy, Z, None, 0.55)
            K.clutter(N + "Flower_3_Single", ox, oy, Z + 25.0, None, 0.25)
    # yews and gnarled dead trees in the corners
    cx, cy = C.centroid(poly)
    for k, (px, py) in enumerate(poly):
        x, y = px + (cx - px) * 0.12, py + (cy - py) * 0.12
        if C.free(x, y, 90.0, zone=True, claims=True) or True:
            if k % 2 == 0:
                K.shrub(x, y, 0.75, flowers=False)
            else:
                K.solid(N + "DeadTree_%d" % R.choice([1, 3]), x, y, Z - 25.0, R.uniform(0, 360), 0.55)
                C.claim(x, y, 80.0)
    # a freshly dug grave with its spoil heap, shovel and waiting coffin (inside the yard, clear of TendGraves)
    for tx_, ty_ in ((0.35, 0.72), (0.7, 0.35), (0.25, 0.3)):
        x, y = poly[0][0] + (poly[1][0] - poly[0][0]) * tx_ + (poly[3][0] - poly[0][0]) * ty_, \
            poly[0][1] + (poly[1][1] - poly[0][1]) * tx_ + (poly[3][1] - poly[0][1]) * ty_
        if math.hypot(x - gx, y - gy) > 300.0 and C.free(x, y, 90.0, claims=True):
            yaw = math.degrees(math.atan2(poly[1][1] - poly[0][1], poly[1][0] - poly[0][0]))
            K.clutter(WP + "DugHole", x, y, Z + 1.0, yaw, (0.9, 1.7, 1.0))
            K.clutter(WP + "DigMound", x + 110.0, y, Z, yaw, (0.9, 1.3, 1.1))
            K.clutter(WP + "Shovel", x + 110.0, y + 20.0, Z + 40.0, yaw + 90.0, 1.0, -65.0, 0.0)
            K.solid(P + "Crate_Wooden", x - 20.0, y + 150.0, Z + 2.0, yaw, (0.55, 2.0, 0.45))
            K.lantern_post(x - 120.0, y - 60.0, yaw, light=4.0, radius=500.0, tall=0.75)
            C.claim(x, y, 130.0)
            break


def gravedigger_shed():
    """A lean-to against the graveyard's back fence: posts, a plank roof, tools, a coffin, a chest."""
    poly = C.cm(C.LAYOUT["landmarks"]["graveyard"]["polygon"])
    # outside the back of the yard (the side away from the gate)
    bx, by = (poly[2][0] + poly[3][0]) / 2, (poly[2][1] + poly[3][1]) / 2
    cx, cy = C.centroid(poly)
    out = K.face_dir(cx, cy, bx, by)
    x, y = bx + math.cos(math.radians(out)) * 260.0, by + math.sin(math.radians(out)) * 260.0
    s = C.find(x, y, 170.0, reach=400.0)
    if not s:
        return
    f = K.F(s[0], s[1], phi=out + 180.0)          # open side facing the yard
    for lx, ly, h in ((-160, -90, 250), (160, -90, 250), (-160, 90, 205), (160, 90, 205)):
        K.post(*f.w(lx, ly), h, 1.0)
    for i in range(4):
        f.clutter(V + "Floor_WoodDark", -120 + i * 80, 0, 238 - 5.0, 0.0, (0.42, 1.05, 1.0), -12.0, 0.0, on_ground=False)
    f.solid(P + "Workbench", 0, 55, 0, 180)
    f.clutter(WP + "Shovel", -140, 70, 60, 180.0, 1.0, -75.0, 0.0)
    f.clutter(P + "Pickaxe_Bronze", 140, 70, 55, 0.0, 1.0, 0.0, -14.0)
    f.solid(P + "Crate_Wooden", -90, -20, 0, 10.0, (0.55, 2.0, 0.45))
    f.clutter(P + "Lantern_Wall", 150, 0, 200, 0.0, 1.0, on_ground=False)
    x_, y_ = f.w(110.0, -10.0)
    C.loot_chest(x_, y_, yaw=f.yaw, name="Gravedigger's Chest", table="Grave")
    C.light(*f.w(0, 0), f.z + 200.0, 4.0, 500.0)
    C.claim(s[0], s[1], 190.0)


def mausoleum():
    mz = C.building("mausoleum")
    for t in (-150.0, 150.0):
        x, y = mz.at("front", t, 60.0)
        K.clutter(P + "Candle_2", x, y, Z, None, 1.8)
        K.clutter(P + "Candle_1", x + 14.0, y + 8.0, Z, None, 1.6)
        K.clutter(P + "Vase_Rubble_Medium", x + 20.0, y - 30.0, Z, None, 0.7)
    for side in ("left", "right", "back"):
        K.ivy(mz, side, 0.0, 0)
    C.light(*mz.at("front", 0.0, 120.0), Z + 90.0, 3.0, 450.0, (255, 140, 70))


def bell():
    bt = C.building("bell_tower")
    x, y = bt.x, bt.y
    top = bt.z + 3 * C.FLOOR_H
    K.prop(WP + "Doorbell", x, y, top + 250.0, yaw=bt.yaw, scale=5.0, collide=False, sub="Bell", cull=0.0)
    K.rod((x - 60.0, y, top + 262.0), (x + 60.0, y, top + 262.0), thick=0.9, cull=20000.0)


def dress():
    K.reset()
    for name, fn in (("belvedere", belvedere), ("cypress", cypress_rows), ("front", church_front),
                     ("graveyard", graveyard), ("shed", gravedigger_shed), ("mausoleum", mausoleum), ("bell", bell), ("smash", lambda: K.smash_piles([C.building("church").at("right", 200.0, 350.0)]))):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
