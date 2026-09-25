"""Town square + market (zone "square"): Morrowmere's market day and social heart.

Circle r=22 m around (0, -1000). The plaza centre (0, -800) and the ring of 12 player starts (r=9 m) stay open:
that is where the town gathers. Around it:
  - a maypole at the centre on a cobbled footing, radial bunting lines to masts around the square (festival canopy)
  - market stalls (fruit, veg, fish, bread, pottery, cloth, flowers, apothecary) with goods and stock behind them
  - the inn's beer garden (tables, stools, mugs), the bakery front (bread baskets, flour sacks)
  - the town hall forecourt (banners, planters, benches), a wanted-poster wall by the notice board
  - a speaker's stage near the gallows (meetings), benches with seats, planters, barrels, a delivery wagon,
    smashable crate stacks and loot chests tucked into corners, sparse warm light pools.
Everything goes through kg_dress_common (C). Repeated small meshes go through C.instanced (one HISM per mesh).
"""
import math
import traceback

import unreal

import kg_dress_common as C

P, V, N, PIR, JP, WP = C.P, C.V, C.N, C.PIR, C.JP, C.WP
KIT = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/"
INT = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/"
BAGUETTE = "/Game/KillGodot/Items/Melee/Baguette_HPvkMpqqTg/StaticMeshes/SM_KG_Baguette"

PLAZA = C.PLAZA
R = C.rng

SEAT_H = {P + "Bench": 49.6, P + "Stool": 58.2, P + "Chair_1": 49.8, KIT + "Kitchen_Chair": 44.2,
          KIT + "Kitchen_Stool": 67.9}

# The builder's own props inside the zone (kg_build_village.py): claimed first so nothing lands on them.
EXISTING = [
    (-700, -300, 160), (-1000, -200, 150), (-1100, -300, 170), (600, -1200, 270), (600, -900, 120),
    (-600, -1400, 130), (-600, -100, 150), (500, 0, 150), (900, -1000, 50), (850, -800, 45), (1300, -500, 130),
    (1510, -340, 130), (1500, -650, 50), (1560, -740, 50), (-1300, -1100, 50), (-1360, -1020, 50), (300, 400, 50),
    (-500, 300, 45), (-250, 551, 40), (-1545, -957, 40), (1327, -234, 40), (-463, -2351, 40), (1648, -2178, 40),
    (913, 508, 70), (1157, 283, 70), (913, -2359, 70), (1245, 870, 70), (110, -1970, 45), (-808, -1564, 60),
    (2055, -830, 80), (1686, -1132, 45), (-1170, -1560, 45), (-340, -2130, 45), (1509, -2532, 45),
    (-1411, -1257, 90), (1322, -91, 90), (-1619, -998, 90), (1117, 551, 90), (-893, 724, 90), (-547, 906, 90),
    (669, -2283, 90), (791, -2580, 90), (-1007, 549, 40), (1740, 270, 40),
]

# ------------------------------------------------------------------------------------------------- batching
_batch = {}


def inst(path, x, y, z, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=False, cull=6000.0):
    """Queue one instance (flushed per mesh at the end through C.instanced)."""
    _batch.setdefault((path, collide, cull), []).append((x, y, z, yaw, scale, pitch, roll))


def flush():
    for (path, collide, cull), tr in _batch.items():
        C.instanced(path, tr, collide=collide, cull=cull, sub="Instanced")
    _batch.clear()


# ------------------------------------------------------------------------------------------------- frames
class F:
    """Local frame at (x, y) facing a point (local -Y = front, +X = right when looking out of the front)."""

    def __init__(self, x, y, face=None, yaw=None, z=None):
        self.yaw = yaw if yaw is not None else C.facing_yaw((x, y), face)
        self.f = C.Frame(x, y, self.yaw)
        self.x, self.y = x, y
        self.z = C.ground(x, y) if z is None else z

    def w(self, lx, ly):
        return self.f.world(lx, ly)

    def put(self, path, lx, ly, lz=0.0, lyaw=0.0, scale=1.0, on_ground=False, **kw):
        x, y = self.w(lx, ly)
        z = (C.ground(x, y) + lz) if on_ground else (self.z + lz)
        return C.place(path, x, y, z, yaw=self.yaw + lyaw, scale=scale, **kw)

    def inst(self, path, lx, ly, lz=0.0, lyaw=0.0, scale=1.0, pitch=0.0, roll=0.0, on_ground=False, **kw):
        x, y = self.w(lx, ly)
        z = (C.ground(x, y) + lz) if on_ground else (self.z + lz)
        inst(path, x, y, z, self.yaw + lyaw, scale, pitch, roll, **kw)


def sit(path, x, y, mesh_yaw, z=None, stand=70.0):
    """A real seat (AKGSeat). Furniture faces its local +Y; the sitter faces the seat's +X, so the actor turns +90
    and the mesh -90 (same convention as kg_interiors.seat)."""
    s = C.seat(path, x, y, z=z, yaw=mesh_yaw + 90.0, seat_height=SEAT_H.get(path, 50.0))
    if s is None:
        return None
    try:
        s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
        s.set_editor_property("stand_distance", stand)
        s.set_seat_mesh(C.mesh(path))
        C.stats["seats"] = C.stats.get("seats", 0) + 1
    except Exception:
        # plain static mesh fallback (class not compiled): turn the mesh back to face mesh_yaw
        try:
            s.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=mesh_yaw), False)
        except Exception:
            pass
    return s


def find(x, y, r, reach=240.0, step=60.0, **kw):
    """Nearest free spot to (x, y) within `reach` (spiral search). Logs a miss in stats['blocked']."""
    if C.free(x, y, r, **kw):
        return x, y
    d = step
    while d <= reach:
        n = max(6, int(2 * math.pi * d / step))
        for k in range(n):
            a = 2 * math.pi * k / n
            xx, yy = x + d * math.cos(a), y + d * math.sin(a)
            if C.free(xx, yy, r, **kw):
                return xx, yy
        d += step
    C.stats.setdefault("blocked", []).append(f"{x:.0f},{y:.0f},r{r:.0f}")
    return None


def rod(ax, ay, az, bx, by, bz, thick=0.1):
    """A thin wooden line between two points (bunting string): Corner_Exterior_Wood is 300 cm along local +Z.
    Local +Z after (pitch p, yaw w) = (-sin p cos w, -sin p sin w, cos p)."""
    if bz < az:
        ax, ay, az, bx, by, bz = bx, by, bz, ax, ay, az
    dx, dy, dz = bx - ax, by - ay, bz - az
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    pitch = math.degrees(math.atan2(math.hypot(dx, dy), dz))
    yaw = math.degrees(math.atan2(-dy, -dx))
    inst(V + "Corner_Exterior_Wood", ax, ay, az, yaw, (thick, thick, L / 300.0), pitch, 0.0, cull=18000.0)


