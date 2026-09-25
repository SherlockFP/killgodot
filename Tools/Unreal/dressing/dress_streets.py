"""Zone "streets": the residential core of Morrowmere outside the square (x -52..62 m, y -40..40 m).

Every house gets a personality (flower boxes, door pots, benches, firewood, rain barrels, tools, crates, gardens),
the lanes get hedges, fences, signposts and clutter, and a set of story vignettes fills the open ground:
the smithy yard, the east market row, the allotments + orchard, the washing green, fish drying racks,
the inn's beer garden, a card table, a broken cart, a wayside shrine, a sea-view bench, a kids' corner.
A final gap-filling pass makes sure every ~6-10 m of walking offers something to look at.

Run live:  kg_dress.run(['streets'])   (see README.md)
"""
import json
import math
import random

import unreal

import kg_dress_common as C

V, N, P, PIR, WP = C.V, C.N, C.P, C.PIR, C.WP
IP = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/"
KI = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/"
FIREWOOD = IP + "SM_KG_Firewood"
R = random.Random(52_62)

_CAT = {e["path"]: e for e in json.load(open("D:/Kill Godot/Tools/Unreal/dressing/asset_catalog.json"))}

# Instanced meshes that block (knee-high or bigger). One HISM per mesh, so one policy per mesh.
COLLIDE = {FIREWOOD, P + "Barrel", P + "Barrel_Apples", P + "Crate_Wooden", P + "Bag", P + "Vase_2",
           V + "Prop_WoodenFence_Single", V + "Corner_Exterior_Wood", V + "Wall_UnevenBrick_Straight",
           N + "Bush_Common", N + "Bush_Common_Flowers", PIR + "Barrel_0", PIR + "Barrel_3", PIR + "Barrel_9",
           PIR + "crates_0", PIR + "crates_1", V + "Prop_Crate", P + "Crate_Metal", N + "CommonTree_1",
           N + "CommonTree_2", N + "CommonTree_5", PIR + "Cannon_Ball_0", PIR + "Cannon_Ball_1"}
CULL = {N + "CommonTree_1": 0.0, N + "CommonTree_2": 0.0, N + "CommonTree_5": 0.0}

FLOWERS = [N + "Petal_1", N + "Petal_3", N + "Petal_2", N + "Petal_5", N + "Clover_1"]
SIDES = {"front": ((0, -1), (1, 0)), "back": ((0, 1), (-1, 0)), "left": ((-1, 0), (0, -1)), "right": ((1, 0), (0, 1))}


# ============================================================================================ helpers
def size(path):
    e = _CAT.get(path)
    return e["size"] if e else [100, 100, 100]


def auto_cull(path, s=1.0):
    k = max(s) if isinstance(s, tuple) else s
    m = max(size(path)) * k
    return 6000.0 if m <= 100 else 10000.0 if m <= 320 else 0.0


_B = {}


def inst(path, x, y, z, yaw=0.0, s=1.0, pitch=0.0, roll=0.0):
    VIG.append((x, y))
    _B.setdefault(path, []).append((x, y, z, yaw, s, pitch, roll))


def flush():
    for path, tr in _B.items():
        cull = CULL.get(path)
        if cull is None:
            cull = auto_cull(path) or 14000.0
        C.instanced(path, tr, collide=path in COLLIDE, cull=cull if cull > 0 else 30000.0, sub="Instanced")
    _B.clear()


def put(path, x, y, z=None, yaw=0.0, s=1.0, pitch=0.0, roll=0.0, sub="Props", collide=None, cull=None, claim=0.0):
    if collide is None:
        sz = size(path)
        k = max(s) if isinstance(s, tuple) else s
        collide = sz[2] * k > 45.0
    return C.place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=s, sub=sub, collide=collide,
                   cull=auto_cull(path, s) if cull is None else cull, claim_r=claim)


def gnd(x, y):
    return C.ground(x, y)


def fwd(yaw, d):
    return math.cos(math.radians(yaw)) * d, math.sin(math.radians(yaw)) * d


HOUSES = []
VIG = []
POIS = []


def poi(name, x, y):
    POIS.append(f"{name}@{x:.0f},{y:.0f}")
DOORS = []
TASKS = []


def door_block(x, y, r):
    for (f, dt) in DOORS:
        lx, ly = f.local(x, y)
        if -f.d * 0.5 - 340.0 < ly < -f.d * 0.5 + 20.0 and abs(lx - dt) < 95.0 + r:
            return True
    return False


def ok(x, y, r, lane_gap=40.0, allow_lane=False, doors=True):
    if not C.free(x, y, r, lane_gap=lane_gap, allow_lane=allow_lane, allow_reserved=True):
        return False
    if doors and door_block(x, y, r):
        return False
    for tx, ty in TASKS:
        if math.hypot(x - tx, y - ty) < r + 190.0:
            return False
    return True


def spots(cands, hw, hd, r=60.0, max_d=900.0, lane_gap=40.0):
    """Yield (x, y, *extra) near each candidate where a hw x hd footprint (9-point grid) is free."""
    rings = [(0.0, 0.0)] + [(d * math.cos(a * math.pi / 4.0), d * math.sin(a * math.pi / 4.0))
                            for d in range(150, int(max_d) + 1, 150) for a in range(8)]
    for c in cands:
        x0, y0, extra = c[0], c[1], tuple(c[2:])
        for dx, dy in rings:
            x, y = x0 + dx, y0 + dy
            if ok_all([(x + ax, y + ay) for ax in (-hw, 0, hw) for ay in (-hd, 0, hd)], r, lane_gap=lane_gap):
                yield (x, y) + extra
                break


def ok_all(pts, r, **kw):
    return all(ok(x, y, r, **kw) for x, y in pts)


_MEADOW = []


def scan_level():
    """Claim what already stands in the zone (trees, rocks, lamp posts, door props) and find the meadow."""
    keep = ("Nature/", "Village/Street", "Village/Props", "Village/Breakables", "Village/Smithy", "Village/Market",
            "Village/Square", "Gameplay")
    for a in C.actors.get_all_level_actors():
        if a.get_actor_label() == "KG_Meadow":
            _MEADOW.extend(a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))
            continue
        f = str(a.get_folder_path())
        if not f.startswith(keep):
            continue
        l = a.get_actor_location()
        if not (-5600 < l.x < 6600 and -4400 < l.y < 4400):
            continue
        _, e = a.get_actor_bounds(False)
        if f == "Nature/Trees":
            r = 110.0
        elif f == "Nature/Rocks":
            r = max(e.x, e.y) * 0.8
        elif f.startswith("Nature"):
            r = 60.0
        elif f in ("Village/Street", "Village/Props"):
            r = 40.0
        else:
            r = min(max(e.x, e.y), 250.0)
        C.claim(l.x, l.y, r)


def clear_grass(x, y, r):
    n = 0
    for comp in _MEADOW:
        hit = comp.get_instances_overlapping_sphere(unreal.Vector(x, y, gnd(x, y)), r, True)
        if hit:
            comp.remove_instances(list(hit))
            n += len(hit)
    C.stats["grass_cleared"] = C.stats.get("grass_cleared", 0) + n


def seat(path, x, y, face_yaw, mesh_yaw=0.0, height=None, z=None, claim=45.0):
    s = C.seat(path, x, y, z, yaw=face_yaw, seat_height=height)
    if s and mesh_yaw:
        try:
            s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=mesh_yaw))
            s.set_seat_mesh(C.mesh(path))
        except Exception as e:   # plain mesh fallback
            unreal.log_warning(f"KG_DRESS streets seat rot: {e}")
            s.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=face_yaw + mesh_yaw), False)
    C.stats["seats"] = C.stats.get("seats", 0) + 1
    C.claim(x, y, claim)
    return s


def breakable(path, x, y, yaw=None):
    C.claim(x, y, 45.0)
    C.stats["breakables"] = C.stats.get("breakables", 0) + 1
    return C.breakable(path, x, y, yaw=R.uniform(0, 360) if yaw is None else yaw)


def chest(x, y, yaw, name, table="Chest", path=None):
    C.claim(x, y, 60.0)
    return C.loot_chest(x, y, yaw=yaw, table=table, name=name, path=path)


def light(x, y, z, i=10.0, r=800.0, color=(255, 170, 95)):
    if C.stats.get("lights", 0) >= 10:
        return None
    return C.light(x, y, z, i, r, color)


