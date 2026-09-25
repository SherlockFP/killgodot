"""Fountain Square, "The Heart" (zone "square", z 5): the living room of Morrowmere - meetings, trials, market day.

The fan-shaped square (22 x 31 m) is enclosed by the Town Hall (N), the Inn "The Latecomer" (E), the Bakery (W) and
the balustrade over the Grand Stair (S, the one window to the sea). The fountain sits on the axis inside the ring of
20 player starts, so the middle stays open; the dressing lives in the four pockets around the ring:
  * N  - Town Hall forecourt: banners on the facade, potted bays at the steps, the notice board corner, the Stage.
  * NE - The Latecomer's beer garden: tables with stools (seats), ale kegs, a lantern, bunting to the inn front.
  * E  - the market row along the east flank: stalls with goods facing the fountain (the builder's stalls turned
         round), stock piles, a smashable crate stack and the merchant's strongbox.
  * SE - the Dead Tree: a candle-lit memorial ring under the black silhouette, a lone bench looking at the sea.
  * S  - the balustrade: benches looking out to sea, planters flanking the Grand Stair head (the view stays open).
  * W  - the Bakery front: bread baskets, flour sacks, firewood for the oven, the Bread sign.
Festival canopy: bunting strung between the Town Hall, the Inn, the Bakery and the clock tower; flower boxes under
every window facing the square; the four builder benches round the fountain become real seats.
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
FS = C.xy("fountain")
Z = 500.0 + 2.0


def m(xm, ym):
    return xm * C.M, ym * C.M


def fountain_seats():
    """The builder's four plain benches round the fountain become real seats (same spots, facing the fountain)."""
    C.remove_builder("V2/Square", "Bench")
    for a in (35.0, 145.0, 215.0, 325.0):
        x, y = FS[0] + 430.0 * math.cos(math.radians(a)), FS[1] + 430.0 * math.sin(math.radians(a))
        K.bench(x, y, a + 180.0, z=Z - 2.0)
    # two rings of pale flag stones round the basin, set into the cobbles (walk-through)
    for rr, n, sc in ((265.0, 24, 0.9), (560.0, 40, 0.75)):
        for k in range(n):
            a = 2 * math.pi * k / n
            x, y = FS[0] + rr * math.cos(a), FS[1] + rr * math.sin(a)
            K.clutter(N + "RockPath_Square_Small_%d" % (1 + k % 3), x, y, Z - 17.0, math.degrees(a) + R.uniform(-6, 6), sc,
                      cull=9000.0)


def facades():
    """Flower boxes under every window facing the square, shop signs, banners on the Town Hall."""
    sq = C.zone_polys("square")[0]
    for f in C.buildings():
        for side in ("front", "back", "left", "right"):
            px, py = f.at(side, 0.0, 60.0)
            if C.pip(sq, px, py) or C.poly_dist(sq, px, py) < 300.0:
                K.dress_facade(f, side, boxes_upper=True, boxes_ground=0.7)
    bak, inn, th = C.building("bakery"), C.building("inn"), C.building("town_hall")
    K.hang_sign("Bread", bak, "front", bak.door_t + 170.0, 290.0)
    K.hang_sign("Ale", inn, "front", inn.door_t - 170.0, 300.0)
    # Town Hall: two long banners either side of the door + the town's two blue pennants
    for t, kind in ((-310.0, "Banner_1"), (310.0, "Banner_1"), (-150.0, "Banner_2"), (150.0, "Banner_2")):
        x, y = K.facade_pt(th, "front", t, 4.0)
        K.prop(P + kind, x, y, th.z + (560.0 if kind == "Banner_1" else 520.0), yaw=th.nyaw("front") + 90.0,
               collide=False, cull=16000.0, sub="TownHall")
    for f, kind in ((inn, "Banner_2"), (bak, "Banner_1")):
        for t in (-f.w * 0.5 + 60.0, f.w * 0.5 - 60.0):
            x, y = K.facade_pt(f, "front", t, 4.0)
            K.prop(P + kind, x, y, f.z + 520.0, yaw=f.nyaw("front") + 90.0, collide=False, cull=16000.0, sub="Banners")
    K.facade_rhythm([bak, inn, th], sides=["front"])
    # potted bays by every door on the square
    for f in (bak, inn, th):
        for sgn in (-1, 1):
            x, y = f.at("front", f.door_t + sgn * 125.0, K.FACADE + 35.0)
            if C.free(x, y, 30.0, allow_lane=True, zone=False):
                K.potted(x, y, big=True)