PENNANTS = [P + "Banner_1_Cloth", P + "Banner_2_Cloth", INT + "SM_KG_Rug_Runner"]


def bunting(a, b, sag=60.0, step=46.0, start=0):
    """String of pennants from a=(x,y,z) to b with a parabolic sag: red / blue / cream cloth."""
    (ax, ay, az), (bx, by, bz) = a, b
    L = math.hypot(bx - ax, by - ay)
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    segs = 5
    pts = [(ax + (bx - ax) * k / segs, ay + (by - ay) * k / segs,
            az + (bz - az) * k / segs - sag * 4 * (k / segs) * (1 - k / segs)) for k in range(segs + 1)]
    for p, q in zip(pts, pts[1:]):
        rod(*p, *q)
    n = int(L / step)
    for k in range(1, n):
        t = k / n
        x, y = ax + (bx - ax) * t, ay + (by - ay) * t
        z = az + (bz - az) * t - sag * 4 * t * (1 - t) - 2.0
        kind = (k + start) % 3
        if kind == 2:   # cream rug strip hung upright (roll 90 turns the flat rug vertical; it is centred in Y)
            inst(PENNANTS[2], x, y, z - 19.0, yaw, (0.1, 0.45, 1.0), 0.0, 90.0, cull=18000.0)
        else:
            inst(PENNANTS[kind], x, y, z, yaw + R.uniform(-6, 6), (0.3, 0.6, 0.17), 0.0, R.uniform(-8, 8),
                 cull=18000.0)


# ------------------------------------------------------------------------------------------------- stall goods
TOP = 83.0   # Stall_Empty / Stall_Cart_Empty counter height


def mk(fr, path, lx, ly, lz=0.0, lyaw=0.0, scale=1.0, collide=False, cull=7000.0, ground=True, sub="Market"):
    return fr.put(path, lx, ly, lz, lyaw, scale=scale, on_ground=ground, sub=sub, collide=collide, cull=cull)


def crate_stack(fr, lx, ly, n=2):
    kinds = [PIR + "crates_0", PIR + "crates_1", P + "FarmCrate_Empty"]
    for k in range(n):
        mk(fr, kinds[k % 2], lx + R.uniform(-4, 4), ly + R.uniform(-4, 4), k * 70.0, R.uniform(-12, 12),
           collide=(k == 0), cull=9000.0)


def stock(fr, sub="Market"):
    """Behind the counter: crates, sacks, a barrel and the vendor's stool."""
    crate_stack(fr, -100, 105, 2 if R.random() < 0.6 else 1)
    for k in range(2):
        mk(fr, P + "Bag", -5 + k * 45 + R.uniform(-5, 5), 118 + R.uniform(-8, 8), 0, R.uniform(0, 360),
           scale=R.uniform(0.8, 1.0), cull=7000.0)
    mk(fr, PIR + "Barrel_%d" % R.choice([0, 2, 5, 7, 9]), 100, 100, 0, R.uniform(0, 360), collide=True, cull=9000.0)
    x, y = fr.w(-20, 60)
    sit(P + "Stool", x, y, mesh_yaw=fr.yaw + 180.0 + R.uniform(-20, 20))


