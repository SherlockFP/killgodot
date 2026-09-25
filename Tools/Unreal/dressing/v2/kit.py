"""Vignette building blocks shared by the v2 zone modules (import as K next to `import kg_dress_common_v2 as C`).

Ported from the v1 zone modules (dress_square / dress_harbour / dress_streets ...) but anchored on the v2 layout:
facades come from C.buildings() frames with the builder's exact window rhythm (window_slots), so flower boxes sit under
real windows and signs hang beside real doors. Everything small goes through one Batch (K.B, HISM per mesh):
call K.reset() at the start of dress() and K.flush() at the end.

Mesh front conventions: the dressing packs (DV/DH/DW) and the KG_Props furniture face their local +Y, so a prop that
should face world direction phi gets yaw = phi - 90 (K.yaw_front). Kit house pieces face -Y.
"""
import math

import kg_dress_common_v2 as C

import sys as _sys
_sys.path.insert(0, "D:/Kill Godot/Tools/Level")
import kg_archetypes_v2 as ARCH  # noqa: E402  (SPRINT-022 house archetypes)

V, N, P, PIR, JP, WP, DV, DH, DW, DX, K, IP = C.V, C.N, C.P, C.PIR, C.JP, C.WP, C.DV, C.DH, C.DW, C.DX, C.K, C.IP
R = C.rng
B = C.Batch()
FACADE = 31.0          # kit wall pieces stick out 31 cm past the footprint edge
POST = V + "Corner_Exterior_Wood"          # 21x24x300, pivot at the base: posts, beams, rods

FLOWERS = [N + "Petal_1", N + "Petal_3", N + "Petal_2", N + "Petal_5", N + "Clover_1"]
FISH = [WP + "Fish_Cod", WP + "Fish_Mackerel", WP + "Fish_Salmon"]


def reset():
    B.b.clear()


def flush():
    B.flush()


def yaw_front(phi):
    """Yaw for a +Y-front prop that should face world direction phi (degrees)."""
    return phi - 90.0


def face_dir(x, y, tx, ty):
    return math.degrees(math.atan2(ty - y, tx - x))


def auto_cull(path, scale=1.0):
    s = C.size(path)
    k = max(scale) if isinstance(scale, (tuple, list)) else scale
    m = max(s[:3]) * k
    return 6000.0 if m <= 100 else 11000.0 if m <= 320 else 0.0


def solid(path, x, y, z=None, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, cull=None, sink=0.0):
    """Colliding repeated prop (HISM, affects nav)."""
    z = C.ground(x, y) - sink if z is None else z
    B.add(path, x, y, z, yaw, scale, pitch, roll, collide=True, cull=auto_cull(path, scale) or 30000.0 if cull is None else cull)


def clutter(path, x, y, z=None, yaw=None, scale=1.0, pitch=0.0, roll=0.0, cull=None, sink=0.0):
    """Walk-through small prop (HISM, no collision)."""
    z = C.ground(x, y) - sink if z is None else z
    yaw = R.uniform(0, 360) if yaw is None else yaw
    B.add(path, x, y, z, yaw, scale, pitch, roll, collide=False, cull=auto_cull(path, scale) or 20000.0 if cull is None else cull)


def prop(path, x, y, z=None, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=None, cull=None, sub="Props", claim=0.0,
         sink=0.0, **kw):
    """One actor with automatic collision (taller than 45 cm) and cull distance (by size)."""
    if collide is None:
        s = C.size(path)
        k = max(scale) if isinstance(scale, (tuple, list)) else scale
        collide = s[2] * k > 45.0
    return C.place(path, x, y, z, yaw=yaw, pitch=pitch, roll=roll, scale=scale, sub=sub, collide=collide,
                   cull=auto_cull(path, scale) if cull is None else cull, claim_r=claim, sink=sink, **kw)


class F:
    """Local frame at (x, y): local -Y = front (looking towards `face`/phi), +X = right. z = pad height."""

    def __init__(self, x, y, phi=None, face=None, z=None, yaw=None):
        if yaw is None:
            if face is not None:
                phi = face_dir(x, y, face[0], face[1])
            yaw = (phi or 0.0) + 90.0
        self.f = C.Frame(x, y, yaw, z=z)
        self.x, self.y, self.yaw, self.z = x, y, yaw, self.f.z

    @property
    def phi(self):
        return self.yaw - 90.0

    def w(self, lx, ly):
        return self.f.world(lx, ly)

    def gz(self, lx, ly):
        return C.ground(*self.w(lx, ly))

    def put(self, path, lx, ly, lz=0.0, lyaw=0.0, scale=1.0, on_ground=False, **kw):
        x, y = self.w(lx, ly)
        z = (C.ground(x, y) + lz) if on_ground else (self.z + lz)
        return prop(path, x, y, z, self.yaw + lyaw, scale, **kw)

    def clutter(self, path, lx, ly, lz=0.0, lyaw=None, scale=1.0, pitch=0.0, roll=0.0, on_ground=True):
        x, y = self.w(lx, ly)
        z = (C.ground(x, y) + lz) if on_ground else (self.z + lz)
        clutter(path, x, y, z, self.yaw + (R.uniform(0, 360) if lyaw is None else lyaw), scale, pitch, roll)

    def solid(self, path, lx, ly, lz=0.0, lyaw=0.0, scale=1.0, pitch=0.0, roll=0.0, on_ground=True):
        x, y = self.w(lx, ly)
        z = (C.ground(x, y) + lz) if on_ground else (self.z + lz)
        solid(path, x, y, z, self.yaw + lyaw, scale, pitch, roll)


# ============================================================================================ facades
def _party_sides():
    """{building id: {side: storeys}} of the builder's skipped party walls (same rule as kg_build_village_v2)."""
    out = {}
    bs = [f for f in C.buildings() if f.kind in ("home", "infill", "civic")]

    def segs(f):
        return {s: (f.world(*a), f.world(*b)) for s, a, b in (
            ("front", (-f.w / 2, -f.d / 2), (f.w / 2, -f.d / 2)), ("back", (-f.w / 2, f.d / 2), (f.w / 2, f.d / 2)),
            ("left", (-f.w / 2, -f.d / 2), (-f.w / 2, f.d / 2)), ("right", (f.w / 2, -f.d / 2), (f.w / 2, f.d / 2)))}

    def on(p, a, b):
        return C.seg_dist(p[0], p[1], a[0], a[1], b[0], b[1]) < 20.0

    for i, b in enumerate(bs):
        if not b.block:
            continue
        sb = segs(b)
        for a in bs:
            if a is b or a.block != b.block:
                continue
            sa = segs(a)
            for side, (p0, p1) in sb.items():
                if any(on(p0, *sg) and on(p1, *sg) for sg in sa.values()):
                    out.setdefault(b.id, {})[side] = max(out.get(b.id, {}).get(side, 0), a.storeys)
    return out


_PARTY = None


