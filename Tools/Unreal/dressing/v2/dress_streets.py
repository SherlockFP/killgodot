"""The ring streets and their back gardens (zone "streets": the Heart rings at z 5 and the Upper Town at z 8).

Figure/ground: the streets are the walls of facades, the gardens behind the rows are the private green rooms.
  * Streets: cobbles on every flat ring street, ope and court; flower boxes under the windows (district colours come
    from the walls); doorstep kits against the facades (pots, benches, barrels, woodpiles, crates, tools) chosen per
    house "persona" and fitted between doors; shop signs on a handful of fronts; festival bunting where the ring
    streets leave the square; washing strung high over the opes. The Long Ope stays dark: nothing on it but the
    washing overhead and the builder's lanterns at its ends.
  * Balcony Lane (Upper Town): geranium pots along the parapet cap, the view left open.
  * Well Court: the Old Oak with a bench ring, buckets and a trough by the well (DrawWater stays clear).
  * Pilgrim Garden: a cypress pair framing the Pilgrim Stair, flower beds, benches, a lantern.
  * Back gardens: each row house gets a fenced plot behind it - vegetable beds, a washing line, a woodyard, a flower
    garden, a hen coop, a bench under a fruit tree - and the long strip under the Crown Wall becomes allotments, an
    orchard row, beehives and a washing green. Smashable crates and a couple of loot chests hide in the yards.
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
STREETS = ["rope_walk", "market_street", "back_lane_west", "back_lane_east", "balcony_lane", "cat_ope_heart",
           "net_ope_heart", "chapel_wynd", "chapel_wynd_upper"]
OPES = ["cat_ope_heart", "net_ope_heart", "chapel_wynd", "chapel_wynd_upper"]


def lane_z(name):
    for l in C.LAYOUT["lanes"]:
        if l["name"] == name:
            return float(l["z"]) * C.M
    return None


def paving():
    for name in STREETS:
        z = lane_z(name)
        if z is None:
            continue
        K.pave_lane(name, z + 1.5, mesh=V + "Floor_UnevenBrick",
                    skip=lambda x, y: C.zone_of(x, y) not in ("streets",) or C.in_square(x, y, "fountain_square"))
    for sq in ("well_court", "pilgrim_garden"):
        poly = C.square_poly(sq)
        K.pave(poly, 13.5, lambda x, y, i, j: V + "Floor_UnevenBrick", C.squares()[sq]["z"] * C.M + 1.5, inset=0.0,
               min_corners=2)


# ============================================================================================ street fronts
PERSONA_KITS = {
    "flowers": ["pots", "bench", "pots", "flowerbed"],
    "cooper": ["barrels", "kegs", "barrels", "crates"],
    "carpenter": ["planks", "woodpile", "crates", "bench"],
    "grocer": ["produce", "crates", "sacks", "pots"],
    "family": ["bench", "pots", "woodpile", "toys"],
    "fisher": ["oars", "barrels", "traps", "pots"],
    "herbalist": ["pots", "herbs", "pots", "bench"],
}
KIT_W = {"pots": 170.0, "bench": 290.0, "flowerbed": 240.0, "barrels": 150.0, "kegs": 150.0, "crates": 170.0,
         "planks": 150.0, "woodpile": 240.0, "produce": 190.0, "sacks": 160.0, "toys": 170.0, "oars": 150.0,
         "traps": 180.0, "herbs": 150.0}


def front_kit(f, side, t, name):
    """One doorstep kit centred at t along a facade, against the wall (depth <= 60 cm)."""
    ty, ny = f.tyaw(side), f.nyaw(side)

    def at(dt, out):
        return f.at(side, t + dt, K.FACADE + out)
    z = f.z
    if name == "pots":
        for i, (dt, out, big) in enumerate(((-55, 30, True), (5, 26, False), (60, 32, True))):
            K.potted(*at(dt, out), big=big, z=z)
    elif name == "bench":
        x, y = at(0.0, 30.0)
        K.bench(x, y, ny, z=z)
    elif name == "flowerbed":
        x, y = at(0.0, 35.0)
        K.flowerbed(x, y, ny, w=220.0, d=55.0)
    elif name == "barrels":
        K.solid(PIR + "Barrel_%d" % R.choice([0, 3, 4]), *at(-35.0, 38.0), z, R.uniform(0, 360), 0.72)
        K.solid(P + "Barrel", *at(35.0, 38.0), z, R.uniform(0, 360), 0.9)
        K.clutter(P + "Bucket_Wooden_1", *at(85.0, 30.0), z, None, 1.0)
    elif name == "kegs":
        K.solid(P + "Barrel_Holder", *at(0.0, 40.0), z, ty)
    elif name == "crates":
        K.solid(P + "Crate_Wooden", *at(-30.0, 48.0), z + 4.0, ty + R.uniform(-8, 8), 0.95)
        K.clutter(PIR + "crates_1", *at(-30.0, 48.0), z + 94.0, ty + R.uniform(-25, 25), 0.62)
        K.clutter(P + "Bag", *at(55.0, 38.0), z, None, 0.85)
    elif name == "planks":
        for i in range(4):
            K.clutter(PIR + "Planks_%d" % (i % 4), *at(-50.0 + i * 14.0, 6.0), z, ny + 180.0, 1.0, -80.0, 0.0)
        K.woodpile(*at(55.0, 30.0), ty, 2)
    elif name == "woodpile":
        x, y = at(0.0, 32.0)
        K.woodpile(x, y, ty, 4)
    elif name == "produce":
        for i, pth in enumerate((P + "FarmCrate_Carrot", P + "FarmCrate_Apple", P + "FarmCrate_Carrot")):
            K.clutter(pth, *at((i - 1) * 62.0, 32.0), z + 2.0, ty + R.uniform(-6, 6), 1.0)
        K.clutter(P + "FarmCrate_Apple", *at(0.0, 34.0), z + 26.0, ty + 8.0, 0.95)
    elif name == "sacks":
        K.sacks(*at(0.0, 36.0), ty, 3)
    elif name == "toys":
        K.clutter(P + "Dummy", *at(-40.0, 40.0), z, ny + 180.0, 0.6)
        K.clutter(P + "Shield_Wooden", *at(40.0, 20.0), z + 30.0, ny + 90.0, 0.8, 0.0, 12.0)
        K.clutter(P + "Bucket_Wooden_1", *at(70.0, 35.0), z, None, 1.0)
    elif name == "oars":
        K.clutter(DH + "Oars", *at(0.0, 35.0), z, ty, 1.0)
        K.solid(DH + "FishBarrel", *at(60.0, 36.0), z, R.uniform(0, 360))
    elif name == "traps":
        K.solid(DH + "LobsterTrap", *at(-40.0, 44.0), z, ty)
        K.clutter(DH + "LobsterTrap", *at(-35.0, 44.0), z + 39.0, ty + 12.0, 0.95)
        K.clutter(DH + "Buoy_Red", *at(55.0, 30.0), z, None, 0.5, 0.0, 85.0)
    elif name == "herbs":
        x, y = at(0.0, 2.0)
        K.clutter(IP + "HerbRack", x, y, z + 210.0, ny - 90.0, 1.0)
        for i in (-1, 1):
            K.potted(*at(i * 45.0, 30.0), big=True, z=z)
    C.stats["kit_" + name] = C.stats.get("kit_" + name, 0) + 1


def street_fronts():
    personas = list(PERSONA_KITS)
    for idx, f in enumerate(K.zone_buildings("streets")):
        persona = personas[(idx * 3 + int(f.x) // 700) % len(personas)]
        kits = list(PERSONA_KITS[persona])
        R.shuffle(kits)
        ki = 0
        for side in K.street_sides(f, max_gap=250.0):
            half = f.half(side)
            t = -half + 40.0
            while t < half - 40.0 and ki < 12:
                name = kits[ki % len(kits)]
                w = KIT_W[name]
                tc = t + w * 0.5
                if tc + w * 0.5 > half - 30.0:
                    break
                if side == "front" and f.door and abs(tc - f.door_t) < w * 0.5 + 105.0:
                    t = f.door_t + 110.0
                    continue
                pts = [f.at(side, tc + dt, K.FACADE + 35.0) for dt in (-w * 0.45, 0.0, w * 0.45)]
                if all(C.free(x, y, 28.0, allow_lane=True, lane_gap=0.0) for x, y in pts):
                    front_kit(f, side, tc, name)
                    for (x, y) in pts:
                        C.claim(x, y, 30.0)
                    ki += 1
                    t += w + R.uniform(60.0, 160.0)
                else:
                    t += 40.0


SHOPS = [("Herb", "market_street"), ("Ale", "back_lane_west"), ("Anvil", "back_lane_east"), ("Fish", "rope_walk"),
         ("Bread", "balcony_lane"), ("Herb", "balcony_lane")]


def shop_signs():
    """A handful of fronts become shops: a swinging sign beside the door."""
    used = set()
    for kind, street in SHOPS:
        best = None
        for f in K.zone_buildings("streets"):
            if f.id in used or not f.door or f.kind not in ("home", "infill"):
                continue
            nw = C.nearest_walkway(*f.door)
            if nw and nw[0] == street:
                if best is None or R.random() < 0.5:
                    best = f
        if best:
            used.add(best.id)
            K.hang_sign(kind, best, "front", best.door_t + (170.0 if R.random() < 0.5 else -170.0), 290.0)


def overhead():
    """Washing across the opes between the upper floors, bunting where the ring streets leave the square."""
    for name in OPES:
        pts = C.lane(name)
        L = C.polyline_len(pts)
        for s in (L * 0.35, L * 0.7):
            x, y, ux, uy = C.at_len(pts, s)
            nx, ny = -uy, ux
            hooks = []
            for sgn in (-1, 1):
                for d in range(130, 330, 20):
                    px, py = x + nx * sgn * d, y + ny * sgn * d
                    b = C.building_hit(px, py, 0.0)
                    if b:
                        hooks.append((px - nx * sgn * 15.0, py - ny * sgn * 15.0, b.z + 520.0))
                        break
            if len(hooks) == 2:
                K.laundry_across(hooks[0], hooks[1], movers=2)
    # bunting across the ring streets where they leave the square (facade to facade, 5 m up)
    for name in ("rope_walk", "market_street"):
        pts = C.lane(name)
        for s in (500.0, 1300.0):
            x, y, ux, uy = C.at_len(pts, s)
            nx, ny = -uy, ux
            hooks = []
            for sgn in (-1, 1):
                for d in range(180, 520, 20):
                    px, py = x + nx * sgn * d, y + ny * sgn * d
                    b = C.building_hit(px, py, 0.0)
                    if b:
                        hooks.append((px - nx * sgn * 15.0, py - ny * sgn * 15.0, b.z + 560.0))
                        break
            if len(hooks) == 2:
                K.bunting(hooks[0], hooks[1])


def balcony_parapet():
    """Geranium pots on the Balcony Lane parapet cap (every other piece), a few trailing flower boxes."""
    for k, (x, y, zt, yaw) in enumerate(K.parapets("streets")):
        if zt < 850.0:            # the upper wall parapets (top z 8 + 1 m); the harbour wall ones belong to the quay
            continue
        if k % 3 == 0:
            K.clutter(DV + "Planter_Pot", x, y, zt, None, 0.62, cull=9000.0)
        elif k % 3 == 1 and k % 2 == 0:
            K.clutter(DV + "FlowerBox", x, y, zt, yaw - 180.0, 0.8, cull=9000.0)


def well_court():
    wx, wy = C.xy("well")
    ox, oy = C.xy("old_oak")
    # bench ring round the oak (facing out), buckets and a trough by the well
    for a in (30.0, 150.0, 270.0):
        x, y = ox + 250.0 * math.cos(math.radians(a)), oy + 250.0 * math.sin(math.radians(a))
        s = C.find(x, y, 60.0, reach=80.0, allow_lane=True)
        if s:
            K.bench(s[0], s[1], a)
    for dx, dy, what in ((180.0, 150.0, "trough"), (-150.0, 190.0, "buckets"), (240.0, -160.0, "cat")):
        s = C.find(wx + dx, wy + dy, 50.0, reach=120.0, allow_lane=True)
        if not s:
            continue
        if what == "trough":
            K.solid(WP + "Rowboat", s[0], s[1], C.ground(*s) + 18.0, R.uniform(0, 180), (0.45, 0.55, 0.45))
        elif what == "buckets":
            K.clutter(P + "Bucket_Wooden_1", s[0], s[1], None, None, 1.0)
            K.clutter(P + "Bucket_Metal", s[0] + 45.0, s[1] + 10.0, None, None, 1.0)
        C.claim(s[0], s[1], 50.0)
    for k in range(10):
        a = 2 * math.pi * k / 10
        K.clutter(N + "Flower_%d_Single" % (3 + k % 2), ox + 150.0 * math.cos(a), oy + 150.0 * math.sin(a), None, None, 0.4)
    C.light(ox + 100.0, oy + 100.0, C.ground(ox, oy) + 330.0, 6.0, 700.0)


def pilgrim_garden():
    st = C.stairs()["pilgrim_stair"]
    (ax, ay), (bx, by) = C.cm([st["from"], st["to"]])
    L = math.hypot(bx - ax, by - ay)
    ux, uy = (bx - ax) / L, (by - ay) / L
    nx, ny = -uy, ux
    foot = (ax, ay) if st["z0"] < st["z1"] else (bx, by)
    for sgn in (-1, 1):                               # the cypress pair framing the stair foot
        x, y = foot[0] + nx * sgn * 380.0 - ux * 150.0, foot[1] + ny * sgn * 380.0 - uy * 150.0
        s = C.find(x, y, 70.0, reach=150.0)
        if s:
            K.cypress(s[0], s[1], 1.15)
    poly = C.square_poly("pilgrim_garden")
    cx, cy = C.centroid(poly)
    for sgn in (-1, 1):
        s = C.find(cx + nx * sgn * 450.0, cy + ny * sgn * 450.0, 110.0, reach=200.0, allow_lane=True)
        if s:
            K.flowerbed(s[0], s[1], K.face_dir(s[0], s[1], cx, cy), w=240.0, d=90.0)
        s = C.find(cx + nx * sgn * 300.0 + ux * 350.0, cy + ny * sgn * 300.0 + uy * 350.0, 110.0, reach=200.0, allow_lane=True)
        if s:
            K.bench(s[0], s[1], K.face_dir(s[0], s[1], cx, cy))
    s = C.find(cx - ux * 250.0, cy - uy * 250.0, 45.0, reach=200.0, allow_lane=True)
    if s:
        K.lantern_post(s[0], s[1], K.face_dir(s[0], s[1], cx, cy), light=7.0)


# ============================================================================================ back gardens
GARDENS = ["veg", "laundry", "woodyard", "flowers", "hens", "tree_bench", "veg", "laundry", "orchard"]


def garden_plot(cx, cy, yaw, W, D, kind):
    """A fenced back-garden plot; local -Y faces the house (open side)."""
    g = C.Frame(cx, cy, yaw, W, D)
    C.clear_grass(cx, cy, max(W, D) * 0.55)
    L = g.world
    hw, hd = W * 0.5, D * 0.5
    if kind not in ("tree_bench",):
        K.fence_run(*L(-hw, -hd + 30.0), *L(-hw, hd))
        K.fence_run(*L(-hw, hd), *L(hw, hd), gate=0.5)
        K.fence_run(*L(hw, hd), *L(hw, -hd + 30.0))
    if kind == "veg":
        K.veg_bed(*L(0.0, 20.0), yaw, W - 120.0, D - 140.0)
        K.clutter(P + "Dummy", *L(hw - 60.0, hd - 60.0), None, yaw + 180.0 + R.uniform(-20, 20), 0.9)
        K.clutter(WP + "Shovel", *L(-hw + 60.0, -hd + 60.0), None, yaw + 30.0, 1.0, -78.0, 0.0)
        K.clutter(P + "Bucket_Wooden_1", *L(-hw + 60.0, hd - 60.0), None, None, 1.0)
    elif kind == "laundry":
        K.laundry_line(*L(0.0, 0.0), yaw + R.uniform(-8, 8))
        K.clutter(P + "Bag", *L(hw - 60.0, hd - 70.0), None, None, 0.8)
        K.potted(*L(-hw + 60.0, hd - 60.0), big=True)
    elif kind == "woodyard":
        K.woodpile(*L(0.0, hd - 60.0), yaw, 5)
        K.solid(P + "Anvil_Log", *L(-hw + 90.0, 0.0), None, R.uniform(0, 360))
        K.clutter(P + "Axe_Bronze", *L(-hw + 90.0, 0.0), C.ground(*L(-hw + 90.0, 0.0)) + 100.0, None, 1.0, 0.0, 20.0)
        if R.random() < 0.6:
            C.breakable(P + "Crate_Wooden", *L(hw - 70.0, -hd + 90.0), yaw=R.uniform(0, 360))
    elif kind == "flowers":
        for r_ in range(max(1, int((D - 100.0) // 90.0))):
            K.flowerbed(*L(0.0, -hd + 90.0 + r_ * 90.0), yaw - 90.0 + 90.0, w=W - 120.0, d=50.0)
        K.potted(*L(hw - 60.0, hd - 60.0), big=True)
    elif kind == "hens":
        for k, (lx, ly, lz) in enumerate(((-45, 0, 0), (45, 5, 0), (0, 2, 80))):
            K.solid(P + "Cage_Small", *L(lx, hd - 80.0 + ly), C.ground(*L(lx, hd - 80.0)) + lz, yaw + R.uniform(-10, 10))
        K.clutter(P + "Bag", *L(hw - 70.0, 0.0), None, None, 0.85)
        K.clutter(P + "Bucket_Wooden_1", *L(-hw + 60.0, 0.0), None, None, 1.0)
        for k in range(6):
            K.clutter(N + "Grass_Wispy_Short", *L(R.uniform(-hw + 40, hw - 40), R.uniform(-hd + 40, hd - 40)), None, None, 0.5)
    elif kind == "tree_bench":
        K.tree(*L(0.0, hd - 120.0), s=0.55)
        K.bench(*L(0.0, hd - 260.0), yaw - 90.0 + 180.0 + 90.0)
        K.flowerbed(*L(-hw + 80.0, 0.0), yaw, w=120.0, d=60.0)
    elif kind == "orchard":
        for i in (-1, 1):
            K.tree(*L(i * hw * 0.5, 0.0), path=N + "CommonTree_2", s=0.45)
        K.clutter(P + "FarmCrate_Apple", *L(0.0, hd - 60.0), None, yaw, 1.0)
        K.clutter(P + "Barrel_Apples", *L(hw - 60.0, -hd + 70.0), None, None, 1.0)
    C.stats["gardens"] = C.stats.get("gardens", 0) + 1
    C.stats.setdefault("garden_kinds", {}).setdefault(kind, 0)
    C.stats["garden_kinds"][kind] += 1


def back_gardens():
    k = 0
    for f in K.zone_buildings("streets"):
        if f.kind not in ("home", "infill"):
            continue
        for side in ("back", "left", "right"):
            if not K.exposed(f, side, 0):
                continue
            half = f.half(side)
            W = min(2.0 * half + 40.0, 700.0)
            done = False
            for D in (520.0, 420.0, 330.0):
                pts = [f.at(side, tt, K.FACADE + out) for tt in (-W * 0.5 + 40.0, 0.0, W * 0.5 - 40.0)
                       for out in (110.0, D * 0.5, D - 30.0)]
                if not all(C.in_garden(x, y) and C.free(x, y, 45.0, lane_gap=60.0) for x, y in pts):
                    continue
                cx, cy = f.at(side, 0.0, K.FACADE + 40.0 + D * 0.5)
                if C.slope(cx, cy, r=max(W, D) * 0.5) > 30.0:
                    continue
                garden_plot(cx, cy, f.nyaw(side) - 90.0, W, D, GARDENS[k % len(GARDENS)])
                for tt in (-W * 0.5 + 60.0, -W * 0.25, 0.0, W * 0.25, W * 0.5 - 60.0):
                    for out in (100.0, D * 0.5 + 40.0, D - 20.0):
                        C.claim(*f.at(side, tt, K.FACADE + out), 75.0)
                k += 1
                done = True
                break
            if done:
                break


def fill_yards():
    """Whatever garden ground is left: allotment rows, fruit trees, beehives, a washing green, benches, shrubs."""
    placed = 0
    xs = range(-6000, 6100, 450)
    ys = range(-4600, 4200, 450)
    cand = [(x + R.uniform(-120, 120), y + R.uniform(-120, 120)) for x in xs for y in ys]
    R.shuffle(cand)
    kinds = ["tree", "veg", "shrubs", "beehives", "tree", "laundry", "bench", "shrubs", "veg", "chest"]
    chests = 0
    for x, y in cand:
        if C.zone_of(x, y) != "streets" or not C.in_garden(x, y, 60.0):
            continue
        kind = kinds[placed % len(kinds)]
        r = {"tree": 150.0, "veg": 190.0, "laundry": 230.0, "bench": 150.0}.get(kind, 110.0)
        if not C.free(x, y, r, lane_gap=60.0) or C.slope(x, y, r) > 30.0:
            continue
        yaw = R.choice([0.0, 90.0]) + K.face_dir(x, y, *C.O_BASIN)
        if kind == "tree":
            K.tree(x, y, s=R.uniform(0.5, 0.7))
            K.grass_tufts(x, y, 150.0, 5)
        elif kind == "veg":
            K.veg_bed(x, y, yaw, 320.0, 220.0)
            K.fence_run(*C.Frame(x, y, yaw).world(-180.0, 130.0), *C.Frame(x, y, yaw).world(180.0, 130.0))
        elif kind == "shrubs":
            for j in range(3):
                K.shrub(x + R.uniform(-90, 90), y + R.uniform(-90, 90), R.uniform(0.35, 0.5), flowers=j != 1)
        elif kind == "beehives":
            for j in range(3):
                K.beehive(x + (j - 1) * 90.0, y + R.uniform(-15, 15), yaw)
        elif kind == "laundry":
            K.laundry_line(x, y, yaw)
        elif kind == "bench":
            K.bench(x, y, yaw + 90.0)
            K.tree(x + math.cos(math.radians(yaw + 270.0)) * 160.0, y + math.sin(math.radians(yaw + 270.0)) * 160.0,
                   s=0.5)
        elif kind == "chest" and chests < 2:
            C.loot_chest(x, y, yaw=yaw, name="Garden Shed Chest")
            K.woodpile(x + 90.0, y, yaw + 90.0, 3)
            chests += 1
        C.claim(x, y, r)
        placed += 1
    C.stats["yard_fill"] = placed


def smashables():
    n = 0
    for f in K.zone_buildings("streets"):
        if n >= 10 or R.random() > 0.35:
            continue
        side = R.choice(["back", "left", "right"])
        if not K.exposed(f, side, 0):
            continue
        x, y = f.at(side, R.uniform(-80, 80), K.FACADE + 60.0)
        if C.free(x, y, 55.0, allow_lane=True, lane_gap=0.0):
            C.breakable(P + R.choice(["Crate_Wooden", "Barrel", "Crate_Wooden"]), x, y, yaw=R.uniform(0, 360))
            n += 1


def dress():
    K.reset()
    for name, fn in (("paving", paving), ("facades", lambda: K.dress_buildings(K.zone_buildings("streets"), 0.5, 0.8, 0.2)),
                     ("well", well_court), ("pilgrim", pilgrim_garden), ("fronts", street_fronts), ("signs", shop_signs),
                     ("rhythm", lambda: K.facade_rhythm(K.zone_buildings("streets"))),
                     ("overhead", overhead), ("parapet", balcony_parapet), ("gardens", back_gardens),
                     ("yards", fill_yards), ("smash", smashables)):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
