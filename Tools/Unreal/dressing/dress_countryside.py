"""Countryside dressing: the working farmland south-east of Morrowmere (zone "countryside").

y < -25 m, x > 8 m, within ~110 m of the plaza. Built around the existing farm (4000,-4400), the mill barn
(4800,-5200) and the farm_track lane (kept clear). Everything here is laid out by hand (metres in the specs,
converted to cm), not random scatter: crop fields with soil ridges, fences and dry-stone walls, an orchard,
beehives, a pen and hen house, a windmill on the rise, hay meadow, a shepherd's fold and small story vignettes.

Run live:  kg_dress.run(['countryside'])
"""
import math
import random

import unreal

import kg_dress_common as C

V, N, P, WP, PIR = C.V, C.N, C.P, C.WP, C.PIR
FURN = "/Game/KillGodot/Env/Furniture/"
JP = C.JP
M = 100.0
R = random.Random(20260924)

# Instanced batches: path -> [transforms]; per-mesh config (collide, cull). One HISM per mesh, so a mesh's
# collision/cull must be the same everywhere in this zone.
_batch = {}
_cfg = {}


def inst(path, x, y, z, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=False, cull=7000.0):
    _batch.setdefault(path, []).append((x, y, z, yaw, scale, pitch, roll))
    if path not in _cfg:
        _cfg[path] = (collide, cull)


def flush():
    for path, tr in _batch.items():
        collide, cull = _cfg[path]
        C.instanced(path, tr, collide=collide, cull=cull, sub="Instanced")
    _batch.clear()


def g(x, y):
    return C.ground(x, y)


def frange(a, b, step):
    v = a
    while v <= b + 1e-6:
        yield v
        v += step


class F(C.Frame):
    """Frame in cm; build with metres via mk()."""
    pass


def mk(xm, ym, yaw=0.0, wm=0.0, dm=0.0, name=""):
    return F(xm * M, ym * M, yaw, wm * M, dm * M, name)


# ---------------------------------------------------------------------------------------------- site checks
_RES = None


def _reserved():
    global _RES
    if _RES is None:
        _RES = C.reserved()
    return _RES


def site_ok(x, y, r=30.0, lane_gap=60.0, zone=True):
    """Ignores this run's claims (use inside an area we already own, e.g. crops in a field)."""
    if zone and not C.in_zone(x, y):
        return False
    if C.lane_distance(x, y) < r + lane_gap:
        return False
    if C.building_hit(x, y, r + 40.0):
        return False
    for (cx, cy, cr) in _reserved():
        if math.hypot(x - cx, y - cy) < r + cr:
            return False
    return True


def claim_rect(f, pad=0.0, step=250.0, r=180.0):
    for lx in frange(-f.w * 0.5 - pad, f.w * 0.5 + pad, step):
        for ly in frange(-f.d * 0.5 - pad, f.d * 0.5 + pad, step):
            x, y = f.world(lx, ly)
            C.claim(x, y, r)


# ---------------------------------------------------------------------------------------------- nature + grass
_nature = None
_meadow = None


def nature():
    """Nature/* actors (random trees, pines, rocks, ground plants from the builder) standing in this zone."""
    global _nature
    if _nature is None:
        _nature = []
        for a in C.actors.get_all_level_actors():
            f = str(a.get_folder_path())
            if not f.startswith("Nature/"):
                continue
            loc = a.get_actor_location()
            if C.zone_of(loc.x, loc.y) == "countryside":
                _nature.append([a, loc.x, loc.y, f, a.get_actor_scale3d().x])
    return _nature


def clear_nature(f, pad=150.0, keep_ground=False):
    """Fields and yards are cleared land: remove the builder's random trees/bushes/rocks inside the footprint.
    (Same pattern as C.clear_grass: the full rebuild regrows them, then this module clears them again.)"""
    doomed = []
    for e in nature():
        a, x, y, folder = e[0], e[1], e[2], e[3]
        if a is None or (keep_ground and folder == "Nature/Ground"):
            continue
        if f.inside(x, y, pad):
            doomed.append(a)
            e[0] = None
    if doomed:
        C.actors.destroy_actors(doomed)
    C.stats["nature_cleared"] = C.stats.get("nature_cleared", 0) + len(doomed)


def clear_nature_circle(x, y, r):
    clear_nature(C.Frame(x, y, 0.0, 2 * r, 2 * r), 0.0)


def claim_nature():
    for a, x, y, folder, s in nature():
        if a is None:
            continue
        r = {"Nature/Trees": 110.0, "Nature/Pines": 140.0, "Nature/Rocks": 150.0}.get(folder, 60.0) * max(0.6, s)
        C.claim(x, y, r)


def meadow():
    global _meadow
    if _meadow is None:
        _meadow = []
        for a in C.actors.get_all_level_actors():
            if a.get_actor_label() == "KG_Meadow":
                _meadow += list(a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))
    return _meadow


def clear_grass_at(x, y, r):
    removed = 0
    for comp in meadow():
        hit = comp.get_instances_overlapping_sphere(unreal.Vector(x, y, g(x, y)), r, True)
        if hit:
            comp.remove_instances(list(hit))
            removed += len(hit)
    C.stats["grass_cleared"] = C.stats.get("grass_cleared", 0) + removed


def clear_grass_rect(f, pad=60.0, step=280.0):
    for lx in frange(-f.w * 0.5 - pad + step * 0.5, f.w * 0.5 + pad, step):
        for ly in frange(-f.d * 0.5 - pad + step * 0.5, f.d * 0.5 + pad, step):
            x, y = f.world(lx, ly)
            clear_grass_at(x, y, step * 0.72)


# ---------------------------------------------------------------------------------------------- spawning sugar
def put(path, x, y, z=None, yaw=0.0, pitch=0.0, roll=0.0, scale=1.0, sub="Props", collide=True, cull=9000.0,
        sink=0.0, claim_r=0.0, shadow=True):
    return C.place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=scale, sub=sub, collide=collide,
                   cull=cull, sink=sink, claim_r=claim_r, shadow=shadow)


def fput(f, path, lx, ly, dz=0.0, yaw=0.0, z=None, **kw):
    """Place in a frame's local coords; yaw is relative to the frame. z=None -> terrain + dz."""
    x, y = f.world(lx, ly)
    zz = (g(x, y) + dz) if z is None else z + dz
    return put(path, x, y, zz, yaw=f.yaw + yaw, **kw)


def small(path, x, y, z=None, yaw=None, scale=1.0, sub="Clutter", pitch=0.0, roll=0.0):
    """Small non-colliding clutter (instanced)."""
    zz = g(x, y) if z is None else z
    inst(path, x, y, zz, R.uniform(0, 360) if yaw is None else yaw, scale, pitch, roll, collide=False, cull=6000.0)


# ---------------------------------------------------------------------------------------------- crops
CROPS = {
    # name: (plant mesh list, row gap, plant gap, scale range, soil ridges?, sink)
    "carrot": ([P + "Carrot"], 70.0, 32.0, (1.1, 1.5), True, 0.0),
    "cabbage": ([N + "Plant_1"], 85.0, 62.0, (0.34, 0.44), True, 4.0),
    "corn": ([N + "Grass_Common_Tall", N + "Grass_Wispy_Tall"], 55.0, 50.0, (0.62, 0.8), True, 3.0),
    "sunflower": ([N + "Flower_4_Single", N + "Flower_3_Single"], 115.0, 75.0, (0.7, 0.95), True, 2.0),
    "beet": ([N + "Plant_7"], 75.0, 55.0, (0.5, 0.7), True, 0.0),
    "rapeseed": ([N + "Flower_4_Group", N + "Flower_4_Single"], 62.0, 58.0, (0.38, 0.52), False, 2.0),
    "poppy": ([N + "Flower_3_Group", N + "Flower_3_Single"], 60.0, 55.0, (0.36, 0.5), False, 2.0),
    "clover": ([N + "Clover_1", N + "Clover_2"], 70.0, 60.0, (0.5, 0.65), True, 3.0),
    "ploughed": ([], 80.0, 999.0, (1.0, 1.0), True, 0.0),
}
PLANT_CULL = {"carrot": 7000.0, "cabbage": 8000.0, "corn": 10000.0, "sunflower": 11000.0, "beet": 8000.0,
              "rapeseed": 11000.0, "poppy": 11000.0, "clover": 8000.0, "ploughed": 8000.0}
EDGE = [N + "Flower_4_Group", N + "Flower_3_Group", N + "Bush_Common_Flowers", N + "Clover_2", N + "Plant_1",
        N + "Fern_1"]