def exposed(f, side, lvl=0):
    """True when a building side is an outside wall at storey `lvl` (not a party wall, not against a neighbour)."""
    global _PARTY
    _PARTY = _PARTY if _PARTY is not None else _party_sides()
    if lvl < _PARTY.get(f.id, {}).get(side, 0):
        return False
    x, y = f.at(side, 0.0, 120.0)
    hit = C.building_hit(x, y, 0.0)
    return not (hit and hit is not f)


_SPECS = None


def spec_of(f):
    """The layout record (home / infill) of a building frame, or None (civic, towers)."""
    global _SPECS
    if _SPECS is None:
        _SPECS = {b["id"]: b for b in C.LAYOUT["houses"] + C.LAYOUT["infill"]}
    return _SPECS.get(getattr(f, "id", None))


def window_slots(f, side, lvl):
    """Local t of the windows of one storey of one side (the builder's exact rhythm): archetype houses follow
    Tools/Level/kg_archetypes_v2.py (SPRINT-022), the rest the classic district rhythm of build_house_classic."""
    sp = spec_of(f)
    if sp is not None and ARCH.arch_of(sp) is not None:
        return ARCH.window_slots(sp, side, lvl)
    n = f.cells(side)
    ts = []
    for k in range(n):
        t = -n * 100.0 + 100.0 + k * 200.0
        if lvl == 0:
            if side == "front":
                win = k % 2 == 1 and k != n // 2
            else:
                win = k % 2 == 1
        else:
            d = f.district
            if d == "crown_hill":
                win = k % 2 == 1
            elif d in ("upper_town", "harbour_row", "brookside", "orchard_upland"):
                win = k % 2 == 0
            else:
                win = (k + lvl) % 2 == 0
        if win:
            ts.append(t)
    return ts


def facade_pt(f, side, t, out=0.0):
    """World point on the facade plane (+out cm) of side at t."""
    return f.at(side, t, FACADE + out)


def jetty_out(f, side, lvl):
    """Extra facade offset (cm) of a jettied archetype's upper front storeys (kg_build_village_v2.JET)."""
    sp = spec_of(f)
    A = ARCH.arch_of(sp) if sp else None
    return 32.0 if (A and A.get("jetty") and side == "front" and lvl >= 1) else 0.0


def flower_box(f, side, t, lvl, sub="FlowerBoxes"):
    """DV FlowerBox under a window: pivot = box bottom centre, back face 14 cm behind it, front +Y outwards."""
    x, y = facade_pt(f, side, t, 16.0 + jetty_out(f, side, lvl))
    z = f.z + lvl * C.FLOOR_H + 92.0
    clutter(DV + "FlowerBox", x, y, z, yaw_front(f.nyaw(side)), 1.0, cull=9000.0)
    C.stats["flower_boxes"] = C.stats.get("flower_boxes", 0) + 1


def dress_facade(f, side, boxes_upper=True, boxes_ground=0.5, max_lvl=3):
    """Flower boxes under the windows of one side: every upper window (boxes_upper), ground windows with chance."""
    n = 0
    sp = spec_of(f)
    if sp is not None and not (sp.get("details") or {}).get("flower_boxes", True):
        boxes_upper, boxes_ground = False, min(boxes_ground, 0.25)     # per-house detail: this one has none upstairs
    for lvl in range(min(f.storeys, max_lvl)):
        if not exposed(f, side, lvl):
            continue
        if getattr(f, "kind", "") == "tower" and lvl >= 2:
            continue   # towers: plain walls (the clock dials) and the open lookout above the first floor
        for t in window_slots(f, side, lvl):
            if lvl == 0 and R.random() > boxes_ground:
                continue
            if lvl > 0 and not boxes_upper:
                continue
            flower_box(f, side, t, lvl)
            n += 1
    return n


def hang_sign(kind, f, side, t, z_above=300.0, sway=5.0):
    """Wrought-iron bracket on the facade + a swinging shop sign (kind: Fish, Bread, Ale, Anvil, Herb)."""
    x, y = facade_pt(f, side, t, 0.0)
    yaw = f.nyaw(side)
    z = f.z + z_above
    prop(DV + "SignBracket", x, y, z, yaw=yaw, collide=False, cull=12000.0, sub="Signs")
    dx, dy = C.fwd(yaw, 55.0)
    C.mover(DV + f"HangingSign_{kind}", x + dx, y + dy, z, yaw=yaw, sway=sway, sway_hz=R.uniform(0.28, 0.42), sub="Signs")


def signpost(kind, x, y, phi, sway=5.0):
    """Free-standing gallows post with a swinging sign; the arm points along phi."""
    z = C.ground_min(x, y, 30.0)
    prop(DV + "SignPost", x, y, z, yaw=phi, cull=12000.0, sub="Signs", claim=45.0)
    dx, dy = C.fwd(phi, 60.0)
    C.mover(DV + f"HangingSign_{kind}", x + dx, y + dy, z + 270.0, yaw=phi, sway=sway, sway_hz=0.35, sub="Signs")


def wall_lantern(f, side, t, z_above=210.0):
    x, y = facade_pt(f, side, t, 0.0)
    prop(P + "Lantern_Wall", x, y, f.z + z_above, yaw=f.nyaw(side) + 90.0, collide=False, cull=9000.0, sub="Lanterns")


def ivy(f, side, t, lvl=1):
    x, y = facade_pt(f, side, t, 2.0)
    clutter(V + R.choice(["Prop_Vine1", "Prop_Vine2", "Prop_Vine4"]), x, y, f.z + lvl * C.FLOOR_H + 250.0,
            f.nyaw(side) + 90.0, 1.0, cull=9000.0)


def street_sides(f, max_gap=500.0):
    """Sides of a building whose facade faces a walkway within max_gap cm (front first)."""
    out = []
    for side in ("front", "left", "right", "back"):
        x, y = f.at(side, 0.0, FACADE + 60.0)
        if C.lane_distance(x, y) < max_gap and exposed(f, side, 0):
            out.append(side)
    return out


# ============================================================================================ ropes, lines, bunting
def rod(a, b, thick=0.12, cull=16000.0, path=POST, collide=False):
    """A thin line between two 3D points: a post mesh (pivot at its base, 300 cm along +Z) stretched from a to b;
    /Engine/BasicShapes/Cylinder (centred, 100 cm) is handled too."""
    (ax, ay, az), (bx, by, bz) = a, b
    dx, dy, dz = bx - ax, by - ay, bz - az
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    if L < 1.0:
        return
    yaw = math.degrees(math.atan2(dy, dx))
    elev = math.degrees(math.asin(max(-1.0, min(1.0, dz / L))))
    if path.endswith("BasicShapes/Cylinder"):
        B.add(path, (ax + bx) / 2, (ay + by) / 2, (az + bz) / 2, yaw, (thick, thick, L / 100.0), elev - 90.0, 0.0,
              collide=collide, cull=cull)
        return
    B.add(path, ax, ay, az, yaw, (thick, thick, L / 300.0), elev - 90.0, 0.0, collide=collide, cull=cull)