def bread_basket(fr, lx, ly):
    """A 'basket' of loaves: an open farm crate on the ground heaped with bread."""
    mk(fr, P + "FarmCrate_Empty", lx, ly, 0, R.uniform(-20, 20), sub="Bakery")
    for k in range(5):
        fr.inst(INT + "SM_KG_Bread", lx - 18 + (k % 3) * 18, ly - 7 + (k // 3) * 14, 13 + (k % 2) * 4,
                R.uniform(-25, 25), 1.0, on_ground=True)


def goods(fr, kind, existing=False, cart=False):
    """Goods on the counter (frame local: counter x +-85, y +-38 at z=83; customers at -Y) + front display."""
    t = TOP
    if kind == "fruit":
        for lx in (-42, 42):
            fr.put(P + "FarmCrate_Apple", lx, -10, t, R.uniform(-5, 5), sub="Market", collide=False, cull=7000.0)
        fr.put(P + "FarmCrate_Apple", 0, 18, t + 22, 90 + R.uniform(-5, 5), sub="Market", collide=False, cull=7000.0)
        if not existing:
            mk(fr, P + "Barrel_Apples", -130, -60, 0, R.uniform(0, 360), collide=True, cull=9000.0)
            mk(fr, P + "FarmCrate_Apple", 125, -70, 0, 20)
            mk(fr, P + "FarmCrate_Apple", 128, -72, 24, 8)
    elif kind == "veg":
        for lx, yw in ((-44, 0), (42, 180)):
            fr.put(P + "FarmCrate_Carrot", lx, -8, t, yw + R.uniform(-6, 6), sub="Market", collide=False, cull=7000.0)
        for k in range(5):   # cabbages / mushrooms along the counter front
            fr.inst(N + "Mushroom_Common", -70 + k * 35, -33, t, R.uniform(0, 360), 0.3)
        if not existing:
            mk(fr, P + "FarmCrate_Carrot", -125, -68, 0, 30)
            mk(fr, P + "Bag", 125, -62, 0, R.uniform(0, 360))
            mk(fr, P + "Bucket_Wooden_1", 95, -105, 0, 0, cull=6000.0)
    elif kind == "fish":
        fish = [WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Salmon", WP + "SM_KG_Fish_Mackerel", WP + "SM_KG_Fish_Cod"]
        for k in range(10):
            lx, ly = -72 + (k % 5) * 36 + R.uniform(-4, 4), -24 + (k // 5) * 28
            fr.inst(fish[k % 4], lx, ly, t + 7, 90 + R.uniform(-15, 15), 1.0, 0.0, 90.0)
        fr.inst(WP + "SM_KG_Fish_GoldenCarp", 62, 24, t + 8, 80, 1.0, 0.0, 90.0)
        if not existing:
            mk(fr, P + "Bucket_Metal", -125, -65, 0, 0, cull=6000.0)
            fr.inst(WP + "SM_KG_Fish_Mackerel", -125, -65, 36, 30, 1.0, -60.0, 0.0, on_ground=True)
            fr.inst(WP + "SM_KG_Fish_Cod", -118, -58, 34, 120, 1.0, -50.0, 0.0, on_ground=True)
            mk(fr, PIR + "Barrel_9", 128, -55, 0, R.uniform(0, 360), collide=True, cull=9000.0)
            mk(fr, WP + "SM_KG_FishingRod", 150, 30, 6, -100, cull=6000.0)
            mk(fr, P + "Rope_2", -160, 20, 1, 0, cull=5000.0)
            mk(fr, WP + "SM_KG_Oar", 170, -20, 3, 100, cull=6000.0)
    elif kind == "bread":
        for k in range(8):
            fr.inst(INT + "SM_KG_Bread", -66 + (k % 4) * 42, -22 + (k // 4) * 26, t, R.uniform(-15, 15) + 90, 1.0)
        fr.inst(INT + "SM_KG_Bread", -45, -9, t + 11, 80, 1.0)
        fr.inst(INT + "SM_KG_Bread", 40, -9, t + 11, 100, 1.0)
        for k in range(3):
            fr.inst(INT + "SM_KG_Cheese", 58, 26, t + k * 8, R.uniform(0, 360), 1.0)
        if not existing:
            bread_basket(fr, -125, -70)
            bread_basket(fr, 128, -62)
            mk(fr, P + "Bucket_Wooden_1", -150, 20, 0, 0, cull=6000.0)
            for k in range(3):   # baguettes standing in the bucket
                fr.inst(BAGUETTE, -150 + (k - 1) * 6, 20 + (k % 2) * 5, 18, R.uniform(0, 360), 0.45,
                        80.0, 0.0, on_ground=True)
    elif kind == "pottery":
        for k, pth in enumerate([P + "Vase_4", KIT + "Kitchen_Jar", KIT + "Kitchen_YellowJar", KIT + "Kitchen_Jar",
                                 P + "Vase_4"]):
            fr.inst(pth, -70 + k * 35, -12, t, R.uniform(0, 360), 0.8 if "Vase" in pth else 1.8)
        for k in range(4):
            fr.inst(KIT + "Kitchen_Plate", 55, 22, t + k * 2.5, R.uniform(0, 360), 1.0)
            fr.inst(KIT + "Kitchen_Bowl", -55, 22, t + k * 6, R.uniform(0, 360), 1.3)
        if not existing:
            for lx, ly, s in [(-125, -60, 1.0), (-165, 15, 0.8), (130, -60, 0.9)]:
                mk(fr, P + "Vase_2", lx, ly, 0, R.uniform(0, 360), scale=s, collide=True, cull=8000.0)
            mk(fr, P + "Pot_1", 120, 20, 0, R.uniform(0, 360), cull=6000.0)
            mk(fr, P + "Pot_1_Lid", 150, -10, 0, R.uniform(0, 360), cull=6000.0)
            mk(fr, P + "Vase_Rubble_Medium", 50, -125, 0, R.uniform(0, 360), cull=5000.0)
    elif kind == "cloth":
        for k in range(3):   # folded cloth stacks on the counter (cream rugs folded small)
            for j in range(3 + k % 2):
                fr.inst(INT + "SM_KG_Rug_Rect", -58 + k * 55, -6, t + 1 + j * 5, R.uniform(-8, 8), (0.2, 0.26, 5.0))
        for side in (-1, 1):  # bolts of red / blue cloth hung from the awning rail
            fr.put(P + ("Banner_1_Cloth" if side < 0 else "Banner_2_Cloth"), side * 72, -36, 232, 0,
                   scale=(0.7, 1.0, 0.55), sub="Market", collide=False, cull=9000.0)
        if not existing:
            fr.put(P + "Banner_2_Cloth", 0, 40, 238, 0, scale=(1.5, 1.0, 0.8), sub="Market", collide=False, cull=9000.0)
            mk(fr, P + "Table_Large", 215, -10, 0, 90, scale=(0.55, 0.75, 0.9), collide=True, cull=9000.0)
            mk(fr, INT + "SM_KG_Rug_Rect", 215, -10, 73, 90, scale=(0.62, 0.5, 1.0), cull=7000.0)
            for k in range(3):
                fr.inst(INT + "SM_KG_Rug_Round", 215, -60 + k * 50, 74 + k, R.uniform(0, 360), (0.26, 0.26, 7.0),
                        on_ground=True)
    elif kind == "apothecary":
        pots = [P + "Potion_1", P + "Potion_2", P + "Potion_4", P + "Bottle_1", P + "Potion_2", P + "SmallBottles_1"]
        for k in range(10):
            fr.inst(pots[k % 6], -72 + (k % 5) * 36 + R.uniform(-4, 4), -20 + (k // 5) * 28, t, R.uniform(0, 360), 1.0)
        fr.inst(P + "CandleStick_Triple", 55, 25, t, 0, 1.0)
        fr.inst(P + "Coin_Pile", 20, -28, t, R.uniform(0, 360), 1.0)
        fr.inst(INT + "SM_KG_HerbBundle", -50, 30, t + 130, 0, 1.0)
        fr.inst(INT + "SM_KG_HerbBundle", 0, 30, t + 128, 40, 1.0)
        fr.inst(INT + "SM_KG_HerbBundle", 45, 30, t + 131, 80, 1.0)
        if not existing:
            mk(fr, P + "Cauldron", -130, -55, 0, R.uniform(0, 360), collide=True, cull=9000.0)
    elif kind == "flowers":
        # potted flowers on the counter and in front: the most colourful stall of the market
        for k in range(4):
            fr.inst(P + "Pot_1", -64 + k * 42, -8, t, R.uniform(0, 360), 0.65)
            fr.inst([N + "Petal_5", N + "Petal_1", N + "Petal_2", N + "Petal_3"][k], -64 + k * 42, -8, t + 10,
                    R.uniform(0, 360), 0.45)
        fr.inst(N + "Flower_3_Single", -40, 22, t, 0, 0.35)
        fr.inst(N + "Flower_4_Single", 30, 22, t, 60, 0.3)
        if not existing:
            for lx, ly in [(-125, -60), (130, -55), (-150, 20)]:
                potted_flowers(*fr.w(lx, ly))
    if not existing:
        stock(fr)


def stall(x, y, face, kind, mesh=None):
    """A market stall at (x, y) facing `face`: counter goods, front display, stock behind, vendor stool."""
    fr = F(x, y, face)
    mesh = mesh or P + "Stall_Empty"
    fr.put(mesh, 0, 0, 0, 180.0, sub="Market")
    C.claim(x, y, 150.0)
    goods(fr, kind)
    trim(fr, kind)
    C.stats.setdefault("stalls", []).append(f"{kind}@{x:.0f},{y:.0f}")
    return fr


STALL_COLOUR = {"fruit": 1, "veg": 2, "fish": 2, "bread": 1, "pottery": 1, "cloth": 2, "apothecary": 2,
                "flowers": 1, "poultry": 1}


def trim(fr, kind, existing=False):
    """Colour and life on a stall: a cloth valance along the awning front, a swinging shop sign, a lantern."""
    cloth = P + ("Banner_1_Cloth" if STALL_COLOUR.get(kind, 1) == 1 else "Banner_2_Cloth")
    fr.put(cloth, 0, -46, 252, 0, scale=(2.25, 1.0, 0.2), sub="Market", collide=False, cull=12000.0)
    sx, sy = fr.w(118, -50)
    C.mover(P + "Shield_Wooden", sx, sy, z=fr.z + 205.0, yaw=fr.yaw, scale=0.75, sway=5.0, sway_hz=0.4,
            sub="Market")
    fr.put(P + "Lantern_Wall", -92, -42, 150, 90, sub="Market", collide=False, cull=9000.0)


# ------------------------------------------------------------------------------------------------- planters
def potted_flowers(x, y, small=False, kind=None):
    pot = P + ("Vase_4" if small else "Vase_2")
    s = 0.75 if small else 1.0
    gz = C.ground(x, y)
    C.place(pot, x, y, gz, yaw=R.uniform(0, 360), scale=s, sub="Planters", cull=8000.0, collide=not small)
    top = gz + (49.0 if small else 52.0) * s
    kind = kind or R.choice(["bush", "flower", "petal", "bush"])
    if kind == "bush":
        inst(N + "Bush_Common_Flowers", x, y, top - 8.0, R.uniform(0, 360), 0.26 if small else 0.36, cull=8000.0)
    elif kind == "flower":
        inst(N + "Flower_3_Group", x, y, top - 5.0, R.uniform(0, 360), 0.3 if small else 0.4, cull=8000.0)
    else:
        inst(N + "Petal_5", x, y, top - 4.0, R.uniform(0, 360), 0.55 if small else 0.8, cull=6000.0)
        inst(N + "Clover_1", x + 8, y - 6, top - 6.0, R.uniform(0, 360), 0.35, cull=6000.0)


def planter_box(x, y, yaw, n=3):
    """Long wooden flower box (farm crates end to end) with flowers."""
    fr = F(x, y, yaw=yaw)
    for k in range(n):
        lx = (k - (n - 1) / 2.0) * 72.0
        mk(fr, P + "FarmCrate_Empty", lx, 0, 0, 0, sub="Planters")
        fr.inst(R.choice([N + "Petal_5", N + "Petal_1", N + "Petal_2"]), lx, 0, 12, R.uniform(0, 360), 0.7,
                on_ground=True)
        fr.inst(N + "Flower_3_Single", lx + 14, 4, 8, R.uniform(0, 360), 0.26, on_ground=True)


# ------------------------------------------------------------------------------------------------- sections
def maypole():
    """Centre: a tall maypole with a flower crown on a cobbled round footing."""
    x, y = PLAZA
    gz = C.ground_min(x, y, 150.0)
    for k in range(7):   # cobbled footing (flat, walkable, no collision)
        a = math.radians(k * 51.4)
        rr = 0.0 if k == 0 else 190.0
        inst(N + "RockPath_Round_Wide", x + rr * math.cos(a), y + rr * math.sin(a), gz - 24.0, R.uniform(0, 360),
             1.0 if k == 0 else 0.9, cull=9000.0)
    C.place(V + "Stairs_Exterior_Platform", x, y, gz - 55.0, yaw=45.0, scale=(0.42, 0.42, 0.9), sub="Maypole")
    C.place(V + "Corner_Exterior_Wood", x, y, gz + 25.0, yaw=0.0, scale=(1.4, 1.4, 2.6), sub="Maypole")
    top = gz + 25.0 + 780.0
    C.place(N + "Bush_Common_Flowers", x, y, top - 75.0, yaw=0.0, scale=(0.45, 0.45, 0.4), sub="Maypole",
            collide=False)
    C.place(P + "Rope_2", x, y, top - 100.0, yaw=0.0, scale=(0.8, 0.8, 2.0), sub="Maypole", collide=False)
    C.place(V + "Corner_Exterior_Wood", x, y, top - 20.0, yaw=0.0, scale=(0.5, 0.5, 0.45), sub="Maypole")
    C.place(P + "Banner_1_Cloth", x, y, top + 112.0, yaw=0.0, scale=(0.5, 1.0, 0.4), sub="Maypole", collide=False)
    for k in range(6):   # little flower pots at the footing
        a = math.radians(k * 60.0 + 30.0)
        potted_flowers(x + 120.0 * math.cos(a), y + 120.0 * math.sin(a), small=True)
    C.claim(x, y, 170.0)
    return (x, y, top - 70.0)


MASTS = []


def mast(x, y, h=520.0):
    """Bunting mast: tall post with a pennant on top."""
    gz = C.ground(x, y)
    C.place(V + "Corner_Exterior_Wood", x, y, gz, yaw=R.uniform(0, 90), scale=(1.1, 1.1, h / 300.0), sub="Bunting")
    C.place(V + "Corner_Exterior_Wood", x, y, gz + h - 5.0, scale=(0.35, 0.35, 0.4), sub="Bunting", collide=False)
    C.place(P + ("Banner_2_Cloth" if len(MASTS) % 2 else "Banner_1_Cloth"), x, y, gz + h + 110.0,
            yaw=R.uniform(0, 360), scale=(0.35, 1.0, 0.33), sub="Bunting", collide=False, cull=15000.0)
    C.claim(x, y, 40.0)
    MASTS.append((x, y, gz + h - 10.0))
    return MASTS[-1]


def festival_canopy(top):
    """Radial bunting from the maypole crown to masts around the square, a ring between the masts, and lines to
    the town hall / inn / bakery facades."""
    px, py = PLAZA
    for k, a in enumerate([15.0, 50.0, 118.0, 150.0, 196.0, 236.0, 300.0, 335.0]):
        for r in (1180.0, 1260.0, 1100.0, 1340.0):
            spot = find(px + r * math.cos(math.radians(a)), py + r * math.sin(math.radians(a)), 40.0, reach=150.0)
            if spot:
                bunting(top, mast(*spot), sag=90.0, start=k)
                break
    ring = sorted(MASTS, key=lambda m: math.atan2(m[1] - py, m[0] - px))
    for a, b in zip(ring, ring[1:] + ring[:1]):
        if math.hypot(a[0] - b[0], a[1] - b[1]) < 1500.0:
            bunting(a, b, sag=80.0, start=1)
    # to the town hall gable, the inn front and the bakery front (hooks on the facades)
    for name, lx, lz in [("town_hall", 0.0, 560.0), ("inn", 150.0, 520.0), ("bakery", -120.0, 480.0)]:
        b = [f for f in C.buildings() if f.name == name][0]
        hx, hy = b.world(lx, -b.d * 0.5 - 25.0)
        best = min(MASTS, key=lambda m: math.hypot(m[0] - hx, m[1] - hy)) if MASTS else top
        bunting(best, (hx, hy, C.ground(hx, hy) + lz), sag=60.0, start=2)


def market():
    """Stalls around the square facing the plaza, plus goods on the village's existing empty stalls."""
    for kind, x, y, mesh in [("pottery", 1160.0, -60.0, None), ("cloth", 860.0, 280.0, None),
                             ("fish", -430.0, 540.0, None), ("flowers", -750.0, 500.0, P + "Stall_Cart_Empty"),
                             ("bread", -1430.0, -1400.0, None), ("apothecary", 720.0, -2650.0, P + "Stall_Cart_Empty")]:
        spot = find(x, y, 150.0, reach=260.0, step=40.0)
        if spot:
            stall(spot[0], spot[1], PLAZA, kind, mesh)
    # The village's own empty stalls get stocked too (builder: Stall_Empty (1300,-500) yaw -70, the StockStall hint
    # stall (1510,-340) yaw 0, the cart (-1100,-300) yaw 60). Mesh front = local +Y -> frame yaw = mesh yaw - 180.
    for (x, y, myaw, kind) in [(1300.0, -500.0, -70.0, "fruit"), (1510.0, -340.0, 0.0, "veg"),
                               (-1100.0, -300.0, 60.0, "fish")]:
        fr = F(x, y, yaw=myaw - 180.0)
        goods(fr, kind, existing=True)
        trim(fr, kind)


def inn_garden():
    """The inn's beer garden between the gallows and the inn: tables with stools, mugs, ale barrels."""
    inn = [b for b in C.buildings() if b.name == "inn"][0]
    spots = []
    for lx, ly in [(450.0, -630.0), (700.0, -620.0), (950.0, -620.0), (700.0, -390.0), (950.0, -390.0)]:
        s = find(*inn.world(lx, ly), 110.0, reach=60.0, step=30.0)
        if s:
            spots.append(s)
            C.claim(s[0], s[1], 115.0)
    for k, (x, y) in enumerate(spots):
        tyaw = inn.yaw + R.uniform(-20, 20)
        fr = F(x, y, yaw=tyaw)
        fr.put(KIT + "Kitchen_Square_Table", 0, 0, 0, 0, sub="Inn", cull=9000.0)
        for j, (sx, sy, syaw) in enumerate([(0, -72, 0), (0, 72, 180), (-72, 0, -90), (72, 0, 90)]):
            if (k + j) % 4 == 3:
                continue
            sxw, syw = fr.w(sx + R.uniform(-6, 6), sy + R.uniform(-6, 6))
            sit(P + "Stool", sxw, syw, mesh_yaw=tyaw + syaw + R.uniform(-15, 15))
        for j in range(R.randint(2, 4)):
            fr.inst(P + "Mug", R.uniform(-28, 28), R.uniform(-28, 28), 67, R.uniform(0, 360), 1.0)
        if k % 2 == 0:
            fr.inst(KIT + "Kitchen_Plate", 10, 5, 67, 0, 1.0)
            fr.inst(INT + "SM_KG_Bread", 10, 5, 68.5, R.uniform(0, 360), 0.8)
            fr.inst(INT + "SM_KG_Cheese", -12, -14, 67, R.uniform(0, 360), 0.8)
        else:
            fr.inst(KIT + "Kitchen_Bottle", -18, 12, 67, 0, 1.0)
            fr.inst(P + "CandleStick", 5, -5, 67, R.uniform(0, 360), 1.0)
    # ale barrels on a rack by the inn's corner, a keg stack
    for (x, y) in [(1560.0, -1330.0), (1520.0, -1420.0)]:
        s = find(x, y, 55.0, reach=120.0, step=40.0)
        if s:
            C.place(PIR + "Barrel_13", s[0], s[1], C.ground(*s), yaw=R.uniform(0, 360), sub="Inn", cull=9000.0)
            C.claim(s[0], s[1], 55.0)
    return spots


def bakery_front():
    b = [f for f in C.buildings() if f.name == "bakery"][0]
    for lx, ly in [(-170.0, -390.0), (-260.0, -400.0), (260.0, -400.0), (170.0, -430.0)]:
        x, y = b.world(lx, ly)
        if C.free(x, y, 40.0):
            bread_basket(F(x, y, yaw=b.yaw), 0, 0)
            C.claim(x, y, 40.0)
    for lx, ly in [(-330.0, -360.0), (-290.0, -350.0), (340.0, -360.0), (300.0, -370.0)]:   # flour sacks
        x, y = b.world(lx, ly)
        if C.free(x, y, 30.0):
            C.place(P + "Bag", x, y, C.ground(x, y), yaw=R.uniform(0, 360), sub="Bakery", collide=False, cull=7000.0)
            C.claim(x, y, 30.0)
    for lx, ly, yaw in [(-380.0, -120.0, 90.0), (380.0, -100.0, 90.0)]:   # firewood for the oven along the sides
        x, y = b.world(lx, ly)
        if C.free(x, y, 50.0):
            C.place(INT + "SM_KG_Firewood", x, y, C.ground(x, y), yaw=b.yaw + yaw, sub="Bakery", cull=7000.0)
            C.claim(x, y, 50.0)


def town_hall_front():
    th = [f for f in C.buildings() if f.name == "town_hall"][0]
    fy = -th.d * 0.5 - 24.0
    for lx, kind in [(-330.0, "Banner_1"), (-190.0, "Banner_2"), (170.0, "Banner_2"), (330.0, "Banner_1")]:
        x, y = th.world(lx, fy)
        C.place(P + kind, x, y, C.ground(x, y) + 480.0, yaw=th.yaw + 180.0, sub="TownHall", collide=False,
                cull=16000.0)
    for lx, ly in [(-300.0, -600.0), (330.0, -600.0), (360.0, -700.0)]:
        x, y = th.world(lx, ly)
        if C.free(x, y, 40.0):
            potted_flowers(x, y, kind="bush")
            C.claim(x, y, 40.0)
    # benches on the forecourt's east side, facing the plaza
    for (x, y) in [(560.0, -1880.0), (620.0, -2150.0)]:
        s = find(x, y, 120.0, reach=150.0, step=50.0)
        if s:
            sit(P + "Bench", s[0], s[1], mesh_yaw=C.facing_yaw(s, PLAZA) + 180.0)
            C.claim(s[0], s[1], 140.0)
    # paved forecourt: flat stone slabs in front of the door (walkable, no collision)
    for i in range(-2, 3):
        for j in range(3):
            x, y = th.world(i * 200.0, -th.d * 0.5 - 120.0 - j * 200.0)
            inst(N + "RockPath_Square_Wide", x, y, C.ground_min(x, y, 100.0) - 29.0, th.yaw + R.choice([0, 90, 180]),
                 1.0, cull=12000.0)


def wanted_wall():
    """A second board beside the notice board plastered with wanted posters (plaster scraps = paper)."""
    s = find(-1150.0, -1240.0, 110.0, reach=120.0, step=40.0)
    if not s:
        return
    x, y = s
    fr = F(x, y, face=PLAZA)
    for sx in (-1, 1):
        fr.put(V + "Corner_Exterior_Wood", sx * 140.0, 0, 0, 0, sub="Wanted")
    fr.put(V + "Wall_Plaster_WoodGrid", 0, 0, 70.0, 0, scale=(1.35, 0.3, 0.55), sub="Wanted")
    fr.put(V + "Roof_Wooden_2x1", 0, 35, 290.0, 180.0, scale=(1.5, 0.6, 0.6), sub="Wanted", collide=False)
    for k in range(9):
        lx = -110 + (k % 5) * 55 + R.uniform(-6, 6)
        lz = 140 + (k // 5) * 62 + R.uniform(-8, 8)
        fr.inst(V + "Wall_Plaster_Straight", lx, -16, lz, R.uniform(-4, 4), (0.17, 0.04, 0.14), 0.0, R.uniform(-6, 6))
    fr.put(P + "Lantern_Wall", 150, -12, 175, -90, sub="Wanted", collide=False, cull=9000.0)
    mk(fr, P + "Bucket_Wooden_1", -150, -45, 0, 0, sub="Wanted", cull=5000.0)
    fr.inst(P + "Scroll_1", -150, -45, 26, 30, 1.0, on_ground=True)
    C.claim(x, y, 150.0)


def speakers_stage():
    """A small plank stage between the gallows and the inn where meetings are called: steps, lectern, torches, bell."""
    s = find(1500.0, -1760.0, 200.0, reach=200.0, step=50.0)
    if not s:
        return None
    x, y = s
    fr = F(x, y, face=PLAZA, z=C.ground_min(x, y, 200.0))
    for lx in (-100.0, 100.0):
        fr.put(V + "Stairs_Exterior_Platform", lx, 0, -25.0, 0, sub="Stage")
    fr.put(V + "Stairs_Exterior_Straight", 0, -200.0, -25.0, 180.0, scale=(1.0, 1.0, 0.85), sub="Stage")
    fr.put(P + "BookStand", 60, -40, 75.0, 0, sub="Stage", cull=9000.0)
    for lx in (-200.0, 200.0):
        fr.put(V + "Corner_Exterior_Wood", lx, 90, 75.0, 0, scale=(1.0, 1.0, 1.1), sub="Stage")
    # cross beam + a long red cloth backdrop
    bx, by = fr.w(-215, 90)
    C.place(V + "Corner_Exterior_Wood", bx, by, fr.z + 75.0 + 330.0, yaw=fr.yaw, pitch=-90.0,
            scale=(1.0, 1.0, 1.43), sub="Stage", collide=False)
    fr.put(P + "Banner_1_Cloth", 0, 95, 75.0 + 325.0, 0, scale=(2.6, 1.0, 0.85), sub="Stage", collide=False)
    fr.put(P + "Banner_2_Cloth", -140, 88, 75.0 + 322.0, 0, scale=(0.6, 1.0, 0.95), sub="Stage", collide=False)
    fr.put(P + "Banner_2_Cloth", 140, 88, 75.0 + 322.0, 0, scale=(0.6, 1.0, 0.95), sub="Stage", collide=False)
    bx, by = fr.w(175, 60)
    C.mover(WP + "SM_KG_Doorbell", bx, by, z=fr.z + 75.0 + 245.0, yaw=fr.yaw + 90.0, scale=3.0, sway=6.0,
            sway_hz=0.5, sub="Stage")
    for lx in (-250.0, 250.0):
        fr.put(PIR + "Torch_3", lx, -140, 0, 0, sub="Stage", cull=12000.0, on_ground=True)
    fr.put(P + "Crate_Wooden", -150, -40, 75.0, 20, sub="Stage", cull=8000.0)
    fr.put(P + "Bucket_Wooden_1", -60, 40, 75.0, 0, sub="Stage", cull=6000.0, collide=False)
    C.claim(x, y, 270.0)
    return fr


def benches():
    """Benches (real seats) around the square's rim facing the maypole, a planter at one end."""
    px, py = PLAZA
    placed, last = 0, -999.0
    for a in range(0, 360, 4):
        if placed >= 9 or a - last < 28:
            continue
        for r in (1150.0, 1230.0, 1310.0):
            x, y = px + r * math.cos(math.radians(a)), py + r * math.sin(math.radians(a))
            if C.free(x, y, 115.0):
                myaw = C.facing_yaw((x, y), PLAZA) + 180.0
                sit(P + "Bench", x, y, mesh_yaw=myaw)
                C.claim(x, y, 130.0)
                last = a
                fr = F(x, y, face=PLAZA)
                ex, ey = fr.w(-180.0, 15.0)
                if C.free(ex, ey, 40.0):
                    potted_flowers(ex, ey)
                    C.claim(ex, ey, 40.0)
                placed += 1
                break
    C.stats["benches"] = placed


def wagon():
    """A delivery wagon parked at the market's south-east corner, loaded with barrels and crates."""
    s = find(1450.0, 620.0, 200.0, reach=250.0, step=50.0)
    if not s:
        return
    fr = F(s[0], s[1], yaw=200.0)
    fr.put(V + "Prop_Wagon", 0, 0, 0, 0, sub="Wagon", cull=15000.0)
    for k, (lx, ly) in enumerate([(-40, -150), (40, -150), (-40, -70), (40, -70)]):
        fr.put(PIR + "Barrel_%d" % (1 + k % 3), lx, ly, 78.0, R.uniform(0, 360), scale=0.7, sub="Wagon",
               collide=False, cull=9000.0)
    for k, (lx, ly) in enumerate([(-35, 20), (35, 25), (0, 22)]):
        fr.put(PIR + "crates_%d" % (k % 2), lx, ly, 78.0 + (60.0 if k == 2 else 0.0), R.uniform(-15, 15), scale=0.8,
               sub="Wagon", collide=False, cull=9000.0)
    mk(fr, P + "Bag", 150, 60, 0, 30, sub="Wagon")
    mk(fr, P + "Bag", 180, 20, 0, 80, sub="Wagon")
    C.claim(s[0], s[1], 230.0)


def smashables():
    """Crate / barrel stacks people can punch apart (AKGBreakable), tucked behind stalls and in corners."""
    n = 0
    for (x, y) in [(1700.0, -150.0), (-1600.0, -1250.0), (-250.0, 850.0), (1450.0, 200.0), (950.0, -1850.0),
                   (-1650.0, -700.0), (-1250.0, 180.0), (400.0, 950.0), (-950.0, 950.0), (1750.0, -1900.0),
                   (-1000.0, -2350.0), (200.0, -2350.0)]:
        s = find(x, y, 70.0, reach=150.0, step=50.0)
        if not s:
            continue
        xx, yy = s
        kinds = [P + "Crate_Wooden", P + "Crate_Wooden", P + "Barrel", PIR + "crates_0"]
        C.breakable(R.choice(kinds), xx, yy, yaw=R.uniform(0, 360))
        C.breakable(P + "Crate_Wooden", xx + 55.0, yy + 30.0, yaw=R.uniform(0, 360))
        n += 2
        if R.random() < 0.5:
            C.breakable(P + "Crate_Wooden", xx + 22.0, yy + 12.0, z=C.ground(xx, yy) + 95.0, yaw=R.uniform(0, 360))
            n += 1
        C.claim(xx + 25.0, yy + 15.0, 85.0)
    C.stats["breakables"] = n


def loot():
    for (x, y, yaw, name) in [(700.0, -2850.0, 90.0, "Town Coffer"), (-1650.0, -1500.0, 70.0, "Baker's Chest"),
                              (1700.0, 500.0, 200.0, "Merchant's Strongbox")]:
        s = find(x, y, 60.0, reach=180.0, step=50.0)
        if s:
            C.loot_chest(s[0], s[1], yaw=yaw, name=name, path=PIR + "chest_common_0")
            C.claim(s[0], s[1], 70.0)
            # a barrel and a sack beside it so it reads as a stash, not a spawn
            C.place(P + "Barrel", s[0] + 80.0, s[1] + 20.0, C.ground(s[0] + 80.0, s[1] + 20.0), yaw=R.uniform(0, 360),
                    sub="Loot", cull=9000.0)
            C.place(P + "Bag", s[0] - 70.0, s[1] + 30.0, C.ground(s[0] - 70.0, s[1] + 30.0), yaw=R.uniform(0, 360),
                    sub="Loot", collide=False, cull=7000.0)


def vignettes():
    """Little story corners in the outer ring of the zone."""
    # poultry seller: stacked chicken cages, grain sacks, a feed bucket, a stool
    s = find(676.0, 650.0, 110.0, reach=150.0, step=50.0)
    if s:
        fr = F(*s, face=PLAZA)
        for k, (lx, ly, lz) in enumerate([(-45, 0, 0), (45, 5, 0), (0, 2, 80), (-50, 70, 0)]):
            mk(fr, P + "Cage_Small", lx, ly, lz, R.uniform(-10, 10), collide=(lz == 0), cull=8000.0, sub="Poultry")
        mk(fr, P + "Bag", 110, 30, 0, 40, sub="Poultry")
        mk(fr, P + "Bag", 120, -20, 0, 120, scale=0.85, sub="Poultry")
        mk(fr, P + "Bucket_Wooden_1", -110, -50, 0, 0, sub="Poultry", cull=5000.0)
        x, y = fr.w(20, -90)
        sit(P + "Stool", x, y, mesh_yaw=fr.yaw + 180.0)
        C.claim(s[0], s[1], 130.0)
    # armourer's table: blades and shields laid out, a weapon rack, a whetstone
    s = find(830.0, 930.0, 130.0, reach=160.0, step=50.0)
    if s:
        fr = F(*s, face=PLAZA)
        mk(fr, P + "Table_Large", 0, 0, 0, 0, scale=(0.75, 0.85, 1.0), collide=True, cull=10000.0, sub="Armourer")
        for k, pth in enumerate([P + "Sword_Bronze", P + "Axe_Bronze", P + "Sword_Bronze"]):
            # roll 90 lays the blade flat (along local -Y); yaw 90 turns it along the table
            fr.inst(pth, -40 - (k % 2) * 20, -28 + k * 28, 81 + 4, 90 + R.uniform(-6, 6), 1.0, 0.0, 90.0,
                    on_ground=True)
        fr.inst(P + "Shield_Wooden", 80, 15, 81 + 9, 0, 1.0, 0.0, 90.0, on_ground=True)
        mk(fr, P + "WeaponStand", 0, 100, 0, 180, collide=True, cull=10000.0, sub="Armourer")
        mk(fr, P + "Whetstone", 170, 40, 0, 90, collide=True, cull=9000.0, sub="Armourer")
        C.claim(s[0], s[1], 170.0)
    # the cooper's barrel pyramid (3-2-1) at the south edge
    s = find(-347.0, 1120.0, 120.0, reach=200.0, step=50.0)
    if s:
        fr = F(*s, face=PLAZA)
        k = 0
        for row, n in enumerate((3, 2, 1)):
            for j in range(n):
                lx = (j - (n - 1) / 2.0) * 98.0
                # on its side, lid to the plaza: yaw +90 then pitch 90 lays local +Z along the frame's -Y
                fr.put(PIR + "Barrel_%d" % (k % 14), lx, 50.0, 50.0 + row * 87.0, 90.0, pitch=90.0, sub="Cooper",
                       collide=(row == 0), cull=10000.0)
                k += 5
        mk(fr, P + "Barrel_Holder", 190, 0, 0, 90, collide=True, cull=9000.0, sub="Cooper")
        C.claim(s[0], s[1], 170.0)
    # town hall garden (east of the forecourt): two benches, planters, a lamp
    for (x, y) in [(620.0, -2230.0), (900.0, -2000.0)]:
        s = find(x, y, 120.0, reach=150.0, step=50.0)
        if s:
            sit(P + "Bench", s[0], s[1], mesh_yaw=C.facing_yaw(s, (0.0, -1600.0)) + 180.0)
            C.claim(s[0], s[1], 130.0)
            fr = F(*s, face=(0.0, -1600.0))
            ex, ey = fr.w(180.0, 10.0)
            if C.free(ex, ey, 40.0):
                potted_flowers(ex, ey, kind="bush")
                C.claim(ex, ey, 40.0)
    # west edge: the miller's delivery - flour sacks piled on pallets of planks, a barrow of firewood
    s = find(-1780.0, -1300.0, 110.0, reach=200.0, step=50.0)
    if s:
        fr = F(*s, face=PLAZA)
        mk(fr, PIR + "Planks_0", 0, 0, 0, 0, collide=False, cull=8000.0, sub="Miller")
        for k in range(5):
            mk(fr, P + "Bag", -60 + (k % 3) * 55, -8 + (k // 3) * 10, 10 + (k // 3) * 50, R.uniform(0, 360),
               scale=0.95, cull=8000.0, sub="Miller")
        mk(fr, INT + "SM_KG_Firewood", 130, 40, 0, 20, collide=True, cull=8000.0, sub="Miller")
        C.claim(s[0], s[1], 130.0)
    # extra benches in the outer ring (the inner ring is full): south strip and west
    for (x, y) in [(300.0, 800.0), (-1350.0, -600.0), (-1000.0, 150.0)]:
        s = find(x, y, 115.0, reach=180.0, step=45.0)
        if s:
            sit(P + "Bench", s[0], s[1], mesh_yaw=C.facing_yaw(s, PLAZA) + 180.0)
            C.claim(s[0], s[1], 130.0)


def ground_detail():
    """Flat detail on the dirt: a ring of paving stones around the maypole, paved aprons in front of the stalls and
    the beer garden, pebbles and flower tufts at the edges. No collision (walkable)."""
    px, py = PLAZA
    n = 28
    for k in range(n):   # stone ring (r=5.2 m) around the maypole
        a = 2 * math.pi * k / n
        x, y = px + 520.0 * math.cos(a), py + 520.0 * math.sin(a)
        inst(N + ("RockPath_Square_Small_%d" % (1 + k % 3)), x, y, C.ground(x, y) - 16.0,
             math.degrees(a) + R.uniform(-8, 8), 1.0, cull=9000.0)
    for spoke in range(8):   # radial stepping-stone spokes from the footing to the stone ring
        a = math.radians(spoke * 45.0 + 22.5)
        for rr in (300.0, 400.0):
            x, y = px + rr * math.cos(a), py + rr * math.sin(a)
            inst(N + "RockPath_Round_Small_%d" % (1 + spoke % 3), x, y, C.ground(x, y) - 12.0, R.uniform(0, 360), 0.8,
                 cull=8000.0)
    for st in C.stats.get("stalls", []):   # paved apron in front of each stall
        kind, xy = st.split("@")
        x, y = (float(v) for v in xy.split(","))
        fr = F(x, y, face=PLAZA)
        for lx in (-100.0, 100.0):
            fr.inst(N + "RockPath_Square_Wide", lx, -150.0, -28.0, R.choice([0, 90, 180, 270]), 1.0, on_ground=True,
                    cull=9000.0)
    # pebbles, clover and petal tufts: along the outside of the ring, never on the gathering area
    placed = 0
    for _ in range(900):
        if placed >= 140:
            break
        a = R.uniform(0, 2 * math.pi)
        rr = R.uniform(1000.0, 2150.0)
        x, y = 0.0 + rr * math.cos(a), -1000.0 + rr * math.sin(a)
        if math.hypot(x - px, y - py) < 1000.0 or not C.in_zone(x, y) or C.building_hit(x, y, 30.0):
            continue
        if C.overlaps(x, y, 20.0):
            continue
        roll = R.random()
        if roll < 0.55:
            inst(N + "Pebble_%s_%d" % (R.choice(["Round", "Square"]), R.randint(1, 5)), x, y, C.ground(x, y) - 3.0,
                 R.uniform(0, 360), R.uniform(0.6, 1.2), cull=4000.0)
        elif C.lane_distance(x, y) > 40.0:
            if roll < 0.8:
                inst(N + "Clover_%d" % R.randint(1, 2), x, y, C.ground(x, y) - 2.0, R.uniform(0, 360),
                     R.uniform(0.35, 0.55), cull=5000.0)
            else:
                inst(N + "Petal_%d" % R.randint(1, 5), x, y, C.ground(x, y) - 1.0, R.uniform(0, 360),
                     R.uniform(0.5, 0.8), cull=5000.0)
        placed += 1


def lights(stage, garden):
    """Sparse warm pools: maypole crown, stage torches, the beer garden, the bakery front, the fish stall."""
    px, py = PLAZA
    C.light(px, py, C.ground(px, py) + 330.0, 10.0, 900.0)
    if stage:
        for lx in (-250.0, 250.0):
            x, y = stage.w(lx, -140)
            C.light(x, y, C.ground(x, y) + 260.0, 9.0, 650.0, color=(255, 150, 70))
    if garden:
        x = sum(t[0] for t in garden) / len(garden)
        y = sum(t[1] for t in garden) / len(garden)
        C.light(x, y, C.ground(x, y) + 320.0, 8.0, 850.0)
    b = [f for f in C.buildings() if f.name == "bakery"][0]
    x, y = b.world(0.0, -450.0)
    C.light(x, y, C.ground(x, y) + 280.0, 6.0, 600.0)


def dress():
    MASTS.clear()
    _batch.clear()
    for (x, y, r) in EXISTING:
        C.claim(float(x), float(y), float(r))
    out = {}
    for name, fn in [("maypole", maypole), ("market", market), ("garden", inn_garden), ("bakery", bakery_front),
                     ("townhall", town_hall_front), ("wanted", wanted_wall), ("stage", speakers_stage),
                     ("wagon", wagon), ("benches", benches), ("vignettes", vignettes), ("smash", smashables),
                     ("loot", loot), ("ground", ground_detail)]:
        try:
            out[name] = fn()
        except Exception:
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-700:]}")
    try:
        if out.get("maypole"):
            festival_canopy(out["maypole"])
    except Exception:
        C.stats.setdefault("errors", []).append(f"canopy: {traceback.format_exc()[-700:]}")
    try:
        lights(out.get("stage"), out.get("garden"))
    except Exception:
        C.stats.setdefault("errors", []).append(f"lights: {traceback.format_exc()[-700:]}")
    flush()