def bunting():
    """Festival canopy: lines between the facades round the square at 5-6 m."""
    th, inn, bak, ct = C.building("town_hall"), C.building("inn"), C.building("bakery"), C.building("clock_tower")

    def hook(f, side, t, z):
        x, y = K.facade_pt(f, side, t, 2.0)
        return (x, y, f.z + z)

    a = hook(th, "front", 280.0, 640.0)
    b = hook(inn, "front", -250.0, 600.0)
    c = hook(bak, "front", -200.0, 520.0)
    d = hook(ct, "front", 0.0, 680.0)
    e = hook(inn, "front", 250.0, 600.0)
    # a mast on each side of the balustrade (the Grand Stair head) to carry lines over the sea window
    masts = []
    for xm, ym in ((6.6, 20.3), (21.2, 16.6)):
        x, y = C.find(*m(xm, ym), 40.0, reach=200.0, allow_lane=True) or m(xm, ym)
        top = K.post(x, y, 560.0, 1.1)
        K.clutter(P + "Banner_2_Cloth", x, y, top + 110.0, None, (0.35, 1.0, 0.33))
        C.claim(x, y, 40.0)
        masts.append((x, y, top - 20.0))
    for p, q in ((a, b), (c, b), (a, c), (d, e), (c, masts[0]), (masts[0], masts[1]), (masts[1], e), (d, a)):
        K.bunting(p, q)


def market():
    """Market stalls: two in the south-west pocket flanking the sea window, one under the Dead Tree, a flower cart on
    the east flank by the inn; the builder's back-to-front stall row is taken over."""
    C.remove_builder("V2/Square", "Stall")
    placed = []
    for (xm, ym), kind, cart in (((6.2, 14.4), "fish", False), ((4.6, 18.4), "veg", False),
                                 ((21.2, 17.4), "fruit", False), ((18.9, -2.2), "flowers", True)):
        s = C.find(*m(xm, ym), 105.0, reach=160.0, step=30.0)
        if not s:
            continue
        f = K.stall(s[0], s[1], K.face_dir(s[0], s[1], *FS), kind, cart=cart)
        placed.append(f)
    # stock piled by the stalls + the merchant's strongbox
    s = C.find(*m(3.2, 15.6), 80.0, reach=200.0)
    if s:
        K.crate_stack(s[0], s[1], K.face_dir(s[0], s[1], *FS), ((0, 0, 0), (1, 0, 0), (0, 0, 1)), breakable_top=True)
    s = C.find(*m(22.0, 19.8), 70.0, reach=200.0)
    if s:
        C.loot_chest(s[0], s[1], yaw=K.face_dir(s[0], s[1], *FS) - 90.0, name="Merchant's Strongbox",
                     path=PIR + "chest_common_0")
        K.sacks(s[0] - 90.0, s[1] - 30.0, 30.0, 2)
    for f in placed[:2]:
        C.light(f.x, f.y, Z + 300.0, 7.0, 800.0)
    return placed


def beer_garden():
    """The Latecomer's garden in the north-east pocket (inn corner, back lane): tables and stools, kegs, a lantern."""
    spots = []
    for xm, ym in ((13.0, -7.2), (15.6, -8.6), (15.0, -5.6), (12.2, -9.6)):
        s = C.find(*m(xm, ym), 105.0, reach=90.0, step=30.0)
        if s:
            spots.append(s)
            K.table_set(s[0], s[1], R.uniform(0, 90), stools=3 + (len(spots) % 2), food=len(spots) % 2 == 0)
    s = C.find(*m(16.8, -9.8), 70.0, reach=160.0)
    if s:
        K.solid(P + "Barrel_Holder", s[0], s[1], None, K.yaw_front(K.face_dir(s[0], s[1], *FS)))
        K.solid(PIR + "Barrel_13", s[0] + 90.0, s[1] + 30.0, None, R.uniform(0, 360), 0.8)
        C.claim(s[0], s[1], 110.0)
    if spots:
        x = sum(p[0] for p in spots) / len(spots)
        y = sum(p[1] for p in spots) / len(spots)
        s = C.find(x, y, 40.0, reach=250.0)
        if s:
            K.lantern_post(s[0], s[1], K.face_dir(s[0], s[1], x, y), light=9.0, radius=800.0)
    # the inn front: kegs and potted bays either side of the door
    inn = C.building("inn")
    for t in (-260.0, 250.0):
        x, y = inn.at("front", inn.door_t + t, K.FACADE + 45.0)
        if C.free(x, y, 45.0, allow_lane=True, zone=False):
            K.solid(PIR + "Barrel_%d" % R.choice([1, 5, 8]), x, y, None, R.uniform(0, 360), 0.8)
            C.claim(x, y, 45.0)
    return spots