def post(x, y, h=300.0, thick=1.0, z=None, collide=True):
    z = C.ground(x, y) - 8.0 if z is None else z
    B.add(POST, x, y, z, R.uniform(0, 90), (thick, thick, (h + 8.0) / 300.0), 0.0, 0.0, collide=collide, cull=20000.0)
    return z + h + 8.0


def bunting(a, b, sway=True):
    """DV Bunting string from a=(x,y,z) to b (pivot at a, +X along the string, 45 cm sag); swings gently."""
    (ax, ay, az), (bx, by, bz) = a, b
    L2 = math.hypot(bx - ax, by - ay)
    L3 = math.sqrt(L2 * L2 + (bz - az) ** 2)
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    pitch = math.degrees(math.atan2(bz - az, L2))
    sc = (L3 / 604.0, 1.0, max(0.7, min(1.4, L3 / 700.0)))
    if sway:
        C.mover(DV + "Bunting", ax, ay, az, yaw=yaw, pitch=pitch, scale=sc, sway=R.uniform(4.0, 7.0),
                sway_hz=R.uniform(0.22, 0.34), sub="Bunting")
    else:
        C.place(DV + "Bunting", ax, ay, az, yaw=yaw, pitch=pitch, scale=sc, collide=False, cull=20000.0, sub="Bunting")
    C.stats["bunting"] = C.stats.get("bunting", 0) + 1


def bunting_chain(pts, sway=True):
    for a, b in zip(pts, pts[1:]):
        bunting(a, b, sway)


CLOTHES = [(P + "Banner_1_Cloth", (0.55, 1.0, 0.32)), (P + "Banner_2_Cloth", (0.6, 1.0, 0.36)),
           (P + "Banner_1_Cloth", (0.4, 1.0, 0.24)), (P + "Banner_2_Cloth", (0.48, 1.0, 0.45))]