def field(xm, ym, wm, dm, yaw, crop, fence="wood", gate=(0, 0.0), name="", scarecrow=True):
    """A cultivated rectangle: cleared of trees + grass, soil ridges along local X, crop rows, fence or stone wall
    around it (gate = (side, local offset) gap; sides 0=-Y front, 1=+X, 2=+Y, 3=-X)."""
    f = mk(xm, ym, yaw, wm, dm, name)
    clear_nature(f, 220.0)
    clear_grass_rect(f, 80.0)
    meshes, row_gap, gap, sc, ridge, sink = CROPS[crop]
    ly = -f.d * 0.5 + row_gap * 0.5
    rows = 0
    while ly <= f.d * 0.5 - row_gap * 0.4:
        lx = -f.w * 0.5 + 40.0
        if ridge:
            rl = -f.w * 0.5 + 30.0
            while rl <= f.w * 0.5 - 30.0:
                x, y = f.world(rl, ly)
                if site_ok(x, y, 40.0):
                    inst(WP + "SM_KG_DigMound", x, y, g(x, y) - 7.0, f.yaw + R.uniform(-4, 4),
                         (0.95, min(0.62, row_gap / 170.0 + 0.1), 0.42), collide=False, cull=8000.0)
                rl += 100.0
        while lx <= f.w * 0.5 - 30.0:
            jx, jy = lx + R.uniform(-8, 8), ly + R.uniform(-6, 6)
            x, y = f.world(jx, jy)
            if meshes and site_ok(x, y, 25.0) and R.random() > 0.04:
                s = R.uniform(*sc)
                path = R.choice(meshes)
                inst(path, x, y, g(x, y) - sink, R.uniform(0, 360), (s, s, s * R.uniform(0.9, 1.1)),
                     collide=False, cull=PLANT_CULL[crop])
            lx += gap
        ly += row_gap
        rows += 1
    if fence == "wood":
        fence_rect(f, gate)
    elif fence == "stone":
        wall_rect(f, gate)
    if scarecrow and crop not in ("ploughed",):
        sx, sy = f.world(R.uniform(-0.2, 0.2) * f.w, R.uniform(-0.15, 0.15) * f.d)
        scarecrow_at(sx, sy, f.yaw + R.uniform(-30, 30))
    edge_dress(f, 1.0 if fence == "wood" else 0.5)
    claim_rect(f, 60.0)
    C.stats["fields"] = C.stats.get("fields", 0) + 1
    return f


def edge_dress(f, amount=1.0):
    """Life along the outside of a field: wildflower strips, a tool left against the fence, buckets, crates."""
    hw, hd = f.w * 0.5 + 150.0, f.d * 0.5 + 150.0
    perim = 2 * (f.w + f.d)
    n = int(perim / 160.0 * amount)
    for k in range(n):
        t = R.uniform(0, perim + 1200.0)
        if t < f.w + 300.0:
            lx, ly = -hw + t, -hd
        elif t < f.w + f.d + 600.0:
            lx, ly = hw, -hd + (t - f.w - 300.0)
        elif t < 2 * f.w + f.d + 900.0:
            lx, ly = hw - (t - f.w - f.d - 600.0), hd
        else:
            lx, ly = -hw, hd - (t - 2 * f.w - f.d - 900.0)
        x, y = f.world(lx + R.uniform(-40, 40), ly + R.uniform(-40, 40))
        if not site_ok(x, y, 40.0) or C.overlaps(x, y, 20.0):
            continue
        inst(R.choice(EDGE), x, y, g(x, y) - 6.0, R.uniform(0, 360), R.uniform(0.45, 0.75), collide=False,
             cull=9000.0)
    props = [(P + "Bucket_Wooden_1", 0.0), (P + "FarmCrate_Empty", 0.0), (WP + "SM_KG_Shovel", 3.0),
             (P + "Bag", 0.0), (P + "Bucket_Metal", 0.0), (P + "FarmCrate_Carrot", 0.0), (P + "Rope_1", 0.0)]
    for k in range(int(3 * amount + 0.5)):
        side = R.randint(0, 3)
        u = R.uniform(-0.4, 0.4)
        lx, ly = [(u * f.w, -hd + 40), (hw - 40, u * f.d), (u * f.w, hd - 40), (-hw + 40, u * f.d)][side]
        x, y = f.world(lx, ly)
        if not site_ok(x, y, 40.0) or C.overlaps(x, y, 30.0):
            continue
        pth, dz = R.choice(props)
        put(pth, x, y, g(x, y) + dz, yaw=R.uniform(0, 360), sub="Clutter", collide=False, cull=5500.0)
        C.claim(x, y, 40.0)


def scarecrow_at(x, y, yaw):
    """Straw training dummy on a pole = scarecrow, with a sack 'head' and a crow-scaring banner rag."""
    clear_grass_at(x, y, 120.0)
    put(P + "Dummy", x, y, g(x, y) - 4.0, yaw=yaw, pitch=R.uniform(-4, 4), roll=R.uniform(-5, 5), sub="Scarecrows",
        cull=12000.0, claim_r=60.0)
    put(P + "Bag", x, y, g(x, y) + 158.0, yaw=yaw + 20, scale=(0.55, 0.55, 0.45), sub="Scarecrows", collide=False,
        cull=9000.0)


# ---------------------------------------------------------------------------------------------- fences + walls
FENCE = V + "Prop_WoodenFence_Single"


def fence_line(ax, ay, bx, by, gaps=(), piece=205.0, check=True):
    """Wooden fence from A to B (cm), pieces follow the terrain slope. gaps: [(t0, t1)] fractions left open."""
    L = math.hypot(bx - ax, by - ay)
    n = max(1, int(round(L / piece)))
    dx, dy = (bx - ax) / L, (by - ay) / L
    yaw = math.degrees(math.atan2(dy, dx))
    step = L / n
    sc = step / 206.0
    placed = 0
    for i in range(n):
        t0, t1 = i / n, (i + 1) / n
        if any(t1 > a and t0 < b for a, b in gaps):
            continue
        cx, cy = ax + dx * (i + 0.5) * step, ay + dy * (i + 0.5) * step
        if check and not site_ok(cx, cy, 60.0, lane_gap=30.0):
            continue
        z0 = g(cx - dx * step * 0.5, cy - dy * step * 0.5)
        z1 = g(cx + dx * step * 0.5, cy + dy * step * 0.5)
        pitch = math.degrees(math.atan2(z1 - z0, step))
        inst(FENCE, cx, cy, min(z0, z1) + abs(z1 - z0) * 0.5 - 4.0, yaw + R.uniform(-1.5, 1.5), (sc, 1.0, 1.0),
             pitch, R.uniform(-2, 2), collide=True, cull=13000.0)
        C.claim(cx, cy, 70.0)
        placed += 1
    return placed


def rect_corners(f, pad=0.0):
    hw, hd = f.w * 0.5 + pad, f.d * 0.5 + pad
    return [f.world(-hw, -hd), f.world(hw, -hd), f.world(hw, hd), f.world(-hw, hd)]


def fence_rect(f, gate=(0, 0.0), pad=60.0, gate_w=260.0):
    c = rect_corners(f, pad)
    for side in range(4):
        (ax, ay), (bx, by) = c[side], c[(side + 1) % 4]
        L = math.hypot(bx - ax, by - ay)
        gaps = ()
        if side == gate[0]:
            mid = 0.5 + gate[1] / L
            gaps = ((mid - gate_w * 0.5 / L, mid + gate_w * 0.5 / L),)
        fence_line(ax, ay, bx, by, gaps)
    # gate posts
    if gate is not None:
        (ax, ay), (bx, by) = c[gate[0]], c[(gate[0] + 1) % 4]
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        mid = L * 0.5 + gate[1]
        for s in (-1, 1):
            px, py = ax + dx * (mid + s * gate_w * 0.5), ay + dy * (mid + s * gate_w * 0.5)
            if site_ok(px, py, 20.0, lane_gap=20.0):
                inst(V + "Corner_Exterior_Wood", px, py, g(px, py) - 10.0, R.uniform(0, 90), (1.1, 1.1, 0.45),
                     collide=True, cull=12000.0)


ROCKS = [N + "Rock_Medium_1", N + "Rock_Medium_2", N + "Rock_Medium_3"]


def wall_line(ax, ay, bx, by, gaps=(), check=True, height=1.0):
    """Dry-stone wall: overlapping squashed boulders, a capping course on top, moss bushes here and there."""
    L = math.hypot(bx - ax, by - ay)
    dx, dy = (bx - ax) / L, (by - ay) / L
    yaw = math.degrees(math.atan2(dy, dx))
    t = 0.0
    k = 0
    while t < L:
        f = t / L
        if not any(a < f < b for a, b in gaps):
            x, y = ax + dx * t + R.uniform(-8, 8), ay + dy * t + R.uniform(-8, 8)
            if not check or site_ok(x, y, 60.0, lane_gap=30.0):
                s = R.uniform(0.26, 0.32)
                inst(R.choice(ROCKS), x, y, g(x, y) - 22.0, yaw + R.uniform(-12, 12) + (180 if k % 2 else 0),
                     (s * 1.1, s * 0.75, s * height * R.uniform(0.95, 1.2)), collide=True, cull=14000.0)
                if k % 2 == 0:
                    s2 = R.uniform(0.15, 0.2)
                    inst(R.choice(ROCKS), x + dx * 30, y + dy * 30, g(x, y) + 30.0 * height, yaw + R.uniform(0, 360),
                         (s2, s2 * 0.8, s2 * 0.7), R.uniform(-8, 8), R.uniform(-8, 8), collide=True, cull=14000.0)
                if R.random() < 0.12:
                    side = R.choice((-1, 1))
                    inst(N + R.choice(["Bush_Common", "Fern_1", "Bush_Common_Flowers"]), x - dy * 70 * side,
                         y + dx * 70 * side, g(x, y) - 10.0, R.uniform(0, 360), R.uniform(0.45, 0.7), collide=False,
                         cull=9000.0)
                C.claim(x, y, 60.0)
        t += 78.0
        k += 1


def wall_rect(f, gate=(0, 0.0), pad=80.0, gate_w=300.0):
    c = rect_corners(f, pad)
    for side in range(4):
        (ax, ay), (bx, by) = c[side], c[(side + 1) % 4]
        L = math.hypot(bx - ax, by - ay)
        gaps = ()
        if gate is not None and side == gate[0]:
            mid = 0.5 + gate[1] / L
            gaps = ((mid - gate_w * 0.5 / L, mid + gate_w * 0.5 / L),)
        wall_line(ax, ay, bx, by, gaps)