def paving():
    """The cobble fan: cream limestone flags over the square on the processional axis grid, a red-brick ring round
    the fountain and a grey axis band from the Grand Stair to the Scala (walk-through, flat)."""
    poly = C.zone_polys("square")[0]
    ax, ay = C.LAYOUT["axis"]["dir_inland"]
    yaw = math.degrees(math.atan2(ay, ax))
    fx, fy = FS

    def pick(x, y, i, j):
        if math.hypot(x - fx, y - fy) < 190.0:
            return None                                   # under the fountain basin
        return V + "Floor_UnevenBrick"
    K.pave(poly, yaw, pick, Z - 0.5, origin=FS, inset=30.0)


def forecourt():
    """Town Hall forecourt + notice-board corner: a wanted wall, a speaker's crate, bay trees."""
    th = C.building("town_hall")
    for t in (-360.0, 360.0):
        x, y = th.at("front", t, K.FACADE + 150.0)
        s = C.find(x, y, 60.0, reach=120.0, allow_lane=True)
        if s:
            K.planter(s[0], s[1], th.front_yaw())
    # wanted posters board beside the notice board
    nb = C.xy("notice_board")
    s = C.find(nb[0] - 260.0, nb[1] - 60.0, 80.0, reach=200.0)
    if s:
        f = K.F(s[0], s[1], face=FS)
        for sx in (-1, 1):
            f.solid(K.POST, sx * 120.0, 0, 0, 0, (1.0, 1.0, 0.8))
        f.put(V + "Wall_Plaster_WoodGrid", 0, 0, 70.0, 0, scale=(1.15, 0.3, 0.5), sub="Wanted")
        for k in range(7):
            f.clutter(V + "Wall_Plaster_Straight", -95 + (k % 4) * 60 + R.uniform(-6, 6), -14,
                      135 + (k // 4) * 55 + R.uniform(-8, 8), R.uniform(-4, 4), (0.17, 0.04, 0.14), 0.0, R.uniform(-6, 6),
                      on_ground=False)
        C.claim(s[0], s[1], 140.0)


def dead_tree():
    """A candle-lit memorial ring round the Dead Tree and a lone bench looking out to sea."""
    tx, ty = C.xy("dead_tree")
    for k in range(9):
        a = 2 * math.pi * k / 9 + 0.2
        x, y = tx + 170.0 * math.cos(a), ty + 170.0 * math.sin(a)
        K.clutter(N + "Pebble_Round_%d" % (1 + k % 5), x, y, Z - 3.0, None, 1.6)
        if k % 3 == 0:
            K.clutter(P + "Candle_2", x, y, Z + 5.0, None, 1.4)
        elif k % 3 == 1:
            K.clutter(N + "Petal_%d" % (1 + k % 5), x, y, Z - 2.0, None, 0.5)
    K.clutter(P + "CandleStick_Stand", tx + 120.0, ty - 60.0, Z, None, 1.0)
    C.light(tx + 60.0, ty - 30.0, Z + 140.0, 4.0, 450.0, (255, 150, 70))
    s = C.find(tx + 40.0, ty + 330.0, 120.0, reach=160.0)
    if s:
        K.bench(s[0], s[1], 90.0 + R.uniform(-10, 10))       # looking south, out to sea


def balustrade():
    """Benches looking out to sea along the balustrade, planters flanking the Grand Stair head."""
    head = C.anchor("grand_stair:head")
    hx, hy = head["x"], head["y"]
    sx, sy = C.stairs()["grand_stair"]["from"], C.stairs()["grand_stair"]["to"]
    ux, uy = sx[0] - sy[0], sx[1] - sy[1]
    L = math.hypot(ux, uy)
    ux, uy = ux / L, uy / L                           # up the stair (into the square)
    px, py = -uy, ux                                  # across the stair
    for sgn in (-1, 1):
        x, y = hx + px * sgn * 560.0 + ux * 120.0, hy + py * sgn * 560.0 + uy * 120.0
        s = C.find(x, y, 60.0, reach=150.0, allow_lane=True)
        if s:
            K.planter(s[0], s[1], math.degrees(math.atan2(uy, ux)))
    sea = math.degrees(math.atan2(-uy, -ux))
    for xm, ym in ((8.8, 18.8), (4.4, 16.2)):
        s = C.find(*m(xm, ym), 120.0, reach=150.0)
        if s:
            K.bench(s[0], s[1], sea + R.uniform(-8, 8))


def maypole():
    """Second focal point: the ribboned Maypole in the south-west pocket by the sea window (seen from the quay up the
    Grand Stair), clear of the meeting ring, the starts, the lanes' central bands and the axis."""
    s = C.find(*m(6.5, 16.0), 170.0, reach=150.0, allow_lane=True)
    if not s:
        C.stats.setdefault("warn", []).append("maypole: no free spot")
        return
    x, y = s
    C.place(DV + "Maypole", x, y, Z - 4.0, yaw=R.uniform(0, 360), sub="Maypole", claim_r=170.0)
    C.place(DV + "Maypole_Ribbons", x, y, Z - 4.0, yaw=R.uniform(0, 360), sub="Maypole", collide=False)
    for k in range(6):
        a = 2 * math.pi * k / 6 + 0.3
        K.clutter(N + "Petal_%d" % (1 + k % 5), x + 150.0 * math.cos(a), y + 150.0 * math.sin(a), Z - 1.0, None, 0.6)
    C.stats["maypole"] = [round(x / C.M, 2), round(y / C.M, 2)]


def bakery_front():
    bak = C.building("bakery")
    for t, out in ((-190.0, 120.0), (-270.0, 175.0), (205.0, 130.0)):
        x, y = bak.at("front", t, K.FACADE + out)
        if C.free(x, y, 40.0, allow_lane=True):
            f = K.F(x, y, phi=bak.front_yaw())
            f.clutter(P + "FarmCrate_Empty", 0, 0, 0, R.uniform(-20, 20))
            for k in range(5):
                f.clutter(IP + "Bread", -18 + (k % 3) * 18, -7 + (k // 3) * 14, 13 + (k % 2) * 4, None, 1.0)
            C.claim(x, y, 45.0)
    x, y = bak.at("front", 270.0, K.FACADE + 60.0)
    if C.free(x, y, 60.0, allow_lane=True):
        K.sacks(x, y, bak.tyaw("front"), 3)
    for side in ("left", "right"):
        if K.exposed(bak, side):
            x, y = bak.at(side, -120.0, K.FACADE + 40.0)
            if C.free(x, y, 70.0, allow_lane=True):
                K.woodpile(x, y, bak.tyaw(side), 5)


def smashables():
    for xm, ym in ((1.2, 12.6), (21.6, 12.8), (-1.6, -4.4)):
        s = C.find(*m(xm, ym), 70.0, reach=200.0)
        if s:
            C.breakable(P + R.choice(["Crate_Wooden", "Barrel"]), s[0], s[1], yaw=R.uniform(0, 360))
            C.breakable(P + "Crate_Wooden", s[0] + 60.0, s[1] + 25.0, yaw=R.uniform(0, 360))


def ground_detail():
    """Flat stones and flower tufts along the edges of the square (never on the gathering ring)."""
    sq = C.zone_polys("square")[0]
    xs = [p[0] for p in sq]
    ys = [p[1] for p in sq]
    n = 0
    for _ in range(900):
        if n >= 90:
            break
        x, y = R.uniform(min(xs), max(xs)), R.uniform(min(ys), max(ys))
        if not C.pip(sq, x, y) or math.hypot(x - FS[0], y - FS[1]) < 900.0 or C.poly_dist(sq, x, y) > 260.0:
            continue
        if not C.free(x, y, 20.0, allow_lane=True, allow_reserved=True):
            continue
        roll = R.random()
        if roll < 0.5:
            K.clutter(N + "Pebble_%s_%d" % (R.choice(["Round", "Square"]), R.randint(1, 5)), x, y, Z - 3.0, None,
                      R.uniform(0.6, 1.2), cull=4000.0)
        elif roll < 0.8:
            K.clutter(N + "Clover_%d" % R.randint(1, 2), x, y, Z - 2.0, None, R.uniform(0.35, 0.55), cull=5000.0)
        else:
            K.clutter(N + "Petal_%d" % R.randint(1, 5), x, y, Z - 1.0, None, R.uniform(0.5, 0.8), cull=5000.0)
        n += 1


def dress():
    K.reset()
    for name, fn in (("maypole", maypole), ("paving", paving), ("seats", fountain_seats), ("facades", facades), ("market", market), ("garden", beer_garden),
                     ("forecourt", forecourt), ("dead_tree", dead_tree), ("balustrade", balustrade),
                     ("bakery", bakery_front), ("bunting", bunting), ("smash", smashables), ("ground", ground_detail)):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