def laundry_across(a, b, movers=3):
    """A washing line strung between two facades (a, b = (x, y, z) hooks): rope + cloths (a few swing)."""
    (ax, ay, az), (bx, by, bz) = a, b
    sag = 22.0
    mx, my, mz = (ax + bx) / 2, (ay + by) / 2, (az + bz) / 2 - sag
    rod((ax, ay, az), (mx, my, mz), 0.06)
    rod((mx, my, mz), (bx, by, bz), 0.06)
    L = math.hypot(bx - ax, by - ay)
    yaw = math.degrees(math.atan2(by - ay, bx - ax))
    n = max(2, int(L // 70.0))
    for i in range(n):
        t = (i + 0.7) / (n + 0.4)
        x, y = ax + (bx - ax) * t, ay + (by - ay) * t
        z = az + (bz - az) * t - sag * (1.0 - (2.0 * t - 1.0) ** 2)
        path, sc = R.choice(CLOTHES)
        if movers > 0 and R.random() < 0.5:
            C.mover(path, x, y, z + 2.0, yaw=yaw, scale=sc, sway=R.uniform(3.0, 6.0), sway_hz=R.uniform(0.25, 0.45),
                    sub="Laundry")
            movers -= 1
        else:
            clutter(path, x, y, z + 2.0, yaw + R.uniform(-4, 4), sc, cull=12000.0)
    C.stats["laundry_lines"] = C.stats.get("laundry_lines", 0) + 1


def laundry_line(x, y, phi):
    """DV LaundryLine (two T-posts 4 m apart, clothes, basket on the +Y side) standing in a yard."""
    prop(DV + "LaundryLine", x, y, C.ground_min(x, y, 200.0), yaw=phi, sub="Laundry", cull=15000.0, claim=0.0)
    for k in range(5):
        dx, dy = C.fwd(phi, -200.0 + k * 100.0)
        C.claim(x + dx, y + dy, 45.0)
    C.stats["laundry_lines"] = C.stats.get("laundry_lines", 0) + 1


# ============================================================================================ planting
def potted(x, y, big=True, kind=None, z=None):
    """Terracotta pot with geraniums (DV Planter_Pot) or a vase with a bush/flowers."""
    z = C.ground(x, y) if z is None else z
    if big and R.random() < 0.6:
        solid(DV + "Planter_Pot", x, y, z, R.uniform(0, 360), R.uniform(0.9, 1.1))
    elif big:
        solid(P + "Vase_2", x, y, z, R.uniform(0, 360), 0.62)
        plant = kind or R.choice([N + "Bush_Common_Flowers", N + "Plant_1", N + "Fern_1"])
        sc = {N + "Bush_Common_Flowers": 0.27, N + "Plant_1": 0.42, N + "Fern_1": 0.22}.get(plant, 0.3)
        clutter(plant, x, y, z + 28.0, None, sc)
    else:
        clutter(DV + "Planter_Pot", x, y, z, None, R.uniform(0.6, 0.75))
    C.claim(x, y, 30.0)


def planter(x, y, phi=0.0):
    solid(DV + "Planter_Large", x, y, C.ground_min(x, y, 55.0), yaw_front(phi))
    C.claim(x, y, 65.0)


def flowerbed(x, y, phi, w=260.0, d=80.0):
    """A small bed of mixed flowers with a stone kerb (walk-through)."""
    f = F(x, y, phi)
    for i in range(int(w // 50)):
        lx = -w / 2 + 25 + i * 50
        f.clutter(R.choice([N + "Flower_3_Single", N + "Flower_4_Single", N + "Bush_Common_Flowers"]), lx + R.uniform(-8, 8),
                  R.uniform(-d / 4, d / 4), -4.0, None, R.choice([0.4, 0.45, 0.35]))
        f.clutter(R.choice(FLOWERS), lx + 22, R.uniform(-d / 3, d / 3), 0.0, None, 0.65)
    for sgn in (-1, 1):
        f.clutter(V + "Prop_ExteriorBorder_Straight1", 0.0, sgn * (d / 2 + 5), -3.0, 0.0 if sgn > 0 else 180.0,
                  (w / 200.0, 0.5, 1.0))
    C.claim(x, y, max(w, d) * 0.5)


def shrub(x, y, s=0.5, flowers=True):
    # knee-high shrubs are walk-through: small colliders between garden fences made navmesh pockets that trapped bots
    (clutter if s < 0.6 else solid)(N + ("Bush_Common_Flowers" if flowers else "Bush_Common"), x, y,
                                     C.ground(x, y) - 8.0, R.uniform(0, 360), s)
    C.claim(x, y, 95.0 * s)


def tree(x, y, path=None, s=1.0, collide=True):
    path = path or N + R.choice(["CommonTree_1", "CommonTree_2", "CommonTree_5"])
    B.add(path, x, y, C.ground_min(x, y, 60.0) - 10.0, R.uniform(0, 360), s, collide=collide, cull=40000.0)
    C.claim(x, y, 120.0 * s)


def grass_tufts(x, y, r, n=6):
    for _ in range(n):
        a, d = R.uniform(0, 6.283), R.uniform(0, r)
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        clutter(R.choice([N + "Grass_Common_Short", N + "Grass_Wispy_Short", N + "Clover_1"]), px, py,
                C.ground(px, py) - 3.0, None, R.uniform(0.4, 0.7), cull=5000.0)


# ============================================================================================ goods + clutter
def crate_stack(x, y, yaw, layout=((0, 0, 0), (1, 0, 0), (0, 0, 1)), breakable_top=False):
    """Wooden crates: layout [(dx, dy, level)] in crate units."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    gz = C.ground_min(x, y, 80.0)
    top = max(l for _, _, l in layout)
    for (dx, dy, lvl) in layout:
        lx, ly = dx * 88.0, dy * 92.0
        wx, wy = x + lx * c - ly * s, y + lx * s + ly * c
        if breakable_top and lvl == top and lvl > 0:
            C.breakable(P + "Crate_Wooden", wx, wy, gz + 5.0 + lvl * 90.0, yaw + R.uniform(-8, 8))
        else:
            solid(P + "Crate_Wooden", wx, wy, gz + 5.0 + lvl * 90.0, yaw + R.uniform(-6, 6))
    C.claim(x, y, 60.0 + 45.0 * max(abs(d[0]) + abs(d[1]) for d in layout))


def barrels(x, y, n=3, r=55.0, pirate=True, breakable=0):
    gz = C.ground_min(x, y, 60.0)
    for k in range(n):
        a = k * 2.0 * math.pi / max(1, n) + R.uniform(-0.3, 0.3)
        bx, by = x + (r if n > 1 else 0.0) * math.cos(a), y + (r if n > 1 else 0.0) * math.sin(a)
        if k < breakable:
            C.breakable(P + "Barrel", bx, by, gz + 3.0, R.uniform(0, 360))
        elif pirate:        # SPRINT-022: each barrel on its own spot of ground (slopes buried the uphill ones)
            solid(PIR + "Barrel_%d" % R.choice([0, 3, 4, 10]), bx, by, C.ground_min(bx, by, 25.0) - 2.0, R.uniform(0, 360), 0.72)
        else:
            solid(P + "Barrel", bx, by, C.ground_min(bx, by, 25.0) - 2.0, R.uniform(0, 360))
    C.claim(x, y, r + 50.0)


def sacks(x, y, yaw, n=3):
    for i, (dx, dy, zz) in enumerate(((-40, 0, 0), (25, 5, 0), (-10, 8, 55), (70, 25, 0))[:n + (1 if n > 2 else 0)]):
        px, py = x + dx * math.cos(math.radians(yaw)) - dy * math.sin(math.radians(yaw)), \
            y + dx * math.sin(math.radians(yaw)) + dy * math.cos(math.radians(yaw))
        clutter(P + "Bag", px, py, C.ground(px, py) + zz, None, 0.85 if zz == 0 else 0.72)
    C.claim(x, y, 70.0)


def woodpile(x, y, yaw, n=4):
    """Split logs stacked against a wall: short lying posts in a 3-2-1 pyramid (along world yaw)."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    gz = C.ground_min(x, y, 60.0)
    for layer in range(3):
        for i in range(n - layer):
            off = (i - (n - layer - 1) / 2.0) * 26.0
            bx, by = x - off * s, y + off * c                  # across the pile
            a = (bx - c * 45.0, by - s * 45.0, gz + 12.0 + layer * 22.0)
            rod(a, (bx + c * 45.0, by + s * 45.0, a[2]), thick=1.15, cull=9000.0)
    C.claim(x, y, max(60.0, n * 16.0))


def fish_crate(x, y, z, yaw, n=4, crate=True):
    """Shallow crate of fresh fish."""
    if crate:
        clutter(P + "FarmCrate_Empty", x, y, z + 2.0, yaw, 1.0, cull=7000.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for k in range(n):
        off = (k - (n - 1) / 2.0) * (10.0 if crate else 16.0)
        clutter(R.choice(FISH), x - off * s, y + off * c, z + (12.0 if crate else 6.0) + (k % 2) * 3.0,
                yaw + R.uniform(-12, 12), R.uniform(0.9, 1.1), roll=90.0, cull=6000.0)


def lantern_post(x, y, phi, light=0.0, radius=900.0, tall=1.0, color=C.WARM):
    """Wooden post with the kit's wall lantern; the arm points along phi (world yaw of the lantern side)."""
    z = C.ground(x, y) - 5.0
    post(x, y, 300.0 * tall, 1.1, z)
    yaw = phi - 90.0     # Lantern_Wall arm is along its local +Y
    prop(P + "Lantern_Wall", x, y, z + 175.0 * tall + 5.0, yaw=yaw, collide=False, cull=12000.0, sub="Lanterns")
    C.claim(x, y, 40.0)
    if light > 0.0:
        dx, dy = C.fwd(phi, 105.0)
        C.light(x + dx, y + dy, z + 230.0 * tall, light, radius, color)


def torch(x, y, light=0.0, z=None):
    z = C.ground(x, y) if z is None else z
    prop(PIR + "Torch_3", x, y, z, yaw=R.uniform(0, 360), cull=12000.0, sub="Torches", claim=30.0)
    if light:
        C.light(x, y, z + 250.0, light, 750.0, (255, 150, 70))


def bench(x, y, phi, z=None):
    """KGSeat bench: the sitter looks along phi."""
    return C.seat(P + "Bench", x, y, phi, z=z)


def stool(x, y, phi, z=None):
    return C.seat(P + "Stool", x, y, phi, z=z)


def table_set(x, y, yaw, stools=3, food=True):
    """Tavern table with stools (seats) and mugs / bread."""
    f = F(x, y, yaw=yaw)
    f.solid(K + "Square_Table", 0, 0, 0, 0)
    for j, (sx, sy, sphi) in enumerate([(0, -72, 90), (0, 72, -90), (-72, 0, 0), (72, 0, 180)][:stools]):
        wx, wy = f.w(sx + R.uniform(-6, 6), sy + R.uniform(-6, 6))
        stool(wx, wy, yaw + sphi + R.uniform(-15, 15))
    for j in range(R.randint(2, 4)):
        f.clutter(P + "Mug", R.uniform(-28, 28), R.uniform(-28, 28), 67, None, 1.0)
    if food:
        f.clutter(K + "Plate", 10, 5, 67, 0, 1.0)
        f.clutter(IP + "Bread", 10, 5, 68.5, None, 0.8)
        f.clutter(IP + "Cheese", -12, -14, 67, None, 0.8)
    else:
        f.clutter(K + "Bottle", -18, 12, 67, 0, 1.0)
        f.clutter(P + "CandleStick", 5, -5, 67, None, 1.0)
    C.claim(x, y, 115.0)


# ============================================================================================ market
TOP = 83.0   # Stall_Empty / Stall_Cart_Empty counter height


def goods(f, kind):
    """Goods on a stall counter (frame local: counter x +-85, y +-38 at z=83; customers at local -Y)."""
    t = TOP
    if kind == "fruit":
        for lx in (-42, 42):
            f.clutter(P + "FarmCrate_Apple", lx, -10, t, R.uniform(-5, 5), on_ground=False)
        f.clutter(P + "FarmCrate_Apple", 0, 18, t + 22, 90 + R.uniform(-5, 5), on_ground=False)
        f.solid(P + "Barrel_Apples", -130, -60, 0, R.uniform(0, 360))
        f.clutter(P + "FarmCrate_Apple", 125, -70, 0, 20)
    elif kind == "veg":
        for lx, yw in ((-44, 0), (42, 180)):
            f.clutter(P + "FarmCrate_Carrot", lx, -8, t, yw + R.uniform(-6, 6), on_ground=False)
        for k in range(5):
            f.clutter(N + "Mushroom_Common", -70 + k * 35, -33, t, None, 0.3, on_ground=False)
        f.clutter(P + "FarmCrate_Carrot", -125, -68, 0, 30)
        f.clutter(P + "Bag", 125, -62, 0, None)
    elif kind == "fish":
        for k in range(10):
            lx, ly = -72 + (k % 5) * 36 + R.uniform(-4, 4), -24 + (k // 5) * 28
            f.clutter(FISH[k % 3], lx, ly, t + 7, 90 + R.uniform(-15, 15), 1.0, 0.0, 90.0, on_ground=False)
        f.clutter(WP + "Fish_GoldenCarp", 62, 24, t + 8, 80, 1.0, 0.0, 90.0, on_ground=False)
        f.solid(DH + "FishBarrel", -128, -55, 0, R.uniform(0, 360))
        f.clutter(DH + "FishBasket", 130, -60, 0, None)
    elif kind == "bread":
        for k in range(8):
            f.clutter(IP + "Bread", -66 + (k % 4) * 42, -22 + (k // 4) * 26, t, R.uniform(-15, 15) + 90, 1.0, on_ground=False)
        for k in range(3):
            f.clutter(IP + "Cheese", 58, 26, t + k * 8, None, 1.0, on_ground=False)
        f.clutter(P + "FarmCrate_Empty", -125, -70, 0, 10)
        for k in range(4):
            f.clutter(IP + "Bread", -135 + (k % 2) * 18, -75 + (k // 2) * 14, 14, None, 1.0)
    elif kind == "pottery":
        for k, pth in enumerate([P + "Vase_4", K + "Jar", K + "YellowJar", K + "Jar", P + "Vase_4"]):
            f.clutter(pth, -70 + k * 35, -12, t, None, 0.8 if "Vase" in pth else 1.8, on_ground=False)
        for k in range(4):
            f.clutter(K + "Plate", 55, 22, t + k * 2.5, None, 1.0, on_ground=False)
            f.clutter(K + "Bowl", -55, 22, t + k * 6, None, 1.3, on_ground=False)
        for lx, ly, s in [(-125, -60, 1.0), (130, -60, 0.9)]:
            f.solid(P + "Vase_2", lx, ly, 0, R.uniform(0, 360), s)
    elif kind == "cloth":
        for k in range(3):
            for j in range(3 + k % 2):
                f.clutter(IP + "Rug_Rect", -58 + k * 55, -6, t + 1 + j * 5, R.uniform(-8, 8), (0.2, 0.26, 5.0), on_ground=False)
        for side in (-1, 1):
            f.clutter(P + ("Banner_1_Cloth" if side < 0 else "Banner_2_Cloth"), side * 72, -36, 232, 0, (0.7, 1.0, 0.55),
                      on_ground=False)
    elif kind == "flowers":
        for k in range(4):
            f.clutter(DV + "Planter_Pot", -64 + k * 42, -8, t, None, 0.55, on_ground=False)
        f.clutter(N + "Flower_3_Single", -40, 22, t, 0, 0.35, on_ground=False)
        f.clutter(N + "Flower_4_Single", 30, 22, t, 60, 0.3, on_ground=False)
        for lx, ly in [(-125, -60), (130, -55)]:
            f.solid(DV + "Planter_Pot", lx, ly, 0, R.uniform(0, 360))
    elif kind == "apothecary":
        pots = [P + "Potion_1", P + "Potion_2", P + "Potion_4", P + "Bottle_1", P + "Potion_2", P + "SmallBottles_1"]
        for k in range(10):
            f.clutter(pots[k % 6], -72 + (k % 5) * 36 + R.uniform(-4, 4), -20 + (k // 5) * 28, t, None, 1.0, on_ground=False)
        f.clutter(P + "CandleStick_Triple", 55, 25, t, 0, 1.0, on_ground=False)
        for k, lx in enumerate((-50, 0, 45)):
            f.clutter(IP + "HerbBundle", lx, 30, t + 130, k * 40.0, 1.0, on_ground=False)
        f.solid(P + "Cauldron", -130, -55, 0, R.uniform(0, 360))
    # stock behind the counter + the vendor's stool
    f.solid(PIR + "crates_%d" % R.randint(0, 1), -100, 105, 0, R.uniform(-12, 12))
    f.clutter(P + "Bag", 0, 118, 0, None, 0.9)
    x, y = f.w(-20, 60)
    stool(x, y, f.phi + 180.0 + R.uniform(-20, 20))


STALL_CLOTH = {"fruit": 1, "veg": 2, "fish": 2, "bread": 1, "pottery": 1, "cloth": 2, "apothecary": 2, "flowers": 1}
SIGN_OF = {"fish": "Fish", "bread": "Bread", "apothecary": "Herb", "flowers": "Herb", "veg": "Herb", "fruit": "Herb"}


def stall(x, y, phi, kind, cart=False, sign=True):
    """A market stall at (x, y) whose counter faces phi: goods, stock, stool, a valance and a swinging sign."""
    f = F(x, y, phi, z=C.ground_min(x, y, 100.0))
    f.put(P + ("Stall_Cart_Empty" if cart else "Stall_Empty"), 0, 0, 0, 180.0, sub="Market")
    C.claim(x, y, 150.0)
    goods(f, kind)
    cloth = P + ("Banner_1_Cloth" if STALL_CLOTH.get(kind, 1) == 1 else "Banner_2_Cloth")
    f.clutter(cloth, 0, -46, 252, 0, (2.25, 1.0, 0.2), on_ground=False)
    if sign and kind in SIGN_OF:
        sx, sy = f.w(118, -50)
        C.mover(DV + f"HangingSign_{SIGN_OF[kind]}", sx, sy, f.z + 238.0, yaw=f.yaw, sway=5.0, sway_hz=0.4, sub="Market")
    C.stats.setdefault("stalls", []).append(kind)
    return f


# ============================================================================================ water
def buoy(x, y, kind=None):
    """Floating buoy bobbing on the swell (DH Buoy_Red / Buoy_Striped)."""
    kind = kind or R.choice(["Buoy_Red", "Buoy_Striped", "Buoy_Red"])
    C.mover(DH + kind, x, y, -35.0 if kind == "Buoy_Red" else -90.0, yaw=R.uniform(0, 360), roll=R.uniform(-6, 6),
            bob=R.uniform(8.0, 14.0), sway=R.uniform(3.0, 6.0), sway_hz=R.uniform(0.2, 0.35), sub="Buoys")


def float_barrel(x, y):
    C.mover(PIR + "Barrel_%d" % R.choice([0, 3, 4]), x, y, -40.0, yaw=R.uniform(0, 360), roll=90.0, scale=0.7,
            bob=R.uniform(8.0, 12.0), sway=3.0, sway_hz=0.22, sub="Buoys")


def moored_boat(x, y, yaw, cargo=True):
    """Rowboat riding the swell (cosmetic mover, not rideable) with oars and a crate in it."""
    C.mover(WP + "Rowboat", x, y, 8.0, yaw=yaw, bob=6.0, sway=2.5, sway_hz=0.18, sub="Boats")
    C.claim(x, y, 170.0)


def beached_boat(x, y, yaw, upturned=False, lean=8.0):
    gz = C.ground_min(x, y, 120.0)
    if upturned:
        prop(WP + "Rowboat", x, y, gz + 52.0, yaw=yaw, roll=180.0 + R.uniform(-3, 3), sub="Boats", cull=15000.0)
    else:
        prop(WP + "Rowboat", x, y, gz + 12.0, yaw=yaw, roll=lean, pitch=R.uniform(-4, 4), sub="Boats", cull=15000.0)
    C.claim(x, y, 185.0)


# ============================================================================================ paving
_STAIR_SEGS = None


def tile_ok(x, y, z, r=90.0, tol=12.0):
    """SPRINT-022: a paving tile only goes where it lies flat on the ground (centre and four points r cm out within
    tol cm of z) and never on a stair or ramp corridor (tiles used to hang over stair flights and slopes)."""
    global _STAIR_SEGS
    if _STAIR_SEGS is None:
        _STAIR_SEGS = [(st["from"][0] * C.M, st["from"][1] * C.M, st["to"][0] * C.M, st["to"][1] * C.M, st["width"] * 50.0)
                       for st in C.LAYOUT["stairs"]]
        for rp in C.LAYOUT["ramps"]:
            for (ax, ay), (bx, by) in zip(rp["points"], rp["points"][1:]):
                _STAIR_SEGS.append((ax * C.M, ay * C.M, bx * C.M, by * C.M, rp["width"] * 50.0))
    for ax, ay, bx, by, hw in _STAIR_SEGS:
        if C.seg_dist(x, y, ax, ay, bx, by) < hw + r:
            return False
    for dx, dy in ((0, 0), (r, 0), (-r, 0), (0, r), (0, -r)):
        if abs(C.ground(x + dx, y + dy) - z) > tol:
            return False
    return True


def pave(poly, yaw, pick, z, origin=None, tile=200.0, inset=40.0, skip=None, cull=0.0, min_corners=4):
    """Lay kit floor tiles (200 x 200, walk-through, flat) over a flat polygon on a grid turned by yaw.
    pick(x, y, i, j) -> mesh path or None (pattern). A tile goes down when its centre and its four corners (pulled in
    by `inset`) are inside the polygon. Returns the count."""
    ox, oy = origin or C.centroid(poly)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    xs = [p[0] for p in poly]
    ys = [p[1] for p in poly]
    R_ = max(max(abs(x - ox) for x in xs), max(abs(y - oy) for y in ys)) * 1.5
    n = int(R_ // tile) + 2
    h = tile * 0.5 - inset
    cnt = 0
    for i in range(-n, n + 1):
        for j in range(-n, n + 1):
            lx, ly = i * tile, j * tile
            x, y = ox + lx * c - ly * s, oy + lx * s + ly * c
            if not C.pip(poly, x, y):
                continue
            corners = [(x + a * c - b * s, y + a * s + b * c) for a, b in ((-h, -h), (h, -h), (h, h), (-h, h))]
            if sum(1 for cx, cy in corners if C.pip(poly, cx, cy)) < min_corners:
                continue
            if skip and skip(x, y):
                continue
            if not tile_ok(x, y, z, tile * 0.56):
                continue
            path = pick(x, y, i, j)
            if not path:
                continue
            B.add(path, x, y, z + (0.6 if (i + j) % 2 else 0.0), yaw + 90.0 * ((i * 7 + j * 3) % 4), 1.0,
                  collide=False, cull=cull or 30000.0)
            cnt += 1
    C.stats["paving_tiles"] = C.stats.get("paving_tiles", 0) + cnt
    return cnt


def pave_lane(name, z, mesh=V + "Floor_Brick", step=200.0, trim=0.0, edge_mesh=None, skip=None, width=None):
    """Tiles along a flat walkway: rows across the width every `step` along the centre line, each tile stretched to the
    local arc length so curved streets close without gaps. edge_mesh: a different tile for the two outer rows."""
    pts = C.lane(name)
    w = width or C.lane_width(name)
    ncol = max(1, int(round(w / 200.0)))
    cw = w / ncol
    cnt = 0
    samples = C.resample(pts, step, start=step * 0.5 + trim, end_trim=trim)
    for k, (x, y, ux, uy, sdist) in enumerate(samples):
        # curvature from the neighbours: stretch outer tiles a little
        yaw = math.degrees(math.atan2(uy, ux))
        for col in range(ncol):
            off = -w * 0.5 + cw * (col + 0.5)
            px, py = x - uy * off, y + ux * off
            if skip and skip(px, py):
                continue
            if not tile_ok(px, py, z, max(step, cw) * 0.56):
                continue
            edge = col in (0, ncol - 1) and ncol > 2
            path = edge_mesh if (edge and edge_mesh) else mesh
            B.add(path, px, py, z + (0.6 if (k + col) % 2 else 0.0), yaw, (step / 200.0 * 1.06, cw / 200.0 * 1.02, 1.0),
                  collide=False, cull=30000.0)
            cnt += 1
    C.stats["paving_tiles"] = C.stats.get("paving_tiles", 0) + cnt
    return cnt


# ============================================================================================ town fronts
def zone_buildings(zone=None):
    zone = zone or C.ZONE
    return [f for f in C.buildings() if C.zone_of(f.x, f.y) == zone and f.kind in ("home", "infill", "civic")]


def door_pots(f, chance=0.8):
    """A potted plant each side of a front door (on the street band, never in the door apron)."""
    if f.kind not in ("home", "infill", "civic") or not f.door:
        return
    for sgn in (-1, 1):
        if R.random() > chance:
            continue
        x, y = f.at("front", f.door_t + sgn * 128.0, FACADE + 32.0)
        if C.free(x, y, 30.0, allow_lane=True, zone=False):
            potted(x, y, big=R.random() < 0.7)


def dress_buildings(bs, boxes_ground=0.45, pots=0.8, ivy_chance=0.15):
    """Flower boxes under the windows of every exposed side, pots by the doors, some ivy (district-agnostic)."""
    n = 0
    for f in bs:
        for side in ("front", "back", "left", "right"):
            n += dress_facade(f, side, boxes_upper=True, boxes_ground=boxes_ground if side == "front" else boxes_ground * 0.5)
            if R.random() < ivy_chance and exposed(f, side, 1) and f.storeys >= 2:
                ivy(f, side, R.choice([-100.0, 100.0]) if f.cells(side) >= 2 else 0.0, 1)
        door_pots(f, pots)
    return n


# ============================================================================================ builder pieces
def parapets(zone=None, wall_prefix=None):
    """[(x, y, z_top, yaw)] of the builder's parapet pieces (KG_V2_Placements V2/Parapets) in a zone: cap centre."""
    out = []
    for it in C.PL["items"]:
        if it["f"] != "V2/Parapets":
            continue
        x, y, z = it["p"]
        yaw = it.get("y", 0.0)
        sz = it.get("s", [1, 1, 1])[2]
        if it["m"].startswith("DT:"):
            if it["m"] != "DT:Balustrade_Post":
                continue                            # SPRINT-022: balustrade rails have no cap to stand a pot on; posts do
            top = z + 115.0 * sz
        else:
            dx, dy = C.fwd(yaw - 90.0, 10.0)        # cap centre: 10 cm towards the piece's -Y face
            x, y = x + dx, y + dy
            top = z + 312.0 * sz
        if zone and C.zone_of(x, y) != zone:
            continue
        out.append((x, y, top, yaw))
    return out


def fence_run(ax, ay, bx, by, gate=None, z=None):
    """Wooden fence A->B (sections scaled to fit, on the terrain). gate = fraction of the skipped section."""
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
        zz = min(C.ground(x0, y0), C.ground(x1, y1), C.ground(mx, my)) if z is None else z
        solid(V + "Prop_WoodenFence_Single", mx, my, zz, yaw + R.uniform(-1.5, 1.5), (seg / 206.0 * 1.01, 1.0, 1.0))
        C.claim(mx, my, 45.0)


def veg_bed(x, y, yaw, w=300.0, d=200.0):
    """Dug rows with carrots / cabbages (Plant_7) / clover, walk-through."""
    f = F(x, y, yaw=yaw)
    rows = max(1, int((d - 40.0) // 60.0))
    for r in range(rows):
        ly = -d / 2 + 40.0 + r * 60.0
        mx, my = f.w(0.0, ly)
        clutter(WP + "DigMound", mx, my, C.ground(mx, my) - 13.0, yaw, ((w - 40.0) / 121.0, 0.42, 0.4), cull=9000.0)
        crop = [P + "Carrot", N + "Plant_7", P + "Carrot", N + "Clover_2"][r % 4]
        step = 24.0 if "Carrot" in crop else 45.0
        for i in range(int((w - 60.0) // step)):
            px, py = f.w(-w / 2 + 40.0 + i * step + R.uniform(-4, 4), ly + R.uniform(-5, 5))
            sc = R.uniform(0.9, 1.1) if "Carrot" in crop else R.uniform(0.42, 0.52)
            clutter(crop, px, py, C.ground(px, py) - 2.0, None, sc, cull=7000.0)
    C.claim(x, y, max(w, d) * 0.5)


def cypress(x, y, h=1.0):
    """A tall narrow cypress (a Pine squeezed thin)."""
    solid(N + "Pine_3", x, y, C.ground(x, y) - 20.0, R.uniform(0, 360), (0.32 * h, 0.32 * h, 1.05 * h), cull=40000.0)
    C.claim(x, y, 70.0)


def beehive(x, y, yaw):
    f = F(x, y, yaw=yaw)
    f.solid(P + "Stool", 0, 0, 0, 0)
    f.solid(P + "Crate_Wooden", 0, 0, 55.0, 0, (0.55, 0.55, 0.45))
    f.clutter(P + "Crate_Wooden", 0, 0, 97.0, 12.0, (0.52, 0.52, 0.35))
    f.clutter(V + "Floor_WoodDark", 0, 0, 132.0, 0.0, (0.3, 0.3, 1.0))
    C.claim(x, y, 50.0)


# ============================================================================================ farm + wild pieces
def bale_stack(x, y, yaw, s=1.0):
    """Straw bales (squashed sacks on their side) in a 3-2-1 pyramid (s >= 1), 2-1 or a loose pair."""
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    z0 = C.ground_min(x, y, 130.0) - 4.0
    layout = [(-125, 0, 0), (0, 0, 0), (125, 0, 0), (-62, 0, 1), (62, 0, 1), (0, 0, 2)] if s >= 1.0 else \
        [(-62, 0, 0), (62, 0, 0), (0, 0, 1)] if s >= 0.85 else [(0, 0, 0), (40, 150, 0)]
    for lx, ly, lvl in layout:
        bx, by = x + lx * c - ly * s_, y + lx * s_ + ly * c
        B.add(P + "Bag", bx, by, z0 + lvl * 62.0 + 33.0, yaw + R.uniform(-5, 5), (1.0, 1.35, 1.5), 90.0, 0.0,
              collide=True, cull=12000.0)
    C.claim(x, y, 200.0 * max(0.8, s))


def scarecrow(x, y, yaw):
    prop(P + "Dummy", x, y, C.ground(x, y) - 4.0, yaw=yaw, pitch=R.uniform(-4, 4), roll=R.uniform(-5, 5), sub="Scarecrows",
         cull=12000.0, claim=60.0)
    clutter(P + "Bag", x, y, C.ground(x, y) + 158.0, yaw + 20, (0.55, 0.55, 0.45), cull=9000.0)
    C.mover(P + "Banner_1_Cloth", x + 25.0, y, C.ground(x, y) + 150.0, yaw=yaw + 90.0, scale=(0.3, 1.0, 0.25),
            sway=10.0, sway_hz=0.5, sub="Scarecrows")


def signpost_arrows(x, y, targets, lantern=False):
    """A waymark: a post with plank arrows pointing at each target (x, y)."""
    top = post(x, y, 250.0, 0.75)
    for i, (tx, ty) in enumerate(targets):
        yaw = face_dir(x, y, tx, ty)
        dx, dy = C.fwd(yaw, 45.0)
        clutter(PIR + "Planks_%d" % (i % 4), x + dx, y + dy, top - 30.0 - i * 34.0, yaw, (0.46, 0.6, 1.0), 0.0, 90.0,
                cull=9000.0)
    C.claim(x, y, 40.0)


def log(x, y, z, yaw, length=300.0, thick=1.6, collide=True):
    """A lying log (a post on its side) centred at (x, y) along world yaw."""
    dx, dy = C.fwd(yaw, length * 0.5)
    rod((x - dx, y - dy, z + 14.0 * thick / 1.6), (x + dx, y + dy, z + 14.0 * thick / 1.6), thick=thick, cull=15000.0,
        collide=collide)


def log_pile(x, y, yaw, rows=3, length=320.0):
    gz = C.ground_min(x, y, 150.0)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for r in range(rows):
        for k in range(rows - r + 1):
            off = (k - (rows - r) / 2.0) * 34.0
            log(x - off * s, y + off * c, gz + r * 28.0, yaw, length, 2.2)
    C.claim(x, y, length * 0.5)


def campfire(x, y, light=18.0, z=None):
    z = C.ground_min(x, y, 90.0) if z is None else z
    prop(DW + "Campfire", x, y, z - 3.0, yaw=R.uniform(0, 360), sub="Camp", cull=15000.0, claim=110.0)
    if light:
        C.light(x, y, z + 90.0, light, 900.0, (255, 130, 60))


def rock_cluster(x, y, s=1.0, n=3):
    for k in range(n):
        a = R.uniform(0, 6.28)
        px, py = x + 120.0 * s * math.cos(a) * (k > 0), y + 120.0 * s * math.sin(a) * (k > 0)
        sc = s * R.uniform(0.5, 0.9)
        B.add(N + "Rock_Medium_%d" % R.randint(1, 3), px, py, C.ground_min(px, py, 90.0 * sc) - 55.0 * sc, R.uniform(0, 360),
              sc, collide=True, cull=40000.0)
        clutter(N + "Fern_1", px + 90.0, py + 30.0, None, None, R.uniform(0.3, 0.5))
    C.claim(x, y, 170.0 * s)


def flower_patch(x, y, r=180.0, n=7):
    for _ in range(n):
        a, d = R.uniform(0, 6.283), R.uniform(0, r)
        px, py = x + d * math.cos(a), y + d * math.sin(a)
        pth = N + R.choice(["Flower_3_Group", "Flower_4_Group", "Flower_3_Single", "Bush_Common_Flowers"])
        clutter(pth, px, py, C.ground(px, py) - 4.0, None,
                R.uniform(0.3, 0.45) if "Bush" in pth else R.uniform(0.18, 0.3))


def smash_piles(points, r=70.0, **free_kw):
    """Small piles of smashable crates / barrels / pots (AKGBreakable, loot inside) near each point that has room."""
    n = 0
    for (x, y) in points:
        s = C.find(x, y, r, reach=220.0, **free_kw)
        if not s:
            continue
        kinds = [P + "Crate_Wooden", P + "Crate_Wooden", P + "Barrel", P + "Vase_2"]
        C.breakable(R.choice(kinds), s[0], s[1], yaw=R.uniform(0, 360))
        C.breakable(P + "Crate_Wooden", s[0] + 60.0, s[1] + 25.0, yaw=R.uniform(0, 360))
        if R.random() < 0.5:
            C.breakable(P + "Crate_Wooden", s[0] + 25.0, s[1] + 10.0, z=C.ground(*s) + 95.0, yaw=R.uniform(0, 360))
        C.claim(s[0] + 25.0, s[1] + 12.0, 90.0)
        n += 1
    C.stats["smash_piles"] = C.stats.get("smash_piles", 0) + n
    return n



def facade_rhythm(bs, step=(230.0, 360.0), kinds=None, max_gap=250.0, sides=None):
    """Something at the foot of every street-facing wall every ~2.5-3.5 m (pots, barrels, a bench, crates, a flower
    bed, a water butt, a woodpile), fitted between doors and windows, kept to the 0.8 m strip against the wall."""
    kinds = kinds or ["pots", "barrel", "pots", "bench", "crate", "flowers", "butt", "pots", "wood", "flowers"]
    n = 0
    for f in bs:
        for side in (sides or street_sides(f, max_gap)):
            half = f.half(side)
            t = -half + 70.0
            k = int(abs(f.x + f.y)) % len(kinds)
            while t < half - 60.0:
                if side == "front" and f.door and abs(t - f.door_t) < 150.0:
                    t = f.door_t + 150.0
                    continue
                kind = kinds[k % len(kinds)]
                wlen = 290.0 if kind == "bench" else 120.0
                x, y = f.at(side, t + wlen * 0.5, FACADE + 30.0)
                ok = C.free_line(x, y, f.tyaw(side), wlen, 26.0, allow_lane=True, lane_gap=0.0, zone=True)
                if ok:
                    ny, ty = f.nyaw(side), f.tyaw(side)
                    if kind == "pots":
                        for j, dt in enumerate((-35.0, 30.0)):
                            potted(*f.at(side, t + wlen * 0.5 + dt, FACADE + 28.0), big=j == 0, z=f.z)
                    elif kind == "barrel":
                        solid(P + "Barrel", x, y, f.z, R.uniform(0, 360), 0.85)
                        clutter(P + "Bucket_Wooden_1", *f.at(side, t + wlen * 0.5 + 55.0, FACADE + 25.0), f.z, None, 1.0)
                    elif kind == "bench":
                        bench(x, y, ny, z=f.z)
                    elif kind == "crate":
                        solid(P + "Crate_Wooden", x, y, f.z + 4.0, ty + R.uniform(-8, 8), 0.8)
                        clutter(P + "Bag", *f.at(side, t + wlen * 0.5 + 60.0, FACADE + 28.0), f.z, None, 0.8)
                    elif kind == "flowers":
                        for j in range(3):
                            clutter(N + R.choice(["Flower_3_Single", "Flower_4_Single", "Bush_Common_Flowers"]),
                                    *f.at(side, t + wlen * 0.5 + (j - 1) * 38.0, FACADE + 22.0), f.z - 4.0, None,
                                    R.uniform(0.28, 0.36))
                    elif kind == "butt":
                        solid(PIR + "Barrel_%d" % R.choice([5, 8, 12]), x, y, f.z, R.uniform(0, 360), 0.75)
                        clutter(K + "Bowl", x, y, f.z + 80.0, None, 2.0)
                    elif kind == "wood":
                        woodpile(x, y, ty, 3)
                    C.claim_line(x, y, f.tyaw(side), wlen, 30.0)
                    n += 1
                    k += 1
                    t += wlen + R.uniform(step[0], step[1]) - 120.0
                else:
                    t += 45.0
    C.stats["facade_rhythm"] = C.stats.get("facade_rhythm", 0) + n
    return n


def lantern_row(pts_side, every=1500.0, light_every=2, max_lights=4, face=None):
    """Lantern posts along a line of (x, y) spots (walk-through check by the caller), every other one lit."""
    n = lit = 0
    last = None
    for (x, y) in pts_side:
        if last and math.hypot(x - last[0], y - last[1]) < every:
            continue
        phi = face(x, y) if face else 0.0
        light = 8.0 if (n % light_every == 0 and lit < max_lights) else 0.0
        lantern_post(x, y, phi, light=light, radius=1000.0)
        lit += 1 if light else 0
        n += 1
        last = (x, y)
    return n