def wall_path(pts_m, gaps_at=()):
    """Stone wall along a polyline in metres; gaps_at = [(segment index, fraction)] -> 3 m openings."""
    pts = [(x * M, y * M) for x, y in pts_m]
    for i, ((ax, ay), (bx, by)) in enumerate(zip(pts, pts[1:])):
        L = math.hypot(bx - ax, by - ay)
        gaps = [(fr - 150.0 / L, fr + 150.0 / L) for (si, fr) in gaps_at if si == i]
        wall_line(ax, ay, bx, by, gaps)


# ---------------------------------------------------------------------------------------------- composites
BALES = {   # candidate straw-bale looks (kit pieces squashed into 120 x 82 x 69 blocks): path, scale, z offset, y offset
    "plaster": (V + "Wall_Plaster_Straight", (0.6, 2.0, 0.22), 0.0, 21.0),
    "floor": (V + "Floor_WoodLight", (0.6, 0.41, 34.0), 34.0, 0.0),
    "brick": (V + "Wall_UnevenBrick_Straight", (0.6, 2.0, 0.22), 0.0, 21.0),
}
BALES["bag"] = (P + "Bag", (1.85, 1.3, 0.82), 0.0, 0.0)
BALES["bagside"] = (P + "Bag", (1.0, 1.35, 1.5), 33.0, 0.0)
BALE = "bagside"


def bale(x, y, z, yaw, kind=None):
    path, sc, dz, dy = BALES[kind or BALE]
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    inst(path, x - s_ * dy, y + c * dy, z + dz, yaw, sc, R.uniform(-1.5, 1.5), R.uniform(-1.5, 1.5), collide=True,
         cull=12000.0)


def petal_stack(x, y, s=1.0):
    """Round haystack: a dome of big golden leaf/straw cards (Petal meshes scaled up), crown on top."""
    clear_grass_at(x, y, 220.0 * s)
    z = C.ground_min(x, y, 100.0 * s) - 6.0
    for ring, (rr, n, sc, dz, tilt) in enumerate(((95.0, 7, 2.6, 0.0, 28.0), (55.0, 5, 2.4, 45.0, 22.0),
                                                   (0.0, 2, 2.2, 88.0, 8.0))):
        for k in range(n):
            a = k / n * 2 * math.pi + ring * 0.4 + R.uniform(-0.15, 0.15)
            px, py = x + rr * s * math.cos(a), y + rr * s * math.sin(a)
            inst(N + R.choice(["Petal_5", "Petal_2", "Petal_3"]), px, py, z + dz * s, math.degrees(a) + R.uniform(-20, 20),
                 (sc * s, sc * s, sc * s * 1.6), R.uniform(-tilt, tilt), R.uniform(-tilt, tilt), collide=False,
                 cull=12000.0)
    put(P + "Bag", x, y, z, scale=(2.2 * s, 2.2 * s, 1.1 * s), sub="Hay", cull=1.0, claim_r=170.0 * s)


def haystack(x, y, s=1.0, yaw=None, kind=None):
    """Stack of straw bales: 3-2-1 pyramid (s >= 1), 2-1 (s >= 0.85) or a couple of loose bales."""
    yaw = R.uniform(0, 360) if yaw is None else yaw
    clear_grass_at(x, y, 220.0)
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    z0 = C.ground_min(x, y, 130.0) - 4.0
    if s >= 1.0:
        layout = [(-125, 0, 0), (0, 0, 0), (125, 0, 0), (-62, 0, 1), (62, 0, 1), (0, 0, 2)]
    elif s >= 0.85:
        layout = [(-62, 0, 0), (62, 0, 0), (0, 0, 1)]
    else:
        layout = [(0, 0, 0), (40, 150, 0)]
    for lx, ly, lvl in layout:
        bx, by = x + lx * c - ly * s_, y + lx * s_ + ly * c
        kk = kind or BALE
        if kk == "bagside":
            path, sc, dz, dy = BALES[kk]
            inst(path, bx, by, z0 + lvl * 62.0 + dz, yaw + R.uniform(-5, 5), sc, 90.0, 0.0, collide=True, cull=12000.0)
        else:
            bale(bx, by, z0 + lvl * (60.0 if kk == "bag" else 68.0), yaw + R.uniform(-5, 5) + (70.0 if ly else 0.0), kk)
    C.claim(x, y, 200.0 * max(0.8, s))


def sacks(x, y, yaw, n=3):
    for k in range(n):
        a = math.radians(yaw) + k * 1.9
        sx, sy = x + 45 * math.cos(a) * (k > 0), y + 45 * math.sin(a) * (k > 0)
        put(P + "Bag", sx, sy, g(sx, sy) - 2.0, yaw=R.uniform(0, 360), scale=R.uniform(0.85, 1.05), sub="Clutter",
            collide=(k == 0), cull=6000.0)


def beehive(x, y, yaw):
    """Stacked box hive on a stool with a plank lid."""
    z = g(x, y)
    put(P + "Stool", x, y, z - 2.0, yaw=yaw, sub="Beehives", cull=7000.0, claim_r=50.0)
    for k in range(2):
        put(PIR + "crates_0", x, y, z + 55.0 + k * 33.0, yaw=yaw + R.uniform(-4, 4), scale=(0.72, 0.72, 0.46),
            sub="Beehives", cull=7000.0, collide=False)
    put(PIR + "Planks_0", x, y, z + 122.0, yaw=yaw + 90, scale=(0.38, 1.35, 1.0), sub="Beehives", cull=7000.0,
        collide=False)


def lantern_post(x, y, yaw, light=True, intensity=6.0):
    """Short wooden post with a hanging lantern (+ warm light)."""
    z = g(x, y)
    put(V + "Corner_Exterior_Wood", x, y, z - 10.0, yaw=yaw, scale=(1.2, 1.2, 0.72), sub="Lights", cull=10000.0,
        claim_r=30.0)
    put(P + "Lantern_Wall", x, y, z + 110.0, yaw=yaw, sub="Lights", collide=False, cull=9000.0)
    if light:
        c, s = math.cos(math.radians(yaw + 90)), math.sin(math.radians(yaw + 90))
        C.light(x + c * 90.0, y + s * 90.0, z + 180.0, intensity=intensity, radius=800.0)


# ---------------------------------------------------------------------------------------------- windmill
def dress_asset(*needles):
    """Path of a factory prop (under /Game/KillGodot/Env/Dress/) whose name contains all needles, else None."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in reg.get_assets_by_path(C.DRESS.rstrip("/"), recursive=True):
        name = str(a.asset_name)
        if all(n.lower() in name.lower() for n in needles) and str(a.asset_class_path.asset_name) == "StaticMesh":
            return f"{a.package_name}"
    return None


def windmill(xm, ym, face):
    """Windmill on the rise. Factory mesh (Windmill_Body + Windmill_Sails spinning on an AKGSpinner) when the village
    pack is imported: sails towards the village, door on the far side. Fallback: a tapered octagonal stone tower from
    kit walls with cloth sails, a ladder and a lookout floor. Then the miller's yard (sacks, cart, loot, bench)."""
    x0, y0 = xm * M, ym * M
    yaw = C.facing_yaw((xm, ym), face)   # frame -Y faces `face`
    f = C.Frame(x0, y0, yaw, 700.0, 700.0, "windmill")
    clear_nature(f, 500.0)
    clear_grass_at(x0, y0, 650.0)
    C.claim(x0, y0, 420.0)
    base = C.ground_min(x0, y0, 320.0) - 6.0
    body = dress_asset("Windmill", "Body")
    sails = dress_asset("Windmill", "Sail")
    C.stats["windmill"] = "factory" if (body and sails) else "kit"
    if body and sails:
        # Measured: body X -530..312 (door + stoop on -X), sails hub on +X at (237, 0, 867); sails mesh in YZ plane.
        yb = yaw - 90.0                     # body +X (sails) -> frame -Y (the village)
        put(body, x0, y0, base, yaw=yb, sub="Windmill", cull=0.0)
        hx, hy = f.world(0.0, -300.0)
        m = C.mover(sails, hx, hy, base + 867.0, yaw=yb, spin=(0.0, 0.0, 20.0), sub="Windmill")
        if m:
            m.set_actor_enable_collision(False)
        ds, door = 1.0, 530.0
        C.claim(*f.world(0.0, 400.0), 160.0)
        x, y = f.world(0.0, 260.0)
        C.light(x, y, base + 420.0, intensity=5.0, radius=700.0)   # door lamp glow (lamp arm on the mesh)
    else:
        ds, door = -1.0, 272.0
        _kit_windmill(f, base, yaw)
    # --- miller's yard (door side), relative to the door direction ds
    sacks(*f.world(-200.0, ds * 380.0), yaw=yaw)
    sacks(*f.world(-150.0, ds * 500.0), yaw=yaw + 60, n=4)
    cx, cy = f.world(210.0, ds * 360.0)
    C.loot_chest(cx, cy, g(cx, cy), yaw=yaw + (90.0 if ds > 0 else -90.0), table="Chest", name="Miller's Chest")
    C.claim(cx, cy, 80.0)
    x, y = f.world(-470.0, ds * 330.0)
    put(P + "Stall_Cart_Empty", x, y, g(x, y), yaw=yaw + 35.0, sub="Windmill", cull=12000.0, claim_r=180.0)
    for k in range(3):
        bx, by = f.world(-470.0 + (k - 1) * 55.0, ds * 330.0 + R.uniform(-20, 20))
        put(P + "Bag", bx, by, g(x, y) + 70.0, yaw=R.uniform(0, 360), scale=0.8, sub="Windmill", collide=False,
            cull=6000.0)
    for lx, ly in ((380.0, 150.0), (420.0, 60.0), (400.0, -30.0)):
        bx, by = f.world(lx, ds * ly)
        C.breakable(P + ("Barrel" if lx != 400.0 else "Crate_Wooden"), bx, by, yaw=R.uniform(0, 360))
        C.claim(bx, by, 50.0)
    lx, ly = f.world(260.0, ds * (door + 60.0))
    lantern_post(lx, ly, yaw + (0.0 if ds > 0 else 180.0), intensity=5.0)
    # bench under the sails, looking at the village
    bx, by = f.world(-330.0, -ds * 560.0)
    st = C.seat(P + "Bench", bx, by, g(bx, by), yaw=yaw - 90.0, seat_height=46.0)
    try:
        st.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        st.set_seat_mesh(C.mesh(P + "Bench"))
    except Exception:
        pass
    C.claim(bx, by, 150.0)
    # footpath round the mill to the door
    pts = [f.world(*p) for p in ((-60.0, -560.0), (430.0, -380.0), (560.0, 120.0), (300.0, ds * 560.0),
                                 (90.0, ds * 640.0))]
    stone_path([(px / M, py / M) for px, py in pts])
    C.ambient("A_Wind_Loop", x0, y0, base + 900.0, volume=0.7)
    return base, f, ds