# ============================================================================================ houses
class House:
    def __init__(self, f):
        self.f, self.name = f, f.name
        w, d = f.w, f.d
        self.gz = max(gnd(f.x + dx, f.y + dy) for dx in (-w * 0.5, w * 0.5) for dy in (-d * 0.5, d * 0.5)) + 2.0
        self.cw, self.cd = int(round(w / 200.0)), int(round(d / 200.0))
        self.door_t = (-self.cw * 100.0 + 100.0 + (self.cw // 2) * 200.0) if f.door else None

    def half(self, s):
        return self.f.w * 0.5 if s in ("front", "back") else self.f.d * 0.5

    def face(self, s):
        return (self.f.d * 0.5 if s in ("front", "back") else self.f.w * 0.5) + 31.0

    def at(self, s, t, out):
        (nx, ny), (tx, ty) = SIDES[s]
        e = self.face(s) + out
        return self.f.world(nx * e + tx * t, ny * e + ty * t)

    def nyaw(self, s):
        (nx, ny), _ = SIDES[s]
        return self.f.yaw + math.degrees(math.atan2(ny, nx))

    def tyaw(self, s):
        _, (tx, ty) = SIDES[s]
        return self.f.yaw + math.degrees(math.atan2(ty, tx))

    def cells(self, s):
        return self.cw if s in ("front", "back") else self.cd

    def windows(self, s, floor):
        n = self.cells(s)
        out = []
        for k in range(n):
            t = -n * 100.0 + 100.0 + k * 200.0
            if floor == 1 and k % 2 == 0:
                out.append(t)
            elif floor == 0 and k % 2 == 1 and not (s == "front" and k == n // 2):
                out.append(t)
        return out


PERSONA = {"house0": "flowers", "house1": "fisher", "house2": "flowers", "house3": "cooper", "house5": "baker",
           "house6": "carpenter", "house7": "woodsman", "house8": "smithfam", "house9": "grocer", "house10": "potter",
           "house11": "herbalist", "house12": "farmer", "house13": "captain", "house15": "family", "inn": "inn"}
KITS = {
    "fisher": ["oars", "barrels", "nets", "firewood", "crates"],
    "flowers": ["bench", "flowerbed", "barrels", "pots", "firewood"],
    "cooper": ["kegs", "barrels", "kegs", "tools", "firewood"],
    "baker": ["sacks", "firewood", "crates", "sacks"],
    "carpenter": ["planks", "firewood", "tools", "crates", "bench"],
    "woodsman": ["firewood", "firewood", "tools", "barrels", "firewood"],
    "smithfam": ["tools", "firewood", "barrels", "bench", "crates"],
    "grocer": ["produce", "crates", "sacks", "barrels"],
    "potter": ["pots", "pots", "bench", "barrels"],
    "herbalist": ["herbs", "pots", "bench", "flowerbed", "barrels"],
    "farmer": ["sacks", "produce", "tools", "firewood", "barrels"],
    "captain": ["cannon", "barrels", "oars", "bench", "crates"],
    "family": ["bench", "toys", "firewood", "barrels", "crates"],
    "inn": ["kegs", "kegs", "crates", "barrels", "firewood"],
}
KIT_W = {"flowers_s": 90, "pots_s": 70, "oars": 110, "barrels": 120, "nets": 120, "firewood": 250, "crates": 170, "bench": 300, "flowerbed": 260,
         "pots": 170, "kegs": 150, "tools": 110, "sacks": 160, "planks": 150, "produce": 180, "herbs": 130,
         "cannon": 160, "toys": 170}


def flower_box(h, s, t, floor, k):
    sill = h.gz + (98.0 if floor == 0 else 398.0)
    x, y = h.at(s, t, 20.0)
    if not C.in_zone(x, y):
        return
    yaw = h.tyaw(s)
    inst(P + "FarmCrate_Empty", x, y, sill - 16.0, yaw, (1.4, 0.9, 1.1))
    for i, dt in enumerate((-32.0, 0.0, 32.0)):
        fx, fy = h.at(s, t + dt, 22.0)
        inst(FLOWERS[(k + i) % len(FLOWERS)], fx, fy, sill + 2.0, R.uniform(0, 360), R.uniform(0.5, 0.62))
    C.stats["flower_boxes"] = C.stats.get("flower_boxes", 0) + 1


def potted(x, y, k, big=True):
    z = gnd(x, y)
    if big:
        inst(P + "Vase_2", x, y, z, R.uniform(0, 360), 0.62)
        plant = [N + "Bush_Common_Flowers", N + "Plant_1", N + "Fern_1"][k % 3]
        sc = {N + "Bush_Common_Flowers": 0.27, N + "Plant_1": 0.42, N + "Fern_1": 0.22}[plant]
        inst(plant, x, y, z + 28.0, R.uniform(0, 360), sc)
    else:
        inst(P + "Vase_4", x, y, z, R.uniform(0, 360), 0.9)
        inst(FLOWERS[k % len(FLOWERS)], x, y, z + 40.0, R.uniform(0, 360), 0.45)
    C.claim(x, y, 30.0)


def lean(path, x, y, toward_yaw, length, pitch=72.0, z=None):
    bx, by = fwd(toward_yaw, -length * math.cos(math.radians(pitch)) - 4.0)
    inst(path, x + bx, y + by, gnd(x, y) if z is None else z, toward_yaw, 1.0, pitch, 0.0)


def kit(h, s, t, name):
    """One wall vignette centred at t along side s (items against the wall)."""
    ty, ny = h.tyaw(s), h.nyaw(s)
    C.stats["kit_" + name] = C.stats.get("kit_" + name, 0) + 1
    at = lambda dt, out: h.at(s, t + dt, out)  # noqa: E731
    if name == "firewood":
        n = 4
        for layer, m in enumerate((n, n - 1, n - 2)):
            for i in range(m):
                x, y = at((i - (m - 1) / 2.0) * 56.0, 36.0)
                inst(FIREWOOD, x, y, gnd(x, y) - 2.0 + layer * 40.0, ty + R.choice((0, 180)) + R.uniform(-6, 6), 1.0)
        x, y = at(n * 30.0 + 10.0, 55.0)
        if R.random() < 0.6:
            inst(P + "Axe_Bronze", x, y, gnd(x, y) + 38.0, ty + 90.0, 1.0, 0.0, 12.0)
    elif name == "barrels":
        x, y = at(0.0, 45.0)
        inst([P + "Barrel", PIR + "Barrel_0", PIR + "Barrel_3"][R.randrange(3)], x, y, gnd(x, y), R.uniform(0, 360),
             1.0 if R.random() < 0.5 else 0.72)
        x, y = at(55.0, 60.0)
        inst(P + "Bucket_Wooden_1", x, y, gnd(x, y), R.uniform(0, 360), 1.0)
    elif name == "crates":
        x, y = at(-30.0, 55.0)
        inst(P + "Crate_Wooden", x, y, gnd(x, y) + 4.0, ty + R.uniform(-8, 8), 1.0)
        inst(PIR + "crates_1", x, y, gnd(x, y) + 97.0, ty + R.uniform(-25, 25), 0.62)
        x, y = at(55.0, 50.0)
        inst(P + "Bag", x, y, gnd(x, y), R.uniform(0, 360), 0.85)
        if R.random() < 0.5:
            x, y = at(40.0, 130.0)
            if ok(x, y, 45.0, lane_gap=10.0):
                breakable(P + "Crate_Wooden", x, y)
    elif name == "bench":
        x, y = at(0.0, 30.0)
        seat(P + "Bench", x, y, ny, mesh_yaw=90.0, height=52.0)
        x, y = at(170.0, 35.0)
        potted(x, y, R.randrange(9), big=False)
    elif name == "flowerbed":
        for i in range(5):
            x, y = at((i - 2) * 50.0, 40.0 + (i % 2) * 22.0)
            inst([N + "Flower_3_Single", N + "Flower_4_Single", N + "Bush_Common_Flowers"][i % 3], x, y, gnd(x, y) - 4.0,
                 R.uniform(0, 360), [0.45, 0.4, 0.4][i % 3])
        for i in range(6):
            x, y = at((i - 2.5) * 45.0, 95.0)
            inst(FLOWERS[i % len(FLOWERS)], x, y, gnd(x, y), R.uniform(0, 360), 0.7)
        for i in (-1, 1):
            x, y = at(i * 65.0, 118.0)
            inst(V + "Prop_ExteriorBorder_Straight1", x, y, gnd(x, y) - 3.0, ty, (0.65, 0.5, 1.0))
    elif name == "flowers_s":
        for i in range(3):
            x, y = at((i - 1) * 30.0, 32.0 + (i % 2) * 20.0)
            p = (N + "Flower_3_Single", N + "Bush_Common_Flowers", N + "Flower_4_Single")[(i + int(abs(t))) % 3]
            inst(p, x, y, gnd(x, y) - 4.0, R.uniform(0, 360), {N + "Bush_Common_Flowers": 0.35}.get(p, 0.42))
        for i in range(3):
            x, y = at((i - 1) * 35.0, 70.0)
            inst(FLOWERS[(i + int(abs(t))) % len(FLOWERS)], x, y, gnd(x, y) - 1.0, R.uniform(0, 360), 0.65)
        x, y = at(0.0, 95.0)
        inst(V + "Prop_ExteriorBorder_Straight1", x, y, gnd(x, y) - 3.0, ty, (0.5, 0.35, 1.0))
    elif name == "pots_s":
        x, y = at(-15.0, 30.0)
        potted(x, y, int(abs(t)) % 7, True)
        x, y = at(25.0, 45.0)
        potted(x, y, int(abs(t)) % 5 + 1, False)
    elif name == "pots":
        for i, (dt, out, big) in enumerate(((-50, 35, True), (10, 30, False), (55, 40, True), (0, 85, False))):
            x, y = at(dt, out)
            potted(x, y, i + R.randrange(3), big)
        x, y = at(-10.0, 120.0)
        if ok(x, y, 40.0, lane_gap=10.0):
            breakable(P + "Vase_4", x, y)
    elif name == "kegs":
        x, y = at(0.0, 45.0)
        inst(P + "Barrel_Holder", x, y, gnd(x, y), ty, 1.0)
        x, y = at(-50.0, 110.0)
        if ok(x, y, 40.0, lane_gap=10.0):
            breakable(P + "Barrel", x, y)
        x, y = at(60.0, 105.0)
        inst(P + "Barrel", x, y, gnd(x, y), R.uniform(0, 360), 1.0)
    elif name == "tools":
        for i, (path, ln) in enumerate(((WP + "SM_KG_Shovel", 147.0), (P + "Pickaxe_Bronze", 0.0),
                                        (WP + "SM_KG_Shovel", 147.0))):
            x, y = at((i - 1) * 38.0, 0.0)
            if ln:
                lean(path, x, y, ny + 180.0, ln, 70.0 + R.uniform(-4, 4))
            else:
                x, y = at((i - 1) * 38.0, 22.0)
                inst(path, x, y, gnd(x, y) + 49.0, ty, 1.0, 0.0, -14.0)
    elif name == "oars":
        for i in range(3):
            x, y = at((i - 1) * 32.0, 0.0)
            lean(WP + ("SM_KG_Oar" if i != 1 else "SM_KG_FishingRod"), x, y, ny + 180.0,
                 220.0 if i != 1 else 190.0, 74.0 + R.uniform(-3, 3))
        x, y = at(70.0, 45.0)
        inst(P + "Bucket_Metal", x, y, gnd(x, y), 0.0, 1.0)
        inst(WP + "SM_KG_Fish_Mackerel", x, y, gnd(x, y) + 30.0, R.uniform(0, 360), 1.0, 0.0, 90.0)
    elif name == "nets":
        for i, path in enumerate((P + "Rope_3", P + "Rope_2", P + "Rope_1")):
            x, y = at((i - 1) * 45.0, 55.0)
            inst(path, x, y, gnd(x, y) + i * 9.0, R.uniform(0, 360), 1.0)
        x, y = at(0.0, 0.0)
        inst(P + "Peg_Rack", x, y, h.gz + 175.0, ny - 90.0, 1.0)
    elif name == "sacks":
        for i, (dt, out, zz) in enumerate(((-45, 45, 0), (20, 45, 0), (-10, 50, 60), (70, 70, 0))):
            x, y = at(dt, out)
            inst(P + "Bag", x, y, gnd(x, y) + zz, R.uniform(0, 360), 0.85 if zz == 0 else 0.7)
    elif name == "planks":
        for i in range(4):
            x, y = at(-50.0 + i * 12.0, 0.0)
            lean(PIR + "Planks_" + str(i % 4), x, y, ny + 180.0, 197.0, 80.0)
        x, y = at(40.0, 50.0)
        for i in range(4):
            inst(PIR + "Planks_1", x, y, gnd(x, y) + i * 10.0, ty + R.uniform(-5, 5), 0.8)
    elif name == "produce":
        for i, path in enumerate((P + "FarmCrate_Carrot", P + "FarmCrate_Apple", P + "FarmCrate_Carrot")):
            x, y = at((i - 1) * 60.0, 40.0)
            inst(path, x, y, gnd(x, y) + 2.0, ty + R.uniform(-6, 6), 1.0)
        x, y = at(0.0, 45.0)
        inst(P + "FarmCrate_Apple", x, y, gnd(x, y) + 26.0, ty + 8.0, 0.95)
        x, y = at(0.0, 115.0)
        if ok(x, y, 45.0, lane_gap=10.0):
            breakable(P + "Barrel_Apples", x, y)
    elif name == "herbs":
        x, y = at(0.0, 0.0)
        inst(IP + "SM_KG_HerbRack", x, y, h.gz + 200.0, ny - 90.0, 1.0)
        for i in range(3):
            x, y = at((i - 1) * 45.0, 40.0)
            potted(x, y, i + 1, big=(i != 1))
    elif name == "cannon":
        x, y = at(0.0, 60.0)
        inst(PIR + "Cannon_Ball_1", x, y, gnd(x, y) - 2.0, R.uniform(0, 360), 0.8)
        x, y = at(-70.0, 50.0)
        inst(PIR + "Barrel_9", x, y, gnd(x, y), R.uniform(0, 360), 0.75)
    elif name == "toys":
        x, y = at(-40.0, 60.0)
        inst(P + "Dummy", x, y, gnd(x, y), ny + 180.0, 0.7)
        x, y = at(40.0, 55.0)
        inst(PIR + "crates_0", x, y, gnd(x, y), ty + 10.0, 0.6)
        inst(P + "Shield_Wooden", x, y, gnd(x, y) + 70.0, ty, 0.8, 0.0, 80.0)
        x, y = at(60.0, 110.0)
        inst(P + "Sword_Bronze", x, y, gnd(x, y) + 3.0, ty, 0.7, 0.0, 90.0)


def dress_house(h):
    persona = PERSONA.get(h.name, "flowers")
    kits = list(KITS[persona])
    R.shuffle(kits)
    fz = 0
    for s in SIDES:
        # Flower boxes: every upper window, ground windows on alternate houses.
        for t in h.windows(s, 1):
            flower_box(h, s, t, 1, fz)
            fz += 1
        if persona in ("flowers", "herbalist", "family", "grocer") or s == "front":
            for t in h.windows(s, 0):
                flower_box(h, s, t, 0, fz)
                fz += 1
    # Door flank pots.
    if h.door_t is not None:
        for sgn in (-1, 1):
            x, y = h.at("front", h.door_t + sgn * 118.0, 26.0)
            if C.in_zone(x, y) and C.lane_distance(x, y) > 15.0:
                potted(x, y, fz + sgn, big=(sgn < 0 or persona in ("flowers", "herbalist")))
    # Gardens behind / beside the house when there is room.
    garden_kind = {"farmer": "veg", "family": "veg", "herbalist": "herb", "flowers": "flower", "woodsman": "veg",
                   "fisher": "drying", "captain": "flower", "carpenter": "veg", "potter": "flower", "grocer": "veg",
                   "cooper": "herb", "smithfam": "veg", "baker": "veg", "inn": "beer"}.get(persona, "veg")
    for s in ("back", "left", "right"):
        if try_garden(h, s, garden_kind):
            break
    # Wall vignettes along every side, walking the wall and fitting kits where the ground is free.
    ki = 0
    for s in ("front", "left", "right", "back"):
        half = h.half(s)
        t = -half + (35.0 if s == "front" else 70.0)
        while t < half - 70.0 and ki < 40:
            name = kits[ki % len(kits)]
            w = KIT_W[name]
            if s == "front":
                name = ("flowers_s", "pots_s", "flowers_s")[ki % 3]
                w = KIT_W[name]
            tc = t + w * 0.5
            if tc + w * 0.5 > half - 50.0:
                break
            if s == "front" and h.door_t is not None and abs(tc - h.door_t) < w * 0.5 + 115.0:
                t = h.door_t + 115.0
                continue
            if s == "front" and tc + w * 0.5 > half - 120.0:     # builder's door barrel
                break
            pts = [h.at(s, tc + dt, 45.0) for dt in (-w * 0.45, 0.0, w * 0.45)] + [h.at(s, tc, 90.0)]
            if ok_all(pts, 20.0 if s == "front" else 28.0, lane_gap=15.0) and C.slope(*h.at(s, tc, 50.0), r=min(w * 0.5, 90.0)) < 110.0:
                kit(h, s, tc, name)
                for (x, y) in pts:
                    C.claim(x, y, 20.0)
                ki += 1
                t += w + R.uniform(30.0, 80.0)
            else:
                t += 40.0


def try_garden(h, s, kind):
    half = h.half(s)
    W = min(2.0 * half + 60.0, 820.0)
    for D in (520.0, 420.0, 340.0):
        pts = [h.at(s, tt, out) for tt in (-W * 0.5 + 40.0, 0.0, W * 0.5 - 40.0) for out in (100.0, D * 0.5, D - 30.0)]
        if not ok_all(pts, 50.0, lane_gap=30.0):
            continue
        cx, cy = h.at(s, 0.0, 30.0 + D * 0.5)
        if C.slope(cx, cy, r=max(W, D) * 0.5) > 110.0:
            continue
        garden(cx, cy, h.nyaw(s) - 90.0, W, D, kind)
        for tt in (-W * 0.5 + 70.0, -W * 0.25, 0.0, W * 0.25, W * 0.5 - 70.0):
            for out in (110.0, D * 0.5 + 30.0, D - 20.0):
                C.claim(*h.at(s, tt, out), 75.0)
        return True
    return False


def fence_run(ax, ay, bx, by, gate=None):
    """Wooden fence from A to B (segments scaled to fit). gate = fraction 0..1 of a skipped middle segment."""
    L = math.hypot(bx - ax, by - ay)
    if L < 60.0:
        return
    n = max(1, int(round(L / 205.0)))
    seg = L / n
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    for i in range(n):
        if gate is not None and i == int(gate * n) and n > 1:
            continue
        t0, t1 = i / n, (i + 1) / n
        x0, y0 = ax + (bx - ax) * t0, ay + (by - ay) * t0
        x1, y1 = ax + (bx - ax) * t1, ay + (by - ay) * t1
        mx, my = (x0 + x1) * 0.5, (y0 + y1) * 0.5
        z = min(gnd(x0, y0), gnd(x1, y1), gnd(mx, my))
        inst(V + "Prop_WoodenFence_Single", mx, my, z, yaw + R.uniform(-1.5, 1.5), (seg / 206.0 * 1.01, 1.0, 1.0))
        C.claim(mx, my, 50.0)


def garden(cx, cy, yaw, W, D, kind):
    """Fenced garden; local -Y is the open side against the house wall."""
    g = C.Frame(cx, cy, yaw, W, D)
    clear_grass(cx, cy, max(W, D) * 0.6)
    L = lambda lx, ly: g.world(lx, ly)  # noqa: E731
    hw, hd = W * 0.5, D * 0.5
    if kind != "beer":
        fence_run(*L(-hw, -hd + 20.0), *L(-hw, hd))
        fence_run(*L(-hw, hd), *L(hw, hd), gate=0.5)
        fence_run(*L(hw, hd), *L(hw, -hd + 20.0))
    C.stats["gardens"] = C.stats.get("gardens", 0) + 1
    if kind == "veg":
        rows = int((D - 130.0) // 75.0)
        for r in range(rows):
            ly = -hd + 95.0 + r * 75.0
            mx, my = L(0.0, ly)
            inst(WP + "SM_KG_DigMound", mx, my, gnd(mx, my) - 12.0, yaw, ((W - 140.0) / 121.0, 0.45, 0.4))
            crop = [N + "Carrot", N + "Plant_7", N + "Carrot", N + "Clover_2"][r % 4] if r < 4 else N + "Plant_7"
            crop = crop.replace(N + "Carrot", P + "Carrot")
            step = 24.0 if "Carrot" in crop else 48.0
            n = int((W - 170.0) // step)
            for i in range(n):
                lx = -hw + 90.0 + i * step + R.uniform(-4, 4)
                x, y = L(lx, ly + R.uniform(-5, 5))
                if "Carrot" in crop:
                    inst(crop, x, y, gnd(x, y) - 2.0, R.uniform(0, 360), R.uniform(0.9, 1.1))
                elif "Plant_7" in crop:
                    inst(crop, x, y, gnd(x, y) - 2.0, R.uniform(0, 360), R.uniform(0.42, 0.52))
                else:
                    inst(crop, x, y, gnd(x, y) - 3.0, R.uniform(0, 360), R.uniform(0.4, 0.5))
        x, y = L(hw - 70.0, hd - 70.0)
        inst(P + "Dummy", x, y, gnd(x, y), yaw + 180.0 + R.uniform(-20, 20), 0.95)
        x, y = L(-hw + 55.0, hd - 60.0)
        inst(P + "FarmCrate_Carrot", x, y, gnd(x, y) + 2.0, yaw + 12.0, 1.0)
        x, y = L(-hw + 60.0, hd - 130.0)
        inst(P + "Bucket_Wooden_1", x, y, gnd(x, y), R.uniform(0, 360), 1.0)
        x, y = L(hw - 60.0, -hd + 70.0)
        inst(WP + "SM_KG_Shovel", x, y, gnd(x, y) + 20.0, yaw + 30.0, 1.0, -78.0, 0.0)
    elif kind in ("flower", "herb"):
        palette = FLOWERS + [N + "Flower_3_Single", N + "Flower_4_Single"] if kind == "flower" else \
            [N + "Plant_1", N + "Fern_1", N + "Clover_1", N + "Clover_2", N + "Plant_7"]
        for r in range(int((D - 100.0) // 70.0)):
            for i in range(int((W - 120.0) // 60.0)):
                lx, ly = -hw + 70.0 + i * 60.0 + R.uniform(-12, 12), -hd + 80.0 + r * 70.0 + R.uniform(-12, 12)
                if abs(lx) < 45.0:
                    continue   # path down the middle
                x, y = L(lx, ly)
                p = R.choice(palette)
                sc = {N + "Flower_3_Single": 0.45, N + "Flower_4_Single": 0.4, N + "Plant_1": 0.5, N + "Fern_1": 0.3,
                      N + "Plant_7": 0.5, N + "Clover_1": 0.6, N + "Clover_2": 0.55}.get(p, 0.75)
                inst(p, x, y, gnd(x, y) - 3.0, R.uniform(0, 360), sc * R.uniform(0.85, 1.15))
        for k in range(int((D - 80.0) // 80.0)):
            x, y = L(R.uniform(-12, 12), -hd + 60.0 + k * 80.0)
            inst(N + "RockPath_Round_Small_" + str(1 + k % 3), x, y, gnd(x, y) - 2.0, R.uniform(0, 360), 0.6)
        x, y = L(hw - 60.0, hd - 60.0)
        potted(x, y, R.randrange(9), True)
    elif kind == "drying":
        a, b = L(-hw + 80.0, 0.0), L(hw - 80.0, 0.0)
        drying_rack(a[0], a[1], b[0], b[1])
        x, y = L(-hw + 70.0, hd - 70.0)
        inst(P + "Rope_3", x, y, gnd(x, y), R.uniform(0, 360), 1.0)
        x, y = L(hw - 70.0, hd - 70.0)
        inst(PIR + "Barrel_3", x, y, gnd(x, y), R.uniform(0, 360), 0.75)
    elif kind == "beer":
        beer_garden(g, W, D)


# ============================================================================================ reusable pieces
def post(x, y, h=270.0, thick=0.7):
    z = gnd(x, y)
    inst(V + "Corner_Exterior_Wood", x, y, z - 10.0, R.uniform(0, 360), (thick, thick, (h + 10.0) / 300.0))
    C.claim(x, y, 30.0)
    return z + h


def rope(ax, ay, az, bx, by, bz, sag=18.0):
    """Thin line A->B with a sag in the middle (two straight pieces)."""
    mx, my, mz = (ax + bx) * 0.5, (ay + by) * 0.5, (az + bz) * 0.5 - sag
    for (x0, y0, z0), (x1, y1, z1) in (((ax, ay, az), (mx, my, mz)), ((mx, my, mz), (bx, by, bz))):
        L = math.hypot(x1 - x0, y1 - y0)
        a = math.degrees(math.atan2(z1 - z0, L))
        yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
        inst(IP + "SM_KG_RailPost", x0, y0, z0, yaw, (0.2, 0.2, math.hypot(L, z1 - z0) / 108.0), a - 90.0, 0.0)
    return lambda t: (ax + (bx - ax) * t, ay + (by - ay) * t,
                      az + (bz - az) * t - sag * (1.0 - (2.0 * t - 1.0) ** 2))


CLOTHES = [(P + "Banner_1_Cloth", (0.55, 1.0, 0.32)), (P + "Banner_2_Cloth", (0.6, 1.0, 0.36)),
           (P + "Banner_1_Cloth", (0.4, 1.0, 0.24)), (P + "Banner_2_Cloth", (0.48, 1.0, 0.45))]


def laundry(ax, ay, az, bx, by, bz, sway=True):
    f = rope(ax, ay, az, bx, by, bz)
    L = math.hypot(bx - ax, by - ay)
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    n = max(2, int(L // 75.0))
    for i in range(n):
        t = (i + 0.7) / (n + 0.4)
        x, y, z = f(t)
        kind = R.random()
        if kind < 0.62:
            path, sc = R.choice(CLOTHES)
            if sway and C.stats.get("movers", 0) < 34 and R.random() < 0.55:
                C.mover(path, x, y, z + 2.0, yaw=yaw, scale=sc, sway=R.uniform(3.0, 6.0), sway_hz=R.uniform(0.25, 0.45))
            else:
                inst(path, x, y, z + 2.0, yaw + R.uniform(-4, 4), sc)
        else:
            rug = R.choice((IP + "SM_KG_Rug_Runner", IP + "SM_KG_Rug_Rect", IP + "SM_KG_Rug_Oval"))
            sc = {IP + "SM_KG_Rug_Runner": 0.34, IP + "SM_KG_Rug_Rect": 0.3, IP + "SM_KG_Rug_Oval": 0.32}[rug]
            half_h = size(rug)[1] * 0.5 * sc
            inst(rug, x, y, z - half_h, yaw, sc, 0.0, 90.0)
    C.stats["laundry_lines"] = C.stats.get("laundry_lines", 0) + 1


def drying_rack(ax, ay, bx, by, fish=True):
    """Fish drying rack: two posts, a pole and a row of hanging fish."""
    za = post(ax, ay, 190.0, 0.6)
    zb = post(bx, by, 190.0, 0.6)
    f = rope(ax, ay, za, bx, by, zb, sag=4.0)
    L = math.hypot(bx - ax, by - ay)
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    fishes = [WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Mackerel", WP + "SM_KG_Fish_Salmon", WP + "SM_KG_Fish_Cod"]
    n = int(L // 32.0)
    for i in range(1, n):
        x, y, z = f(i / n)
        p = fishes[i % len(fishes)]
        ln = size(p)[0] * 0.5
        inst(p, x, y, z - ln - 3.0, yaw + 90.0, 1.0, -88.0 + R.uniform(-6, 6), 0.0)
    C.stats["drying_racks"] = C.stats.get("drying_racks", 0) + 1
    poi("Fish drying rack", (ax + bx) * 0.5, (ay + by) * 0.5)


def signpost(x, y, targets, lantern=False):
    """targets: [(yaw_deg, label_colour_idx)] boards pointing along lanes."""
    top = post(x, y, 250.0, 0.75)
    for i, yaw in enumerate(targets):
        z = top - 30.0 - i * 34.0
        dx, dy = fwd(yaw, 45.0)
        inst(PIR + "Planks_" + str(i % 4), x + dx, y + dy, z, yaw, (0.46, 0.6, 1.0), 0.0, 90.0)
    inst(N + "Petal_" + str(1 + R.randrange(5)), x + 30, y + 10, gnd(x, y), 0.0, 0.8)
    inst(N + "Grass_Common_Short", x - 25, y - 15, gnd(x, y) - 3.0, R.uniform(0, 360), 0.6)
    if lantern:
        put(P + "Lantern_Wall", x, y, top - 95.0, yaw=targets[0] + 90.0, collide=False, sub="Signposts")
    C.stats["signposts"] = C.stats.get("signposts", 0) + 1


def stall(x, y, yaw, goods, sign=None):
    """Market stall facing local -Y (goods on a counter table in front)."""
    put(P + "Stall_Empty", x, y, C.ground_min(x, y, 90.0), yaw=yaw, sub="Market", claim=130.0)
    fx, fy = x + math.sin(math.radians(yaw)) * 20.0, y - math.cos(math.radians(yaw)) * 20.0
    tz = gnd(fx, fy)
    put(KI + "Kitchen_LongTable", fx, fy, tz, yaw=yaw, s=(1.05, 0.9, 1.1), sub="Market")
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for i, g in enumerate(goods):
        lx = (i - (len(goods) - 1) / 2.0) * (150.0 / max(1, len(goods)))
        gx, gy = fx + lx * c, fy + lx * s_
        inst(g[0], gx, gy, tz + 74.0, yaw + g[2], g[1])


# ============================================================================================ vignettes
def smithy_yard():
    sx, sy = -3800.0, 400.0
    # Ore / cannonball-sized iron heap and a coal heap west of the shed.
    for (x, y, path, sc) in [(-4330, 180, PIR + "Cannon_Ball_1", 1.0), (-4420, 300, PIR + "Cannon_Ball_0", 0.9),
                             (-4300, 340, PIR + "Cannon_Ball_0", 0.7)]:
        if ok(x, y, 60.0):
            inst(path, x, y, gnd(x, y) - 4.0, R.uniform(0, 360), sc)
            C.claim(x, y, 60.0)
    for (x, y, sc, yw) in ((-4330, 560, 1.15, 0.0), (-4400, 470, 0.8, 70.0), (-4250, 640, 0.7, 150.0)):   # coal heap
        inst(PIR + "Cannon_Ball_1", x, y, gnd(x, y) - 6.0, yw, (sc * 1.2, sc * 1.2, sc * 0.55))
    inst(WP + "SM_KG_Shovel", -4290, 470, gnd(-4290, 470) + 25.0, 30.0, 1.0, -60.0, 0.0)
    C.claim(-4330, 560, 90.0)
    # Quench trough + buckets on the north side.
    for x, y, yaw in [(-3760, 830, 0.0)]:
        if ok(x, y, 90.0):
            put(P + "FarmCrate_Empty", x, y, gnd(x, y), yaw=yaw, s=(2.3, 1.5, 2.2), sub="Smithy", claim=90.0)
            put(P + "Bucket_Metal", x + 110, y + 10, yaw=30.0, sub="Smithy", collide=False)
            put(P + "Bucket_Wooden_1", x - 115, y - 20, yaw=0.0, sub="Smithy", collide=False)
    # Finished goods: weapon barrel, tool rack, shields and a second anvil.
    x, y = -4250.0, 820.0
    if ok(x, y, 60.0):
        inst(P + "Barrel", x, y, gnd(x, y), 0.0, 1.0)
        for i in range(4):
            a = i * 1.57 + 0.4
            inst(P + "Sword_Bronze", x + 14 * math.cos(a), y + 14 * math.sin(a), gnd(x, y) + 88.0, R.uniform(0, 360), 1.0,
                 R.uniform(-12, 12), R.uniform(-12, 12))
        C.claim(x, y, 60.0)
    x, y = -4450.0, 700.0
    if ok(x, y, 90.0):
        put(P + "Workbench", x, y, yaw=90.0, sub="Smithy", claim=100.0)
        for i, (path, dx, rl) in enumerate(((P + "Axe_Bronze", -50, 90), (P + "Pickaxe_Bronze", 0, 90),
                                            (WP + "SM_KG_Shovel", 40, 0))):
            inst(path, x + 5, y + dx, gnd(x, y) + 92.0, 90.0 + R.uniform(-10, 10), 0.9, 0.0, rl)
    x, y = -4150.0, -60.0
    if ok(x, y, 80.0):
        put(P + "Anvil_Log", x, y, yaw=40.0, sub="Smithy", claim=70.0)
        for i in range(5):   # horseshoes stand-in: a scatter of iron bits and chain
            inst(P + "Chain_Coil", x + 80, y - 40, gnd(x, y), R.uniform(0, 360), 0.6)
            break
    # Supply wagon loaded with ore, parked by the lane.
    for x, y, yaw in [(-4700, 200, 60.0), (-4750, 900, 90.0), (-4550, 1050, 100.0)]:
        if ok(x, y, 190.0, lane_gap=30.0):
            put(V + "Prop_Wagon", x, y, gnd(x, y), yaw=yaw, sub="Smithy", claim=200.0)
            c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            for i, (lx, ly) in enumerate(((0, -60), (0, -150), (30, -110))):
                wx, wy = x + lx * c - ly * s_, y + lx * s_ + ly * c
                inst(PIR + "Cannon_Ball_0", wx, wy, gnd(x, y) + 75.0, R.uniform(0, 360), 0.55)
            break
    # Smith's tool chest, stool, torch + glow.
    for x, y, yaw in [(-4300, 950, 180.0), (-3500, 950, 180.0), (-4450, 450, 90.0)]:
        if ok(x, y, 60.0):
            chest(x, y, yaw, "Smith's Tool Chest", "Chest", PIR + "chest_common_0")
            break
    for x, y in [(-3560, 790), (-4200, 620)]:
        if ok(x, y, 35.0):
            seat(P + "Stool", x, y, R.uniform(0, 360))
            break
    x, y = -4180.0, 100.0
    if ok(x, y, 30.0):
        put(PIR + "Torch_1", x, y, sub="Smithy", collide=True)
        light(x, y, gnd(x, y) + 250.0, 16.0, 700.0, (255, 140, 60))
    # Sign: a shield swinging from the shed's corner post.
    C.mover(P + "Shield_Wooden", -3500.0 + 25.0, 190.0 - 20.0, gnd(-3500, 190) + 225.0, yaw=0.0, sway=5.0, sway_hz=0.3)
    clear_grass(-4300, 500, 450)


def market_row():
    goods_a = [(P + "FarmCrate_Apple", 1.0, 5), (P + "FarmCrate_Carrot", 0.9, -8), (P + "Barrel_Apples", 0.55, 0)]
    goods_b = [(WP + "SM_KG_Fish_Salmon", 1.3, 80), (WP + "SM_KG_Fish_Cod", 1.3, 95), (KI + "Kitchen_Bottle", 1.3, 0),
               (IP + "SM_KG_Cheese", 1.4, 0), (IP + "SM_KG_Bread", 1.6, 30)]
    goods_c = [(P + "Vase_4", 0.8, 0), (P + "Pot_1_Lid", 1.0, 20), (P + "Vase_4", 0.7, 0), (KI + "Kitchen_Jar", 2.0, 0)]
    placed = 0
    for (x, y, yaw, goods) in [(2470.0, -150.0, 5.0, goods_a), (2720.0, -165.0, 5.0, goods_b),
                               (3700.0, 350.0, -60.0, goods_c), (1500.0, 1200.0, 100.0, goods_c)]:
        if ok(x, y, 120.0, lane_gap=20.0):
            stall(x, y, yaw, goods)
            placed += 1
            fx, fy = x + math.sin(math.radians(yaw)) * -80.0, y + math.cos(math.radians(yaw)) * 80.0
            if R.random() < 0.8:
                inst(P + "Crate_Wooden", x + 120.0, y + 60.0, gnd(x + 120, y + 60), R.uniform(0, 360), 0.8)
    if placed:
        light(2600.0, -60.0, gnd(2600, -60) + 250.0, 12.0, 900.0)
        C.stats["stalls"] = placed


def beer_garden(g, W, D):
    L = g.world
    x, y = L(0.0, 0.0)
    put(P + "Table_Large", x, y, gnd(x, y), yaw=g.yaw, sub="BeerGarden", claim=150.0)
    for sgn in (-1, 1):
        bx, by = L(0.0, sgn * 95.0)
        seat(P + "Bench", bx, by, g.yaw + (90.0 if sgn < 0 else -90.0), mesh_yaw=90.0, height=52.0)
    z = gnd(x, y) + 81.0
    for i, lx in enumerate((-110, -60, -10, 40, 95)):
        mx, my = L(lx, R.uniform(-30, 30))
        inst(P + "Mug", mx, my, z, R.uniform(0, 360), 1.2)
    for lx in (-85, 70):
        mx, my = L(lx, 10)
        inst(P + "Table_Plate", mx, my, z, 0.0, 1.0)
    bx, by = L(20.0, -5.0)
    inst(IP + "SM_KG_Bread", bx, by, z + 2, 30.0, 1.0)
    for (lx, ly) in ((W * 0.5 - 60, -D * 0.5 + 60), (W * 0.5 - 60, D * 0.5 - 60), (-W * 0.5 + 60, D * 0.5 - 60)):
        kx, ky = L(lx, ly)
        if ok(kx, ky, 40.0, lane_gap=10.0):
            inst(P + "Barrel", kx, ky, gnd(kx, ky), R.uniform(0, 360), 1.0)
            inst(P + "Mug", kx, ky, gnd(kx, ky) + 90.0, 0.0, 1.2)
    lx, ly = L(-W * 0.5 + 40.0, -D * 0.5 + 40.0)
    top = post(lx, ly, 260.0)
    put(P + "Lantern_Wall", lx, ly, top - 110.0, yaw=g.yaw + 45.0, collide=False, sub="BeerGarden")
    light(*L(-W * 0.5 + 60.0, -D * 0.5 + 120.0), gnd(lx, ly) + 220.0, 10.0, 800.0)
    rx, ry = L(W * 0.5 - 40.0, -D * 0.5 + 40.0)
    top2 = post(rx, ry, 260.0)
    f = rope(lx, ly, top - 15.0, rx, ry, top2 - 15.0, sag=30.0)
    for i in range(1, 8):   # bunting: little cloth flags
        fx, fy, fz = f(i / 8.0)
        inst(P + "Banner_2_Cloth", fx, fy, fz, g.yaw, (0.25, 1.0, 0.14))
    C.stats["beer_garden"] = 1
    poi("Inn beer garden", x, y)


def allotments():
    """Community vegetable plots north-west of the smithy, with a shed-chest and a bench."""
    got = []
    cands = [(x, y) for y in (1400.0, 1950.0, 2500.0, 3050.0) for x in (-4850.0, -4300.0, -3750.0, -3200.0)]
    R.shuffle(cands)
    kinds = ["veg", "veg", "flower", "veg", "herb", "veg", "flower", "veg"]
    for (x, y) in spots(cands, 240.0, 200.0, r=50.0, max_d=450.0):
        if len(got) >= 7:
            break
        yaw = R.uniform(-6, 6)
        g = C.Frame(x, y, yaw, 460, 380)
        if C.slope(x, y, 230.0) < 110.0:
            garden(x, y, yaw, 440.0, 360.0, kinds[len(got) % len(kinds)])
            fx, fy = g.world(0.0, -190.0)
            fence_run(*g.world(-220.0, -180.0), *g.world(220.0, -180.0), gate=0.5)
            C.claim(x, y, 250.0)
            got.append((x, y))
    if got:
        # Shared tool shed stand-in: a chest, a wheelbarrow-ish cart, a bench and a water barrel between plots.
        x0, y0 = got[0]
        for dx, dy in ((0, -300), (300, 0), (-300, 0), (0, 300)):
            x, y = x0 + dx, y0 + dy
            if ok(x, y, 90.0):
                chest(x, y, R.uniform(0, 360), "Allotment Tool Box", "Crate")
                inst(P + "Barrel", x + 90, y + 20, gnd(x + 90, y + 20), 0.0, 1.0)
                inst(P + "Bucket_Wooden_1", x - 70, y + 40, gnd(x - 70, y + 40), 0.0, 1.0)
                break
        for (x, y) in got[1:3]:
            for dx, dy in ((0, -290), (290, 0), (0, 290), (-290, 0)):
                bx, by = x + dx, y + dy
                if ok(bx, by, 150.0):
                    seat(P + "Bench", bx, by, math.degrees(math.atan2(y - by, x - bx)), mesh_yaw=90.0, height=52.0,
                         claim=150.0)
                    break
        C.ambient("A_Birds_Loop", got[0][0], got[0][1], gnd(*got[0]) + 300.0, 0.6)
    C.stats["allotments"] = len(got)
    for (x, y) in got:
        poi("Allotment plot", x, y)


def orchard():
    """Small fruit trees with apple crates, north of the allotments (fills in around free ground)."""
    n = 0
    for y in (2700.0, 3150.0, 3450.0):
        for x in (-4900.0, -4450.0, -4000.0, -3550.0, -3100.0):
            x2, y2 = x + R.uniform(-80, 80), y + R.uniform(-80, 80)
            if ok(x2, y2, 150.0) and C.slope(x2, y2, 150.0) < 80.0:
                p = R.choice((N + "CommonTree_1", N + "CommonTree_2", N + "CommonTree_5"))
                inst(p, x2, y2, gnd(x2, y2) - 20.0, R.uniform(0, 360), R.uniform(0.5, 0.62))
                C.claim(x2, y2, 160.0)
                n += 1
                a = R.uniform(0, 6.28)
                cx, cy = x2 + 130 * math.cos(a), y2 + 130 * math.sin(a)
                if R.random() < 0.6 and ok(cx, cy, 40.0):
                    inst(P + "FarmCrate_Apple", cx, cy, gnd(cx, cy) + 2.0, R.uniform(0, 360), 1.0)
                    C.claim(cx, cy, 40.0)
                elif R.random() < 0.5 and ok(cx, cy, 40.0):
                    breakable(P + "Barrel_Apples", cx, cy)
    C.stats["orchard_trees"] = n


def washing_green():
    """Rows of posts with the whole village's washing, between the allotments and house 15."""
    lines = 0
    for (ax, ay, bx, by) in [(-3500, 1250, -2750, 1300), (-3450, 1600, -2700, 1650), (-2550, 1500, -2300, 2000),
                             (-4600, 3300, -3900, 3350), (-3300, 3400, -2700, 3350)]:
        if ok(ax, ay, 35.0) and ok(bx, by, 35.0) and ok((ax + bx) / 2, (ay + by) / 2, 60.0):
            za = post(ax, ay, 240.0, 0.6)
            zb = post(bx, by, 240.0, 0.6)
            laundry(ax, ay, za - 12.0, bx, by, zb - 12.0)
            lines += 1
            clear_grass((ax + bx) / 2, (ay + by) / 2, 250.0)
            for t in (0.25, 0.5, 0.75):
                C.claim(ax + (bx - ax) * t, ay + (by - ay) * t, 60.0)
            if lines == 1:
                x, y = ax + 60, ay - 80
                inst(P + "Cauldron", x, y, gnd(x, y), 0.0, 0.8)     # wash tub
                inst(P + "Bucket_Wooden_1", x + 70, y - 20, gnd(x + 70, y - 20), 0.0, 1.0)
                inst(IP + "SM_KG_Rug_Rect", x + 150, y - 60, gnd(x + 150, y - 60) + 2.0, 20.0, 0.45)


def alley_laundry():
    """Lines strung between neighbouring houses across the gaps (endpoints on upper-floor walls)."""
    done = set()
    cands = []
    for i, ha in enumerate(HOUSES):
        for hb in HOUSES[i + 1:]:
            if math.hypot(ha.f.x - hb.f.x, ha.f.y - hb.f.y) > 1500.0:
                continue
            for sa in SIDES:
                for sb in SIDES:
                    na, nb = ha.nyaw(sa), hb.nyaw(sb)
                    if abs(((na - nb) % 360.0) - 180.0) > 35.0:    # walls must face each other
                        continue
                    for ta in (-100.0, 100.0):
                        pa = ha.at(sa, ta, 3.0)
                        best = None
                        for tb in (-150.0, -50.0, 50.0, 150.0):
                            pb = hb.at(sb, tb, 3.0)
                            d = math.hypot(pa[0] - pb[0], pa[1] - pb[1])
                            if best is None or d < best[0]:
                                best = (d, pb)
                        d, pb = best
                        if not (220.0 < d < 900.0):
                            continue
                        ang = math.degrees(math.atan2(pb[1] - pa[1], pb[0] - pa[0]))
                        if abs(((ang - na + 180.0) % 360.0) - 180.0) > 40.0:
                            continue
                        mids = [(pa[0] + (pb[0] - pa[0]) * t, pa[1] + (pb[1] - pa[1]) * t) for t in (0.3, 0.5, 0.7)]
                        if not all(C.in_zone(*m) and not C.building_hit(m[0], m[1], 10.0) for m in mids):
                            continue
                        cands.append((d, ha, sa, pa, hb, sb, pb))
    cands.sort(key=lambda c: c[0])
    for d, ha, sa, pa, hb, sb, pb in cands:
        if (ha.name, sa) in done or (hb.name, sb) in done:
            continue
        done.add((ha.name, sa))
        done.add((hb.name, sb))
        za = ha.gz + (265.0 if ha.name != "smithy" else 235.0)
        zb = hb.gz + (265.0 if hb.name != "smithy" else 235.0)
        laundry(pa[0], pa[1], za, pb[0], pb[1], zb)
        if C.stats.get("laundry_lines", 0) >= 12:
            break


def wall_ivy(h):
    """Ivy hanging from under the eaves / window bands on a couple of walls."""
    sides = [s for s in ("left", "right", "back", "front") if C.in_zone(*h.at(s, 0.0, 30.0))]
    R.shuffle(sides)
    for s in sides[:2]:
        half = h.half(s)
        for t in (-half + 70.0, half * 0.3, half - 90.0):
            if R.random() < 0.35:
                continue
            if s == "front" and h.door_t is not None and abs(t - h.door_t) < 150.0:
                continue
            x, y = h.at(s, t, 6.0)
            p = R.choice((V + "Prop_Vine1", V + "Prop_Vine2", V + "Prop_Vine1"))
            top = h.gz + R.choice((595.0, 560.0, 290.0))
            inst(p, x, y, top, h.tyaw(s) + R.choice((0.0, 180.0)), (R.uniform(0.8, 1.1), 1.0, R.uniform(0.8, 1.15)))
            C.stats["ivy"] = C.stats.get("ivy", 0) + 1


FLAGS = [(P + "Banner_2_Cloth", (0.22, 1.0, 0.13), 0.0), (IP + "SM_KG_Rug_Round", 0.16, 90.0),
         (P + "Banner_1_Cloth", (0.22, 1.0, 0.12), 0.0), (IP + "SM_KG_Rug_Oval", 0.14, 90.0)]


def bunting(ax, ay, az, bx, by, bz, sag=45.0):
    f = rope(ax, ay, az, bx, by, bz, sag=sag)
    L = math.hypot(bx - ax, by - ay)
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    n = int(L // 48.0)
    for i in range(1, n):
        x, y, z = f(i / n)
        path, sc, roll = FLAGS[i % len(FLAGS)]
        if roll:
            hh = size(path)[1] * 0.5 * sc
            inst(path, x, y, z - hh, yaw, sc, 0.0, roll)
        else:
            inst(path, x, y, z, yaw, sc)
    C.stats["bunting"] = C.stats.get("bunting", 0) + 1


def lane_bunting():
    """Festive flag lines strung across the lanes on tall poles."""
    for name, w, pts in C.lanes():
        if name in ("church_lane", "farm_track"):
            continue
        for (x, y, dx, dy) in C.lane_samples(name, 1150.0)[1:]:
            if not C.in_zone(x, y):
                continue
            ends = []
            for side in (-1, 1):
                nx, ny = -dy * side, dx * side
                got = None
                for off in (w * 0.5 + 110.0, w * 0.5 + 170.0, w * 0.5 + 240.0):
                    px, py = x + nx * off, y + ny * off
                    if ok(px, py, 30.0, lane_gap=20.0):
                        got = (px, py)
                        break
                ends.append(got)
            if None in ends:
                continue
            (ax, ay), (bx, by) = ends
            za = post(ax, ay, 420.0, 0.55)
            zb = post(bx, by, 420.0, 0.55)
            bunting(ax, ay, za - 15.0, bx, by, zb - 15.0)


def training_yard():
    """The watch's training ground: dummies, a weapon rack, shield targets on posts, a ring of fence."""
    for (x, y) in spots([(5500.0, -1900.0), (5300.0, -1500.0), (-4600.0, -2300.0), (5700.0, 900.0)], 300, 250):
        pts = [(x + dx, y + dy) for dx in (-300, 0, 300) for dy in (-250, 0, 250)]
        if not ok_all(pts, 60.0):
            continue
        clear_grass(x, y, 380.0)
        for k in range(10):
            if k == 3:
                continue   # gate
            a0, a1 = math.radians(k * 36.0), math.radians((k + 1) * 36.0)
            fence_run(x + 330 * math.cos(a0), y + 280 * math.sin(a0), x + 330 * math.cos(a1), y + 280 * math.sin(a1))
        for i, (dx, dy, yw) in enumerate(((-120, -60, 20), (40, -120, 70), (140, 60, 200))):
            put(P + "Dummy", x + dx, y + dy, yaw=yw, sub="Training", claim=45.0)
        put(P + "WeaponStand", x - 60, y + 170, yaw=0.0, sub="Training", claim=80.0)
        for i, (dx, dy) in enumerate(((-230, 120), (200, -150))):
            top = post(x + dx, y + dy, 170.0, 0.7)
            inst(P + "Shield_Wooden", x + dx, y + dy - 12.0, top - 45.0, 0.0, 1.1)
        seat(P + "Bench", x + 20, y + 330, -90.0, mesh_yaw=90.0, height=52.0, claim=140.0)
        inst(P + "Barrel", x + 200, y + 360, gnd(x + 200, y + 360), 0.0, 1.0)
        for i in range(3):
            inst(P + "Sword_Bronze", x + 200 + 12 * i, y + 360, gnd(x, y) + 90.0 + 5 * i, 0.0, 1.0, 10.0 - 10 * i, 0.0)
        chest(x - 200, y + 330, 180.0, "Watch Armoury Chest", "Chest", PIR + "chest_silver_0")
        C.stats["training_yard"] = 1
        poi("Watch training yard", x, y)
        VIG.append((x, y))
        return


def feast_green():
    """Harvest feast being set up: two long tables with benches, bunting overhead, kegs, a cauldron."""
    for (x, y, yaw) in spots([(4700.0, -2150.0, 20.0), (-4200.0, -2200.0, 0.0), (5600.0, -1000.0, 60.0), (-1700.0, 2500.0, 0.0)], 350, 280):
        pts = [(x + dx, y + dy) for dx in (-350, 0, 350) for dy in (-280, 0, 280)]
        if not ok_all(pts, 60.0):
            continue
        clear_grass(x, y, 420.0)
        g = C.Frame(x, y, yaw, 700, 560)
        for row in (-120.0, 120.0):
            tx, ty = g.world(0.0, row)
            put(P + "Table_Large", tx, ty, gnd(tx, ty), yaw=yaw, sub="Feast", claim=150.0)
            for sgn in (-1, 1):
                bx, by = g.world(0.0, row + sgn * 85.0)
                seat(P + "Bench", bx, by, yaw + (90.0 if sgn < 0 else -90.0), mesh_yaw=90.0, height=52.0, claim=60.0)
            z = gnd(tx, ty) + 81.0
            for lx in (-110, -55, 0, 55, 110):
                mx, my = g.world(lx + R.uniform(-10, 10), row + R.uniform(-25, 25))
                inst(R.choice((P + "Mug", P + "Table_Plate", IP + "SM_KG_Bread", IP + "SM_KG_Cheese", KI + "Kitchen_Bowl")),
                     mx, my, z, R.uniform(0, 360), 1.1)
            cx, cy = g.world(-20.0, row)
            inst(P + "CandleStick_Triple", cx, cy, z, yaw, 1.0)
        kx, ky = g.world(-300.0, 0.0)
        inst(P + "Barrel_Holder", kx, ky, gnd(kx, ky), yaw + 90.0, 1.0)
        kx, ky = g.world(310.0, -60.0)
        put(P + "Cauldron", kx, ky, yaw=0.0, sub="Feast", claim=60.0)
        for lx in (-330.0, 330.0):
            for ly in (-270.0, 270.0):
                px, py = g.world(lx, ly)
                post(px, py, 400.0, 0.55)
        for (l0, l1) in (((-330, -270), (330, 270)), ((-330, 270), (330, -270)), ((-330, -270), (-330, 270)),
                         ((330, -270), (330, 270))):
            a, b = g.world(*l0), g.world(*l1)
            bunting(a[0], a[1], gnd(*a) + 385.0, b[0], b[1], gnd(*b) + 385.0, sag=55.0)
        light(x, y, gnd(x, y) + 330.0, 10.0, 900.0)
        breakable(P + "Barrel", *g.world(-300.0, 150.0))
        breakable(P + "Crate_Wooden", *g.world(-300.0, -160.0))
        C.stats["feast"] = 1
        poi("Harvest feast green", x, y)
        VIG.append((x, y))
        return


def mason_yard():
    """Stonemason's yard: cut blocks, loose bricks, tools."""
    for (x, y) in spots([(4600.0, 250.0), (5800.0, 300.0), (4500.0, 1000.0)], 200, 200):
        if not ok(x, y, 200.0, lane_gap=40.0):
            continue
        for k in range(6):
            a = k * 1.05
            bx, by = x + 150 * math.cos(a), y + 120 * math.sin(a)
            inst(V + "Stairs_Exterior_Platform", bx, by, gnd(bx, by) - 2.0, R.uniform(0, 90),
                 (0.35, 0.3, 0.3 + 0.15 * (k % 2)))
        inst(V + "Stairs_Exterior_Platform", x, y, gnd(x, y) - 2.0, 15.0, (0.38, 0.38, 0.62))
        for (p, dx, dy, pitch, roll) in ((P + "Pickaxe_Bronze", 60, -170, 0, -15), (WP + "SM_KG_Shovel", -120, -150, -80, 0)):
            inst(p, x + dx, y + dy, gnd(x + dx, y + dy) + 45.0, R.uniform(0, 360), 1.0, pitch, roll)
        for k in range(10):
            bx, by = x + R.uniform(-220, 220), y + R.uniform(-200, 200)
            inst(V + "Prop_Brick" + str(1 + k % 4), bx, by, gnd(bx, by) + 8.0, R.uniform(0, 360), 1.2)
        inst(P + "Bucket_Metal", x + 170, y - 120, gnd(x + 170, y - 120), 0.0, 1.0)
        C.claim(x, y, 220.0)
        VIG.append((x, y))
        C.stats["mason_yard"] = 1
        poi("Stonemason's yard", x, y)
        return


def fish_racks():
    for (ax, ay, bx, by) in [(1450, 3180, 1850, 3280), (600, 3150, 950, 3350), (-300, 3300, 100, 3150)]:
        if ok(ax, ay, 35.0, lane_gap=20.0) and ok(bx, by, 35.0, lane_gap=20.0):
            drying_rack(ax, ay, bx, by)
            mx, my = (ax + bx) / 2 + 60, (ay + by) / 2 - 90
            if ok(mx, my, 45.0, lane_gap=15.0):
                inst(P + "Rope_3", mx, my, gnd(mx, my), R.uniform(0, 360), 1.0)
                inst(P + "Bucket_Metal", mx + 70, my + 10, gnd(mx + 70, my + 10), 0.0, 1.0)
                inst(WP + "SM_KG_Fish_Mackerel", mx + 70, my + 10, gnd(mx + 70, my + 10) + 32.0, 40.0, 1.0, 0.0, 90.0)
                C.claim(mx, my, 60.0)
            clear_grass((ax + bx) / 2, (ay + by) / 2, 200.0)


def card_table():
    """Four stools around a square table: cards (books), coins, mugs and a candle. A gambling corner."""
    for (x, y) in spots([(-2350.0, 700.0), (-2400.0, 1150.0), (5600.0, -300.0), (-2100.0, 150.0)], 140, 140, max_d=450):
        if not ok(x, y, 160.0):
            continue
        z = gnd(x, y)
        put(KI + "Kitchen_Square_Table", x, y, z, yaw=15.0, sub="CardTable", claim=80.0)
        for k in range(4):
            a = 15.0 + k * 90.0
            sx, sy = fwd(a, 85.0)
            seat(P + "Stool", x + sx, y + sy, a + 180.0, height=57.0, claim=35.0)
        tz = z + 67.0
        inst(P + "Coin_Pile", x + 10, y - 5, tz, 20.0, 1.2)
        inst(P + "Coin_Pile_2", x - 20, y + 18, tz, 70.0, 1.0)
        inst(P + "Mug", x + 28, y + 25, tz, 0.0, 1.1)
        inst(P + "Mug", x - 25, y - 28, tz, 0.0, 1.1)
        inst(P + "Book_5", x, y + 5, tz, 35.0, 0.6)
        inst(P + "CandleStick", x - 5, y - 25, tz, 0.0, 1.0)
        inst(P + "Candle_2", x - 8, y - 25, tz + 8.0, 0.0, 1.0)
        light(x, y, tz + 60.0, 5.0, 400.0, (255, 180, 110))
        clear_grass(x, y, 200.0)
        C.stats["card_table"] = 1
        poi("Card table", x, y)
        return


def broken_cart():
    """A cart that lost a wheel: tilted, apples spilled, a crate burst open. Loot in the crate."""
    for (x, y, yaw) in spots([(5700.0, -1100.0, 30.0), (5500.0, 800.0, -40.0), (4300.0, -2100.0, 10.0),
                        (-1300.0, 3100.0, 80.0)], 230, 230):
        if not ok(x, y, 260.0, lane_gap=40.0):
            continue
        put(V + "Prop_Wagon", x, y, gnd(x, y) - 12.0, yaw=yaw, roll=9.0, pitch=-4.0, sub="BrokenCart", claim=240.0)
        c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        W = lambda lx, ly: (x + lx * c - ly * s_, y + lx * s_ + ly * c)  # noqa: E731
        for (lx, ly, p, sc, pitch, roll) in [(150, 60, P + "FarmCrate_Apple", 1.0, 0, 60), (170, -40, P + "FarmCrate_Empty", 1.0, 70, 0),
                                             (-160, 20, P + "Barrel_Apples", 1.0, 90, 0), (140, 140, PIR + "crates_1", 0.7, 0, 20)]:
            px, py = W(lx, ly)
            inst(p, px, py, gnd(px, py) + (30.0 if pitch or roll else 0.0), yaw + R.uniform(-30, 30), sc, pitch, roll)
        px, py = W(-40, 190)
        chest(px, py, yaw + 20.0, "Spilled Cargo", "Crate", PIR + "crates_0")
        clear_grass(x, y, 300.0)
        C.stats["broken_cart"] = 1
        poi("Broken cart (spilled cargo)", x, y)
        return


def shrine():
    """Wayside shrine at the west_lane/back_alley junction: a stone, candles and flowers."""
    for (x, y) in [(-2280.0, -250.0), (-2300.0, -1250.0), (-1250.0, 1150.0)]:
        if not ok(x, y, 110.0, lane_gap=20.0):
            continue
        z = gnd(x, y)
        put(N + "Rock_Medium_2", x, y, z - 40.0, yaw=R.uniform(0, 360), s=(0.35, 0.35, 0.45), sub="Shrine", claim=100.0)
        put(P + "CandleStick_Stand", x + 70, y - 50, z, sub="Shrine", collide=False)
        for i in range(4):
            a = i * 1.3 + 0.5
            inst(P + "Candle_1" if i % 2 else P + "Candle_2", x + 55 * math.cos(a), y + 55 * math.sin(a) - 30,
                 gnd(x, y) + 2.0, 0.0, 1.2)
        for i in range(7):
            a = i * 0.9
            inst(FLOWERS[i % len(FLOWERS)], x + 95 * math.cos(a), y + 95 * math.sin(a), z - 2.0, R.uniform(0, 360), 0.6)
        inst(P + "Vase_4", x - 60, y - 60, z, 0.0, 0.8)
        inst(N + "Flower_3_Single", x - 60, y - 60, z + 35.0, 0.0, 0.25)
        light(x + 60, y - 50, z + 150.0, 4.0, 350.0, (255, 190, 120))
        C.stats["shrine"] = 1
        poi("Wayside shrine", x, y)
        return


def sea_bench():
    """A bench on the northern rise looking over the sea, a lantern and a flowering bush."""
    n = 0
    for (x, y) in [(-3600.0, 3450.0), (-2000.0, 3450.0), (5600.0, 1900.0), (-4200.0, 2950.0)]:
        if n >= 2 or not ok(x, y, 170.0):
            continue
        seat(P + "Bench", x, y, 90.0 if y > 3000 else 45.0, mesh_yaw=90.0, height=52.0, claim=160.0)
        inst(N + "Bush_Common_Flowers", x + 220, y + 30, gnd(x + 220, y + 30) - 10.0, R.uniform(0, 360), 0.7)
        inst(N + "Flower_3_Group", x - 210, y + 40, gnd(x - 210, y + 40) - 5.0, R.uniform(0, 360), 0.55)
        top = post(x - 170, y - 30, 230.0, 0.6)
        put(P + "Lantern_Wall", x - 170, y - 30, top - 110.0, yaw=0.0, collide=False, sub="SeaBench")
        if n == 0:
            light(x - 170, y + 60, top - 20.0, 6.0, 500.0)
        clear_grass(x, y, 220.0)
        n += 1
    C.stats["sea_benches"] = n


def kids_corner():
    """Children's play corner by house 15: a dummy 'knight', a crate fort, wooden sword, cannonball 'balls'."""
    for (x, y) in spots([(-2300.0, 1900.0), (-2500.0, 800.0), (-800.0, 1750.0)], 160, 160, max_d=600):
        if not ok(x, y, 180.0):
            continue
        z = gnd(x, y)
        put(P + "Dummy", x, y, z, yaw=200.0, sub="KidsCorner", claim=50.0)
        put(P + "Shield_Wooden", x + 20, y - 25, z + 95.0, yaw=200.0, roll=0.0, collide=False, sub="KidsCorner")
        for (dx, dy, zz, yw) in ((150, 40, 0, 10), (150, -60, 0, -5), (150, -10, 70, 30)):
            inst(PIR + "crates_" + str((dx + dy) % 2), x + dx, y + dy, gnd(x + dx, y + dy) + zz, yw, 0.7)
        inst(P + "Sword_Bronze", x - 70, y + 60, z + 3.0, 40.0, 0.7, 0.0, 90.0)
        for i in range(3):
            inst(PIR + "Cannon_Ball_0", x - 120 + i * 40, y - 110 + i * 25, gnd(x, y) - 2.0, 0.0, 0.25)
        breakable(P + "Crate_Wooden", x + 70, y + 160)
        clear_grass(x, y, 220.0)
        C.stats["kids_corner"] = 1
        poi("Kids' play corner", x, y)
        return


def captain_flag():
    """House 13 belongs to a retired captain: a flagpole with a pirate pennant and a sea chest."""
    hs = {h.name: h for h in HOUSES}
    h = hs.get("house13")
    if not h:
        return
    for s, t, out in (("back", 0.0, 140.0), ("right", 0.0, 140.0), ("left", 150.0, 160.0)):
        x, y = h.at(s, t, out)
        if ok(x, y, 60.0, lane_gap=20.0):
            put(PIR + "FlagLow_0", x, y, gnd(x, y) - 5.0, s=(1.0, 1.0, 0.7), sub="Captain", claim=50.0)
            C.mover(PIR + "FlagLow_1", x, y, gnd(x, y) + 400.0, yaw=h.nyaw(s), sway=6.0, sway_hz=0.4)
            cx, cy = h.at(s, t + 120.0, 50.0)
            if ok(cx, cy, 50.0, lane_gap=10.0):
                inst(PIR + "Cannon_Ball_0", cx, cy, gnd(cx, cy) - 3.0, 0.0, 0.8)
                C.claim(cx, cy, 50.0)
            return


def signposts():
    spots = [(-2250.0, -330.0, [0.0, 90.0, 200.0]), (-300.0, 1850.0, [-80.0, 180.0, 90.0]),
             (650.0, 3250.0, [90.0, 20.0]), (3700.0, 150.0, [200.0, 60.0, 95.0]), (-5000.0, 250.0, [150.0, -10.0]),
             (1000.0, -1650.0, [-60.0, 110.0])]
    for i, (x, y, tg) in enumerate(spots):
        for dx, dy in ((0, 0), (80, 0), (-80, 0), (0, 80), (0, -80), (120, 120), (-120, -120)):
            if ok(x + dx, y + dy, 40.0, lane_gap=25.0):
                signpost(x + dx, y + dy, tg, lantern=(i == 0))
                break


def meadow_trees(n_max=26):
    """Shade trees in the open ground between the houses: volume and silhouettes (with a skirt of ferns)."""
    pts = [(x + R.uniform(-200, 200), y + R.uniform(-200, 200)) for x in range(-5000, 6100, 650)
           for y in range(-2600, 3600, 650)]
    R.shuffle(pts)
    n = 0
    for (x, y) in pts:
        if n >= n_max or not C.in_zone(x, y):
            continue
        if C.building_hit(x, y, 420.0) or C.lane_distance(x, y) < 330.0 or not ok(x, y, 260.0):
            continue
        if C.slope(x, y, 150.0) > 120.0:
            continue
        p = R.choice((N + "CommonTree_1", N + "CommonTree_2", N + "CommonTree_5", N + "CommonTree_2"))
        inst(p, x, y, gnd(x, y) - 22.0, R.uniform(0, 360), R.uniform(0.7, 0.95))
        for j in range(3):
            a = R.uniform(0, 6.28)
            fx, fy = x + R.uniform(110, 190) * math.cos(a), y + R.uniform(110, 190) * math.sin(a)
            inst(R.choice((N + "Fern_1", N + "Plant_1", N + "Mushroom_Common", N + "Flower_3_Single")), fx, fy,
                 gnd(fx, fy) - 3.0, R.uniform(0, 360), R.uniform(0.35, 0.5))
        C.claim(x, y, 200.0)
        VIG.append((x, y))
        n += 1
    C.stats["meadow_trees"] = n


def lane_benches():
    """A bench every ~20 m of lane, set back from the edge and facing the lane, with a pot or bush beside it."""
    n = 0
    for name, w, pts in C.lanes():
        for i, (x, y, dx, dy) in enumerate(C.lane_samples(name, 1900.0)):
            if i == 0:
                continue
            for side in (1, -1):
                nx, ny = -dy * side, dx * side
                off = w * 0.5 + 165.0
                bx, by = x + nx * off, y + ny * off
                if not ok(bx, by, 115.0, lane_gap=20.0) or C.slope(bx, by, 140.0) > 70.0:
                    continue
                face = math.degrees(math.atan2(-ny, -nx))
                seat(P + "Bench", bx, by, face, mesh_yaw=90.0, height=52.0, claim=150.0)
                ex, ey = bx + dx * 190.0, by + dy * 190.0
                potted(ex, ey, i, True)
                ex, ey = bx - dx * 190.0 + nx * 20, by - dy * 190.0 + ny * 20
                inst(N + "Bush_Common_Flowers", ex, ey, gnd(ex, ey) - 15.0, R.uniform(0, 360), 0.5)
                C.claim(ex, ey, 50.0)
                VIG.append((bx, by))
                n += 1
                break
    C.stats["lane_benches"] = n


SIGNS = {"house9": (IP + "SM_KG_Cheese", 3.0), "house10": (P + "Vase_4", 1.2), "house11": (IP + "SM_KG_HerbBundle", 3.0),
         "house1": (WP + "SM_KG_Fish_Salmon", 1.6), "house6": (P + "Axe_Bronze", 1.3), "house3": (P + "Barrel", 0.5),
         "house12": (P + "Carrot", 2.5), "house13": (P + "Shield_Wooden", 1.0)}


def shop_signs():
    """Swinging trade signs: a bracket beam out of the front wall and the trade's emblem hanging from it."""
    for h in HOUSES:
        if h.name not in SIGNS or h.door_t is None:
            continue
        t = h.door_t - 150.0
        bx, by = h.at("front", t, 0.0)
        if not C.in_zone(bx, by):
            continue
        z = h.gz + 262.0
        yaw = h.nyaw("front")
        inst(IP + "SM_KG_RailPost", bx, by, z, yaw, (0.7, 0.7, 1.05), -90.0, 0.0)     # beam out of the wall
        ex, ey = h.at("front", t, 95.0)
        rope(ex, ey, z, ex, ey, z - 25.0, sag=0.0)
        path, sc = SIGNS[h.name]
        hz = size(path)[2] * sc * 0.5
        C.mover(path, ex, ey, z - 30.0 - hz, yaw=yaw + 90.0, scale=sc, sway=5.0, sway_hz=0.33)
        C.stats["shop_signs"] = C.stats.get("shop_signs", 0) + 1


def extra_chests():
    """Loot at the points of interest that do not own one yet (fisher's shed, herbalist, inn yard, captain)."""
    want = [("house1", "Fisherman's Locker", "Fishing", None), ("house11", "Herbalist's Chest", "Chest", None),
            ("inn", "Inn Cellar Crate", "Crate", PIR + "crates_1"), ("house13", "Captain's Sea Chest", "Chest", PIR + "chest_silver_0"),
            ("house7", "Woodsman's Chest", "Chest", None), ("house12", "Farmer's Seed Box", "Crate", None)]
    hs = {h.name: h for h in HOUSES}
    for name, label, table, path in want:
        h = hs.get(name)
        if not h:
            continue
        done = False
        for s in ("left", "right", "back", "front"):
            for t in (-h.half(s) + 90.0, h.half(s) - 90.0, 0.0):
                x, y = h.at(s, t, 55.0)
                if ok(x, y, 45.0, lane_gap=10.0):
                    chest(x, y, h.tyaw(s), label, table, path)
                    done = True
                    break
            if done:
                break


def fill_small(step=230.0, clear_r=190.0):
    """Second, finer pass: little clumps (flowers, tufts, stones, mushrooms, a pot) in the remaining bare spots."""
    pts = [(x + R.uniform(-90, 90), y + R.uniform(-90, 90))
           for x in range(-5200, 6200, int(step)) for y in range(-4000, 4000, int(step))]
    R.shuffle(pts)
    n = 0
    for (x, y) in pts:
        if not C.in_zone(x, y) or any(math.hypot(x - vx, y - vy) < clear_r for vx, vy in VIG[-4000:]):
            continue
        if not ok(x, y, 60.0, lane_gap=25.0):
            continue
        k = R.random()
        if k < 0.4:
            for j in range(R.randint(3, 5)):
                fx, fy = x + R.uniform(-70, 70), y + R.uniform(-70, 70)
                p = R.choice(FLOWERS + [N + "Flower_3_Single", N + "Clover_2"])
                inst(p, fx, fy, gnd(fx, fy) - 2.0, R.uniform(0, 360), (0.4 if "Single" in p or "Clover_2" in p else 0.75))
        elif k < 0.6:
            inst(N + "Bush_Common" + R.choice(("", "_Flowers")), x, y, gnd(x, y) - 18.0, R.uniform(0, 360), R.uniform(0.45, 0.65))
        elif k < 0.75:
            for j in range(3):
                fx, fy = x + R.uniform(-60, 60), y + R.uniform(-60, 60)
                inst(N + "Pebble_Round_" + str(1 + R.randrange(5)), fx, fy, gnd(fx, fy) - 2.0, R.uniform(0, 360), R.uniform(1.2, 2.2))
            inst(N + "Mushroom_Common", x, y, gnd(x, y) - 2.0, R.uniform(0, 360), R.uniform(0.6, 0.9))
        elif k < 0.88:
            inst(N + "Fern_1", x, y, gnd(x, y) - 3.0, R.uniform(0, 360), R.uniform(0.35, 0.5))
            inst(N + "Plant_1", x + 50, y - 30, gnd(x + 50, y - 30) - 3.0, R.uniform(0, 360), 0.4)
        else:
            if R.random() < 0.5:
                breakable(R.choice((P + "Vase_2", P + "Vase_4", P + "Crate_Wooden")), x, y)
            else:
                inst(P + "Bucket_Wooden_1", x, y, gnd(x, y), R.uniform(0, 360), 1.0)
                inst(N + "Grass_Common_Short", x + 40, y, gnd(x + 40, y) - 3.0, R.uniform(0, 360), 0.6)
        C.claim(x, y, 60.0)
        VIG.append((x, y))
        n += 1
    C.stats["fill_small"] = n


def shed(x, y, yaw, w, d, kind):
    """Open lean-to shed: four posts under a small round-tile roof, filled according to `kind`."""
    g = C.Frame(x, y, yaw, w, d)
    corners = [g.world(sx * (w * 0.5 - 12.0), sy * (d * 0.5 - 12.0)) for sx in (-1, 1) for sy in (-1, 1)]
    roof_z = max(gnd(cx, cy) for cx, cy in corners) + 225.0
    for cx, cy in corners:
        post(cx, cy, roof_z - gnd(cx, cy) + 8.0, 0.75)
    put(V + "Roof_RoundTiles_4x4", x, y, roof_z, yaw=yaw, s=((w + 90.0) / 551.0, (d + 90.0) / 584.0, 0.42), sub="Sheds",
        collide=True, cull=14000.0)
    clear_grass(x, y, max(w, d) * 0.6)
    L = g.world
    if kind == "wood":
        for row, ly in enumerate((-d * 0.25, d * 0.2)):
            n = int((w - 60.0) // 56.0)
            for layer in range(3):
                for i in range(n - (layer % 2)):
                    px, py = L(-w * 0.5 + 45.0 + i * 56.0 + (layer % 2) * 28.0, ly)
                    inst(FIREWOOD, px, py, gnd(px, py) - 2.0 + layer * 40.0, yaw + R.choice((0, 180)) + R.uniform(-5, 5), 1.0)
        px, py = L(w * 0.5 + 60.0, -d * 0.5 + 20.0)
        inst(N + "DeadTree_1", px, py, gnd(px, py) - 25.0, R.uniform(0, 360), (0.12, 0.12, 0.06))
        inst(P + "Axe_Bronze", px, py, gnd(px, py) + 68.0, R.uniform(0, 360), 1.0, 0.0, 20.0)
    elif kind == "work":
        px, py = L(0.0, d * 0.5 - 70.0)
        put(P + "Workbench", px, py, yaw=yaw + 180.0, sub="Sheds", claim=0.0)
        for i, (p, lx) in enumerate(((P + "Axe_Bronze", -60), (P + "Pickaxe_Bronze", 0), (WP + "SM_KG_Shovel", 55))):
            qx, qy = L(lx, d * 0.5 - 70.0)
            inst(p, qx, qy, gnd(qx, qy) + 90.0, yaw + R.uniform(-15, 15), 0.9, 0.0, 90.0)
        px, py = L(-w * 0.25, -d * 0.1)
        seat(P + "Stool", px, py, yaw + 90.0, claim=0.0)
        px, py = L(w * 0.5 - 50.0, -d * 0.5 + 50.0)
        inst(PIR + "Planks_2", px, py, gnd(px, py), yaw + 5.0, 0.9)
        inst(PIR + "Planks_0", px, py, gnd(px, py) + 10.0, yaw - 4.0, 0.9)
    elif kind == "store":
        for i, (lx, ly) in enumerate(((-w * 0.25, d * 0.2), (w * 0.2, d * 0.22), (-w * 0.2, -d * 0.2))):
            px, py = L(lx, ly)
            inst((P + "Crate_Wooden", P + "Barrel", P + "Crate_Wooden")[i], px, py, gnd(px, py) + 3.0, yaw + R.uniform(-10, 10), 0.95)
        px, py = L(-w * 0.25, d * 0.2)
        inst(PIR + "crates_1", px, py, gnd(px, py) + 97.0, yaw + 20.0, 0.6)
        px, py = L(w * 0.22, -d * 0.2)
        inst(P + "Bag", px, py, gnd(px, py), R.uniform(0, 360), 0.85)
        px, py = L(0.0, -d * 0.5 - 60.0)
        if ok(px, py, 45.0, lane_gap=10.0):
            breakable(P + "Crate_Wooden", px, py)
    elif kind == "smoke":
        a, b = L(-w * 0.5 + 15.0, 0.0), L(w * 0.5 - 15.0, 0.0)
        f = rope(a[0], a[1], roof_z - 15.0, b[0], b[1], roof_z - 15.0, sag=3.0)
        fishes = [WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Salmon", WP + "SM_KG_Fish_Mackerel"]
        for i in range(1, 8):
            fx, fy, fz = f(i / 8.0)
            p = fishes[i % 3]
            inst(p, fx, fy, fz - size(p)[0] * 0.5 - 3.0, yaw + 90.0, 1.0, -88.0, 0.0)
        px, py = L(0.0, d * 0.25)
        inst(P + "Barrel", px, py, gnd(px, py), 0.0, 1.0)
        inst(P + "Rope_2", px + 60, py - 60, gnd(px, py) + 1.0, 0.0, 1.0)
    for cx, cy in corners:
        C.claim(cx, cy, 40.0)
    C.claim(x, y, max(w, d) * 0.5)
    VIG.append((x, y))
    C.stats["sheds"] = C.stats.get("sheds", 0) + 1
    poi("Shed (" + kind + ")", x, y)


SHED_KIND = {"fisher": "smoke", "woodsman": "wood", "carpenter": "work", "farmer": "store", "grocer": "store",
             "cooper": "store", "family": "wood", "smithfam": "wood", "captain": "store", "herbalist": "work",
             "flowers": "wood", "potter": "work", "inn": "store", "baker": "wood"}


def house_sheds():
    """Each house tries to put a small shed in its yard, 1.5-3 m off a free side wall, parallel to it."""
    for h in HOUSES:
        if h.name == "smithy":
            continue
        kind = SHED_KIND.get(PERSONA.get(h.name, ""), "wood")
        for s in ("left", "right", "back"):
            done = False
            for out in (230.0, 330.0):
                for t in (0.0, -h.half(s) * 0.4, h.half(s) * 0.4):
                    x, y = h.at(s, t, out)
                    g = C.Frame(x, y, h.tyaw(s), 260, 200)
                    pts = [g.world(lx, ly) for lx in (-140, 0, 140) for ly in (-110, 0, 110)]
                    if ok_all(pts, 45.0, lane_gap=40.0) and C.slope(x, y, 140.0) < 90.0:
                        shed(x, y, h.tyaw(s), 260.0, 200.0, kind)
                        done = True
                        break
                if done:
                    break
            if done:
                break


def free_sheds():
    for (x, y, yaw, kind) in spots([(-4700.0, 2200.0, 10.0, "work"), (4400.0, -1900.0, 30.0, "store"),
                                    (-2500.0, 3200.0, 0.0, "smoke"), (5600.0, 1200.0, 100.0, "wood"),
                                    (-4900.0, -1900.0, 80.0, "wood"), (1500.0, 2600.0, 0.0, "smoke")],
                                   170, 140, r=50.0, max_d=900.0):
        shed(x, y, yaw, 280.0, 220.0, kind)


def neighbourhood_well():
    """A second, smaller well for the east end (same build as the square's), with buckets and a trough."""
    for (x, y) in spots([(4300.0, 1100.0), (-2600.0, 1000.0), (5700.0, -600.0)], 170, 170, r=60.0, max_d=900.0):
        gz = gnd(x, y)
        put(V + "Stairs_Exterior_Platform", x, y, gz - 3.0, s=(0.75, 0.75, 0.8), sub="Well", collide=True)
        for sx in (-1, 1):
            put(V + "Corner_Exterior_Wood", x + sx * 80.0, y, gz, s=(1.0, 1.0, 0.75), sub="Well", collide=True)
        put(V + "Corner_Exterior_Wood", x - 95.0, y, gz + 225.0, pitch=-90.0, s=(1.0, 1.0, 0.63), sub="Well", collide=False)
        put(V + "Roof_Wooden_2x1", x, y + 70.0, gz + 225.0, sub="Well", collide=False)
        put(P + "Bucket_Wooden_1", x + 30.0, y - 70.0, gz + 80.0, sub="Well", collide=False)
        for i, (dx, dy) in enumerate(((150, 60), (170, -40), (-160, 80))):
            inst((P + "Bucket_Wooden_1", P + "Bucket_Metal", P + "Barrel")[i], x + dx, y + dy, gnd(x + dx, y + dy), R.uniform(0, 360), 1.0)
        put(P + "FarmCrate_Empty", x - 40, y - 190, s=(2.3, 1.4, 2.0), yaw=5.0, sub="Well", collide=True)   # trough
        for i in range(8):
            a = i * 0.8
            fx, fy = x + 210 * math.cos(a), y + 210 * math.sin(a)
            if ok(fx, fy, 30.0, lane_gap=10.0):
                inst(FLOWERS[i % len(FLOWERS)], fx, fy, gnd(fx, fy) - 2.0, R.uniform(0, 360), 0.8)
        clear_grass(x, y, 250.0)
        C.claim(x, y, 180.0)
        VIG.append((x, y))
        C.stats["well"] = 1
        poi("Neighbourhood well", x, y)
        return


def yard_laundry():
    """Washing lines from a house wall to a pole in its yard."""
    n = 0
    for h in HOUSES:
        if n >= 9 or h.name == "smithy":
            continue
        for s in ("left", "right", "back"):
            a = h.at(s, R.uniform(-60, 60), 3.0)
            b = h.at(s, R.uniform(-150, 150), 520.0)
            mids = [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t) for t in (0.3, 0.6, 0.9)]
            if not ok(b[0], b[1], 35.0, lane_gap=20.0) or not all(C.in_zone(*m) for m in mids):
                continue
            if any(C.lane_distance(*m) < 40.0 for m in mids):
                continue
            zb = post(b[0], b[1], 250.0, 0.6)
            laundry(a[0], a[1], h.gz + 250.0, b[0], b[1], zb - 12.0)
            for m in mids:
                C.claim(m[0], m[1], 40.0)
            n += 1
            break


def pergola():
    """A vine-covered pergola over a bench and potted plants (a quiet corner for two)."""
    for (x, y, yaw) in spots([(-1400.0, 2900.0, 80.0), (3900.0, 1500.0, 20.0), (-3400.0, -900.0, 0.0),
                              (5200.0, -2200.0, 40.0)], 190, 120, r=50.0, max_d=800.0):
        g = C.Frame(x, y, yaw, 360, 200)
        cs = [g.world(sx * 170.0, sy * 90.0) for sx in (-1, 1) for sy in (-1, 1)]
        top = max(gnd(cx, cy) for cx, cy in cs) + 240.0
        for cx, cy in cs:
            post(cx, cy, top - gnd(cx, cy), 0.65)
        for sy in (-1, 1):
            a, b = g.world(-190.0, sy * 90.0), g.world(190.0, sy * 90.0)
            inst(IP + "SM_KG_RailPost", a[0], a[1], top, yaw, (0.9, 0.9, 380.0 / 108.0), -90.0, 0.0)
        for lx in (-120.0, 0.0, 120.0):
            a = g.world(lx, -110.0)
            inst(IP + "SM_KG_RailPost", a[0], a[1], top + 8.0, yaw + 90.0, (0.7, 0.7, 220.0 / 108.0), -90.0, 0.0)
        vx, vy = g.world(0.0, 0.0)
        inst(V + "Prop_Vine6", vx, vy, top + 60.0, yaw, (0.95, 1.3, 0.55))
        for sx in (-1, 1):
            px, py = g.world(sx * 170.0, -90.0)
            inst(V + "Prop_Vine1", px, py, top, yaw + 90.0 * sx, (0.6, 1.0, 1.0))
        bx, by = g.world(0.0, 40.0)
        seat(P + "Bench", bx, by, yaw - 90.0, mesh_yaw=90.0, height=52.0, claim=0.0)
        for sx in (-1, 1):
            px, py = g.world(sx * 210.0, 40.0)
            potted(px, py, 3 + sx, True)
        clear_grass(x, y, 220.0)
        C.claim(x, y, 190.0)
        VIG.append((x, y))
        C.stats["pergolas"] = C.stats.get("pergolas", 0) + 1
        poi("Vine pergola", x, y)
        if C.stats["pergolas"] >= 2:
            return


# ============================================================================================ lanes
def lane_edges():
    """Handcrafted-looking runs along the lane edges: hedges, short fences, flower strips, rocks, clutter."""
    patterns = ["hedge", "fence", "flowers", "gap", "rocks", "hedge", "clutter", "gap", "fence", "flowers"]
    for name, w, pts in C.lanes():
        samples = C.lane_samples(name, 170.0)
        for side in (-1, 1):
            run, k = None, R.randrange(len(patterns))
            left = 0
            for (x, y, dx, dy) in samples:
                if left <= 0:
                    run = patterns[k % len(patterns)]
                    k += 1
                    left = R.randint(3, 6)
                left -= 1
                nx, ny = -dy * side, dx * side
                off = w * 0.5 + 118.0 + R.uniform(0, 30)
                px, py = x + nx * off, y + ny * off
                yaw = math.degrees(math.atan2(dy, dx))
                if run == "gap":
                    continue
                r = 55.0 if run in ("hedge", "fence") else 45.0
                if not ok(px, py, r, lane_gap=30.0):
                    continue
                if C.slope(px, py, 80.0) > 80.0:
                    continue
                z = gnd(px, py)
                if run == "hedge":
                    inst(N + ("Bush_Common" if R.random() < 0.7 else "Bush_Common_Flowers"), px, py, z - 18.0,
                         R.uniform(0, 360), (R.uniform(0.75, 0.95), R.uniform(0.75, 0.95), R.uniform(0.55, 0.7)))
                elif run == "fence":
                    inst(V + "Prop_WoodenFence_Single", px, py, z - 2.0, yaw + R.uniform(-3, 3), (0.84, 1.0, 1.0))
                elif run == "flowers":
                    for j in range(3):
                        fx, fy = px + R.uniform(-60, 60), py + R.uniform(-45, 45)
                        p = R.choice(FLOWERS + [N + "Flower_3_Single", N + "Flower_4_Single"])
                        sc = 0.4 if "Single" in p else 0.8
                        inst(p, fx, fy, gnd(fx, fy) - 2.0, R.uniform(0, 360), sc * R.uniform(0.8, 1.2))
                elif run == "rocks":
                    inst(N + "Pebble_Round_" + str(1 + R.randrange(5)), px, py, z - 2.0, R.uniform(0, 360), R.uniform(1.5, 2.6))
                    inst(N + "Grass_Common_Short", px + 40, py - 20, z - 3.0, R.uniform(0, 360), 0.7)
                elif run == "clutter":
                    c = R.random()
                    if c < 0.3:
                        breakable(R.choice((P + "Crate_Wooden", P + "Barrel", P + "Vase_2")), px, py)
                    elif c < 0.6:
                        inst(P + "Barrel", px, py, z, R.uniform(0, 360), 1.0)
                        inst(P + "Bucket_Wooden_1", px + 55 * nx, py + 55 * ny, gnd(px + 55 * nx, py + 55 * ny), 0.0, 1.0)
                    else:
                        inst(P + "Crate_Wooden", px, py, z + 4.0, yaw + R.uniform(-15, 15), 0.9)
                        inst(P + "Bag", px + 50 * dx, py + 50 * dy, gnd(px + 50 * dx, py + 50 * dy), R.uniform(0, 360), 0.8)
                C.claim(px, py, r)


# ============================================================================================ gap filler
def micro(x, y, kind):
    z = gnd(x, y)
    if kind == "bushes":
        inst(N + "Bush_Common", x, y, z - 20.0, R.uniform(0, 360), R.uniform(0.6, 0.9))
        for j in range(3):
            a = R.uniform(0, 6.28)
            fx, fy = x + 110 * math.cos(a), y + 110 * math.sin(a)
            inst(R.choice((N + "Fern_1", N + "Plant_1", N + "Flower_3_Single")), fx, fy, gnd(fx, fy) - 3.0,
                 R.uniform(0, 360), R.uniform(0.35, 0.5))
    elif kind == "flowers":
        for j in range(6):
            fx, fy = x + R.uniform(-120, 120), y + R.uniform(-120, 120)
            p = R.choice(FLOWERS + [N + "Flower_3_Group", N + "Flower_4_Group"])
            inst(p, fx, fy, gnd(fx, fy) - 3.0, R.uniform(0, 360), (0.4 if "Group" in p else 0.8) * R.uniform(0.8, 1.2))
    elif kind == "stones":
        inst(N + "Rock_Medium_" + str(1 + R.randrange(3)), x, y, z - 40.0, R.uniform(0, 360),
             (R.uniform(0.3, 0.45), R.uniform(0.3, 0.45), R.uniform(0.25, 0.4)))
        for j in range(4):
            fx, fy = x + R.uniform(-140, 140), y + R.uniform(-140, 140)
            inst(N + "Pebble_Round_" + str(1 + R.randrange(5)), fx, fy, gnd(fx, fy) - 2.0, R.uniform(0, 360), R.uniform(1, 2))
        inst(N + "Mushroom_Common", x + 90, y - 60, gnd(x + 90, y - 60) - 2.0, 0.0, 0.8)
    elif kind == "stack":
        inst(P + "Crate_Wooden", x, y, z + 4.0, R.uniform(0, 360), 1.0)
        inst(PIR + "crates_0", x + 90, y + 10, gnd(x + 90, y + 10), R.uniform(0, 360), 0.8)
        inst(P + "Bag", x - 70, y + 50, gnd(x - 70, y + 50), R.uniform(0, 360), 0.85)
        breakable(P + "Crate_Wooden", x + 20, y - 110)
    elif kind == "barrels":
        for j, (dx, dy) in enumerate(((0, 0), (80, 20), (35, 75))):
            inst(R.choice((P + "Barrel", PIR + "Barrel_0", PIR + "Barrel_3", PIR + "Barrel_9")), x + dx, y + dy,
                 gnd(x + dx, y + dy), R.uniform(0, 360), 0.8 if j else 1.0)
        breakable(P + "Barrel", x - 90, y - 40)
    elif kind == "pots":
        for j in range(3):
            fx, fy = x + R.uniform(-70, 70), y + R.uniform(-70, 70)
            breakable(R.choice((P + "Vase_2", P + "Vase_4")), fx, fy)
        inst(P + "Vase_Rubble_Medium", x + 60, y + 90, gnd(x + 60, y + 90), R.uniform(0, 360), 1.0)
    elif kind == "tree":
        inst(R.choice((N + "CommonTree_1", N + "CommonTree_2", N + "CommonTree_5")), x, y, z - 20.0, R.uniform(0, 360),
             R.uniform(0.55, 0.8))
        inst(N + "Fern_1", x + 120, y + 50, gnd(x + 120, y + 50) - 3.0, R.uniform(0, 360), 0.4)
    elif kind == "log_seat":
        seat(P + "Bench", x, y, R.uniform(0, 360), mesh_yaw=90.0, height=52.0, claim=140.0)
        inst(N + "Flower_3_Group", x + 170, y, gnd(x + 170, y) - 5.0, 0.0, 0.4)
    elif kind == "firewood":
        for i in range(4):
            inst(FIREWOOD, x + (i - 1.5) * 56.0, y, z - 2.0, R.choice((0, 180)), 1.0)
        for i in range(3):
            inst(FIREWOOD, x + (i - 1.0) * 56.0, y, z + 38.0, R.choice((0, 180)), 1.0)
        inst(P + "Axe_Bronze", x + 150, y + 20, z + 38.0, 0.0, 1.0, 0.0, 15.0)
    elif kind == "coop":
        put(P + "Cage_Small", x, y, yaw=R.uniform(0, 360), s=1.3, sub="Coops", claim=70.0)
        inst(P + "FarmCrate_Empty", x + 90, y + 20, gnd(x + 90, y + 20), R.uniform(0, 360), 1.0)
        inst(P + "Bucket_Wooden_1", x - 70, y + 60, gnd(x - 70, y + 60), 0.0, 1.0)
        for j in range(5):
            fx, fy = x + R.uniform(-150, 150), y + R.uniform(-150, 150)
            inst(N + "Grass_Wispy_Short", fx, fy, gnd(fx, fy) - 3.0, R.uniform(0, 360), 0.45)
    elif kind == "picnic":
        inst(IP + "SM_KG_Rug_Rect", x, y, gnd(x, y) + 1.5, R.uniform(0, 360), 0.7)
        inst(P + "Bag", x + 60, y + 30, gnd(x + 60, y + 30), R.uniform(0, 360), 0.5)
        inst(IP + "SM_KG_Bread", x - 20, y + 10, gnd(x - 20, y + 10) + 2.0, R.uniform(0, 360), 1.2)
        inst(IP + "SM_KG_Cheese", x + 10, y - 30, gnd(x + 10, y - 30) + 2.0, 0.0, 1.2)
        inst(KI + "Kitchen_Bottle", x - 50, y - 25, gnd(x - 50, y - 25) + 2.0, 0.0, 1.0)
        inst(P + "Mug", x + 30, y - 45, gnd(x + 30, y - 45) + 2.0, 0.0, 1.2)
    elif kind == "cart":
        put(P + "Stall_Cart_Empty", x, y, yaw=R.uniform(0, 360), sub="Carts", claim=170.0)
        breakable(P + "Crate_Wooden", x + 150, y + 120)
    elif kind == "woodpile":
        for i in range(5):
            inst(FIREWOOD, x + (i - 2) * 56.0, y + 90, gnd(x, y + 90) - 2.0, R.choice((0, 180)), 1.0)
        for i in range(4):
            inst(FIREWOOD, x + (i - 1.5) * 56.0, y + 90, gnd(x, y + 90) + 38.0, R.choice((0, 180)), 1.0)
        inst(N + "DeadTree_1", x, y - 30, gnd(x, y - 30) - 25.0, R.uniform(0, 360), (0.12, 0.12, 0.06))
        inst(P + "Axe_Bronze", x, y - 30, gnd(x, y - 30) + 70.0, R.uniform(0, 360), 1.0, 0.0, 20.0)
    C.stats["micro_" + kind] = C.stats.get("micro_" + kind, 0) + 1


def fill_gaps(step=300.0, clear_r=280.0):
    """Anything more than ~4 m from the nearest vignette gets a small one (flowers, bushes, stacks, pots...)."""
    kinds = ["bushes", "flowers", "stones", "stack", "barrels", "pots", "tree", "flowers", "bushes", "log_seat",
             "woodpile", "tree", "flowers", "coop", "picnic", "cart", "tree", "coop"]
    pts = [(x + R.uniform(-140, 140), y + R.uniform(-140, 140))
           for x in range(-5200, 6200, int(step)) for y in range(-4000, 4000, int(step))]
    R.shuffle(pts)
    for (x, y) in pts:
        if not C.in_zone(x, y):
            continue
        if any(math.hypot(x - vx, y - vy) < clear_r for vx, vy in VIG):
            continue
        if not ok(x, y, 130.0, lane_gap=50.0) or C.slope(x, y, 120.0) > 90.0:
            continue
        kind = R.choice(kinds)
        if kind == "tree" and (C.building_hit(x, y, 380.0) or not ok(x, y, 240.0)):
            kind = "bushes"
        micro(x, y, kind)
        C.claim(x, y, 130.0)
        VIG.append((x, y))


# ============================================================================================ entry
def dress():
    global HOUSES, DOORS, TASKS
    _B.clear()
    _MEADOW.clear()
    VIG.clear()
    POIS.clear()
    skip = {"church", "bell_tower", "lighthouse", "boathouse", "mill_barn", "town_hall", "bakery", "house4", "house14"}
    frames = C.buildings()
    HOUSES = [House(f) for f in frames if f.name not in skip]
    DOORS = [(f, House(f).door_t) for f in frames if f.door is not None and f.name not in ("bell_tower", "lighthouse")]
    TASKS = list(C.task_spots().values())
    scan_level()

    smithy_yard()
    market_row()
    for h in HOUSES:
        if h.name != "smithy":
            dress_house(h)
            wall_ivy(h)
    shop_signs()
    captain_flag()
    extra_chests()
    alley_laundry()
    house_sheds()
    yard_laundry()
    allotments()
    washing_green()
    orchard()
    fish_racks()
    card_table()
    broken_cart()
    shrine()
    sea_bench()
    kids_corner()
    training_yard()
    feast_green()
    mason_yard()
    neighbourhood_well()
    pergola()
    free_sheds()
    signposts()
    lane_bunting()
    lane_benches()
    meadow_trees()
    lane_edges()
    fill_gaps()
    fill_small()
    flush()
    C.stats["pois"] = POIS