def _kit_windmill(f, base, yaw):
    H = 312.0
    tapers = [1.0, 0.93, 0.86]
    for lvl, s in enumerate(tapers):
        ap = 241.4 * s
        z = base + lvl * H
        for k in range(8):
            th = -90.0 + k * 45.0   # k=0 -> local -Y (front)
            lx, ly = ap * math.cos(math.radians(th)), ap * math.sin(math.radians(th))
            if lvl == 0:
                piece = "Wall_UnevenBrick_Door_Round" if k == 0 else "Wall_UnevenBrick_Straight"
            elif lvl == 1:
                piece = "Wall_UnevenBrick_Window_Thin_Round" if k in (2, 6) else "Wall_UnevenBrick_Straight"
            else:
                piece = "Wall_UnevenBrick_Window_Thin_Round" if k % 2 == 0 else "Wall_UnevenBrick_Straight"
            fput(f, V + piece, lx, ly, z=z, yaw=th + 90.0, scale=(s, 1.0, 1.0), sub="Windmill", cull=0.0)
            vx, vy = (ap / math.cos(math.radians(22.5))) * math.cos(math.radians(th + 22.5)), \
                     (ap / math.cos(math.radians(22.5))) * math.sin(math.radians(th + 22.5))
            fput(f, V + "Corner_Exterior_Brick", vx, vy, z=z, yaw=th + 22.5 + 90.0, scale=(0.75 * s, 0.75 * s, 1.0),
                 sub="Windmill", cull=0.0)
    for i in (-100.0, 100.0):
        for j in (-100.0, 100.0):
            fput(f, V + "Floor_Brick", i * 0.72, j * 0.72, z=base + 2.0, scale=(0.72, 0.72, 1.0), sub="Windmill",
                 cull=0.0)
    zf = base + 2 * H + 2.0
    for i, j in ((-70.0, -70.0), (-70.0, 70.0), (70.0, 70.0)):
        fput(f, V + "Floor_WoodDark", i, j, z=zf, scale=(0.7, 0.7, 4.0), sub="Windmill", cull=0.0)
    for (i, j, sx, sy) in ((165.0, 0.0, 0.25, 0.8), (-165.0, 0.0, 0.25, 0.8), (0.0, 165.0, 0.8, 0.25),
                           (0.0, -165.0, 0.8, 0.25)):
        fput(f, V + "Floor_WoodDark", i, j, z=zf, scale=(sx, sy, 4.0), sub="Windmill", cull=0.0)
    wx, wy = f.world(70.0, -8.0)
    lad = C.spawn_class("/Script/KillGodot.KGLadder", wx, wy, base + 2.0, yaw + 90.0, sub="Windmill")
    if lad:
        lad.set_height(2 * H)
    c, s_ = math.cos(math.radians(yaw + 90.0)), math.sin(math.radians(yaw + 90.0))
    for side in (-28.0, 28.0):
        put(V + "Corner_Exterior_Wood", wx - s_ * side, wy + c * side, base + 2.0, yaw=yaw + 90.0,
            scale=(0.35, 0.35, 2 * H / 300.0 + 0.05), sub="Windmill", cull=9000.0)
    rung = 30.0
    while rung < 2 * H:
        inst(V + "Corner_Exterior_Wood", wx + s_ * 28.0, wy - c * 28.0, base + 2.0 + rung, yaw + 90.0,
             (0.25, 0.25, 0.19), 0.0, -90.0, collide=False, cull=9000.0)
        rung += 38.0
    fput(f, V + "Roof_Tower_RoundTiles", 0.0, 0.0, z=base + 3 * H, scale=(0.93, 0.93, 0.85), sub="Windmill", cull=0.0)
    hub_ly = -(241.4 * tapers[2] + 95.0)
    hx, hy = f.world(0.0, hub_ly)
    hz = base + 2 * H + 290.0
    fput(f, V + "Corner_Exterior_Wood", 0.0, hub_ly + 60.0, z=hz, yaw=0.0, roll=90.0, scale=(1.6, 1.6, 0.5),
         sub="Windmill", cull=0.0, collide=False)
    fput(f, P + "Barrel", 0.0, hub_ly - 30.0, z=hz, yaw=0.0, roll=90.0, scale=(0.6, 0.6, 0.45), sub="Windmill",
         cull=0.0, collide=False)
    for k in range(2):
        a = C.mover(PIR + "Planks_1", hx, hy, hz - 8.0, yaw=yaw, pitch=k * 90.0 + 45.0, scale=(5.6, 0.7, 1.6),
                    spin=(24.0, 0.0, 0.0), sub="Windmill")
        if a:
            a.set_actor_enable_collision(False)
    fx, fy = f.world(0.0, hub_ly - 22.0)
    for k in range(4):
        a = C.mover(P + "Banner_1_Cloth", fx, fy, hz, yaw=yaw, pitch=k * 90.0 + 45.0, scale=(2.3, 1.0, 2.75),
                    spin=(24.0, 0.0, 0.0), sub="Windmill")
        if a:
            a.set_actor_enable_collision(False)
    for lx, ly in ((-120.0, 90.0), (-60.0, 140.0)):
        x, y = f.world(lx, ly)
        put(P + "Bag", x, y, base + 2.0, yaw=R.uniform(0, 360), sub="Windmill", cull=6000.0, collide=False)


# ---------------------------------------------------------------------------------------------- vignettes
def animal_pen(xm, ym, yaw, wm, dm):
    """Fenced pen with a trough (an old rowboat), feed sacks, water buckets and a hay pile."""
    f = mk(xm, ym, yaw, wm, dm, "pen")
    clear_nature(f, 200.0)
    fence_rect(f, gate=(0, 200.0), pad=0.0)
    x, y = f.world(0.0, 60.0)
    put(WP + "SM_KG_Rowboat", x, y, g(x, y) - 18.0, yaw=f.yaw, scale=(0.62, 0.62, 0.55), sub="Pen", cull=10000.0,
        claim_r=120.0)
    for lx, ly, nm in ((-300.0, 120.0, "Bucket_Wooden_1"), (-250.0, 170.0, "Bucket_Metal"), (320.0, -150.0, "FarmCrate_Empty"),
                       (380.0, -80.0, "FarmCrate_Empty")):
        fput(f, P + nm, lx, ly, yaw=R.uniform(0, 360), sub="Pen", cull=6000.0)
    haystack(*f.world(-f.w * 0.5 + 170.0, -f.d * 0.5 + 170.0), s=0.75)
    sacks(*f.world(f.w * 0.5 - 120.0, f.d * 0.5 - 90.0), yaw=f.yaw)
    small(N + "Grass_Wispy_Short", *f.world(100.0, -100.0), scale=1.0)
    C.claim(f.x, f.y, 200.0)
    return f


def hen_house(xm, ym, yaw):
    """Little roofed coop: four posts, a wooden roof, stacked bird cages, feed crate and a nesting straw bed."""
    f = mk(xm, ym, yaw, 3.0, 2.2, "coop")
    clear_nature(f, 150.0)
    clear_grass_at(f.x, f.y, 250.0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            fput(f, V + "Corner_Exterior_Wood", sx * 105.0, sy * 70.0, dz=-8.0, scale=(1.0, 1.0, 0.62), sub="Coop",
                 cull=11000.0)
    zt = max(g(*f.world(sx * 105.0, sy * 70.0)) for sx in (-1, 1) for sy in (-1, 1))
    fput(f, V + "Roof_Wooden_2x1", 0.0, -80.0, z=zt + 168.0, scale=(1.1, 1.05, 0.8), sub="Coop", cull=11000.0)
    for k, (lx, ly, dz) in enumerate(((-55.0, 20.0, 0.0), (35.0, 25.0, 0.0), (-10.0, 20.0, 78.0))):
        fput(f, P + "Cage_Small", lx, ly, dz=dz, yaw=R.uniform(-10, 10), sub="Coop", cull=7000.0, collide=(dz == 0.0))
    fput(f, P + "FarmCrate_Empty", 70.0, -110.0, yaw=20.0, sub="Coop", cull=6000.0, collide=False)
    fput(f, P + "Bucket_Wooden_1", -90.0, -120.0, sub="Coop", cull=6000.0, collide=False)
    for k in range(3):
        x, y = f.world(R.uniform(-80, 80), R.uniform(-40, 40))
        small(N + "Grass_Wispy_Short", x, y, g(x, y) - 5.0, scale=0.7)
    C.claim(f.x, f.y, 190.0)


def rest_spot(xm, ym, yaw):
    """Farmer's rest under the shade tree: bench (sit), a barrel table with a mug, bread and cheese, stools,
    a lantern post, a pitchfork... and a barrel you can smash."""
    f = mk(xm, ym, yaw, 5.0, 4.0, "rest")
    clear_nature(f, 100.0)
    clear_grass_at(f.x, f.y, 300.0)
    tx, ty = f.world(-250.0, 150.0)
    put(N + "CommonTree_1", tx, ty, g(tx, ty) - 15.0, yaw=40.0, scale=1.15, sub="Rest", cull=0.0, claim_r=120.0)
    bx, by = f.world(0.0, 60.0)
    s = C.seat(P + "Bench", bx, by, g(bx, by), yaw=f.yaw - 90.0, seat_height=46.0)
    try:
        s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        s.set_seat_mesh(C.mesh(P + "Bench"))
    except Exception:
        pass
    C.claim(bx, by, 150.0)
    x, y = f.world(0.0, -70.0)
    put(P + "Barrel", x, y, g(x, y), yaw=10.0, sub="Rest", cull=8000.0, claim_r=45.0)
    z = g(x, y) + 90.0
    for nm, ox, oy in (("Mug", -12, 8), ("Mug", 14, -10), ("SM_KG_Bread", 5, 18)):
        pth = (FURN + "KG_InteriorProps/StaticMeshes/" + nm) if nm.startswith("SM_") else P + nm
        put(pth, x + ox, y + oy, z, yaw=R.uniform(0, 360), sub="Rest", collide=False, cull=4000.0)
    put(FURN + "KG_InteriorProps/StaticMeshes/SM_KG_Cheese", x - 10, y - 16, z, yaw=30.0, sub="Rest", collide=False,
        cull=4000.0)
    for lx, ly, yw in ((-110.0, -90.0, 90.0), (110.0, -110.0, -90.0)):
        sx, sy = f.world(lx, ly)
        C.seat(P + "Stool", sx, sy, g(sx, sy), yaw=f.yaw + yw + R.uniform(-15, 15), seat_height=57.0)
        C.claim(sx, sy, 40.0)
    lx, ly = f.world(210.0, 40.0)
    lantern_post(lx, ly, f.yaw - 90.0, intensity=6.0)
    x, y = f.world(-200.0, -40.0)
    C.breakable(P + "Barrel", x, y, yaw=R.uniform(0, 360))
    C.claim(x, y, 50.0)
    x, y = f.world(-170.0, 60.0)
    put(P + "Pickaxe_Bronze", x, y, g(x, y) + 58.0, yaw=f.yaw, pitch=0.0, roll=12.0, sub="Rest", collide=False,
        cull=5000.0)
    put(WP + "SM_KG_Shovel", *f.world(150.0, 150.0), z=g(*f.world(150.0, 150.0)) + 3.0, yaw=f.yaw + 30.0, sub="Rest",
        collide=False, cull=5000.0)


def barn_yard():
    """North-east side of the mill barn: haystacks, crates, feed sacks, loot chest, smashables, a lantern."""
    barn = [b for b in C.buildings() if b.name == "mill_barn"][0]
    # local -X of the barn frame faces north-east (towards the pumpkin field)
    spots = []
    x, y = barn.world(-560.0, 150.0)
    C.loot_chest(x, y, g(x, y), yaw=barn.yaw + 90.0, table="Chest", name="Farmer's Chest")
    C.claim(x, y, 80.0)
    for k, (lx, ly) in enumerate(((-540.0, -60.0), (-560.0, -150.0), (-640.0, -110.0), (-540.0, -105.0))):
        bx, by = barn.world(lx, ly)
        z = g(bx, by) + (95.0 if k == 3 else 5.0)
        C.breakable(P + ("Crate_Wooden" if k != 1 else "Barrel"), bx, by, z, yaw=R.uniform(0, 360))
        C.claim(bx, by, 55.0)
    haystack(*barn.world(-780.0, 320.0), s=1.0)
    haystack(*barn.world(-900.0, 80.0), s=0.8)
    x, y = barn.world(-560.0, 330.0)
    put(P + "Barrel_Holder", x, y, g(x, y), yaw=barn.yaw + 90.0, sub="BarnYard", cull=9000.0, claim_r=80.0)
    sacks(*barn.world(-520.0, -260.0), yaw=barn.yaw)
    for lx, ly, nm in ((-520.0, 260.0, "Bucket_Wooden_1"), (-600.0, -330.0, "FarmCrate_Carrot"),
                       (-660.0, -300.0, "FarmCrate_Apple"), (-500.0, -330.0, "Rope_2")):
        bx, by = barn.world(lx, ly)
        put(P + nm, bx, by, g(bx, by), yaw=R.uniform(0, 360), sub="BarnYard", collide=False, cull=6000.0)
    lx, ly = barn.world(-470.0, 0.0)
    lantern_post(lx, ly, barn.yaw + 90.0, intensity=6.0)
    # the cart with a pile of sacks, waiting to go to the windmill
    x, y = barn.world(-900.0, -300.0)
    if site_ok(x, y, 150.0):
        put(V + "Prop_Wagon", x, y, g(x, y), yaw=barn.yaw + 20.0, sub="BarnYard", cull=14000.0, claim_r=200.0)


def lean_ladder(x, y, yaw, h=300.0, lean=16.0):
    """Decorative wooden ladder leaning towards `yaw` (against a tree or a wall)."""
    sl, cl = math.sin(math.radians(lean)), math.cos(math.radians(lean))
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    z0 = g(x, y) - 4.0
    for side in (-28.0, 28.0):
        inst(V + "Corner_Exterior_Wood", x - s_ * side, y + c * side, z0, yaw, (0.3, 0.3, h / 300.0), -lean, 0.0,
             collide=False, cull=7000.0)
    rung = 30.0
    while rung < h - 10.0:
        rx, ry = x + c * rung * sl, y + s_ * rung * sl
        inst(V + "Corner_Exterior_Wood", rx + s_ * 28.0, ry - c * 28.0, z0 + rung * cl, yaw, (0.22, 0.22, 0.19), 0.0,
             -90.0, collide=False, cull=7000.0)
        rung += 38.0


def orchard(xm, ym, yaw, cols, rows, sx=450.0, sy=420.0, seat=True):
    """Mown orchard: rows of low round fruit trees, apple barrels and crates, picking ladders, baskets, a bench."""
    f = mk(xm, ym, yaw, cols * sx / M, rows * sy / M, "orchard")
    clear_nature(f, 250.0)
    clear_grass_rect(f, 60.0)
    trees = [N + f"CommonTree_{i}" for i in (1, 2, 5)]
    spots = []
    for c in range(cols):
        for r in range(rows):
            lx = -f.w * 0.5 + sx * (c + 0.5) + R.uniform(-25, 25)
            ly = -f.d * 0.5 + sy * (r + 0.5) + R.uniform(-25, 25)
            x, y = f.world(lx, ly)
            if not site_ok(x, y, 120.0):
                continue
            s = R.uniform(0.4, 0.5)
            inst(R.choice(trees), x, y, g(x, y) - 12.0, R.uniform(0, 360), (s * 1.1, s * 1.1, s * R.uniform(0.8, 0.9)),
                 collide=True, cull=22000.0)
            C.claim(x, y, 80.0)
            spots.append((x, y, lx, ly))
            for q in range(3):   # windfalls
                small(N + "Petal_" + str(R.randint(1, 5)), x + R.uniform(-110, 110), y + R.uniform(-110, 110),
                      scale=0.7)
    # between the rows: barrels of apples, stacked crates, baskets, ladders against trees
    for k, (x, y, lx, ly) in enumerate(spots):
        mode = k % 4
        bx, by = f.world(lx + sx * 0.5, ly)
        if mode == 0 and site_ok(bx, by, 50.0) and not C.overlaps(bx, by, 50.0):
            put(P + "Barrel_Apples", bx, by, g(bx, by), yaw=R.uniform(0, 360), sub="Orchard", cull=8000.0,
                claim_r=45.0)
            for q in range(2):
                ax, ay = bx + R.uniform(-70, 70), by + R.uniform(60, 90)
                put(P + "FarmCrate_Apple", ax, ay, g(ax, ay) + 1.0 + q * 23.0, yaw=R.uniform(0, 360), sub="Orchard",
                    collide=False, cull=6000.0)
        elif mode == 1:
            a = R.uniform(0, 360)
            lxx, lyy = x + 95.0 * math.cos(math.radians(a)), y + 95.0 * math.sin(math.radians(a))
            if site_ok(lxx, lyy, 30.0):
                lean_ladder(lxx, lyy, a + 180.0, h=R.uniform(270.0, 330.0))
        elif mode == 2 and site_ok(bx, by, 50.0) and not C.overlaps(bx, by, 50.0):
            for q in range(3):
                put(P + "FarmCrate_Empty" if q == 2 else P + "FarmCrate_Apple", bx + (q % 2) * 20.0, by + q * 8.0,
                    g(bx, by) + 1.0 + q * 23.0, yaw=R.uniform(-15, 15) + f.yaw, sub="Orchard", collide=False,
                    cull=6000.0)
            put(P + "Bag", bx + 60.0, by - 50.0, g(bx, by), yaw=R.uniform(0, 360), scale=0.8, sub="Orchard",
                collide=False, cull=6000.0)
            C.claim(bx, by, 60.0)
    if seat and spots:
        x, y, lx, ly = spots[len(spots) // 2]
        bx, by = f.world(lx, ly - 130.0)
        if site_ok(bx, by, 60.0):
            st = C.seat(P + "Bench", bx, by, g(bx, by), yaw=f.yaw - 90.0, seat_height=46.0)
            try:
                st.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
                st.set_seat_mesh(C.mesh(P + "Bench"))
            except Exception:
                pass
            C.claim(bx, by, 150.0)
    return f


def cherry_row(pts_m):
    for (xm, ym) in pts_m:
        x, y = xm * M, ym * M
        clear_nature_circle(x, y, 300.0)
        if site_ok(x, y, 150.0):
            s = R.uniform(0.42, 0.52)
            inst(JP + R.choice(["Sakura_A", "Sakura_B"]), x, y, g(x, y) - 15.0, R.uniform(0, 360), s, collide=True,
                 cull=22000.0)
            C.claim(x, y, 110.0)


def apiary(xm, ym, yaw, n=5):
    """Beekeeper's corner: a row of hives, flower beds, a table with honey jars and a smoker pot."""
    f = mk(xm, ym, yaw, n * 1.3, 3.0, "apiary")
    clear_nature(f, 200.0)
    clear_grass_rect(f, 40.0)
    for k in range(n):
        x, y = f.world(-f.w * 0.5 + 65.0 + k * 130.0, 60.0)
        beehive(x, y, f.yaw + R.uniform(-8, 8))
    for k in range(9):
        x, y = f.world(R.uniform(-f.w * 0.5, f.w * 0.5), R.uniform(-160.0, -60.0))
        inst(N + R.choice(["Flower_3_Group", "Flower_4_Group", "Bush_Common_Flowers"]), x, y, g(x, y) - 5.0,
             R.uniform(0, 360), R.uniform(0.5, 0.8), collide=False, cull=9000.0)
    x, y = f.world(f.w * 0.5 + 120.0, 0.0)
    put(FURN + "KG_Kitchen/StaticMeshes/Kitchen_Square_Table", x, y, g(x, y), yaw=f.yaw, sub="Apiary", cull=8000.0,
        claim_r=70.0)
    z = g(x, y) + 67.0
    for k in range(5):
        put(FURN + "KG_Kitchen/StaticMeshes/Kitchen_YellowJar", x - 25.0 + (k % 3) * 22.0, y - 12.0 + (k // 3) * 24.0, z,
            yaw=R.uniform(0, 360), sub="Apiary", collide=False, cull=3500.0)
    put(P + "Pot_1_Lid", x + 20.0, y + 18.0, z, sub="Apiary", collide=False, cull=3500.0)
    C.claim(f.x, f.y, 250.0)


def shepherd_fold(xm, ym, r=520.0):
    """Circular dry-stone sheepfold with an opening, a campfire, stools, a cauldron and the shepherd's bedroll."""
    x0, y0 = xm * M, ym * M
    clear_nature_circle(x0, y0, r + 250.0)
    clear_grass_at(x0, y0, r * 0.5)
    n = int(2 * math.pi * r / 78.0)
    pts = []
    for k in range(n + 1):
        a = k / n * 2 * math.pi
        pts.append((x0 + r * math.cos(a), y0 + r * math.sin(a)))
    for k, ((ax, ay), (bx, by)) in enumerate(zip(pts, pts[1:])):
        a = (k + 0.5) / n * 360.0
        if 60.0 < a < 90.0:   # opening towards the village (north = +... y is -north here: opening faces -Y? no: +Y)
            continue
        wall_line(ax, ay, bx, by, check=False)
    # campfire
    fx, fy = x0 + 80.0, y0 - 60.0
    for k in range(3):
        put(FURN + "KG_InteriorProps/StaticMeshes/SM_KG_Firewood", fx, fy, g(fx, fy) - 4.0, yaw=k * 60.0,
            scale=0.9, sub="Fold", collide=False, cull=7000.0)
    for k in range(8):
        a = k * 0.785
        small(N + "Pebble_Round_" + str(k % 5 + 1), fx + 55 * math.cos(a), fy + 55 * math.sin(a), g(fx, fy) - 2.0,
              scale=1.1)
    C.light(fx, fy, g(fx, fy) + 80.0, intensity=12.0, radius=900.0, color=(255, 140, 60))
    C.ambient("A_Fire_Loop", fx, fy, g(fx, fy) + 50.0, volume=0.6)
    put(P + "Cauldron", fx + 130.0, fy + 40.0, g(fx + 130.0, fy + 40.0), sub="Fold", cull=7000.0, claim_r=60.0)
    for a in (2.2, 3.4, 4.6):
        sx, sy = fx + 170.0 * math.cos(a), fy + 170.0 * math.sin(a)
        C.seat(P + "Stool", sx, sy, g(sx, sy), yaw=math.degrees(a) + 180.0, seat_height=57.0)
        C.claim(sx, sy, 40.0)
    rx, ry = x0 - 220.0, y0 + 150.0
    put(FURN + "KG_InteriorProps/StaticMeshes/SM_KG_Rug_Runner", rx, ry, g(rx, ry) + 2.0, yaw=70.0, sub="Fold",
        collide=False, cull=6000.0)
    put(P + "Bag", rx - 60.0, ry - 110.0, g(rx, ry), yaw=10.0, sub="Fold", collide=False, cull=6000.0)
    put(P + "Bucket_Wooden_1", x0 + 250.0, y0 + 200.0, g(x0 + 250.0, y0 + 200.0), sub="Fold", collide=False,
        cull=6000.0)
    haystack(x0 - 200.0, y0 - 260.0, s=0.8)
    C.loot_chest(x0 - 330.0, y0 - 30.0, yaw=80.0, table="Chest", name="Shepherd's Chest")
    C.claim(x0, y0, r + 60.0)


def woodpile(xm, ym, yaw):
    f = mk(xm, ym, yaw, 3.0, 2.0, "woodpile")
    clear_nature(f, 100.0)
    clear_grass_at(f.x, f.y, 250.0)
    for k in range(3):
        for j in range(3 - k):
            fput(f, V + "Roof_Log", 0.0, -40.0 + j * 36.0 + k * 18.0, z=g(f.x, f.y) - 385.0 * 0.28 + 8.0 + k * 34.0,
                 yaw=90.0, scale=(0.28, 0.2, 0.28), sub="Woodpile", cull=9000.0)
    fput(f, P + "Anvil_Log", 170.0, -60.0, sub="Woodpile", cull=8000.0)
    fput(f, P + "Axe_Bronze", 175.0, -55.0, dz=100.0, roll=70.0, sub="Woodpile", collide=False, cull=5000.0)
    fput(f, FURN + "KG_InteriorProps/StaticMeshes/SM_KG_Firewood", 120.0, 70.0, yaw=30.0, sub="Woodpile",
         collide=False, cull=6000.0)
    C.claim(f.x, f.y, 200.0)


def harvest_cart(xm, ym, yaw):
    """Hand cart loaded with carrot crates and sacks at the field edge; spilled carrots in the grass."""
    x, y = xm * M, ym * M
    clear_nature_circle(x, y, 300.0)
    clear_grass_at(x, y, 220.0)
    put(P + "Stall_Cart_Empty", x, y, g(x, y), yaw=yaw, sub="Vignettes", cull=12000.0, claim_r=170.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for k in range(3):
        ox = -120.0 + k * 75.0
        put(P + "FarmCrate_Carrot", x + c * ox, y + s * ox, g(x, y) + 72.0, yaw=yaw + R.uniform(-8, 8),
            sub="Vignettes", collide=False, cull=7000.0)
    put(P + "FarmCrate_Carrot", x + c * -80.0, y + s * -80.0, g(x, y) + 113.0, yaw=yaw + 12.0, sub="Vignettes",
        collide=False, cull=7000.0)
    for k in range(6):
        cx, cy = x - s * 150.0 + R.uniform(-60, 60), y + c * 150.0 + R.uniform(-60, 60)
        small(P + "Carrot", cx, cy, g(cx, cy) + 6.0, scale=1.0, pitch=90.0)


def picnic(xm, ym, yaw):
    """Field workers' lunch on a blanket at the wheat's edge."""
    x, y = xm * M, ym * M
    clear_nature_circle(x, y, 250.0)
    clear_grass_at(x, y, 200.0)
    put(FURN + "KG_InteriorProps/StaticMeshes/SM_KG_Rug_Oval", x, y, g(x, y) + 2.0, yaw=yaw, sub="Picnic",
        collide=False, cull=6000.0)
    z = g(x, y) + 3.0
    for nm, ox, oy in (("SM_KG_Bread", -30, 10), ("SM_KG_Cheese", 20, -20), ("SM_KG_Bread", 40, 25)):
        put(FURN + "KG_InteriorProps/StaticMeshes/" + nm, x + ox, y + oy, z, yaw=R.uniform(0, 360), sub="Picnic",
            collide=False, cull=3500.0)
    for ox, oy in ((-10, -35), (55, -5)):
        put(P + "Mug", x + ox, y + oy, z, yaw=R.uniform(0, 360), sub="Picnic", collide=False, cull=3500.0)
    put(FURN + "KG_Kitchen/StaticMeshes/Kitchen_Bottle", x - 60, y - 20, z, sub="Picnic", collide=False, cull=3500.0)
    put(P + "Bag", x + 110, y + 40, z, yaw=40.0, scale=0.7, sub="Picnic", collide=False, cull=5000.0)
    put(WP + "SM_KG_Shovel", x - 140, y + 50, z + 2.0, yaw=yaw + 80, sub="Picnic", collide=False, cull=5000.0)
    C.claim(x, y, 150.0)


def broken_wagon(xm, ym, yaw):
    """A wagon slumped on a broken wheel, barrels rolled off, carrots spilled: somebody left in a hurry."""
    x, y = xm * M, ym * M
    clear_nature_circle(x, y, 400.0)
    clear_grass_at(x, y, 280.0)
    put(V + "Prop_Wagon", x, y, g(x, y) - 18.0, yaw=yaw, roll=7.0, pitch=-3.0, sub="Vignettes", cull=14000.0,
        claim_r=230.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for k, (ox, oy) in enumerate(((150.0, 170.0), (220.0, 90.0))):
        bx, by = x + c * ox - s * oy, y + s * ox + c * oy
        put(P + "Barrel", bx, by, g(bx, by) + 34.0, yaw=R.uniform(0, 360), roll=90.0, sub="Vignettes", cull=8000.0,
            claim_r=50.0)
    for k in range(7):
        cx, cy = x + c * 200.0 + R.uniform(-90, 90), y + s * 200.0 + R.uniform(-90, 90)
        small(P + "Carrot", cx, cy, g(cx, cy) + 6.0, scale=1.0, pitch=90.0)
    bx, by = x - c * 60.0 + s * 160.0, y - s * 60.0 - c * 160.0
    C.breakable(P + "Crate_Wooden", bx, by, yaw=R.uniform(0, 360))
    C.claim(bx, by, 60.0)


def stone_path(pts_m, width=110.0, step=95.0):
    """Stepping-stone footpath between points of interest (flat stones, no collision)."""
    pts = [(x * M, y * M) for x, y in pts_m]
    stones = [N + "RockPath_Round_Small_1", N + "RockPath_Round_Small_2", N + "RockPath_Round_Small_3",
              N + "RockPath_Square_Small_1", N + "RockPath_Square_Small_2"]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        t = 0.0
        while t < L:
            off = R.uniform(-width * 0.3, width * 0.3)
            x, y = ax + dx * t - dy * off, ay + dy * t + dx * off
            if site_ok(x, y, 20.0, lane_gap=0.0, zone=True) and not C.building_hit(x, y, 60.0):
                inst(R.choice(stones), x, y, g(x, y) - 7.0, R.uniform(0, 360), R.uniform(0.55, 0.75), collide=False,
                     cull=8000.0)
                clear_grass_at(x, y, 70.0)
            t += step * R.uniform(0.85, 1.15)


def field_shed(xm, ym, yaw):
    """Open field shelter: four posts under a tiled roof, workbench, tools, sacks, barrels (smashable), lantern."""
    f = mk(xm, ym, yaw, 4.0, 3.0, "shed")
    clear_nature(f, 200.0)
    clear_grass_at(f.x, f.y, 330.0)
    z0 = max(g(*f.world(sx * 190.0, sy * 140.0)) for sx in (-1, 1) for sy in (-1, 1))
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = f.world(sx * 190.0, sy * 140.0)
            put(V + "Corner_Exterior_Wood", x, y, g(x, y) - 10.0, yaw=f.yaw, scale=(1.3, 1.3, (z0 - g(x, y) + 290.0) / 300.0),
                sub="Shed", cull=12000.0)
    fput(f, V + "Roof_RoundTiles_4x4", 0.0, 0.0, z=z0 + 270.0, scale=(0.82, 0.62, 0.6), sub="Shed", cull=14000.0)
    for i in (-100.0, 100.0):
        fput(f, V + "Floor_WoodDark", i, 0.0, z=g(f.x, f.y) + 1.0, scale=(1.0, 1.4, 1.0), sub="Shed", collide=False,
             cull=8000.0)
    fput(f, P + "Workbench", 0.0, 90.0, yaw=180.0, sub="Shed", cull=9000.0)
    for nm, lx, ly, dz, rl in (("Axe_Bronze", -40.0, 90.0, 118.0, 70.0), ("Pickaxe_Bronze", 45.0, 95.0, 128.0, 60.0)):
        fput(f, P + nm, lx, ly, dz=dz, roll=rl, sub="Shed", collide=False, cull=4500.0)
    fput(f, P + "Peg_Rack", 0.0, 135.0, dz=160.0, sub="Shed", collide=False, cull=5000.0)
    sacks(*f.world(-140.0, -40.0), yaw=f.yaw, n=4)
    x, y = f.world(150.0, -50.0)
    C.breakable(P + "Barrel", x, y, yaw=R.uniform(0, 360))
    x, y = f.world(150.0, 40.0)
    C.breakable(P + "Crate_Wooden", x, y, yaw=R.uniform(0, 360))
    fput(f, WP + "SM_KG_Shovel", -100.0, -120.0, dz=3.0, yaw=35.0, sub="Shed", collide=False, cull=5000.0)
    fput(f, P + "Bucket_Metal", 60.0, -110.0, sub="Shed", collide=False, cull=5000.0)
    x, y = f.world(-190.0, -160.0)
    lantern_post(x, y, f.yaw + 180.0, intensity=5.0)
    C.claim(f.x, f.y, 300.0)


def produce_stand(xm, ym, yaw):
    """Roadside farm stand on the track: stall with crates of carrots and apples, apple barrel, sacks, a stool."""
    f = mk(xm, ym, yaw, 3.0, 2.0, "stand")
    clear_nature(f, 200.0)
    clear_grass_at(f.x, f.y, 280.0)
    fput(f, P + "Stall_Empty", 0.0, 0.0, sub="Stand", cull=12000.0)
    z = g(f.x, f.y)
    for k, nm in enumerate(("FarmCrate_Carrot", "FarmCrate_Apple", "FarmCrate_Carrot")):
        fput(f, P + nm, -55.0 + k * 55.0, -10.0, z=z, dz=82.0, yaw=R.uniform(-6, 6), sub="Stand", collide=False,
             cull=6000.0)
    fput(f, P + "Barrel_Apples", 140.0, -40.0, sub="Stand", cull=8000.0)
    fput(f, P + "FarmCrate_Apple", -140.0, -50.0, yaw=20.0, sub="Stand", collide=False, cull=6000.0)
    fput(f, P + "FarmCrate_Apple", -150.0, -45.0, dz=23.0, yaw=35.0, sub="Stand", collide=False, cull=6000.0)
    sacks(*f.world(-170.0, 60.0), yaw=f.yaw, n=3)
    x, y = f.world(60.0, 110.0)
    C.seat(P + "Stool", x, y, g(x, y), yaw=f.yaw - 90.0, seat_height=57.0)
    x, y = f.world(190.0, 70.0)
    lantern_post(x, y, f.yaw, intensity=5.0)
    C.claim(f.x, f.y, 260.0)


def windmill_skirt(xm, ym, r=345.0, skip_dir=None):
    """Stones and flowers around the windmill foot so it sits into the hill (skip_dir: world yaw of the door)."""
    x0, y0 = xm * M, ym * M
    for k in range(22):
        a = k / 22.0 * 2 * math.pi + R.uniform(-0.08, 0.08)
        if skip_dir is not None and abs((math.degrees(a) - skip_dir + 180.0) % 360.0 - 180.0) < 42.0:
            continue
        rr = r + R.uniform(-20, 40)
        x, y = x0 + rr * math.cos(a), y0 + rr * math.sin(a)
        s = R.uniform(0.12, 0.2)
        inst(R.choice(ROCKS), x, y, g(x, y) - 18.0, R.uniform(0, 360), (s, s, s * 0.8), collide=False, cull=12000.0)
        if k % 3 == 0:
            inst(N + R.choice(["Flower_4_Group", "Bush_Common_Flowers", "Flower_3_Group"]), x + 60 * math.cos(a),
                 y + 60 * math.sin(a), g(x, y) - 6.0, R.uniform(0, 360), R.uniform(0.45, 0.7), collide=False,
                 cull=9000.0)


def field_well(xm, ym):
    """Stone field well: a ring of stacked stones round a dark shaft, two posts, a crossbar, a little roof,
    a bucket on the rim, a rope, a water barrel and a watering bucket."""
    x0, y0 = xm * M, ym * M
    clear_nature_circle(x0, y0, 480.0)
    clear_grass_at(x0, y0, 300.0)
    z0 = C.ground_min(x0, y0, 90.0)
    put(WP + "SM_KG_DugHole", x0, y0, z0 + 12.0, sub="Well", collide=False, cull=8000.0)
    for lvl in range(2):
        for k in range(11):
            a = k / 11.0 * 2 * math.pi + lvl * 0.28
            s = R.uniform(0.12, 0.14)
            inst(R.choice(ROCKS), x0 + 78.0 * math.cos(a), y0 + 78.0 * math.sin(a), z0 - 12.0 + lvl * 32.0,
                 math.degrees(a) + 90.0, (s, s * 0.9, s * 0.75), collide=True, cull=14000.0)
    for sx in (-1, 1):
        put(V + "Corner_Exterior_Wood", x0 + sx * 88.0, y0, z0, scale=(1.0, 1.0, 0.75), sub="Well", cull=10000.0)
    put(V + "Corner_Exterior_Wood", x0 - 100.0, y0, z0 + 205.0, pitch=-90.0, scale=(0.9, 0.9, 0.67), sub="Well",
        cull=10000.0, collide=False)
    put(V + "Roof_Wooden_2x1", x0, y0 + 70.0, z0 + 215.0, sub="Well", cull=10000.0)
    put(P + "Bucket_Wooden_1", x0 + 35.0, y0 - 80.0, z0 + 60.0, sub="Well", collide=False, cull=6000.0)
    put(P + "Rope_1", x0 - 20.0, y0 + 10.0, z0 + 198.0, sub="Well", collide=False, cull=5000.0)
    put(P + "Bucket_Metal", x0 + 150.0, y0 - 60.0, g(x0 + 150.0, y0 - 60.0), sub="Well", collide=False, cull=6000.0)
    put(P + "Barrel_Holder", x0 - 160.0, y0 - 90.0, g(x0 - 160.0, y0 - 90.0), yaw=30.0, sub="Well", cull=9000.0)
    C.claim(x0, y0, 220.0)


def drying_rack(xm, ym, yaw):
    """Herb drying rack: two posts with a crossbar hung with herb racks and bundles, a crate and a sack."""
    f = mk(xm, ym, yaw, 2.4, 0.6, "rack")
    clear_nature(f, 100.0)
    clear_grass_at(f.x, f.y, 200.0)
    for sx in (-1, 1):
        fput(f, V + "Corner_Exterior_Wood", sx * 115.0, 0.0, dz=-8.0, scale=(0.9, 0.9, 0.62), sub="Rack", cull=9000.0)
    fput(f, V + "Corner_Exterior_Wood", -125.0, 0.0, dz=170.0, pitch=-90.0, scale=(0.7, 0.7, 0.84), sub="Rack",
         collide=False, cull=9000.0)
    for k in (-1, 1):
        fput(f, FURN + "KG_InteriorProps/StaticMeshes/SM_KG_HerbRack", k * 55.0, 0.0, dz=165.0, sub="Rack",
             collide=False, cull=6000.0)
    for k in range(5):
        fput(f, FURN + "KG_InteriorProps/StaticMeshes/SM_KG_HerbBundle", -90.0 + k * 45.0, 8.0, dz=168.0,
             sub="Rack", collide=False, cull=5000.0)
    fput(f, P + "FarmCrate_Empty", 60.0, -60.0, yaw=15.0, sub="Rack", collide=False, cull=6000.0)
    fput(f, P + "Bag", -70.0, -70.0, yaw=40.0, scale=0.8, sub="Rack", collide=False, cull=6000.0)
    C.claim(f.x, f.y, 150.0)


def crate_stack(xm, ym, yaw, smash=True):
    """Pile of empty farm crates + a smashable crate and sacks: a work spot at a field edge."""
    x, y = xm * M, ym * M
    if not C.free(x, y, 120.0):
        C.stats["skipped"] = C.stats.get("skipped", 0) + 1
        return
    clear_grass_at(x, y, 180.0)
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for q, (ox, oy, dz) in enumerate(((0, 0, 0), (75, 0, 0), (35, 0, 24), (10, 50, 0))):
        put(P + ("FarmCrate_Empty" if q != 1 else "FarmCrate_Carrot"), x + ox * c - oy * s_, y + ox * s_ + oy * c,
            g(x, y) + 1.0 + dz, yaw=yaw + R.uniform(-10, 10), sub="Clutter", collide=False, cull=6000.0)
    if smash:
        C.breakable(P + "Crate_Wooden", x - 90.0 * c, y - 90.0 * s_, yaw=R.uniform(0, 360))
    sacks(x + 40.0 * s_, y - 40.0 * c, yaw)
    C.claim(x, y, 140.0)


def hedgerow(pts_m):
    """Line of bushes and flowering shrubs with a young tree now and then (soft, no collision)."""
    pts = [(x * M, y * M) for x, y in pts_m]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        dx, dy = (bx - ax) / L, (by - ay) / L
        t = 0.0
        while t < L:
            x, y = ax + dx * t + R.uniform(-30, 30), ay + dy * t + R.uniform(-30, 30)
            if site_ok(x, y, 60.0) and not C.overlaps(x, y, 50.0):
                if R.random() < 0.1:
                    s = R.uniform(0.35, 0.45)
                    inst(N + "CommonTree_" + str(R.choice((1, 2, 5))), x, y, g(x, y) - 12.0, R.uniform(0, 360), s,
                         collide=True, cull=22000.0)
                else:
                    inst(N + R.choice(["Bush_Common", "Bush_Common", "Bush_Common_Flowers", "Plant_1_Big"]), x, y,
                         g(x, y) - 10.0, R.uniform(0, 360), R.uniform(0.6, 0.9), collide=False, cull=11000.0)
                C.claim(x, y, 70.0)
            t += R.uniform(110.0, 160.0)


def lineup():
    """TEMP: bale look-test (removed once the look is chosen)."""
    x, y = 3000.0, -9000.0
    for k, kind in enumerate(("petal", "bagside")):
        px = x + k * 450.0
        clear_grass_at(px, y, 260.0)
        if kind == "petal":
            petal_stack(px, y, 1.0)
        else:
            haystack(px, y, 1.0, yaw=0.0, kind=kind)


# ---------------------------------------------------------------------------------------------- main
def dress():
    R.seed(20260924)
    _batch.clear()
    _cfg.clear()
    global _nature, _meadow, _RES
    _nature, _meadow, _RES = None, None, None
    nature()

    # --- landmark: windmill on the rise east of the farm, sails facing the village
    _, wf, ds = windmill(77.0, -38.5, (0.0, -8.0))
    windmill_skirt(77.0, -38.5, skip_dir=wf.yaw + (90.0 if ds > 0 else -90.0))

    # --- footpaths first (fields avoid them through the claims)
    stone_path([(50.5, -41.0), (56.0, -39.4), (63.0, -39.0), (70.0, -38.0), (73.6, -37.4)])
    stone_path([(26.5, -38.5), (27.5, -45.5), (27.0, -53.0), (28.0, -62.0), (31.0, -70.0), (36.0, -79.5)])

    # --- crop fields (hand laid)
    field(20.5, -47.5, 10.0, 7.0, 8.0, "carrot", gate=(1, 0.0))
    field(19.5, -59.5, 10.0, 8.0, 4.0, "sunflower", gate=(1, 0.0))
    field(33.5, -57.0, 8.0, 6.0, -6.0, "cabbage", gate=(3, 0.0))
    field(60.0, -31.5, 11.0, 6.5, -5.0, "corn", fence="stone", gate=(0, 0.0))
    field(62.0, -44.5, 8.0, 6.5, 10.0, "beet", gate=(2, 0.0))
    field(74.0, -53.0, 11.0, 7.0, 15.0, "rapeseed", fence="stone", gate=(2, 0.0))
    field(90.0, -45.0, 9.0, 11.0, -10.0, "poppy", fence="wood", gate=(3, 0.0))
    field(58.0, -60.0, 8.0, 6.0, 20.0, "clover", gate=(2, 0.0), fence="stone")
    field(95.0, -30.5, 8.0, 5.0, 5.0, "cabbage", gate=(3, 0.0))
    field(80.0, -69.0, 10.0, 5.0, -10.0, "ploughed", fence="none")
    field(64.0, -91.0, 10.0, 6.0, 0.0, "carrot", gate=(2, 0.0))

    # --- orchards + cherry row + bees
    orchard(40.5, -67.5, 5.0, 4, 3)
    orchard(74.0, -82.0, -8.0, 3, 3)
    cherry_row([(31.0, -76.5), (40.0, -77.5), (46.0, -77.0)])
    apiary(24.5, -69.5, 95.0, n=5)

    # --- yards and vignettes
    animal_pen(44.0, -30.5, 0.0, 11.0, 5.5)
    hen_house(34.5, -30.0, 10.0)
    rest_spot(29.0, -27.5, 0.0)
    barn_yard()
    woodpile(12.5, -31.5, -20.0)
    harvest_cart(28.5, -44.5, 100.0)
    picnic(55.0, -36.8, 20.0)
    broken_wagon(13.0, -38.5, -60.0)
    shepherd_fold(15.0, -80.0)
    field_shed(84.0, -31.5, 180.0)
    produce_stand(23.5, -39.5, C.facing_yaw((23.5, -39.5), (26.5, -34.8)))
    field_well(65.5, -57.5)
    drying_rack(80.0, -27.8, 0.0)
    drying_rack(11.0, -46.5, 90.0)
    for (cx, cy, cyaw) in ((51.5, -35.0, 10.0), (68.0, -46.0, 80.0), (84.0, -55.0, 30.0), (24.0, -64.6, 0.0),
                           (70.0, -61.0, -20.0), (13.0, -55.0, 90.0), (45.0, -80.5, 15.0), (90.0, -57.0, 0.0),
                           (38.0, -50.8, 0.0), (58.5, -50.5, 45.0)):
        crate_stack(cx, cy, cyaw)
    hedgerow([(9.5, -49.0), (9.5, -64.0)])
    hedgerow([(38.0, -84.0), (46.0, -86.5)])
    hedgerow([(99.0, -37.0), (100.0, -51.0)])
    hedgerow([(29.0, -83.0), (34.0, -92.0), (40.0, -95.0)])

    # --- dry-stone walls dividing the land
    wall_path([(10.0, -67.0), (21.0, -66.5)])
    wall_path([(52.0, -66.0), (68.0, -65.0), (86.0, -60.0), (98.0, -57.0)], gaps_at=[(1, 0.5)])
    wall_path([(48.0, -90.0), (50.0, -70.0)], gaps_at=[(0, 0.4)])

    # --- hay meadow south of the orchard: mown field with bale stacks and loose bales
    for (hx, hy, s) in ((53.5, -74.0, 1.0), (57.0, -79.5, 0.8), (62.5, -73.5, 0.9), (60.0, -84.5, 1.0),
                        (55.0, -88.0, 0.8), (66.0, -78.0, 0.7), (53.0, -81.5, 0.7)):
        x, y = hx * M, hy * M
        clear_nature_circle(x, y, 350.0)
        if C.free(x, y, 200.0):
            haystack(x, y, s)

    lineup()

    claim_nature()
    # --- ambience
    C.ambient("A_Birds_Loop", 4000.0, -6700.0, 900.0, volume=0.8)
    C.ambient("A_Crickets_Loop", 5800.0, -7800.0, 800.0, volume=0.6)
    flush()
