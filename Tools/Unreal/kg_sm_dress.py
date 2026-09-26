"""Storm Manor room dressing: every room gets a story vignette on top of kg_interiors' furnishing, the grounds too.

Called by Tools/Unreal/kg_build_stormmanor.py (run(builder, kg_interiors)) after the architecture exists. Each room
is a kg_interiors House/Room over its rectangle (frame at the centre, local -Y = north), so the proven free-space
bookkeeping keeps doorways, stair wells, chore spots, secret-passage ends, the meeting ring and window sills clear and
puts every prop on the floor or on a measured surface (kg_interiors.TOP / on()). Meshes go through the builder's
batcher (HISM for repeats). Lights: the builder's budget; hearths in the hall, library, kitchen and red room are the
four shadow-casting hero lights.
Story beats (Docs/Level/StormManor_Plan.md section 1): the empty chair at the head of the long table (Godot's), the
nursery kept ready (blue hat on its hook), Anselm's clocks all stopped at the same minute, the "G." parcels.
"""
import math
import random

import unreal

V = "/Game/KillGodot/Env/KG_Village/StaticMeshes/"
P = "/Game/KillGodot/Env/KG_Props/StaticMeshes/"
K = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/Kitchen_"
BD = "/Game/KillGodot/Env/Furniture/KG_Bedroom/StaticMeshes/Bedroom_"
I = "/Game/KillGodot/Env/Furniture/KG_Interior/StaticMeshes/SM_KG_"
DV = "/Game/KillGodot/Env/Dress/KG_DressVillage_Clean/StaticMeshes/SM_KG_"
DH = "/Game/KillGodot/Env/Dress/KG_DressHarbour_Clean/StaticMeshes/SM_KG_"
DW = "/Game/KillGodot/Env/Dress/KG_DressWilds_Clean/StaticMeshes/SM_KG_"
DL = "/Game/KillGodot/Env/Dress/KG_DressLandmarks_Clean/StaticMeshes/SM_KG_"
DT = "/Game/KillGodot/Env/Dress/KG_DressTerrace_Clean/StaticMeshes/SM_KG_"
DU = "/Game/KillGodot/Env/Dress/KG_DressUnder_Clean/StaticMeshes/SM_KG_"
N = "/Game/KillGodot/Env/KG_Nature/StaticMeshes/"
WP = "/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_"
PIR = "/Game/KillGodot/Env/PirateC3/KG_PirateProps_Clean2/StaticMeshes/SM_KG_Pirate_"
E = "/Engine/BasicShapes/"

Bm = None           # the builder module
KI = None           # kg_interiors
DRESSED = {}        # room id -> number of meshes placed


class SMFrame:
    def __init__(self, x, y, z):
        self.x, self.y, self.z, self.yaw = x, y, z, 0.0
        self.c, self.s = 1.0, 0.0


def make_house_class():
    class SMHouse(KI.House):
        """kg_interiors House over one manor room (w, d metres; wall segments from the builder's registry)."""

        def __init__(self, rid, rect, fid, style, cap):
            x0, y0, x1, y1 = rect
            f = SMFrame((x0 + x1) * 50.0, (y0 + y1) * 50.0, Bm.fz(fid))
            KI.House.__init__(self, f, x1 - x0, y1 - y0, style, Bm.interior_light)
            self.kind = "manor"                       # no family-chest ownership, no landmark plan
            self.rid, self.rect, self.fid, self.cap = rid, rect, fid, cap
            self.hero = False
            self._segs = {}
            for (wf, room, orient, coord, t0, t1, kind) in Bm.WALL_REG:
                if wf != fid or room != rid:
                    continue
                if orient == "h" and abs(coord - y0) < 0.01:
                    side, off = "front", f.x
                elif orient == "h" and abs(coord - y1) < 0.01:
                    side, off = "back", f.x
                elif orient == "v" and abs(coord - x0) < 0.01:
                    side, off = "left", f.y
                elif orient == "v" and abs(coord - x1) < 0.01:
                    side, off = "right", f.y
                else:
                    continue
                self._segs.setdefault(side, []).append((t0 * 100.0 - off, t1 * 100.0 - off, kind))

        def segments(self, level, wall):
            return sorted(self._segs.get(wall, []))

        def light(self, lx, ly, lz, intensity, radius):
            if self.lights >= self.cap:
                return
            if lz > KI.FLOOR_H - 10.0:          # kg_interiors' upper-floor heights: our rooms are all level 0
                lz -= KI.FLOOR_H
            wx, wy = self.world(lx, ly)
            a = Bm.light(wx, wy, self.f.z + lz, intensity * 1.6, radius * 1.3, hero=self.hero)
            self.hero = False
            if a:
                self.lights += 1
                self.count += 1

    return SMHouse


SMHouse = None


# =================================================================================================== helpers
def loc(h, xm, ym):
    return xm * 100.0 - h.f.x, ym * 100.0 - h.f.y


def room_for(rid, cap=1, style=0, inset_int=22.0):
    r = Bm.R[rid]
    x0, y0, x1, y1 = Bm.G.rect_of(r["poly"])
    h = SMHouse(rid, (x0, y0, x1, y1), r["floor"], 300 + style, cap)
    g = KI.Room(h, 0)
    enclosed = Bm.G.enclosed(r)
    pad = inset_int if enclosed else 30.0
    g.x0, g.x1, g.y0, g.y1 = -h.ex + pad, h.ex - pad, -h.ey + pad, h.ey - pad
    g.rects = []
    # SPRINT-040: an L / U room's bounding-box cells outside its polygon are not its floor (the vault's notch is the
    # catacomb stair landing)
    inside = set(Bm.G.cells_of(r["poly"]))
    for i in range(int(math.floor(x0 / 2.0)), int(math.ceil(x1 / 2.0))):
        for j in range(int(math.floor(y0 / 2.0)), int(math.ceil(y1 / 2.0))):
            if (i, j) not in inside and not Bm.GRID.is_void(r["floor"], (i, j)):
                lx0, ly0 = loc(h, i * 2.0 - 0.3, j * 2.0 - 0.3)
                lx1, ly1 = loc(h, i * 2.0 + 2.3, j * 2.0 + 2.3)
                keep_rect(g, lx0, ly0, lx1, ly1)
    reserve(h, g, rid)
    Bm.SHIM.room = rid
    CUR[0] = (h, g)
    return h, g


CUR = [None]        # the room being dressed (done() runs its external-kit vignette layer on the same free-space map)


def keep_rect(g, x0, y0, x1, y1, tag="keep"):
    g.take((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)), tag)


def reserve(h, g, rid):
    """Door aprons, stair bodies and wells (+ approach / exit), chore spots, secret ends, panel stations, windows."""
    fid = h.fid
    for d in Bm.DOOR_REG:
        if d["fid"] != fid or rid not in d["rooms"]:
            continue
        lx, ly = d["x"] - h.f.x, d["y"] - h.f.y
        tx, ty = -d["ny"], d["nx"]
        for sgn in (-1, 1):
            cx, cy = lx + d["nx"] * sgn * 70.0, ly + d["ny"] * sgn * 70.0
            hw = 85.0 if d["width"] <= 2.0 else 190.0
            ax, ay = abs(tx) * hw + abs(d["nx"]) * 75.0, abs(ty) * hw + abs(d["ny"]) * 75.0
            keep_rect(g, cx - ax, cy - ay, cx + ax, cy + ay)
    for s in Bm.STAIR_REG:
        x0, y0, x1, y1 = s["rect"]
        ux, uy = s["u"]
        if s["kind"] == "ladder":
            if s["lower"] == rid or s["upper"] == rid:
                keep_rect(g, *loc(h, x0 - 1.2, y0 - 0.6), *loc(h, x1 + 0.4, y1 + 0.6), tag="stair")
            continue
        if s["lower"] == rid or s["upper"] == rid:
            keep_rect(g, *loc(h, x0, y0), *loc(h, x1, y1), tag="stair")
        if s["lower"] == rid:           # approach at the bottom
            bx, by = s["bottom"]
            keep_rect(g, *loc(h, bx - ux * 1.4 - abs(uy) * 1.0, by - uy * 1.4 - abs(ux) * 1.0),
                      *loc(h, bx + abs(uy) * 1.0, by + abs(ux) * 1.0))
        if s["upper"] == rid:           # step-off at the top
            tx, ty = s["top"]
            keep_rect(g, *loc(h, tx - abs(uy) * 1.0, ty - abs(ux) * 1.0),
                      *loc(h, tx + ux * 1.4 + abs(uy) * 1.0, ty + uy * 1.4 + abs(ux) * 1.0))
    for a in Bm.L["anchors"]:
        if a["room"] == rid:
            lx, ly = loc(h, *a["at"])
            rr = 95.0
            keep_rect(g, lx - rr, ly - rr, lx + rr, ly + rr)
    for s in Bm.L["secrets"]:
        for end in ("a", "b"):
            if s[end] == rid:
                lx, ly = loc(h, *s["at_" + end])
                keep_rect(g, lx - 110.0, ly - 110.0, lx + 110.0, ly + 110.0)
                # and where the passage puts the traveller: 1.3 m in front, towards the room's middle (builder secrets())
                ax, ay = s["at_" + end]
                cx, cy = (h.rect[0] + h.rect[2]) / 2.0, (h.rect[1] + h.rect[3]) / 2.0
                d = math.hypot(cx - ax, cy - ay) or 1.0
                px, py = loc(h, ax + (cx - ax) / d * 1.3, ay + (cy - ay) / d * 1.3)
                keep_rect(g, px - 80.0, py - 80.0, px + 80.0, py + 80.0)
    for tid, name, prid, (x, y) in Bm.PANELS:
        if prid == rid:
            lx, ly = loc(h, x, y)
            keep_rect(g, lx - 90.0, ly - 90.0, lx + 90.0, ly + 90.0)
    # SPRINT-040: hidden compartments (reach them), traps (a trapdoor / chandelier zone stays open floor, a witness
    # portrait keeps its wall), secret-room hides
    for c in Bm.L.get("compartments", []):
        if c["room"] == rid:
            lx, ly = loc(h, *c["at"])
            keep_rect(g, lx - 80.0, ly - 80.0, lx + 80.0, ly + 80.0)
    for t in Bm.L.get("traps", []):
        if t["room"] == rid and t["effect"] in ("Trapdoor", "FallingObject", "Witness", "Alarm"):
            lx, ly = loc(h, *t["at"])
            zx, zy = t["zone"]
            pad = 60.0 if t["effect"] != "Witness" else 90.0
            keep_rect(g, lx - zx * 100.0 - pad, ly - zy * 100.0 - pad, lx + zx * 100.0 + pad, ly + zy * 100.0 + pad)


def done(h, rid):
    fn = EXT.get(rid)
    if fn and CUR[0] and CUR[0][0] is h:
        try:
            fn(h, CUR[0][1])
        except Exception:
            import traceback
            Bm.log(f"dress ext {rid} FAILED:\n{traceback.format_exc()}")
            Bm.stats.setdefault("dress_failed", []).append("ext_" + rid)
    # SPRINT-040: the luxury layer fills every indoor room up to its density target (layered furniture, pictures,
    # shelves full of small things); the lights still come from the room's own recipe
    if CUR[0] and CUR[0][0] is h and Bm.G.enclosed(Bm.R[rid]):
        try:
            lux(h, CUR[0][1], rid)
        except Exception:
            import traceback
            Bm.log(f"dress lux {rid} FAILED:\n{traceback.format_exc()}")
            Bm.stats.setdefault("dress_failed", []).append("lux_" + rid)
    DRESSED[rid] = DRESSED.get(rid, 0) + h.count


def place(h, g, path, xm, ym, yaw=0.0, scale=(1.0, 1.0, 1.0), check=True, tag="block", collide=True, lz=0.0):
    lx, ly = loc(h, xm, ym)
    return g.place(path, lx, ly, yaw, scale=scale, tag=tag, check=check, collide=collide, lz=lz)


def hang(h, path, xm, ym, zc, yaw=0.0, scale=(1.0, 1.0, 1.0)):
    """Something hanging from the ceiling / mounted in the air (no floor footprint, no collision)."""
    lx, ly = loc(h, xm, ym)
    return h.put(path, lx, ly, zc, yaw, scale=scale, collide=False)


def mounts(h, g, path, walls, z, every=400.0, gap=0.5, skip=0):
    """Mount copies along walls every `every` cm on closed wall (between openings)."""
    n = 0
    for wall in walls:
        lo, hi = (g.x0, g.x1) if wall in ("front", "back") else (g.y0, g.y1)
        t = lo + every / 2.0
        k = 0
        while t < hi - 40.0:
            k += 1
            if k > skip and h.wall_ok(0, wall, t - 50.0, t + 50.0, 999.0):
                if g.wall_mount(path, wall, t, z, gap=gap):
                    n += 1
            t += every
    return n


def along(h, g, path, wall, count, gap=4.0, extra_yaw=0.0, scale=(1.0, 1.0, 1.0), collide=True, tag="block"):
    """Up to `count` copies against one wall, spread out."""
    n = 0
    for _ in range(count):
        it = g.on_wall(path, [wall], gap=gap, extra_yaw=extra_yaw, scale=scale, collide=collide, tag=tag)
        if it:
            n += 1
    return n


def on(h, base, path, u, v, z=None, lyaw=0.0, scale=(1.0, 1.0, 1.0)):
    return KI.on(h, base, path, u, v, z=z, lyaw=lyaw, scale=scale) if base else None


def face(xm, ym, txm, tym):
    """Furniture yaw (it faces its local +Y) that looks from (xm, ym) at (txm, tym)."""
    return math.degrees(math.atan2(-(txm - xm), tym - ym))


def bench_on_wall(h, g, wall, gap=4.0, path=P + "Bench"):
    for lx, ly, yaw, w, t in g.wall_candidates(path, [wall], gap=gap):
        r = g.spot(path, lx, ly, yaw)
        if r and h.wall_ok(0, wall, (r[0] if wall in ("front", "back") else r[1]),
                           (r[2] if wall in ("front", "back") else r[3]), 60.0):
            return KI.seat(g, path, lx, ly, yaw, stand=58.0)
    return None


def hearth_at(h, g, wall, t_world_m, hero=False, mantel=True):
    """A stone fireplace on `wall` as close as possible to the world coordinate t (x for front/back, y for sides)."""
    target = t_world_m * 100.0 - (h.f.x if wall in ("front", "back") else h.f.y)
    item = g.on_wall(I + "Hearth", [wall], prefer=lambda c: abs(c[4] - target), tag="block")
    if not item:
        return None
    x0, y0 = item.at(-100.0, 95.0)
    x1, y1 = item.at(100.0, 150.0)
    keep_rect(g, x0, y0, x1, y1)
    fx, fy = item.at(0.0, 45.0)
    h.hero = hero
    h.light(fx, fy, 70.0, 16.0, 900.0)
    if mantel:
        on(h, item, P + "CandleStick", -70.0, 50.0, lyaw=h.R.uniform(-20, 20))
        on(h, item, h.R.choice([P + "Vase_4", K + "Bottle", P + "Bottle_1"]), 62.0, 45.0, lyaw=h.R.uniform(0, 360))
        on(h, item, P + "Mug", 30.0, 40.0, lyaw=h.R.uniform(0, 360))
    for s in (1, -1):
        x, y = item.at(s * 136.0, 34.0)
        if g.spot(I + "Firewood", x, y, item.lyaw):
            g.place(I + "Firewood", x, y, item.lyaw)
            break
    return item


def wall_table_row(h, g, path, wall, n, gap=3.0):
    items = []
    for _ in range(n):
        it = g.on_wall(path, [wall], gap=gap)
        if it:
            items.append(it)
    return items


def bookcases(h, g, walls, n):
    out = []
    for _ in range(n):
        bc = g.on_wall(P + "Bookcase_2", walls, tag="tall")
        if not bc:
            break
        for z in KI.SHELVES[P + "Bookcase_2"][1:]:
            if h.R.random() < 0.9:
                on(h, bc, h.R.choice(KI.BOOKS), h.R.uniform(-8, 8), 2.0, z=z + 0.2)
        on(h, bc, h.R.choice(KI.SMALL_BOOKS), h.R.uniform(-40, 40), 0.0, z=KI.SHELVES[P + "Bookcase_2"][0] + 0.2,
           lyaw=h.R.uniform(-30, 30))
        out.append(bc)
    return out


def rug_runner(h, g, x0m, y0m, x1m, y1m):
    """Runner rugs end to end along a corridor centre line (visual, walkable)."""
    horiz = abs(x1m - x0m) >= abs(y1m - y0m)
    length = abs(x1m - x0m) if horiz else abs(y1m - y0m)
    n = max(1, int(length // 2.6))
    for k in range(n):
        f = (k + 0.5) / n
        xm, ym = x0m + (x1m - x0m) * f, y0m + (y1m - y0m) * f
        lx, ly = loc(h, xm, ym)
        h.put(I + "Rug_Runner", lx, ly, g.z + 0.3, 0.0 if horiz else 90.0, collide=False)


def lamp_post(h, g, xm, ym, yaw, lit=True, tilt=0.0):
    lx, ly = loc(h, xm, ym)
    if not g.spot(V + "Corner_Exterior_Wood", lx, ly, yaw):
        return
    h.put(V + "Corner_Exterior_Wood", lx, ly, g.z - 2.0, yaw, roll=tilt, scale=(1.4, 1.4, 1.05))
    h.put(P + "Lantern_Wall", lx, ly, g.z + 175.0, yaw, roll=tilt, collide=False)
    g.take((lx - 20, ly - 20, lx + 20, ly + 20), "block")
    if lit:
        c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        h.light(lx - 105.0 * s_, ly + 105.0 * c, 230.0, 12.0, 1100.0)


def scatter(h, g, paths, count, pad=10.0, yaw_any=True, collide=True, tries=30):
    n = 0
    for _ in range(count * tries):
        if n >= count:
            break
        path = h.R.choice(paths)
        lx = h.R.uniform(g.x0 + 60, g.x1 - 60)
        ly = h.R.uniform(g.y0 + 60, g.y1 - 60)
        yaw = h.R.uniform(0, 360) if yaw_any else 0.0
        if g.spot(path, lx, ly, yaw, pad=pad):
            g.place(path, lx, ly, yaw, collide=collide)
            n += 1
    return n


def keep_ring(h, g, cxm, cym, radius, half=0.55):
    for k in range(40):
        a = 2 * math.pi * k / 40
        lx, ly = loc(h, cxm + radius * math.cos(a), cym + radius * math.sin(a))
        keep_rect(g, lx - half * 100, ly - half * 100, lx + half * 100, ly + half * 100)


# =================================================================================================== ground floor
def great_hall():
    h, g = room_for("great_hall", cap=2, style=1)
    # the meeting ring (player starts r 7 m) and the gather arc stay free
    keep_ring(h, g, 0.0, -9.0, 7.0)
    keep_ring(h, g, 0.0, -9.0, 6.5)
    # the long table: four oak tables end to end on runners, 9 chairs a side, a carved chair at each end
    tables = []
    for k in range(4):
        ym = -13.275 + k * 2.85
        t = place(h, g, P + "Table_Large", 0.0, ym, 90.0, check=False)
        tables.append(t)
        lx, ly = loc(h, 0.0, ym)
        h.put(I + "Rug_Runner", lx, ly, g.z + 0.3, 90.0, scale=(1.15, 1.9, 1.0), collide=False)
    for k in range(9):
        if k in (0, 4):
            continue            # gaps in the benches: the decanter (hall_table) and the chandelier chore are reached here
        ym = -13.9 + k * 1.2
        for side, yaw in ((-1.25, -90.0), (1.25, 90.0)):
            lx, ly = loc(h, side, ym)
            KI.seat(g, P + "Chair_1", lx, ly, yaw + h.R.uniform(-4, 4), allow=("keep",), stand=-60.0)
    head = loc(h, 0.0, -15.25)
    KI.seat(g, BD + "FancyChairRed", head[0], head[1], 0.0, allow=("keep",), stand=-60.0)       # Godot's: empty
    hx, hy = loc(h, 0.0, -15.25)
    h.put(P + "Pouch_Large", hx, hy + 5.0, g.z + 50.0, 20.0, scale=(2.4, 2.4, 2.0), collide=False)   # his blue hat
    foot = loc(h, 0.0, -2.75)
    KI.seat(g, BD + "FancyChairRed", foot[0], foot[1], 180.0, allow=("keep",), stand=-60.0)
    for t in tables:
        for u in (-100.0, 0.0, 100.0):
            for v in (-32.0, 32.0):
                on(h, t, h.R.choice([P + "Table_Plate", K + "Plate"]), u + h.R.uniform(-6, 6), v)
                if h.R.random() < 0.7:
                    on(h, t, h.R.choice([P + "Chalice", P + "Mug"]), u + 18.0, v * 0.55, lyaw=h.R.uniform(0, 360))
        on(h, t, P + "CandleStick_Triple", h.R.uniform(-30, 30), 0.0)
    on(h, tables[1], K + "Bottle", 40.0, 0.0)                                            # the decanter
    on(h, tables[2], I + "Bread", -40.0, 0.0, lyaw=30.0)
    on(h, tables[2], I + "Cheese", 60.0, 5.0)
    # the hearth on the north wall (hero light), Pozzo's banners, candle stands, armour
    hearth_at(h, g, "front", 2.5, hero=True)
    for xm in (-6.0, -3.0, 6.0):
        lx, ly = loc(h, xm, -20.0)
        g.wall_mount(P + "Banner_1_Cloth", "front", lx, 290.0)
    for wall in ("left", "right"):
        for ym in (-17.0, -12.0):
            g.wall_mount(P + "Banner_2_Cloth", wall, loc(h, 0, ym)[1], 285.0)
    for xm, ym in ((-5.0, -17.5), (5.0, -17.5), (-5.0, -1.2), (5.0, -1.2)):
        place(h, g, P + "CandleStick_Stand", xm, ym)
    for xm, ym, yaw in ((-8.5, -18.7, 0.0), (8.5, -18.7, 0.0), (-8.6, -12.0, 90.0), (8.6, -12.0, -90.0)):
        st = place(h, g, P + "WeaponStand", xm, ym, yaw)
    # the great chandelier (hangs from the clock room's rope hole) and its twin over the south end
    for ym in (-13.5, -5.0):
        hang(h, P + "Chandelier", 0.0, ym, 2.0 * Bm.H - 20.0, scale=(2.2, 2.2, 2.0))
    lx, ly = loc(h, 0.0, -9.0)
    h.light(lx, ly, 480.0, 34.0, 1800.0)
    storage_small(h, g, 3)
    done(h, "great_hall")


def storage_small(h, g, n):
    KI.storage(h, g, n)


def vestibule():
    h, g = room_for("vestibule", cap=1, style=2)
    # post desk with pigeonholes (the Satchel spot), the G. parcel corner, coat racks, benches, umbrella stand
    desk = place(h, g, P + "Cabinet", 7.0, 9.25, 180.0)
    if desk:
        for u in (-40.0, 0.0, 40.0):
            on(h, desk, h.R.choice([P + "Book_Stack_1", P + "Scroll_1", P + "BookGroup_Small_2"]), u, 0.0)
    shelf = g.wall_mount(P + "Shelf_Simple", "back", loc(h, 7.0, 0)[0], 170.0)
    for k in range(3):
        on(h, shelf, P + "Scroll_2", -35.0 + k * 35.0, 14.0, z=10.1, lyaw=h.R.uniform(-20, 20))
    for xm in (-7.0, -5.3):
        place(h, g, P + "Crate_Wooden", xm, 9.0, h.R.uniform(-10, 10), scale=(0.8, 0.8, 0.8))
    for wall in ("front",):
        for xm in (-6.0, 6.0):
            g.wall_mount(P + "Peg_Rack", wall, loc(h, xm, 0)[0], 165.0)
    for xm, ym in ((-9.0, 5.0), (9.0, 5.0)):
        it = place(h, g, P + "Bucket_Metal", xm, ym)
        on(h, it, P + "Sword_Bronze", 0.0, 0.0, z=5.0, lyaw=20.0)
    for wall in ("left", "right"):
        bench_on_wall(h, g, wall)
    mounts(h, g, P + "Lantern_Wall", ["left", "right"], 150.0, every=500.0)
    for xm in (-4.0, 4.0):
        hang(h, P + "Chandelier", xm, 5.0, Bm.H - 10.0)
    lx, ly = loc(h, 0.0, 5.0)
    h.light(lx, ly, 250.0, 16.0, 1200.0)
    done(h, "vestibule")


def dining():
    h, g = room_for("dining", cap=1, style=3)
    # the banquet laid for thirteen
    ts = [place(h, g, P + "Table_Large", -16.0, -14.4 + k * 2.85, 90.0, check=False) for k in range(2)]
    for k in range(6):
        ym = -15.3 + k * 1.05
        for side, yaw in ((-1.25, -90.0), (1.25, 90.0)):
            lx, ly = loc(h, -16.0 + side, ym)
            KI.seat(g, P + "Chair_1", lx, ly, yaw, allow=("keep",), stand=-60.0)
    lx, ly = loc(h, -16.0, -16.2)
    KI.seat(g, BD + "FancyChairRed", lx, ly, 0.0, allow=("keep",), stand=-60.0)
    for t in ts:
        if t:
            for u in (-95.0, -30.0, 30.0, 95.0):
                for v in (-30.0, 30.0):
                    on(h, t, K + "Plate", u, v)
                    on(h, t, P + "Table_Fork", u + 20.0, v, lyaw=90.0)
            on(h, t, P + "CandleStick_Triple", 0.0, 0.0)
    # sideboards, portraits (tapestries), candelabras
    for wall in ("left", "back"):
        sb = g.on_wall(P + "Cabinet", [wall], gap=3.0)
        if sb:
            on(h, sb, P + "CandleStick_Triple", -40.0, 0.0)
            on(h, sb, P + "Vase_4", 35.0, 0.0)
    mounts(h, g, P + "Banner_2_Cloth", ["left", "front"], 280.0, every=450.0)
    KI.tall_storage(h, g)
    h.light(*loc(h, -16.0, -13.0), 250.0, 14.0, 1100.0)
    done(h, "dining")


def kitchen():
    h, g = room_for("kitchen", cap=1, style=4)
    fire = hearth_at(h, g, "left", -14.0, hero=True)
    KI.stove(h, g)
    KI.stove(h, g)
    KI.counter(h, g, fire)
    KI.counter(h, g, fire)
    place(h, g, P + "Cauldron", -29.5, -22.4)
    sink = g.on_wall(K + "Sink", ["front"], gap=3.0)
    t = place(h, g, K + "LongTable", -26.5, -18.0, 0.0)
    if t:
        for u in (-55.0, -10.0, 35.0):
            on(h, t, h.R.choice([I + "Bread", I + "Cheese", K + "Bowl", K + "SmallPot", P + "Carrot"]), u, 0.0,
               lyaw=h.R.uniform(0, 360))
        for side in (-1, 1):
            x, y = t.at(0.0, side * 70.0)
            KI.seat(g, P + "Stool", x, y, 0.0, allow=("keep",))
    for _ in range(2):
        KI.tall_storage(h, g)
    for xm in (-31.0, -27.0, -24.0):
        g.wall_mount(I + "HerbRack", "front", loc(h, xm, 0)[0], 205.0)
    # the dumbwaiter (S2) in the north-west corner: a crate on a rope
    hang(h, P + "Rope_2", -33.0, -23.2, 260.0)
    KI.storage(h, g, 4)
    done(h, "kitchen")


def pantry():
    h, g = room_for("pantry", cap=1, style=5)
    for _ in range(4):
        KI.tall_storage(h, g)
    KI.storage(h, g, 10)
    for _ in range(4):
        g.on_wall(P + "Bag", ["left", "right", "back", "front"], extra_yaw=h.R.uniform(-30, 30))
    china = place(h, g, K + "Shelf2", -32.6, 2.6, 180.0)
    if china:
        for z in KI.SHELVES[K + "Shelf2"]:
            for u in (-25.0, 0.0, 25.0):
                on(h, china, K + "Plate", u, 0.0, z=z + 0.2)
    lx, ly = loc(h, -27.0, 0.5)
    h.put(P + "Vase_Rubble_Medium", lx, ly, g.z, 0.0, collide=False)          # the flour on the floor
    h.light(*loc(h, -27.0, 0.0), 240.0, 10.0, 900.0)
    done(h, "pantry")


def laundry():
    h, g = room_for("laundry", cap=1, style=6)
    for xm, ym in ((-20.0, -4.0), (-13.0, -4.0), (-12.5, 1.5)):
        it = place(h, g, P + "Bucket_Metal", xm, ym, h.R.uniform(0, 90), scale=(2.4, 2.4, 1.6))
    place(h, g, P + "Whetstone", -20.5, 2.5, 90.0)                          # the mangle
    for xm, ym, yaw in ((-16.0, -2.5, 0.0), (-16.0, 1.2, 0.0)):
        lx, ly = loc(h, xm, ym)
        if g.spot(DV + "LaundryLine", lx, ly, yaw):
            g.place(DV + "LaundryLine", lx, ly, yaw, collide=False, tag="keep")
    bench = g.on_wall(P + "Workbench", ["front", "back"], gap=3.0)
    if bench:
        on(h, bench, P + "Bag", -50.0, 0.0, scale=(0.6, 0.6, 0.6))
        on(h, bench, K + "Bowl", 40.0, 0.0)
    KI.storage(h, g, 4)
    h.light(*loc(h, -16.0, -1.0), 240.0, 10.0, 900.0)
    done(h, "laundry")


def chapel():
    h, g = room_for("chapel", cap=1, style=7)
    altar = place(h, g, P + "Cabinet", 16.0, -19.6, 0.0)
    if altar:
        for u in (-45.0, 45.0):
            on(h, altar, P + "CandleStick_Triple", u, 0.0)
        on(h, altar, P + "Book_7", 0.0, 2.0)
    for xm in (14.2, 17.8):
        place(h, g, P + "CandleStick_Stand", xm, -19.2)
    if Bm.have(DU + "CandleCluster"):
        for xm in (11.0, 21.0):
            place(h, g, DU + "CandleCluster", xm, -19.4, collide=False)
    ym = -17.2
    while ym < -12.5:
        for xm in (13.8, 18.2):
            lx, ly = loc(h, xm, ym)
            if g.spot(P + "Bench", lx, ly, 180.0):
                KI.seat(g, P + "Bench", lx, ly, 180.0 + h.R.uniform(-2, 2), stand=58.0)
        ym += 1.15
    # the confessional (S1's secret end): a wardrobe against the east wall
    place(h, g, BD + "Wardrobe", 21.45, -17.8, 90.0, check=False, tag="tall")
    mounts(h, g, P + "Banner_1_Cloth", ["left", "right"], 285.0, every=350.0)
    g.on_wall(P + "BookStand", ["left"], extra_yaw=h.R.uniform(-10, 10))
    h.light(*loc(h, 16.0, -18.0), 230.0, 12.0, 900.0)
    done(h, "chapel")


def library():
    h, g = room_for("library", cap=2, style=8)
    hearth_at(h, g, "right", -20.0, hero=True)
    # the rotating bookcase (S1): on the west wall at the confessional
    rb = place(h, g, P + "Bookcase_2", 22.45, -16.0, -90.0, check=False, tag="tall")
    if rb:
        on(h, rb, P + "Book_Simplified_Single", 20.0, 2.0, z=KI.SHELVES[P + "Bookcase_2"][2] + 0.2)   # the red book
    bookcases(h, g, ["front"], 5)
    bookcases(h, g, ["left"], 3)
    bookcases(h, g, ["back"], 2)
    # ladders against the shelves, a reading corner by the fire, a globe on a stand, the desk
    if Bm.have(DU + "Ladder_4m"):
        for xm in (25.5, 30.0):
            hang(h, DU + "Ladder_4m", xm, -23.1, 0.0, yaw=0.0, scale=(1.0, 1.0, 0.75))
    for xm, ym in ((30.4, -18.4), (30.4, -21.8)):
        lx, ly = loc(h, xm, ym)
        yaw = face(xm, ym, 33.5, -20.0)
        if g.spot(BD + "FancyChairRed", lx, ly, yaw):
            KI.seat(g, BD + "FancyChairRed", lx, ly, yaw)
    stand = place(h, g, P + "BookStand", 27.5, -14.0, 30.0)
    lx, ly = loc(h, 27.5, -14.0)
    h.put(E + "Sphere", lx, ly, g.z + 175.0, 0.0, scale=(0.55, 0.55, 0.55), collide=False)
    KI.desk(h, g)
    KI.centre_rug(h, g, loc(h, 28.0, -17.0))
    h.light(*loc(h, 26.0, -13.0), 240.0, 12.0, 1000.0)
    done(h, "library")


def ballroom():
    h, g = room_for("ballroom", cap=2, style=9)
    # the orchestrion (music-box organ): cabinet base, pipes, a bookcase case
    base = place(h, g, P + "Cabinet", 33.6, -7.0, 90.0, check=False)
    lx, ly = loc(h, 33.5, -7.0)
    for k in range(7):
        hgt = 1.0 + 0.35 * (3 - abs(k - 3))
        h.put(V + "Corner_Exterior_Wood", lx - 10.0, ly - 90.0 + k * 30.0, g.z + 100.0, 0.0,
              scale=(0.9, 0.9, hgt), collide=False)
    # chairs under dust sheets along the walls, candle stands, two chandeliers
    for wall in ("front", "back"):
        for _ in range(6):
            it = g.on_wall(P + "Chair_1", [wall], gap=6.0)
            if it:
                on(h, it, E + "Cube", 0.0, 0.0, z=48.0, scale=(0.62, 0.62, 0.02))
    for xm, ym in ((11.0, -9.0), (11.0, 5.0), (33.0, 5.0), (33.0, -9.0), (22.0, -9.0)):
        place(h, g, P + "CandleStick_Stand", xm, ym)
    mounts(h, g, P + "Banner_2_Cloth", ["front"], 290.0, every=500.0)
    for xm in (16.0, 28.0):
        hang(h, P + "Chandelier", xm, -2.0, Bm.H - 6.0, scale=(1.6, 1.6, 1.6))
    lx, ly = loc(h, 22.0, -2.0)
    for rug, dx in ((I + "Rug_Rect", -3.0), (I + "Rug_Rect", 3.0)):
        h.put(rug, lx + dx * 100.0, ly, g.z + 0.3, 0.0, scale=(1.8, 1.8, 1.0), collide=False)
    h.light(*loc(h, 16.0, -2.0), 260.0, 16.0, 1300.0)
    h.light(*loc(h, 28.0, -2.0), 260.0, 16.0, 1300.0)
    done(h, "ballroom")


def servants_corridor():
    h, g = room_for("servants_corridor", cap=2, style=10)
    # bell board by the kitchen end, coat hooks, crates: all against the north wall (2 m stays clear)
    for xm in (-19.0, -8.0, 4.0, 13.0):
        g.wall_mount(P + "Peg_Rack", "front", loc(h, xm, 0)[0], 160.0)
    for xm in (-20.0, 20.5):
        place(h, g, P + "Crate_Wooden", xm, -23.4, h.R.uniform(-8, 8), scale=(0.7, 0.7, 0.7))
    board = g.wall_mount(P + "Shelf_Small_Bottles", "front", loc(h, -14.0, 0)[0], 170.0)
    for xm in (-21.0, 21.0):
        lx, ly = loc(h, xm, -22.0)
        h.light(lx, ly, 240.0, 8.0, 700.0)
    rug_runner(h, g, -18.0, -22.0, 18.0, -22.0)
    done(h, "servants_corridor")


# =================================================================================================== cellar
def wine_cellar():
    h, g = room_for("wine_cellar", cap=1, style=11)
    for _ in range(9):
        g.on_wall(P + "Barrel_Holder", ["front", "back", "left", "right"], tall=126.0, gap=4.0)
    for xm, ym in ((-24.0, -13.8), (-22.6, -13.8), (-25.4, -13.8)):
        place(h, g, PIR + "Barrel_0", xm, ym, h.R.uniform(0, 360), scale=(0.9, 0.9, 0.9))
    for xm in range(-32, -19, 3):
        place(h, g, PIR + h.R.choice(["Barrel_3", "Barrel_4", "Barrel_10"]), xm + 0.5, -2.0, h.R.uniform(0, 360),
              scale=(0.9, 0.9, 0.9))
    # the false wine rack at S4's end
    place(h, g, P + "Barrel_Holder", -33.4, -14.0, 90.0, check=False)
    if Bm.have(DU + "CandleCluster"):
        for xm, ym in ((-19.0, -15.2), (-33.0, -1.0)):
            place(h, g, DU + "CandleCluster", xm, ym, collide=False)
    KI.storage(h, g, 4)
    h.light(*loc(h, -26.0, -9.0), 240.0, 9.0, 1000.0)
    done(h, "wine_cellar")


def spark_room():
    h, g = room_for("spark_room", cap=1, style=12)
    board = place(h, g, P + "Cabinet", -12.0, -15.4, 0.0, check=False)
    lx, ly = loc(h, -12.0, -15.6)
    h.put(E + "Cube", lx, ly, g.z + 150.0, 0.0, scale=(1.2, 0.08, 0.8), collide=False)
    if board:
        on(h, board, P + "Lantern_Wall", 0.0, 0.0, lyaw=0.0)
    for xm in (-16.5, -7.0):
        c = place(h, g, P + "Workbench", xm, -14.9, 0.0)
        if c:
            for u in (-70.0, -25.0, 20.0, 65.0):
                on(h, c, h.R.choice([P + "Potion_2", P + "Potion_1", P + "Potion_4", P + "SmallBottles_1"]), u, 0.0)
    for _ in range(3):
        g.on_wall(P + "Shelf_Small_Bottles", ["left", "back"], gap=3.0)
    for xm, ym in ((-15.0, -8.0), (-10.0, -8.0)):
        lx, ly = loc(h, xm, ym)
        for k in range(4):
            h.put(P + "Chain_Coil", lx, ly, g.z + 5.0 + k * 9.0, k * 25.0, collide=False, scale=(0.8, 0.8, 1.0))
        g.take((lx - 55, ly - 55, lx + 55, ly + 55), "block")
    place(h, g, P + "Cage_Small", -17.0, -5.0)
    lx, ly = loc(h, -12.0, -10.0)
    Bm.light(lx + h.f.x, ly + h.f.y, h.f.z + 240.0, 14.0, 900.0, color=(140, 185, 255))
    done(h, "spark_room")


def cistern():
    h, g = room_for("cistern", cap=1, style=13)
    # columns standing in black water, plank walkways, drips
    for xm in (-2.0, 2.0, 6.0, 10.0):
        for ym in (-14.0, -6.0):
            lx, ly = loc(h, xm, ym)
            if g.spot(V + "Corner_ExteriorWide_Brick", lx, ly, 0.0):
                g.place(V + "Corner_ExteriorWide_Brick", lx, ly, 0.0)
    lx, ly = loc(h, 4.0, -10.0)
    for xm, ym, sx, sy in ((-1.0, -10.0, 7.0, 7.0), (9.0, -10.0, 6.0, 7.0)):
        wx, wy = loc(h, xm, ym)
        h.put(E + "Plane", wx, wy, g.z + 1.5, 0.0, scale=(sx, sy, 1.0), collide=False)
        Bm.B.recs[-1]["material"] = "/Game/KillGodot/Materials/M_KG_PondWater"
    for xm in (-4.0, 12.0):
        for k in range(4):
            wx, wy = loc(h, xm, -16.0 + k * 3.0)
            h.put(PIR + "Planks_0", wx, wy, g.z + 3.0, 90.0, collide=False)
    KI.storage(h, g, 3)
    Bm.light(lx + h.f.x, ly + h.f.y, h.f.z + 260.0, 10.0, 1300.0, color=(150, 210, 190))
    done(h, "cistern")


def smugglers_tunnel():
    h, g = room_for("smugglers_tunnel", cap=2, style=14)
    # block the inside of the L (bounding rect x -30..-12, y 0..42)
    keep_rect(g, *loc(h, -26.0, 0.0), *loc(h, -12.0, 38.0), tag="block")
    for ym in (6.0, 16.0, 26.0, 34.0):
        lx, ly = loc(h, -29.6, ym)
        h.put(PIR + "Torch_0", lx, ly, g.z, 90.0, scale=(0.8, 0.8, 0.8), collide=False)
    for xm, ym in ((-26.8, 3.0), (-26.8, 12.0), (-29.0, 21.0), (-26.8, 30.0), (-20.0, 41.2), (-15.0, 38.8)):
        place(h, g, PIR + h.R.choice(["crates_0", "crates_1", "Barrel_2"]), xm, ym, h.R.uniform(0, 90),
              scale=(0.8, 0.8, 0.8))
    h.light(*loc(h, -28.0, 10.0), 200.0, 8.0, 1200.0)
    h.light(*loc(h, -24.0, 40.0), 200.0, 8.0, 1200.0)
    done(h, "smugglers_tunnel")


def boathouse():
    h, g = room_for("boathouse", cap=2, style=15)
    # the slip: rowboat on its ramp by the sea doors, the pump and mooring, nets, oars, the "G." parcel
    boat = place(h, g, WP + "Rowboat", 0.0, 46.0, 90.0, check=False)
    place(h, g, P + "Anvil_Log", 6.0, 46.9, 0.0, check=False)
    place(h, g, P + "Bucket_Metal", 6.8, 46.2, 0.0)
    if Bm.have(DH + "Bollard"):
        place(h, g, DH + "Bollard", -2.0, 49.3, 0.0, check=False)
    lx, ly = loc(h, -2.0, 48.6)
    h.put(P + "Rope_3", lx, ly, g.z + 1.0, 0.0, collide=False)
    for path, wall in ((DH + "NetRack", "left"), (DH + "FishRack", "right")):
        if Bm.have(path):
            g.on_wall(path, [wall], gap=6.0, tag="tall")
    for path in (DH + "NetPile", DH + "LobsterTrapStack", DH + "FishBarrel", DH + "Oars", DH + "FishBasket"):
        if Bm.have(path):
            g.on_wall(path, ["front", "left", "right"], gap=8.0, extra_yaw=h.R.uniform(-15, 15))
    mounts(h, g, DH + "Lifebuoy" if Bm.have(DH + "Lifebuoy") else P + "Shield_Wooden", ["front"], 170.0, every=600.0)
    for xm, ym in ((-10.0, 49.0), (10.0, 39.0), (-6.0, 39.0)):
        place(h, g, PIR + h.R.choice(["crates_0", "crates_1", "Barrel_1"]), xm, ym, h.R.uniform(0, 90))
    for ym in (39.2,):
        for xm in (-4.0, 4.0):
            lx, ly = loc(h, xm, ym)
            h.put(P + "Lantern_Wall", lx, ly, g.z + 150.0, 0.0, collide=False)
    h.light(*loc(h, -4.0, 42.0), 250.0, 12.0, 1200.0)
    h.light(*loc(h, 5.0, 45.0), 250.0, 12.0, 1200.0)
    done(h, "boathouse")


# =================================================================================================== first floor
def gallery():
    h, g = room_for("gallery", cap=1, style=16)
    keep_rect(g, *loc(h, -6.0, -16.0), *loc(h, 6.0, 0.0), tag="block")          # the hall void
    # benches facing the void (watch the meeting table), busts on stands, portraits (tapestries) on the walls
    for xm, ym, yaw in ((-8.0, -13.0, -90.0), (8.0, -13.0, 90.0), (-3.0, -16.9, 0.0), (3.5, -16.9, 0.0)):
        lx, ly = loc(h, xm, ym)
        if g.spot(P + "Bench", lx, ly, yaw, allow=("keep",)):
            KI.seat(g, P + "Bench", lx, ly, yaw, allow=("keep",), stand=58.0)
    for xm, ym in ((-9.2, -19.2), (9.2, -19.2), (-9.2, -12.5), (9.2, -12.5)):
        st = place(h, g, P + "Nightstand_Shelf", xm, ym, 0.0)
        on(h, st, P + "Vase_2", 0.0, 0.0, scale=(0.55, 0.55, 0.8))
    mounts(h, g, P + "Banner_1_Cloth", ["front"], 285.0, every=380.0)
    mounts(h, g, P + "Banner_2_Cloth", ["left", "right"], 285.0, every=420.0)
    # Anselm's portrait (S3): a dark framed canvas on the west wall, its eyes over the long table
    lx, ly = loc(h, -9.75, -1.2)
    h.put(E + "Cube", lx, ly, g.z + 175.0, 0.0, scale=(0.06, 0.9, 1.2), collide=False)
    Bm.B.recs[-1]["material"] = Bm.MAT.get("Portrait")
    h.put(P + "Shield_Wooden", lx + 6.0, ly, g.z + 240.0, -90.0 + 180.0, scale=(0.5, 0.5, 0.5), collide=False)
    # the empty frame for the portrait chore and the chandelier rope's cleat
    g.wall_mount(P + "Shelf_Arch", "front", loc(h, -4.0, 0)[0], 110.0)
    h.light(*loc(h, 0.0, -18.0), 250.0, 10.0, 1200.0)
    done(h, "gallery")


def corridor(rid, clocks):
    h, g = room_for(rid, cap=1, style=17 if clocks else 18)
    x0, y0, x1, y1 = h.rect
    rug_runner(h, g, x0 + 1.5, (y0 + y1) / 2, x1 - 1.5, (y0 + y1) / 2)
    mounts(h, g, P + "Lantern_Wall", ["front", "back"], 150.0, every=700.0)
    if clocks:
        face = DT + "ClockFace" if Bm.have(DT + "ClockFace") else DL + "ClockDial"
        mounts(h, g, face, ["front", "back"], 190.0, every=520.0, skip=0)
    for wall in ("front", "back"):
        it = g.on_wall(BD + "BedsideCabinet", [wall], gap=3.0)
        if it:
            on(h, it, P + "Vase_4", 0.0, 0.0)
    h.light(*loc(h, (x0 + x1) / 2, (y0 + y1) / 2), 240.0, 9.0, 1300.0)
    done(h, rid)


def bedroom_room(rid, colour, style, fire=None, hero=False, extra=None):
    h, g = room_for(rid, cap=1, style=style)
    if fire:
        f = hearth_at(h, g, fire[0], fire[1], hero=hero)
        KI.fireside_seat(h, g, f)
        KI.hearth_rug(h, g, f)
    bed = None
    for path in (BD + f"DoubleBed{colour}", P + "Bed_Twin1"):
        bed = g.on_wall(path, ["back", "right", "left", "front"], gap=3.0, tall=90.0)
        if bed:
            break
    if bed:
        x0, y0, _, x1, y1, _ = KI.bounds(bed.path)
        for side in (-1, 1):
            nb = KI.bounds(BD + "BedsideTable")
            u = (x1 + 6 - nb[0]) if side > 0 else (x0 - 6 - nb[3])
            x, y = bed.at(u, y0 - nb[1] + 1.0)
            it = g.place(BD + "BedsideTable", x, y, bed.lyaw) if g.spot(BD + "BedsideTable", x, y, bed.lyaw) else None
            if it:
                on(h, it, P + "CandleStick", 0.0, 0.0)
        ch = BD + "ChestRoundIron"
        cb = KI.bounds(ch)
        x, y = bed.at(0.0, y1 + 4.0 - cb[1])
        if g.spot(ch, x, y, bed.lyaw):
            g.place(ch, x, y, bed.lyaw)
    KI.wardrobe(h, g)
    KI.desk(h, g)
    KI.corner_set(h, g)
    if extra:
        extra(h, g)
    KI.centre_rug(h, g, (0.0, 0.0))
    if h.lights < 1:
        h.light(0.0, 0.0, 240.0, 10.0, 1000.0)
    done(h, rid)
    return h, g


def bath():
    h, g = room_for("bath", cap=1, style=19)
    for xm, ym in ((-19.5, -21.5), (-15.5, -21.5)):
        tub = place(h, g, P + "Bucket_Metal", xm, ym, 0.0, scale=(3.4, 2.0, 1.5))
        if tub:
            Bm.B.recs[-1]["material"] = Bm.MAT.get("Copper")
    place(h, g, PIR + "Barrel_5", -11.4, -22.5, 0.0, scale=(0.9, 0.9, 1.3))
    lx, ly = loc(h, -11.4, -22.5)
    h.put(V + "Prop_Chimney2", lx, ly, g.z + 150.0, 0.0, scale=(0.25, 0.25, 0.5), collide=False)
    lx, ly = loc(h, -19.0, -14.8)
    if g.spot(DV + "LaundryLine", lx, ly, 0.0):
        g.place(DV + "LaundryLine", lx, ly, 0.0, collide=False, tag="keep")
    bench_on_wall(h, g, "left")
    g.on_wall(P + "Bucket_Wooden_1", ["front", "left"], extra_yaw=h.R.uniform(0, 360))
    KI.corner_set(h, g)
    h.light(*loc(h, -16.0, -18.0), 240.0, 10.0, 1000.0)
    done(h, "bath")


def studio():
    h, g = room_for("studio", cap=1, style=20)
    for xm, ym, yaw in ((-16.0, -3.0, 200.0), (-19.5, 1.5, 160.0), (-12.5, 1.5, 200.0)):
        st = place(h, g, P + "BookStand", xm, ym, yaw)
        if st:
            lx, ly = loc(h, xm, ym)
            c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            h.put(P + "Shield_Wooden", lx - s_ * 12.0, ly + c * 12.0, g.z + 128.0, yaw, pitch=0.0, scale=(1.4, 1.0, 1.3),
                  collide=False)
    place(h, g, P + "Dummy", -12.0, -6.0, 150.0)
    lx, ly = loc(h, -20.0, -6.0)
    KI.seat(g, BD + "FancyChairRed", lx, ly, 45.0)
    for _ in range(3):
        it = g.on_wall(P + "Crate_Wooden", ["front", "left"], extra_yaw=h.R.uniform(-15, 15), scale=(0.7, 0.7, 0.7))
        if it:
            on(h, it, E + "Cube", 0.0, 0.0, scale=(0.62, 0.66, 0.02))
    bench = g.on_wall(P + "Workbench", ["back", "left"], gap=3.0)
    if bench:
        for u in (-70.0, -30.0, 10.0, 50.0):
            on(h, bench, h.R.choice([P + "Pot_1", P + "Potion_1", P + "Potion_2", K + "Jar", K + "YellowJar"]), u, 0.0)
    h.light(*loc(h, -16.0, -2.0), 240.0, 10.0, 1000.0)
    done(h, "studio")


def nursery():
    h, g = room_for("nursery", cap=1, style=21)
    keep_rect(g, *loc(h, -6.0, 0.0), *loc(h, 6.0, 1.2))      # the inner window over the hall
    bed = g.on_wall(BD + "SingleBedBlue", ["back"], gap=3.0, tall=90.0)
    # the blue hat on its hook, always ready
    peg = g.wall_mount(P + "Peg_Rack", "right", loc(h, 0, 2.5)[1], 165.0)
    lx, ly = loc(h, 9.7, 2.5)
    h.put(P + "Pouch_Large", lx - 12.0, ly, g.z + 150.0, 90.0, scale=(2.0, 2.0, 1.8), collide=False)
    box = place(h, g, BD + "BedsideTable", 4.0, 5.2, 0.0)
    on(h, box, P + "CandleStick", 0.0, 0.0)                     # the night light, still lit
    for xm, ym, yaw in ((-3.0, 4.0, 30.0), (1.5, 6.5, -60.0)):
        lx, ly = loc(h, xm, ym)
        h.put(WP + "Rowboat", lx, ly, g.z + 5.0, yaw, scale=(0.18, 0.18, 0.18), collide=False)
    place(h, g, BD + "ChestRoundIron", -8.6, 6.8, 0.0)
    # the wardrobe hiding S6 in the south-east corner
    place(h, g, BD + "Wardrobe", 8.2, 7.35, 180.0, check=False, tag="tall")
    lx, ly = loc(h, 0.0, 4.5)
    h.put(I + "Rug_Round", lx, ly, g.z + 0.3, 0.0, collide=False)
    h.light(*loc(h, 0.0, 4.0), 240.0, 9.0, 1000.0)
    done(h, "nursery")


def study():
    h, g = room_for("study", cap=1, style=22)
    t = place(h, g, P + "Table_Large", 16.0, -19.0, 0.0)
    if t:
        for u, p in ((-90.0, P + "Scroll_1"), (-30.0, P + "Scroll_2"), (40.0, P + "Book_7"), (100.0, P + "Candle_2")):
            on(h, t, p, u, h.R.uniform(-15, 15), lyaw=h.R.uniform(-40, 40))
        x, y = t.at(0.0, 80.0)
        KI.seat(g, BD + "FancyChairRed", x, y, 180.0, stand=-62.0)
    place(h, g, P + "Crate_Metal", 21.3, -23.3, 0.0, scale=(0.7, 0.7, 0.8))            # the safe
    if Bm.have(DL + "Telescope"):
        place(h, g, DL + "Telescope", 12.0, -14.0, 135.0, scale=(0.6, 0.6, 0.6))
    ws = g.on_wall(P + "WeaponStand", ["left", "back"], gap=4.0)
    mounts(h, g, P + "Shield_Wooden", ["left"], 200.0, every=320.0)
    bookcases(h, g, ["back", "right"], 2)
    KI.corner_set(h, g)
    h.light(*loc(h, 16.0, -18.0), 240.0, 10.0, 1000.0)
    done(h, "study")


def master_extra(h, g):
    v = g.on_wall(P + "Cabinet", ["front", "left"], gap=3.0)
    if v:
        on(h, v, P + "Vase_4", -40.0, 0.0)
        on(h, v, P + "CandleStick_Triple", 30.0, 0.0)
    g.wall_mount(P + "Peg_Rack", "left", loc(h, 0, -15.0)[1], 170.0)          # Lucky's rope hook
    lx, ly = loc(h, 22.3, -15.0)
    h.put(P + "Rope_3", lx + 10.0, ly, g.z + 150.0, 0.0, pitch=90.0, scale=(0.6, 0.6, 0.6), collide=False)


def billiard():
    h, g = room_for("billiard", cap=1, style=23)
    t = place(h, g, P + "Table_Large", 16.0, -1.0, 90.0, scale=(1.0, 1.25, 1.05))
    if t:
        lx, ly = loc(h, 16.0, -1.0)
        h.put(E + "Cube", lx, ly, g.z + 85.5, 0.0, scale=(1.26, 2.72, 0.02), collide=False)
        Bm.B.recs[-1]["material"] = Bm.MAT.get("Felt")
        for k in range(6):
            h.put(PIR + "Cannon_Ball_0", lx + h.R.uniform(-45, 45), ly + h.R.uniform(-110, 110), g.z + 87.0, 0.0,
                  scale=(0.06, 0.06, 0.06), collide=False)
    g.on_wall(P + "WeaponStand", ["left"], gap=4.0)                                   # the cue rack
    ct = place(h, g, K + "Square_Table", 12.5, -5.5, 0.0)
    if ct:
        on(h, ct, P + "Coin_Pile", 0.0, 0.0)
        on(h, ct, P + "Mug", 20.0, 15.0)
        for u, v in ((-70.0, 0.0), (70.0, 0.0), (0.0, 70.0)):
            x, y = ct.at(u, v)
            KI.seat(g, K + "Chair", x, y, KI.face_table(K + "Square_Table", u, v), stand=-60.0)
    g.on_wall(P + "Shelf_Small_Bottles", ["back", "right"], gap=3.0)
    for xm, ym in ((20.3, -6.8), (20.3, 4.4)):
        lx, ly = loc(h, xm, ym)
        yaw = face(xm, ym, 16.0, -1.0)
        if g.spot(BD + "FancyChairRed", lx, ly, yaw):
            KI.seat(g, BD + "FancyChairRed", lx, ly, yaw)
    h.light(*loc(h, 16.0, -1.0), 250.0, 12.0, 1000.0)
    done(h, "billiard")


def storm_terrace():
    h, g = room_for("storm_terrace", cap=0, style=24)
    for xm, ym, roll in ((25.0, -4.0, 90.0), (29.0, 1.0, 85.0), (31.5, -6.0, 0.0)):
        lx, ly = loc(h, xm, ym)
        h.put(P + "Chair_1", lx, ly, g.z + (28.0 if roll else 0.0), h.R.uniform(0, 360), roll=roll, collide=False)
    if Bm.have(PIR + "FlagLow_0"):
        place(h, g, PIR + "FlagLow_0", 32.5, 4.5, 0.0, check=False)                    # the lightning rod mast
    for xm in (23.5, 33.0):
        place(h, g, DV + "Planter_Large", xm, -6.8)
    done(h, "storm_terrace")


# =================================================================================================== attic, tower
def attic():
    h, g = room_for("attic", cap=2, style=25)
    # rafters across the whole roof space
    for xm in range(-33, -10, 3):
        lx, ly = loc(h, xm + 0.5, -4.3)
        h.put(V + "Corner_Exterior_Wood", lx, ly, g.z + 280.0, 90.0, pitch=90.0, scale=(0.7, 0.7, 19.4 / 3.0),
              collide=False)
    KI.bedroom(h, g, beds=3, double=False)
    for _ in range(5):
        g.on_wall(h.R.choice([BD + "ChestSquareIron", BD + "ChestRoundIron", PIR + "chest_common_0"]),
                  ["front", "left", "back", "right"], extra_yaw=h.R.uniform(-15, 15))
    for _ in range(4):
        it = g.on_wall(P + "Crate_Wooden", ["front", "back", "left"], extra_yaw=h.R.uniform(-20, 20))
        if it:
            on(h, it, E + "Cube", 0.0, 0.0, scale=(0.9, 0.95, 0.02))
    lx, ly = loc(h, -20.0, -21.8)
    if g.spot(DV + "LaundryLine", lx, ly, 0.0):
        g.place(DV + "LaundryLine", lx, ly, 0.0, collide=False, tag="keep")
    for xm in (-31.2, -29.0):
        place(h, g, P + "Barrel", xm, -5.0)
    KI.storage(h, g, 5)
    h.light(*loc(h, -26.0, -14.0), 240.0, 10.0, 1300.0)
    done(h, "attic")


def clock_room():
    h, g = room_for("clock_room", cap=1, style=26)
    # the great dial seen from inside on the north wall, its hands, gears, the pendulum, the chandelier winch
    dial = DL + "ClockDial" if Bm.have(DL + "ClockDial") else DT + "ClockFace"
    lx, ly = loc(h, -5.0, -19.6)
    h.put(dial, lx, ly, g.z + 150.0, 180.0, scale=(2.2, 2.2, 2.2), collide=False)
    for hand in ("ClockHand_Hour", "ClockHand_Minute"):
        if Bm.have(DL + hand):
            h.put(DL + hand, lx, ly + 12.0, g.z + 150.0, 180.0, scale=(2.2, 2.2, 2.2), collide=False)
    for xm, ym, sc in ((-8.0, -16.0, 2.4), (-6.0, -14.0, 1.6), (4.0, -14.5, 2.0), (8.0, -16.5, 1.4)):
        lx, ly = loc(h, xm, ym)
        if g.spot(P + "Chain_Coil", lx, ly, 0.0, scale=(sc, sc, 1.0)):
            for k in range(3):
                h.put(P + "Chain_Coil", lx, ly, g.z + 2.0 + k * 10.0, k * 30.0, scale=(sc, sc, 1.4), collide=False)
            g.take((lx - 60 * sc, ly - 60 * sc, lx + 60 * sc, ly + 60 * sc), "block")
    lx, ly = loc(h, 0.0, -14.0)
    h.put(V + "Corner_Exterior_Wood", lx, ly, g.z + 60.0, 0.0, scale=(0.3, 0.3, 0.75), collide=False)   # pendulum rod
    h.put(P + "Shield_Wooden", lx, ly, g.z + 50.0, 0.0, pitch=90.0, scale=(1.2, 1.2, 1.2), collide=False)
    place(h, g, P + "Anvil_Log", -2.0, -11.0, 0.0, check=False)
    lx, ly = loc(h, 6.0, -11.2)
    h.put(E + "Cube", lx, ly, g.z + 120.0, 0.0, scale=(0.3, 0.3, 0.3), collide=False)
    Bm.B.recs[-1]["material"] = Bm.MAT.get("Spark")
    KI.storage(h, g, 3)
    h.light(*loc(h, 0.0, -15.0), 240.0, 10.0, 1100.0)
    done(h, "clock_room")


def storm_tower():
    h, g = room_for("storm_tower", cap=1, style=27)
    for xm in (32.4, 33.6):
        rack = place(h, g, P + "Shelf_Small_Bottles", xm, -23.6, 0.0, check=False)
    for k in range(5):
        lx, ly = loc(h, 31.6 + k * 0.4, -23.1)
        h.put(P + "Potion_2", lx, ly, g.z + 2.0, 0.0, scale=(1.8, 1.8, 1.6), collide=False)
    Bm.light(h.f.x, h.f.y, h.f.z + 240.0, 10.0, 900.0, color=(150, 190, 255))
    done(h, "storm_tower")


def tower_top():
    h, g = room_for("tower_top", cap=1, style=28)
    if Bm.have(PIR + "FlagTall_1"):
        place(h, g, PIR + "FlagTall_1", 27.2, -23.2, 0.0, check=False)                 # the lightning rod
    h.light(*loc(h, 30.0, -20.0), 150.0, 18.0, 1600.0)
    done(h, "tower_top")


def roof_walk():
    h, g = room_for("roof_walk", cap=0, style=29)
    for xm in (14.0, 21.0):
        lx, ly = loc(h, xm, -21.3)
        h.put(P + "Rope_2", lx, ly, g.z + 1.0, h.R.uniform(0, 360), collide=False)
    done(h, "roof_walk")


# =================================================================================================== grounds
def courtyard():
    h, g = room_for("courtyard", cap=2, style=30)
    keep_rect(g, *loc(h, -10.0, 6.0), *loc(h, 10.0, 10.0), tag="block")        # the vestibule notch
    # the front path from the door to the sea stair, the statue fountain, lemon pots, iron lamps, benches
    for ym in range(11, 34, 2):
        lx, ly = loc(h, 0.0, ym)
        h.put(N + "RockPath_Square_Wide", lx, ly, g.z + 3.0 - Bm.bounds(N + "RockPath_Square_Wide")[5],
              h.R.choice((0.0, 90.0)), collide=False)
    keep_rect(g, *loc(h, -1.4, 10.0), *loc(h, 1.4, 34.0))
    fountain = place(h, g, DV + "Fountain", 16.0, 21.0, 0.0)
    for xm, ym, tip in ((23.0, 23.0, False), (25.0, 25.5, True), (22.5, 26.0, False)):
        lx, ly = loc(h, xm, ym)
        if tip:
            h.put(DV + "Planter_Pot", lx, ly, g.z + 25.0, 30.0, roll=85.0, collide=False)
        elif g.spot(DV + "Planter_Pot", lx, ly, 0.0):
            g.place(DV + "Planter_Pot", lx, ly, 0.0)
    for xm, ym in ((8.0, 12.0), (30.0, 9.0), (30.0, 31.0), (12.0, 31.0), (-7.0, 31.0), (-7.0, 13.0)):
        place(h, g, DV + "Planter_Large", xm, ym)
    for xm, ym in ((-3.0, 18.0), (3.0, 26.0)):
        lamp_post(h, g, xm, ym, 90.0 if xm < 0 else -90.0, lit=True)
    for xm, ym in ((6.0, 32.8), (24.0, 32.8)):
        lx, ly = loc(h, xm, ym)
        if g.spot(P + "Bench", lx, ly, 180.0):
            KI.seat(g, P + "Bench", lx, ly, 180.0, stand=58.0)
    for xm, ym in ((28.0, 15.0), (-6.0, 22.0), (19.0, 30.0)):
        lx, ly = loc(h, xm, ym)
        h.put(E + "Plane", lx, ly, g.z + 1.0, h.R.uniform(0, 90), scale=(2.2, 1.4, 1.0), collide=False)
        Bm.B.recs[-1]["material"] = "/Game/KillGodot/Materials/M_KG_PondWater"
    if Bm.have(DW + "GateArch"):
        place(h, g, DW + "GateArch", 0.0, 33.6, 0.0, check=False, collide=False)
    scatter(h, g, [N + "Bush_Common", N + "Bush_Common_Flowers", N + "Grass_Common_Short"], 14, collide=False)
    done(h, "courtyard")


def service_yard():
    h, g = room_for("service_yard", cap=1, style=31)
    shed = place(h, g, P + "Stall_Empty", -31.6, 6.6, 180.0, check=False)
    for k in range(6):
        lx, ly = loc(h, -32.3 + (k % 3) * 0.6, 5.4 + (k // 3) * 0.3)
        h.put(I + "Firewood", lx, ly, g.z + (k // 3) * 40.0, 90.0)
    place(h, g, P + "Anvil_Log", -24.5, 9.5, 0.0)
    lx, ly = loc(h, -24.5, 9.5)
    h.put(P + "Axe_Bronze", lx, ly, g.z + 100.0, 30.0, pitch=20.0, collide=False)
    for xm, ym in ((-20.0, 20.0), (-20.0, 26.0)):
        lx, ly = loc(h, xm, ym)
        if g.spot(DV + "LaundryLine", lx, ly, 0.0):
            g.place(DV + "LaundryLine", lx, ly, 0.0, collide=False, tag="keep")
    for xm, ym in ((-15.0, 32.0), (-12.5, 32.0)):
        place(h, g, P + "Cage_Small", xm, ym)
    place(h, g, P + "Stall_Cart_Empty", -31.0, 30.0, 90.0)
    place(h, g, V + "Prop_Wagon", -30.5, 20.0, 0.0)
    KI.storage(h, g, 6)
    for xm, ym in ((-16.0, 16.0), (-26.0, 26.0)):
        lx, ly = loc(h, xm, ym)
        h.put(E + "Plane", lx, ly, g.z + 1.0, h.R.uniform(0, 90), scale=(2.0, 1.2, 1.0), collide=False)
        Bm.B.recs[-1]["material"] = "/Game/KillGodot/Materials/M_KG_PondWater"
    lamp_post(h, g, -12.0, 8.0, 180.0, lit=True)
    scatter(h, g, [N + "Grass_Common_Short", N + "Grass_Wispy_Short"], 10, collide=False)
    done(h, "service_yard")


def greenhouse():
    h, g = room_for("greenhouse", cap=1, style=32)
    for xm in (38.5, 45.5):
        for ym in (8.0, 16.0):
            b = place(h, g, P + "Workbench", xm, ym, 0.0)
            if b:
                for u in (-70.0, -20.0, 30.0, 75.0):
                    on(h, b, DV + "Planter_Pot", u, 0.0, scale=(0.4, 0.4, 0.4))
    for xm, ym in ((36.0, 20.5), (48.5, 2.0), (48.5, 12.0), (36.2, 12.0)):
        place(h, g, N + "Plant_1_Big", xm, ym, h.R.uniform(0, 360))
    scatter(h, g, [N + "Fern_1", N + "Flower_4_Group", N + "Bush_Common_Flowers", N + "Plant_1"], 10)
    if Bm.have(DU + "WellMouth"):
        place(h, g, DU + "WellMouth", 47.6, 20.4, 0.0, check=False, scale=(0.8, 0.8, 0.8))
    h.light(*loc(h, 42.0, 11.0), 240.0, 9.0, 1300.0)
    done(h, "greenhouse")


def graveyard():
    h, g = room_for("graveyard", cap=1, style=33)
    if Bm.have(DW + "Mausoleum"):
        place(h, g, DW + "Mausoleum", -48.0, 23.8, 0.0, check=False, scale=(0.75, 0.75, 0.75))
        keep_rect(g, *loc(h, -50.0, 21.6), *loc(h, -46.0, 26.0), tag="block")
    stones = [DW + "Gravestone_Cross", DW + "Gravestone_Round"]
    for xm in (-50.0, -46.0, -42.0, -38.0):
        for ym in (7.0, 13.0, 19.0):
            lx, ly = loc(h, xm + h.R.uniform(-0.4, 0.4), ym + h.R.uniform(-0.3, 0.3))
            p = h.R.choice(stones)
            if g.spot(p, lx, ly, 0.0, pad=20.0):
                g.place(p, lx, ly, h.R.uniform(-8, 8))
    if Bm.have(DU + "Sarcophagus"):
        place(h, g, DU + "Sarcophagus", -40.0, 23.5, 90.0)
    place(h, g, N + "DeadTree_2", -36.8, 24.2, 40.0, scale=(0.7, 0.7, 0.7), collide=False)
    lx, ly = loc(h, -44.0, 15.0)
    h.light(lx, ly, 260.0, 10.0, 1400.0)
    done(h, "graveyard")


def cliff_path():
    h, g = room_for("cliff_path", cap=1, style=34)
    lamp_post(h, g, -39.2, -14.0, 90.0, lit=True, tilt=6.0)
    lamp_post(h, g, -39.2, -2.0, 90.0, lit=False, tilt=-8.0)
    lx, ly = loc(h, -39.2, -20.0)
    if g.spot(P + "Bench", lx, ly, -90.0):
        KI.seat(g, P + "Bench", lx, ly, -90.0, stand=58.0)
    scatter(h, g, [N + "Grass_Wispy_Short", N + "Pebble_Round_2", N + "Rock_Medium_2"], 6, collide=False)
    done(h, "cliff_path")


# =================================================================================================== external kits
# SPRINT-018 "big, full and detailed inside": a second layer per room from the 42 free packs under /Env/Ext
# (Tools/Unreal/dressing/pack_ext_*.md). It runs from done() on the same free-space map, so the first layer's
# reservations (doors, stairs, chores, secrets, meeting ring) still hold. Pack scales (notes): KKFurniture, KKRestaurant,
# KGraveyard, QDungeon ~ real size; KFurniture x1.9 (Kenney is small); KKDungeon x0.5 (chunky); PPManor per mesh.
XROOT = "/Game/KillGodot/Env/Ext/KG_Ext_{0}/StaticMeshes/SM_KG_{0}_{1}"


def xp(tag, piece):
    return XROOT.format(tag, piece)


def KKF(p): return xp("KKFurniture", p)
def KF(p): return xp("KFurniture", p)
def PPM(p): return xp("PPManor", p)
def KKR(p): return xp("KKRestaurant", p)
def KKD(p): return xp("KKDungeon", p)
def KKH(p): return xp("KKHalloween", p)
def KGY(p): return xp("KGraveyard", p)
def QD(p): return xp("QDungeon", p)


BATHTUB = "Bathtub_2MbbdwbTjt"
BOOKCASE = "Bookcase_with_Books_tACDGJ4CGW"
CLOCK = "Grandfathers_Clock_09YKIkFZnA"
GLOBE = "Globe_Y4Dof9b2p5"
TELESCOPE = "Telescope_RjyTCQvA8b"
LUTE = "Lute_q3IXa6QH1C"
MANNEQUIN = "Mannequin_tYwjQJvcFX"
STATUE = "Statue_gieXYyUTYr"
ANGEL = "AngelStatue_6v4CL0nKfT"
HORSE = "Horse_Statue_AK9CmjFnL6"
CHANDELIER = "Light_Chandelier_q3k8I8YYX9"
CHANDELIER2 = "Chandelier_RPLTkXHOOM"
COFFIN = "Coffin_ySERERWPgE"
CAULDRON = "Cauldron_QaWJOPa6Gt"
RUG = "Rug_7H5qKjuxVY"
ARMCHAIR = "Armchair_myd1WSucAz"
FRAMES = ["pictureframe_large_A", "pictureframe_medium", "pictureframe_small_A", "pictureframe_small_B",
          "pictureframe_small_C"]


def S(f):
    return (f, f, f) if isinstance(f, (int, float)) else tuple(f)


def xwall(h, g, path, walls, n=1, s=1.0, gap=3.0, tag="block", collide=True, yaw=0.0, tall=None):
    """Up to n copies with their back to one of `walls` (kit furniture faces local +Y)."""
    out = []
    if not Bm.have(path):
        return out
    for _ in range(n):
        it = g.on_wall(path, walls, scale=S(s), gap=gap, tag=tag, collide=collide, extra_yaw=yaw, tall=tall)
        if not it:
            break
        out.append(it)
    return out


def xat(h, g, path, xm, ym, yaw=0.0, s=1.0, tag="block", collide=True, check=True):
    if not Bm.have(path):
        return None
    return place(h, g, path, xm, ym, yaw, scale=S(s), tag=tag, collide=collide, check=check)


def xscatter(h, g, paths, n, s=1.0, pad=15.0, collide=True, yaw_any=True, tries=40, area=None, tag="block"):
    """n free-standing pieces on free floor (area = world-metre rect to stay in)."""
    paths = [p for p in paths if Bm.have(p)]
    if not paths:
        return 0
    if area:
        ax0, ay0 = loc(h, area[0], area[1])
        ax1, ay1 = loc(h, area[2], area[3])
    else:
        ax0, ay0, ax1, ay1 = g.x0 + 60, g.y0 + 60, g.x1 - 60, g.y1 - 60
    k = 0
    for _ in range(n * tries):
        if k >= n:
            break
        path = h.R.choice(paths)
        lx, ly = h.R.uniform(min(ax0, ax1), max(ax0, ax1)), h.R.uniform(min(ay0, ay1), max(ay0, ay1))
        yaw = h.R.uniform(0, 360) if yaw_any else h.R.choice((0.0, 90.0, 180.0, 270.0))
        if g.spot(path, lx, ly, yaw, scale=S(s), pad=pad):
            g.place(path, lx, ly, yaw, scale=S(s), collide=collide, tag=tag)
            k += 1
    return k


def xon(h, base, path, u, v, s=1.0, lyaw=0.0, z=None):
    if base and Bm.have(path):
        return on(h, base, path, u, v, z=z, lyaw=lyaw, scale=S(s))
    return None


def xtop(h, base, paths, n, s=1.0):
    """n pieces of clutter spread over the top of `base`."""
    if not base:
        return
    b = KI.bounds(base.path)
    w = (b[3] - b[0]) * base.scale[0]
    d = (b[4] - b[1]) * base.scale[1]
    cx = (b[0] + b[3]) / 2 * base.scale[0]
    cy = (b[1] + b[4]) / 2 * base.scale[1]
    for k in range(n):
        u = cx + (-0.5 + (k + 0.5) / n) * w * 0.75
        xon(h, base, h.R.choice(paths), u / base.scale[0], (cy + h.R.uniform(-0.2, 0.2) * d) / base.scale[1],
            s=s, lyaw=h.R.uniform(-40, 40))


def xmount(h, g, path, walls, z, every=450.0, s=1.0, skip=0):
    """Hang copies on closed wall every `every` cm (pictures, banners, mirrors), scaled."""
    if not Bm.have(path):
        return 0
    n = 0
    sc = S(s)
    for wall in walls:
        yaw = {"back": 180.0, "front": 0.0, "left": -90.0, "right": 90.0}[wall]
        fp = KI.footprint(path, yaw, sc)
        lo, hi = (g.x0, g.x1) if wall in ("front", "back") else (g.y0, g.y1)
        t = lo + every / 2.0
        k = 0
        while t < hi - 40.0:
            k += 1
            half = max(fp[2] - fp[0], fp[3] - fp[1]) / 2.0 + 10.0
            if k > skip and h.wall_ok(0, wall, t - half, t + half, 999.0):
                if wall == "back":
                    lx, ly = t - (fp[0] + fp[2]) / 2, g.y1 - 0.5 - fp[3]
                elif wall == "front":
                    lx, ly = t - (fp[0] + fp[2]) / 2, g.y0 + 0.5 - fp[1]
                elif wall == "right":
                    lx, ly = g.x1 - 0.5 - fp[2], t - (fp[1] + fp[3]) / 2
                else:
                    lx, ly = g.x0 + 0.5 - fp[0], t - (fp[1] + fp[3]) / 2
                r = (lx + fp[0], ly + fp[1], lx + fp[2], ly + fp[3])
                if g.inside(r) and not any(KI.overlap(r, q) for q, tag in g.rects if tag in ("stair", "tall")):
                    h.put(path, lx, ly, g.z + z, yaw, scale=sc, collide=False)
                    n += 1
            t += every
    return n


def pictures(h, g, walls, z=170.0, every=380.0):
    return xmount(h, g, KKF(h.R.choice(FRAMES)), walls, z, every=every)


def reading_table(h, g, chairs, table=None, s=1.0):
    """A free-standing table with chairs round it and a few things on it, wherever the floor is free."""
    table = table or KKF("table_medium_long")
    if not Bm.have(table):
        return None
    for _ in range(60):
        lx, ly = h.R.uniform(g.x0 + 200, g.x1 - 200), h.R.uniform(g.y0 + 200, g.y1 - 200)
        yaw = h.R.choice((0.0, 90.0))
        if not g.spot(table, lx, ly, yaw, scale=S(s), pad=110.0):
            continue
        t = g.place(table, lx, ly, yaw, scale=S(s))
        b = KI.bounds(table)
        hy = (b[4] - b[1]) * s / 2.0 + 30.0
        for k in range(chairs):
            side = 1.0 if k % 2 == 0 else -1.0
            u = 0.0 if chairs <= 2 else (-45.0 if k < 2 else 45.0) * s
            x, y = t.at(u / s, side * hy / s)
            cyaw = t.lyaw + (180.0 if side > 0 else 0.0)
            if g.spot(KKF("chair_B_wood"), x, y, cyaw):
                g.place(KKF("chair_B_wood"), x, y, cyaw)
        xtop(h, t, [KKF("book_set"), KKF("book_single"), KKF("lamp_table"), KKR("jar_A_small")], 3, s=0.6)
        return t
    return None


def xhang(h, path, xm, ym, zc, s=1.0):
    if Bm.have(path):
        return hang(h, path, xm, ym, zc, scale=S(s))
    return None


def clock_stopped(h, g, walls):
    """Anselm's grandfather clocks: all stopped at the same minute (story beat)."""
    return xwall(h, g, PPM(CLOCK), walls, 1, s=1.5, gap=2.0, tag="tall")


def sitting_corner(h, g, walls, lamp=True):
    """Armchair + side table + lamp + books against a wall."""
    ch = xwall(h, g, KKF("armchair_pillows"), walls, 1, gap=6.0)
    if not ch:
        return None
    x, y = ch[0].at(95.0, 10.0)
    t = g.place(KKF("table_small"), x, y, ch[0].lyaw, scale=S(0.8)) if g.spot(KKF("table_small"), x, y, ch[0].lyaw,
                                                                                   scale=S(0.8)) else None
    if t:
        xon(h, t, KKF("lamp_table"), 0.0, 0.0, s=0.7) if lamp else None
        xon(h, t, KKF("book_single"), 15.0, 15.0, lyaw=30.0)
    return ch[0]


# --------------------------------------------------------------------------------------------------- ground floor
def ext_great_hall(h, g):
    clock_stopped(h, g, ["front"])
    xwall(h, g, KKF("cabinet_medium_decorated"), ["left", "right"], 2, gap=3.0, tag="tall")
    for it in xwall(h, g, KKF("shelf_B_large_decorated"), ["left", "right"], 2, gap=2.0):
        pass
    for xm, ym in ((-8.6, -1.2), (8.6, -1.2), (-8.6, -19.0), (8.6, -19.0)):
        xat(h, g, KKF("lamp_standing"), xm, ym)
    xscatter(h, g, [PPM(STATUE)], 2, s=1.0, pad=40.0, area=(-9.2, -19.3, 9.2, -16.5), yaw_any=False)
    xscatter(h, g, [KF("pottedPlant")], 4, s=1.9, pad=30.0)
    xmount(h, g, KKD("banner_shield_red"), ["left", "right"], 180.0, every=700.0, s=0.55)
    pictures(h, g, ["back"], z=190.0, every=520.0)
    for ym in (-17.5, -9.0, -1.5):
        xhang(h, PPM(CHANDELIER), -6.5, ym, 2.0 * Bm.H - 90.0, s=1.4)
        xhang(h, PPM(CHANDELIER), 6.5, ym, 2.0 * Bm.H - 90.0, s=1.4)


def ext_vestibule(h, g):
    # the guests' luggage, just carried in out of the rain
    xscatter(h, g, [KKD("trunk_small_A"), KKD("trunk_small_B"), KKD("trunk_medium_A"), KKD("trunk_medium_C")], 6, s=0.8,
             pad=8.0, area=(-9.0, 6.5, -3.0, 9.3))
    xwall(h, g, KF("coatRackStanding"), ["back", "left", "right"], 3, s=2.0, gap=10.0)
    xwall(h, g, KF("benchCushion"), ["left", "right"], 2, s=2.1, gap=3.0)
    clock_stopped(h, g, ["front", "right"])
    xwall(h, g, KKF("cabinet_small_decorated"), ["left", "right"], 2, gap=3.0)
    xscatter(h, g, [KF("rugDoormat")], 2, s=2.2, pad=0.0, collide=False, tag=None)
    xscatter(h, g, [KF("pottedPlant")], 3, s=1.9, pad=30.0)
    pictures(h, g, ["left", "right"], z=180.0)
    xhang(h, PPM(CHANDELIER2), 0.0, 5.0, Bm.H - 80.0, s=1.3)


def ext_dining(h, g):
    for sb in xwall(h, g, KKF("cabinet_medium_decorated"), ["left", "front", "back"], 3, gap=3.0, tag="tall"):
        pass
    for sb in xwall(h, g, KKF("cabinet_medium"), ["left", "front", "back"], 2, gap=3.0):
        xtop(h, sb, [KKR("jar_A_medium"), KKR("plate_small"), KKR("bowl_small"), KKD("bottle_A_labeled_brown")], 3,
             s=0.8)
    clock_stopped(h, g, ["back", "left"])
    xwall(h, g, KKF("lamp_standing"), ["front", "back"], 2, gap=8.0)
    pictures(h, g, ["front", "back", "left"], z=180.0, every=420.0)
    xscatter(h, g, [KF("pottedPlant")], 2, s=1.9, pad=30.0)
    xhang(h, PPM(CHANDELIER), -16.0, -13.0, Bm.H - 70.0, s=1.6)


def ext_kitchen(h, g):
    st = xwall(h, g, KKR("stove_multi_decorated"), ["front", "left"], 1, gap=2.0)
    for c in xwall(h, g, KKR("kitchencounter_straight_A_decorated"), ["front", "left", "back"], 3, gap=2.0):
        pass
    for c in xwall(h, g, KKR("kitchencounter_straight_B"), ["front", "left", "back"], 2, gap=2.0):
        xtop(h, c, [KKR("pot_A_stew"), KKR("cuttingboard"), KKR("jar_B_large"), KKR("food_ingredient_carrot_chopped")], 2)
    xwall(h, g, KKR("shelf_papertowel_decorated"), ["back", "right"], 1, gap=2.0)
    xwall(h, g, KKR("dishrack_plates"), ["back", "right"], 1, gap=2.0)
    # dinner abandoned mid-preparation: the big table with chopped vegetables and a knife
    t = xscatter(h, g, [KKR("kitchentable_A_large_decorated")], 1, pad=60.0, yaw_any=False)
    xscatter(h, g, [KKR("crate_carrots"), KKR("crate_potatoes"), KKR("crate_onions"), KKR("crate_tomatoes")], 4,
             s=0.6, pad=12.0)
    xscatter(h, g, [PPM(CAULDRON)], 1, s=2.6, pad=20.0)
    xscatter(h, g, [KKR("chair_stool")], 3, pad=15.0)


def ext_pantry(h, g):
    xwall(h, g, KKD("shelves"), ["front", "back", "left", "right"], 2, s=0.55, gap=2.0, tag="tall")
    for sh in xwall(h, g, KKR("shelf_papertowel_decorated"), ["front", "back", "left", "right"], 2, gap=2.0):
        pass
    for t in xwall(h, g, KKR("kitchentable_A"), ["front", "back", "left", "right"], 1, gap=2.0):
        xtop(h, t, [KKR("jar_A_small"), KKR("jar_B_small"), KKR("jar_C_small"), KKR("jar_D_small")], 4, s=0.9)
    xscatter(h, g, [KKR("crate_potatoes"), KKR("crate_onions"), KKR("crate_cheese"), KKR("crate_ham"),
                    KKR("crate_buns"), KKR("crate_lettuce")], 6, s=0.6, pad=10.0)
    xscatter(h, g, [KKD("barrel_small"), QD("Barrel")], 4, s=0.55, pad=10.0)


def ext_laundry(h, g):
    for it in xwall(h, g, KKD("trunk_medium_B"), ["front", "back", "left", "right"], 3, s=0.9, gap=3.0):
        pass
    xwall(h, g, KKR("towelrail"), ["left", "right"], 2, s=1.2, gap=2.0)
    for sh in xwall(h, g, KKF("shelf_B_small_decorated"), ["front", "back"], 2, gap=2.0):
        pass
    xscatter(h, g, [QD("Bucket")], 4, s=1.6, pad=8.0)
    xscatter(h, g, [KKF("chair_stool_wood")], 2, pad=15.0)
    xscatter(h, g, [KF("cardboardBoxOpen"), KF("cardboardBoxClosed")], 3, s=1.9, pad=10.0)


def ext_chapel(h, g):
    # a closed coffin on trestles before the altar: the vigil nobody explains
    xscatter(h, g, [PPM(COFFIN)], 1, s=0.6, pad=40.0, yaw_any=False, area=(13.0, -18.5, 19.0, -15.0))
    for c in xwall(h, g, KGY("candle_multiple"), ["front", "left", "right"], 5, gap=6.0, collide=False):
        pass
    xwall(h, g, KGY("urn_round"), ["front", "back"], 2, gap=6.0)
    xwall(h, g, KGY("cross_wood"), ["front"], 1, s=0.7, gap=4.0, tag="tall")
    xscatter(h, g, [PPM(ANGEL)], 1, s=0.45, pad=30.0, yaw_any=False)
    xscatter(h, g, [KKH("candle_triple"), KKH("candle_melted")], 5, s=1.0, pad=8.0, collide=False)
    xmount(h, g, KKD("banner_thin_white"), ["left", "right"], 150.0, every=500.0, s=0.5)


def ext_library(h, g):
    for bc in xwall(h, g, PPM(BOOKCASE), ["front", "back", "left", "right"], 5, s=0.8, gap=2.0, tag="tall"):
        pass
    sitting_corner(h, g, ["left", "right", "back"])
    sitting_corner(h, g, ["left", "right", "back", "front"])
    t = xscatter(h, g, [KKF("table_medium")], 1, s=0.8, pad=60.0)
    xscatter(h, g, [PPM(TELESCOPE)], 1, s=1.3, pad=20.0)
    xscatter(h, g, [KKF("rug_oval_A")], 1, pad=0.0, collide=False, tag=None, yaw_any=False)
    for bc in xwall(h, g, KF("bookcaseClosedWide"), ["front", "back", "left", "right"], 6, s=2.2, gap=2.0, tag="tall"):
        pass
    reading_table(h, g, 4)
    for gl in xwall(h, g, KKF("table_small"), ["front", "back"], 1, s=0.8, gap=6.0):
        xon(h, gl, PPM(GLOBE), 0.0, 0.0, s=1.8)
    xwall(h, g, KKF("lamp_standing"), ["front", "back", "left"], 2, gap=8.0)


def ext_ballroom(h, g):
    # the ball that never started: chairs lining the walls, punch tables, the lute on its chair, dust sheets
    xwall(h, g, KKF("couch_pillows"), ["front", "back", "left", "right"], 3, gap=4.0)
    xwall(h, g, KKF("chair_B_wood"), ["front", "back", "left", "right"], 10, gap=6.0)
    for t in xwall(h, g, KKR("table_round_A_small_decorated"), ["front", "back"], 2, gap=30.0):
        pass
    xscatter(h, g, [PPM(STATUE)], 2, pad=40.0, yaw_any=False)
    xwall(h, g, PPM(LUTE), ["left", "right", "back"], 1, s=0.75, gap=4.0, yaw=15.0)
    xwall(h, g, KKF("lamp_standing"), ["front", "back", "left", "right"], 4, gap=8.0)
    xscatter(h, g, [KF("pottedPlant")], 4, s=2.0, pad=30.0)
    pictures(h, g, ["front", "back"], z=200.0, every=600.0)
    for xm, ym in ((16.0, -2.0), (22.0, -2.0), (28.0, -2.0)):
        xhang(h, PPM(CHANDELIER), xm, ym, Bm.H - 90.0, s=1.6)


def ext_servants_corridor(h, g):
    xwall(h, g, KKD("shelf_small_candles"), ["front", "back"], 3, s=0.6, gap=2.0)
    xwall(h, g, KKD("trunk_small_C"), ["front", "back"], 2, s=0.8, gap=3.0)
    xscatter(h, g, [QD("Bucket")], 2, s=1.6, pad=8.0)


# --------------------------------------------------------------------------------------------------- cellar
def ext_wine_cellar(h, g):
    # (the KayKit kegs render untextured white: barrels only)
    xwall(h, g, QD("Barrel"), ["front", "back", "left", "right"], 6, s=0.9, gap=3.0)
    xwall(h, g, KKD("barrel_small_stack"), ["front", "left"], 2, s=0.5, gap=3.0)
    xwall(h, g, KKD("shelves"), ["back", "right"], 3, s=0.55, gap=2.0, tag="tall")
    for t in xwall(h, g, KKD("table_long"), ["back", "right", "left"], 1, s=0.45, gap=3.0, yaw=90.0):
        xtop(h, t, [KKD("bottle_A_brown"), KKD("bottle_A_green"), KKD("bottle_B_green"), KKD("candle_lit")], 4, s=0.35)
    xscatter(h, g, [KKD("barrel_small"), KKD("barrel_small_stack"), QD("Barrel2")], 5, s=0.5, pad=10.0)
    xmount(h, g, QD("Cobweb"), ["front", "back"], 200.0, every=600.0)
    xmount(h, g, KKD("torch_mounted"), ["left", "right"], 140.0, every=700.0, s=0.5)


def ext_spark_room(h, g):
    for t in xwall(h, g, KKD("table_medium"), ["front", "back"], 1, s=0.5, gap=3.0):
        xtop(h, t, [KKD("candle_lit"), KKD("key"), KKD("bottle_C_green"), KKD("coin_stack_small")], 3, s=0.4)
    xwall(h, g, KKD("chest"), ["left", "right", "front"], 1, s=0.5, gap=3.0)
    xscatter(h, g, [QD("Crate"), KKD("box_small")], 3, s=0.6, pad=10.0)
    xmount(h, g, QD("Cobweb2"), ["front", "back"], 210.0, every=700.0)


def ext_cistern(h, g):
    xscatter(h, g, [QD("Bucket")], 3, s=1.6, pad=8.0)
    xscatter(h, g, [KKH("skull_candle"), KKH("bone_A"), KKH("bone_C")], 3, s=0.6, pad=8.0, collide=False)
    xwall(h, g, KGY("coffin_old"), ["front", "left"], 1, s=0.8, gap=4.0)
    xmount(h, g, QD("Cobweb"), ["front", "back", "left"], 200.0, every=650.0)
    xmount(h, g, KKD("torch_mounted"), ["left", "right"], 140.0, every=900.0, s=0.5)


def ext_smugglers_tunnel(h, g):
    # the smugglers' cache: an open chest of coins, crates and a lantern left burning
    # (a 2 m passage: nothing solid along its walls, the bots and the chore routes need the full width)
    xscatter(h, g, [KKD("coin_stack_small"), KKD("coin_stack_medium")], 3, s=0.4, pad=6.0, collide=False)
    xscatter(h, g, [KKH("lantern_standing"), KKH("candle_melted")], 3, pad=6.0, collide=False)
    xmount(h, g, KKD("torch_mounted"), ["left", "right"], 140.0, every=800.0, s=0.5)
    xmount(h, g, QD("Cobweb2"), ["left", "right"], 180.0, every=900.0)


def ext_boathouse(h, g):
    xwall(h, g, QD("Crate"), ["front", "left", "right"], 4, s=0.7, gap=3.0)
    xwall(h, g, KKD("barrel_small"), ["front", "left", "right"], 3, s=0.55, gap=3.0)
    xscatter(h, g, [KKH("lantern_standing")], 2, pad=10.0, collide=False)
    xwall(h, g, KKD("shelf_large"), ["front"], 1, s=0.6, gap=2.0)


# --------------------------------------------------------------------------------------------------- first floor
def ext_gallery(h, g):
    for wall in ("front", "back", "left", "right"):
        xmount(h, g, KKF("pictureframe_large_B"), [wall], 190.0, every=420.0)
        xmount(h, g, KKF("pictureframe_medium"), [wall], 120.0, every=420.0, skip=0)
    xscatter(h, g, [PPM(STATUE)], 2, pad=40.0, yaw_any=False)
    xwall(h, g, QD("Pedestal2"), ["left", "right"], 3, s=0.55, gap=6.0)
    xwall(h, g, KF("benchCushion"), ["left", "right"], 2, s=2.2, gap=40.0)
    xwall(h, g, KKF("lamp_standing"), ["front", "back"], 2, gap=8.0)


def ext_corridor(h, g):
    pictures(h, g, ["front", "back"], z=175.0, every=520.0)
    xwall(h, g, KKF("cabinet_small_decorated"), ["front", "back"], 2, gap=3.0)
    xwall(h, g, KF("pottedPlant"), ["front", "back"], 2, s=1.9, gap=4.0)


def ext_bedroom(h, g):
    for d in xwall(h, g, KKF("cabinet_medium_decorated"), ["front", "left", "right", "back"], 1, gap=3.0, tag="tall"):
        pass
    sitting_corner(h, g, ["front", "left", "right", "back"])
    xwall(h, g, KKF("lamp_standing"), ["front", "left", "right", "back"], 1, gap=8.0)
    # the guest's trunk, half unpacked
    xscatter(h, g, [KKD("trunk_medium_A"), KKD("trunk_medium_B")], 1, s=0.9, pad=15.0)
    xscatter(h, g, [KKF("rug_oval_B"), KKF("rug_rectangle_stripes_A")], 1, pad=0.0, collide=False, tag=None,
             yaw_any=False)
    pictures(h, g, ["front", "left", "right"], z=170.0, every=460.0)
    xmount(h, g, KKF("pictureframe_large_B"), ["back", "front"], 190.0, every=700.0)
    xscatter(h, g, [KF("pottedPlant")], 2, s=1.9, pad=25.0)
    # the rest of a 12 m guest room: a dresser, a wardrobe, a couch, shelves, a writing desk, a tea table
    xwall(h, g, KKF("cabinet_medium"), ["front", "left", "right", "back"], 1, gap=3.0)
    xwall(h, g, KKF("cabinet_small_decorated"), ["front", "left", "right", "back"], 2, gap=3.0)
    xwall(h, g, KKF("couch_pillows"), ["front", "left", "right", "back"], 1, gap=4.0)
    xwall(h, g, KKF("shelf_B_large_decorated"), ["front", "left", "right", "back"], 2, gap=2.0)
    for d in xwall(h, g, KF("desk"), ["front", "left", "right", "back"], 1, s=1.9, gap=3.0):
        xtop(h, d, [KKF("book_single"), KKF("lamp_table"), KKF("pictureframe_standing_A")], 3, s=0.5)
        x, y = d.at(0.0, 60.0)
        if g.spot(KKF("chair_A_wood"), x, y, d.lyaw + 180.0):
            g.place(KKF("chair_A_wood"), x, y, d.lyaw + 180.0)
    reading_table(h, g, 2, table=KKF("table_small"), s=0.9)
    xwall(h, g, KKF("lamp_standing"), ["front", "left", "right", "back"], 1, gap=8.0)


def ext_bath(h, g):
    # the bath still drawn, a towel dropped, the copper boiler ticking
    xwall(h, g, PPM(BATHTUB), ["back", "right", "front"], 1, s=1.2, gap=4.0)
    xwall(h, g, KF("bathroomSinkSquare"), ["front", "right", "back"], 2, s=2.0, gap=2.0)
    xmount(h, g, KF("bathroomMirror"), ["front", "right"], 140.0, every=500.0, s=2.0)
    xwall(h, g, KF("bathroomCabinetDrawer"), ["front", "left", "right"], 2, s=2.0, gap=2.0)
    xwall(h, g, KKR("towelrail"), ["left", "right", "back"], 2, s=1.2, gap=2.0)
    for sh in xwall(h, g, KKF("shelf_B_small_decorated"), ["front", "back", "left"], 2, gap=2.0):
        pass
    xscatter(h, g, [KKF("chair_stool")], 2, pad=12.0)
    xscatter(h, g, [KF("pottedPlant")], 2, s=1.9, pad=20.0)
    xscatter(h, g, [KKF("rug_rectangle_A")], 1, pad=0.0, collide=False, tag=None, yaw_any=False)
    xscatter(h, g, [QD("Bucket")], 2, s=1.6, pad=8.0)


def ext_studio(h, g):
    xscatter(h, g, [PPM(MANNEQUIN)], 2, s=0.2, pad=25.0)
    xwall(h, g, KKF("pictureframe_standing_A"), ["front", "back", "left", "right"], 4, s=1.8, gap=3.0, yaw=0.0)
    for t in xwall(h, g, KKF("table_small"), ["front", "back", "right"], 2, s=0.9, gap=3.0):
        xtop(h, t, [KKR("jar_A_small"), KKR("jar_C_small"), KKR("jar_D_small"), KKR("bowl_small")], 3, s=0.8)
    xscatter(h, g, [KKF("rug_rectangle_stripes_B")], 1, pad=0.0, collide=False, tag=None, yaw_any=False)
    xwall(h, g, KKF("lamp_standing"), ["front", "back"], 2, gap=8.0)
    pictures(h, g, ["front", "back"], z=180.0, every=360.0)


def ext_nursery(h, g):
    # kept ready: toys out, the toy chest open, a lamp left on
    xscatter(h, g, [KF("bear")], 2, s=1.6, pad=6.0, collide=False)
    for bc in xwall(h, g, KF("bookcaseOpenLow"), ["front", "left", "back"], 3, s=2.0, gap=2.0):
        xon(h, bc, KKF("book_set"), 0.0, 0.0, s=0.5)
    xwall(h, g, KKF("cabinet_small"), ["front", "left", "back"], 1, gap=3.0)          # the toy chest
    xscatter(h, g, [KKF("chair_stool"), KKF("pillow_A"), KKF("pillow_B")], 4, pad=8.0)
    reading_table(h, g, 2, table=KKF("table_small"), s=0.8)                              # the tea party table
    xwall(h, g, KKF("armchair"), ["front", "left", "back"], 1, gap=6.0)
    xscatter(h, g, [KKF("rug_rectangle_stripes_A")], 1, pad=0.0, collide=False, tag=None, yaw_any=False)
    xwall(h, g, KKF("lamp_table"), ["front", "left"], 1, s=0.9, gap=3.0)
    xmount(h, g, KKF("pictureframe_small_B"), ["front", "left", "back"], 150.0, every=300.0)
    xwall(h, g, KKF("shelf_A_small"), ["front", "left"], 1, gap=2.0)


def ext_study(h, g):
    clock_stopped(h, g, ["front", "right", "left"])
    sitting_corner(h, g, ["front", "right", "left"])
    xscatter(h, g, [PPM(GLOBE)], 1, s=3.2, pad=12.0)
    xwall(h, g, KKF("cabinet_small_decorated"), ["front", "right"], 1, gap=3.0)
    xscatter(h, g, [KKF("rug_rectangle_B")], 1, pad=0.0, collide=False, tag=None, yaw_any=False)
    xwall(h, g, KF("bookcaseClosedWide"), ["front", "right", "left", "back"], 2, s=2.2, gap=2.0, tag="tall")
    pictures(h, g, ["front", "right"], z=175.0, every=420.0)


def ext_billiard(h, g):
    xwall(h, g, KKF("couch"), ["front", "back", "left", "right"], 1, gap=4.0)
    sitting_corner(h, g, ["front", "back", "left", "right"])
    for sh in xwall(h, g, KKF("shelf_B_large_decorated"), ["front", "back", "left", "right"], 1, gap=2.0):
        pass
    for t in xwall(h, g, KKF("table_small"), ["front", "back", "left", "right"], 1, s=0.8, gap=4.0):
        xtop(h, t, [KKD("bottle_A_labeled_brown"), KKD("bottle_C_brown")], 2, s=0.35)
    xwall(h, g, KF("coatRackStanding"), ["front", "back", "left", "right"], 1, s=2.0, gap=10.0)
    xwall(h, g, KKF("lamp_standing"), ["front", "back", "left", "right"], 2, gap=8.0)
    pictures(h, g, ["front", "back", "left", "right"], z=180.0, every=480.0)


def ext_storm_terrace(h, g):
    xwall(h, g, KGY("urn_round"), ["front", "back", "right"], 4, gap=8.0)
    xscatter(h, g, [KKH("lantern_standing")], 2, pad=12.0, collide=False)
    xscatter(h, g, [KKH("bench")], 2, pad=25.0, yaw_any=False)
    # one urn blown over by the wind
    xscatter(h, g, [KGY("debris")], 1, s=0.6, pad=10.0, collide=False)


# --------------------------------------------------------------------------------------------------- upper floors
def ext_attic(h, g):
    # a century of the family under dust: trunks, a dress form, old frames, a broken bed, cobwebs
    xscatter(h, g, [KKD("trunk_large_A"), KKD("trunk_large_B"), KKD("trunk_large_C"), KKD("trunk_medium_C")], 7,
             s=0.6, pad=15.0)
    xscatter(h, g, [PPM(MANNEQUIN)], 1, s=0.2, pad=25.0)
    xscatter(h, g, [KF("cardboardBoxClosed"), KF("cardboardBoxOpen")], 6, s=1.9, pad=8.0)
    xwall(h, g, KKF("pictureframe_standing_B"), ["front", "back", "left"], 3, s=1.8, gap=3.0)
    xscatter(h, g, [KKD("bed_frame")], 1, s=0.6, pad=30.0, yaw_any=False)
    xscatter(h, g, [KKF("chair_A_wood"), KKF("chair_C")], 3, pad=15.0)
    xscatter(h, g, [PPM(ARMCHAIR)], 1, s=1.4, pad=15.0)
    xmount(h, g, QD("Cobweb"), ["front", "back", "left", "right"], 190.0, every=600.0)
    xwall(h, g, KKF("cabinet_small"), ["back", "left"], 2, gap=3.0)


def ext_clock_room(h, g):
    for _ in range(3):
        clock_stopped(h, g, ["front", "left", "right", "back"])
    for t in xwall(h, g, KKD("table_small"), ["back", "left"], 1, s=0.55, gap=3.0):
        xtop(h, t, [KKD("key"), KKD("candle_melted"), KKD("coin_stack_small")], 3, s=0.35)
    xmount(h, g, QD("Cobweb2"), ["front", "back"], 200.0, every=700.0)


def ext_storm_tower(h, g):
    # the storm watcher's post: a telescope at the window, a log book, a cold supper, a chest of flares
    xscatter(h, g, [PPM(TELESCOPE)], 1, s=1.3, pad=20.0)
    for t in xwall(h, g, KKD("table_small"), ["front", "left", "right"], 1, s=0.6, gap=3.0):
        xtop(h, t, [KKD("candle_lit"), KKD("plate_food_B"), KKD("bottle_A_green")], 3, s=0.35)
        x, y = t.at(0.0, 75.0)
        if g.spot(KKF("chair_A_wood"), x, y, t.lyaw + 180.0):
            g.place(KKF("chair_A_wood"), x, y, t.lyaw + 180.0)
    xwall(h, g, KKD("chest"), ["front", "left", "right", "back"], 1, s=0.45, gap=3.0)
    xwall(h, g, KKD("trunk_small_A"), ["front", "left", "right", "back"], 1, s=0.8, gap=3.0)
    xscatter(h, g, [QD("Bucket"), KKH("lantern_standing")], 3, pad=8.0, collide=False)
    xmount(h, g, KKD("torch_mounted"), ["left", "right"], 140.0, every=500.0, s=0.5)
    xmount(h, g, QD("Cobweb"), ["front", "back"], 200.0, every=500.0)
    xscatter(h, g, [KKF("rug_oval_A")], 1, pad=0.0, collide=False, tag=None, yaw_any=False)


def ext_tower_top(h, g):
    xscatter(h, g, [KKH("lantern_standing")], 2, pad=12.0, collide=False)
    xscatter(h, g, [KKD("barrel_small")], 1, s=0.5, pad=12.0)


# --------------------------------------------------------------------------------------------------- grounds
def ext_courtyard(h, g):
    xscatter(h, g, [PPM(HORSE)], 1, s=0.5, pad=60.0, yaw_any=False, area=(4.0, 12.0, 30.0, 30.0))
    for xm in (-1.9, 1.9):
        for ym in (14.0, 22.0, 30.0):
            xat(h, g, KGY("urn_round"), xm, ym)
    xscatter(h, g, [KKH("pumpkin_orange_small"), KKH("pumpkin_yellow_small"), KKH("pumpkin_orange")], 6, pad=10.0,
             area=(22.0, 26.0, 33.0, 33.0))
    xscatter(h, g, [KGY("hay_bale")], 2, pad=20.0, area=(24.0, 8.0, 33.0, 16.0))
    xscatter(h, g, [KKH("bench")], 3, pad=30.0, yaw_any=False)


def ext_service_yard(h, g):
    # the G. parcels' delivery crates, still stacked where the boat left them
    xscatter(h, g, [KKR("crate_carrots"), KKR("crate_potatoes"), KKR("crate_lid"), KKR("crate_cheese")], 6, s=0.6,
             pad=10.0, area=(-33.0, 12.0, -22.0, 18.0))
    xscatter(h, g, [KKD("barrel_small"), QD("Barrel")], 4, s=0.55, pad=10.0)
    xscatter(h, g, [KGY("hay_bale"), KGY("hay_bale_bundled")], 3, pad=15.0)
    xscatter(h, g, [KGY("shovel"), QD("Bucket")], 3, s=1.0, pad=8.0, collide=False)
    xscatter(h, g, [KKD("trunk_medium_B")], 1, s=0.9, pad=12.0)


def ext_greenhouse(h, g):
    xscatter(h, g, [KKF("cactus_medium_A"), KKF("cactus_medium_B"), KKF("cactus_small_A"), KKF("cactus_small_B")], 6,
             pad=10.0)
    xscatter(h, g, [KF("pottedPlant")], 5, s=1.9, pad=15.0)
    xscatter(h, g, [KKR("crate_tomatoes"), KKR("crate_lettuce")], 2, s=0.6, pad=10.0)
    xscatter(h, g, [QD("Bucket")], 2, s=1.6, pad=8.0)


def ext_graveyard(h, g):
    stones = [KGY("gravestone_bevel"), KGY("gravestone_decorative"), KGY("gravestone_round"), KGY("gravestone_roof"),
              KGY("gravestone_cross"), KGY("gravestone_broken"), KKH("gravestone"), KKH("grave_A")]
    xscatter(h, g, stones, 12, pad=25.0, yaw_any=False)
    # the freshly dug grave with the shovel still in the mound (whose?)
    xscatter(h, g, [KGY("grave_border")], 1, pad=40.0, yaw_any=False, collide=False)
    xscatter(h, g, [KGY("shovel_dirt")], 1, pad=15.0)
    xscatter(h, g, [PPM(ANGEL)], 1, s=0.5, pad=40.0, yaw_any=False)
    xscatter(h, g, [KKH("tree_dead_medium"), KKH("tree_dead_small")], 2, pad=30.0, collide=False)
    xscatter(h, g, [KKH("post_lantern"), KGY("lantern_candle")], 3, pad=20.0)
    xscatter(h, g, [KGY("pumpkin_carved"), KKH("candle_melted"), KKH("skull_candle")], 5, pad=10.0, collide=False)
    xscatter(h, g, [KGY("urn_square")], 2, pad=15.0)


def ext_cliff_path(h, g):
    xscatter(h, g, [KGY("rocks"), KGY("rocks_tall")], 2, s=0.6, pad=20.0)
    xscatter(h, g, [KGY("lantern_glass")], 1, pad=10.0)


# =================================================================================================== SPRINT-040
# The luxury layer (every indoor room) and the new wings' rooms. Density target: SPRINT-018 had ~0.24 meshes per m2
# indoors; the contract asks >= 1.5x, so lux() fills each room to LUX_DENSITY per m2 (corridors a little less), with
# layered pieces: pictures in two rows, sconces, cabinets and shelves carrying small things, bookcases with full
# shelves, seating groups, plants, rugs. Everything goes through the same free-space map (doors, stairs, chore
# spots, secrets, compartments and trap zones stay clear).
LUX_DENSITY = {"room": 0.44, "circulation": 0.34}
LUX_ROOM = {"blue_room": 0.62, "nursery": 0.6, "study": 0.6}   # SPRINT-018's sparse rooms: filled up
LUX_SKIP = {"storm_tower", "clock_room"}       # ladder / winch rooms: the SPRINT-018 dressing only
CLUTTER = [KKF("book_set"), KKF("book_single"), KKF("lamp_table"), KKR("jar_A_small"), KKR("jar_C_small"),
           KKR("bowl_small"), KKD("candle_lit"), KKD("candle_triple"), KKD("bottle_A_labeled_brown"),
           KKD("bottle_B_green"), KKD("coin_stack_small"), KKD("box_small_decorated"), KKF("pictureframe_standing_A"),
           KKF("cactus_small_A"), P + "Vase_4", P + "CandleStick", P + "Chalice", P + "Book_7", P + "Scroll_1"]
SHELF_BITS = [KKF("book_set"), KKF("book_single"), KKR("jar_A_small"), KKR("jar_D_small"), KKD("bottle_A_brown"),
              KKD("candle_melted"), KKD("box_small"), P + "Book_7", P + "Vase_4"]
LUX_WALL = [KKF("cabinet_medium_decorated"), KKF("cabinet_small_decorated"), KKF("shelf_B_large_decorated"),
            KKF("shelf_B_small_decorated"), KF("sideTableDrawers"), P + "Bookcase_2", KKF("lamp_standing"),
            KF("pottedPlant"), QD("Pedestal2"), KKF("cabinet_medium"), KF("bookcaseClosedWide")]
LUX_SCALE = {KF("sideTableDrawers"): 2.0, KF("pottedPlant"): 1.9, QD("Pedestal2"): 0.55, KF("bookcaseClosedWide"): 2.2}
FLAT_TOP = (KF("sideTableDrawers"), KKF("cabinet_medium"))      # (decorated tops are not flat: nothing on them)
LUX_FLOOR = [KKF("rug_oval_A"), KKF("rug_oval_B"), KKF("rug_rectangle_A"), KKF("rug_rectangle_stripes_A")]


def _area(rid):
    x0, y0, x1, y1 = Bm.G.rect_of(Bm.R[rid]["poly"])
    try:
        from_cells = len(Bm.G.cells_of(Bm.R[rid]["poly"])) * 4.0
        return from_cells
    except Exception:
        return (x1 - x0) * (y1 - y0)


def lux_piece(h, g, walls):
    """One wall piece of the luxury set + what stands on it. Returns True when something was placed."""
    path = h.R.choice(LUX_WALL)
    if not Bm.have(path):
        return False
    s = LUX_SCALE.get(path, 1.0)
    tall = 180.0 if "bookcase" in path or "cabinet_medium" in path else None
    items = xwall(h, g, path, walls, 1, s=s, gap=3.0, tag="tall" if tall else "block")
    if not items:
        return False
    it = items[0]
    if "Pedestal" in path:
        xon(h, it, h.R.choice([PPM(STATUE), P + "Vase_4", KKD("candle_triple")]), 0.0, 0.0,
            s=0.35 if "Statue" in it.path else 1.0)
    elif path == P + "Bookcase_2":
        # the measured shelf boards (kg_interiors.SHELVES): three book groups per board
        for z in KI.SHELVES[P + "Bookcase_2"][1:]:
            for u in (-32.0, 0.0, 32.0):
                if h.R.random() < 0.85:
                    on(h, it, h.R.choice(KI.BOOKS + KI.SMALL_BOOKS), u + h.R.uniform(-4, 4), 2.0, z=z + 0.2)
    elif path in FLAT_TOP:
        xtop(h, it, CLUTTER, h.R.choice((2, 3, 4)), s=0.55)
    return True


CENTRE_DONE = {"great_hall", "ballroom", "dining", "kitchen", "chapel", "billiard", "attic", "wine_cellar", "cistern",
               "servants_hall", "morning_room", "map_room", "games_room", "orangery", "crypt", "ossuary", "vault",
               "wine_catacombs", "sewing_room", "music_room", "staircase_hall", "storm_tower", "clock_room"}


def centre_group(h, g):
    """A furnished island in the middle of a big room: a large rug, a table with chairs and things on it, a
    chandelier over it."""
    x0, y0, x1, y1 = h.rect
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    lx, ly = loc(h, cx, cy)
    if g.spot(PPM(RUG), lx, ly, 0.0, scale=S(1.5), pad=0.0):
        h.put(PPM(RUG), lx, ly, g.z + 0.5, 0.0 if (x1 - x0) >= (y1 - y0) else 90.0, scale=S(1.5), collide=False)
    table = h.R.choice([KKF("table_medium_long"), KKF("table_low"), KKF("table_medium")])
    for dx, dy in ((0, 0), (1.5, 0), (-1.5, 0), (0, 1.5), (0, -1.5), (2.5, 2.0), (-2.5, -2.0)):
        tx, ty = loc(h, cx + dx, cy + dy)
        yaw = 0.0 if (x1 - x0) >= (y1 - y0) else 90.0
        if not g.spot(table, tx, ty, yaw, pad=100.0):
            continue
        t = g.place(table, tx, ty, yaw)
        b = KI.bounds(table)
        hy = (b[4] - b[1]) / 2.0 + 32.0
        chair = h.R.choice([KKF("chair_B_wood"), KKF("chair_A_wood"), KKF("armchair")])
        for u in ((-45.0, 45.0) if (b[3] - b[0]) > 120 else (0.0,)):
            for side in (1.0, -1.0):
                px, py = t.at(u, side * hy)
                cyaw = t.lyaw + (180.0 if side > 0 else 0.0)
                if g.spot(chair, px, py, cyaw):
                    g.place(chair, px, py, cyaw)
        xtop(h, t, CLUTTER, 3, s=0.55)
        break
    xhang(h, PPM(CHANDELIER2), cx, cy, Bm.H - 75.0, s=1.2)


def lux(h, g, rid):
    r = Bm.R[rid]
    area = _area(rid)
    if area < 40.0 or rid in LUX_SKIP:
        return              # the stair landings / the tower: nothing more in the way of the steps and the ladder
    dens = LUX_ROOM.get(rid, LUX_DENSITY.get(r["kind"], 0.4))
    target = int(area * dens)
    walls = ["front", "back", "left", "right"]
    if r["kind"] == "room" and area >= 80.0 and rid not in CENTRE_DONE:
        centre_group(h, g)          # look round 1: the middle of the big rooms read empty
    # pictures in two rows on closed wall first (cheap: no floor), sconces between them
    if h.count < target:
        pictures(h, g, walls, z=175.0, every=300.0)
    if h.count < target and r["kind"] == "room":
        xmount(h, g, KKF("pictureframe_small_B"), walls, 120.0, every=340.0)
    if h.count < target:
        xmount(h, g, P + "Lantern_Wall", walls, 205.0, every=560.0)
    fails = 0
    while h.count < target and fails < 6:
        ok = lux_piece(h, g, walls)
        if r["kind"] == "room" and area >= 80.0 and h.count < target and h.R.random() < 0.25:
            ok = bool(sitting_corner(h, g, walls)) or ok
        if r["kind"] == "room" and h.count < target and h.R.random() < 0.15 and area > 70:
            ok = bool(reading_table(h, g, 2, table=KKF("table_small"), s=0.9)) or ok
        if area >= 80.0 and h.count < target and h.R.random() < 0.2:
            ok = xscatter(h, g, [KF("pottedPlant"), KKF("cactus_medium_A"), QD("Vase")], 1, s=1.4, pad=20.0) > 0 or ok
        fails = 0 if ok else fails + 1
    if r["kind"] == "room" and area > 40:
        xscatter(h, g, LUX_FLOOR, 1, pad=0.0, collide=False, tag=None, yaw_any=False)
    Bm.stats.setdefault("lux", {})[rid] = [h.count, target]


def new_room(rid, recipe, style, light=True, cap=1, lz=250.0, li=10.0, lr=1100.0):
    """A SPRINT-040 room: its recipe, one light over the middle (unless light=False: lit by a neighbour's), done()."""
    h, g = room_for(rid, cap=cap, style=style)
    recipe(h, g)
    x0, y0, x1, y1 = h.rect
    if light and h.lights < 1:
        h.light(*loc(h, (x0 + x1) / 2, (y0 + y1) / 2), lz, li, lr)
    done(h, rid)
    return h, g


def salon(colour_rug, sofa=KKF("couch_pillows")):
    def recipe(h, g):
        xwall(h, g, sofa, ["back", "front"], 2, gap=5.0)
        for _ in range(2):
            sitting_corner(h, g, ["left", "right", "back", "front"])
        t = reading_table(h, g, 4, table=KKF("table_low"), s=1.2)
        xwall(h, g, KKF("cabinet_medium_decorated"), ["left", "right"], 2, gap=3.0, tag="tall")
        xwall(h, g, PPM(CLOCK), ["front", "back"], 1, s=1.4, gap=3.0, tag="tall")
        xscatter(h, g, [colour_rug], 1, pad=0.0, collide=False, tag=None, yaw_any=False)
        x0, y0, x1, y1 = h.rect
        xhang(h, PPM(CHANDELIER), (x0 + x1) / 2, (y0 + y1) / 2, Bm.H - 70.0, s=1.2)
    return recipe


def music_room(h, g):
    # the grand piano: a black-lacquered long table with its lid propped open, a stool, the harp (a lute, big)
    x, y = loc(h, -29.0, -35.6)
    if g.spot(KKF("table_medium_long"), x, y, 90.0, scale=S(1.1)):
        pn = g.place(KKF("table_medium_long"), x, y, 90.0, scale=S(1.1))
        Bm.B.recs[-1]["material"] = Bm.MAT.get("Iron")
        on(h, pn, E + "Cube", 0.0, 0.0, scale=(1.5, 0.95, 0.04), lyaw=0.0)
        Bm.B.recs[-1]["material"] = Bm.MAT.get("Iron")
        x2, y2 = pn.at(0.0, 95.0)
        if g.spot(KKF("chair_stool_wood"), x2, y2, 0.0):
            g.place(KKF("chair_stool_wood"), x2, y2, 0.0)
    xscatter(h, g, [PPM(LUTE)], 2, s=2.2, pad=20.0, yaw_any=False)
    for _ in range(3):
        xwall(h, g, KKF("chair_B_wood"), ["left", "front"], 2, gap=1.0)
    hearth_at(h, g, "back", -26.0)
    xwall(h, g, KKF("shelf_B_large_decorated"), ["back", "front"], 2, gap=2.0)


def staircase_hall(h, g):
    for xm in (-7.0, 7.0):
        for ym in (-38.0, -30.0):
            p = xat(h, g, QD("Pedestal2"), xm, ym, s=0.6)
            xon(h, p, PPM(STATUE), 0.0, 0.0, s=0.4)
    xmount(h, g, KKD("banner_shield_red"), ["left", "right"], 230.0, every=420.0, s=0.6)
    xwall(h, g, PPM(CLOCK), ["front"], 1, s=1.6, gap=3.0, tag="tall")
    xscatter(h, g, [PPM(RUG)], 1, s=1.4, pad=0.0, collide=False, tag=None, yaw_any=False)
    xwall(h, g, KF("benchCushion"), ["left", "right"], 2, s=2.2, gap=20.0)


def gallery_run(h, g):
    x0, y0, x1, y1 = h.rect
    if x1 - x0 > y1 - y0:
        rug_runner(h, g, x0 + 1.5, (y0 + y1) / 2, x1 - 1.5, (y0 + y1) / 2)
    else:
        rug_runner(h, g, (x0 + x1) / 2, y0 + 1.5, (x0 + x1) / 2, y1 - 1.5)
    for wall in ("front", "back", "left", "right"):
        xmount(h, g, KKF("pictureframe_large_B"), [wall], 185.0, every=260.0)
    for p in xwall(h, g, QD("Pedestal2"), ["front", "back", "left", "right"], 6, s=0.55, gap=8.0):
        xon(h, p, h.R.choice([PPM(STATUE), P + "Vase_4"]), 0.0, 0.0, s=0.35)
    xwall(h, g, KF("benchCushion"), ["front", "back"], 3, s=2.2, gap=40.0)
    xwall(h, g, PPM(CLOCK), ["front", "back"], 1, s=1.4, gap=3.0, tag="tall")


def servants_hall(h, g):
    x, y = loc(h, -49.0, -39.0)
    for k, dy in enumerate((-85.0, 85.0)):
        if g.spot(K + "LongTable", x, y + dy, 0.0):
            t = g.place(K + "LongTable", x, y + dy, 0.0)
            for u in (-80.0, -20.0, 40.0):
                on(h, t, h.R.choice([K + "Plate", K + "Cup", K + "Bowl", P + "Mug"]), u, 0.0)
    hearth_at(h, g, "left", -38.0)
    xwall(h, g, KKR("shelf_papertowel_decorated"), ["back", "right"], 1, gap=2.0)
    for sh in xwall(h, g, KKD("shelves"), ["back", "front"], 2, s=0.8, gap=2.0, tag="tall"):
        xtop(h, sh, [KKD("plate_stack"), KKD("bottle_A_brown"), KKD("candle")], 3, s=0.5)
    xwall(h, g, KKD("keyring_hanging"), ["front"], 1, gap=1.0)


def housekeeper(h, g):
    KI.desk(h, g)
    xwall(h, g, KKF("bed_single_A"), ["back", "left"], 1, gap=3.0)
    xwall(h, g, KKD("shelf_large"), ["front", "right"], 2, s=0.8, gap=2.0, tag="tall")
    xmount(h, g, KKD("keyring_hanging"), ["right"], 150.0, every=120.0)
    sitting_corner(h, g, ["left", "back", "front"])
    xwall(h, g, K + "Shelf4", ["front", "back"], 1, gap=2.0, tag="tall")


def silver_room(h, g):
    for c in xwall(h, g, KKF("cabinet_medium"), ["back", "front", "left"], 3, gap=2.0, tag="tall"):
        xtop(h, c, [P + "Chalice", P + "CandleStick", KKD("plate"), P + "CandleStick_Triple"], 3, s=0.7)
    for b in xwall(h, g, P + "Workbench", ["right"], 1, gap=2.0):
        for u in (-60.0, -20.0, 20.0, 60.0):
            on(h, b, h.R.choice([P + "Chalice", P + "CandleStick", P + "Table_Plate"]), u, 0.0)


def boiler_room(h, g):
    x, y = loc(h, -41.0, -26.2)
    bx = place(h, g, PIR + "Barrel_5", -41.5, -27.0, 0.0, scale=(1.1, 1.1, 1.5))
    if bx:
        Bm.B.recs[-1]["material"] = Bm.MAT.get("Copper")
    lx, ly = loc(h, -41.5, -27.0)
    for k in range(4):
        lx, ly = loc(h, -39.0 - k * 1.2, -33.0)
        h.put(E + "Cylinder", lx, ly, g.z + 240.0, 0.0, roll=90.0, scale=(0.18, 0.18, 1.2), collide=False)
        Bm.B.recs[-1]["material"] = Bm.MAT.get("Copper")
    n0 = len(Bm.B.recs)
    xscatter(h, g, [KKD("rubble_half")], 2, s=0.8, pad=10.0, collide=False)
    for r_ in Bm.B.recs[n0:]:
        if "rubble" in r_["m"]:
            r_["material"] = Bm.MAT.get("Soot")          # the coal heap
    for sh in xwall(h, g, KKD("shelf_large"), ["left", "front"], 2, s=0.8, gap=2.0, tag="tall"):
        xtop(h, sh, [P + "Bottle_1", KKD("bottle_C_brown"), KKD("candle")], 3, s=0.6)
    xscatter(h, g, [QD("Bucket"), KKD("barrel_small")], 3, s=0.8, pad=10.0)


def east_room(kind):
    def recipe(h, g):
        if kind == "smoking":
            xwall(h, g, KKF("couch"), ["back", "front"], 2, gap=4.0)
            for _ in range(3):
                sitting_corner(h, g, ["left", "right", "back", "front"])
            for t in xwall(h, g, KKF("table_small"), ["left", "right"], 2, s=0.8, gap=4.0):
                xtop(h, t, [KKD("box_small_decorated"), KKD("bottle_A_labeled_brown"), KKD("candle_lit")], 3, s=0.4)
            hearth_at(h, g, "right", -39.0)
        elif kind == "gun":
            for ws in xwall(h, g, P + "WeaponStand", ["back", "left", "right"], 4, gap=3.0):
                pass
            xmount(h, g, KKD("sword_shield"), ["back", "left", "right"], 170.0, every=180.0, s=0.8)
            xwall(h, g, P + "Workbench", ["front"], 1, gap=3.0)
            xwall(h, g, PPM("Armor_Golden_65ltdmlBcy"), ["back", "right"], 1, s=1.2, gap=3.0, tag="tall")
            xscatter(h, g, [KKD("trunk_medium_A"), KKD("barrel_small")], 3, s=0.7, pad=10.0)
        elif kind == "map":
            t = reading_table(h, g, 3, table=KKF("table_medium_long"), s=1.1)
            xtop(h, t, [P + "Scroll_1", P + "Scroll_2", P + "Book_7"], 4, s=0.8)
            xscatter(h, g, [PPM(GLOBE)], 2, s=3.0, pad=15.0)
            xscatter(h, g, [PPM(TELESCOPE)], 1, s=1.2, pad=20.0)
            xwall(h, g, KKD("chest"), ["back", "left", "right"], 3, s=0.45, gap=3.0)
        elif kind == "games":
            t = reading_table(h, g, 2, table=KKF("table_small"), s=1.0)
            if t:
                xon(h, t, E + "Cube", 0.0, 0.0, s=(0.5, 0.5, 0.02))
                Bm.B.recs[-1]["material"] = Bm.MAT.get("Felt")
            reading_table(h, g, 4, table=KKF("table_medium"), s=1.0)
            xmount(h, g, P + "Shield_Wooden", ["back"], 170.0, every=400.0)
            sitting_corner(h, g, ["left", "right", "front"])
        elif kind == "orangery":
            xscatter(h, g, [KF("pottedPlant"), DV + "Planter_Pot"], 14, s=1.9, pad=25.0)
            for r_ in Bm.B.recs[-14:]:
                if "Planter_Pot" in r_["m"]:
                    r_["material"] = Bm.MAT.get("Terracotta")
            xat(h, g, xp("Goblin", "branch_cage"), 52.2, -14.2, 0.0, s=0.7)
            xwall(h, g, KKH("bench"), ["front", "back"], 3, gap=30.0)
            xscatter(h, g, [QD("Bucket"), KKF("cactus_medium_B")], 4, s=1.2, pad=12.0)
        elif kind == "morning":
            t = reading_table(h, g, 6, table=KKF("table_medium_long"), s=1.2)
            xtop(h, t, [KKD("plate_food_A"), KKD("plate_food_B"), K + "Cup", KKD("candle_triple")], 5, s=0.5)
            xwall(h, g, KKF("cabinet_medium_decorated"), ["back"], 1, gap=3.0, tag="tall")
            xwall(h, g, KKF("couch_pillows"), ["right", "front"], 1, gap=5.0)
            xscatter(h, g, [KF("pottedPlant")], 3, s=1.9, pad=20.0)
    return recipe


def guest_bed(colour):
    def recipe(h, g):
        bed = None
        for path in (BD + f"DoubleBed{colour}", KKF("bed_double_A"), P + "Bed_Twin1"):
            bed = g.on_wall(path, ["back", "right", "left", "front"], gap=3.0, tall=90.0)
            if bed:
                break
        if bed:
            x0, y0, _, x1, y1, _ = KI.bounds(bed.path)
            for side in (-1, 1):
                nb = KI.bounds(BD + "BedsideTable")
                u = (x1 + 6 - nb[0]) if side > 0 else (x0 - 6 - nb[3])
                x, y = bed.at(u, y0 - nb[1] + 1.0)
                it = g.place(BD + "BedsideTable", x, y, bed.lyaw) if g.spot(BD + "BedsideTable", x, y, bed.lyaw) else None
                if it:
                    on(h, it, P + "CandleStick", 0.0, 0.0)
        KI.wardrobe(h, g)
        KI.desk(h, g)
        KI.corner_set(h, g)
        ext_bedroom(h, g)
    return recipe


def chinese_room(h, g):
    for p in xwall(h, g, QD("Pedestal2"), ["front", "back", "left", "right"], 5, s=0.55, gap=6.0):
        xon(h, p, QD("Vase"), 0.0, 0.0, s=1.2)
    for c in xwall(h, g, KKF("cabinet_medium"), ["back", "front"], 3, gap=2.0, tag="tall"):
        Bm.B.recs[-1]["material"] = Bm.MAT.get("Velvet")
        xtop(h, c, [QD("Vase"), P + "Vase_4", KKD("candle_thin_lit")], 3, s=0.6)
    xmount(h, g, KKD("banner_patternC_red"), ["left", "right"], 190.0, every=380.0, s=0.5)
    xwall(h, g, KKF("couch_pillows"), ["left", "right"], 1, gap=5.0)
    reading_table(h, g, 2, table=KKF("table_low"), s=1.0)


def sewing_room(h, g):
    xscatter(h, g, [PPM(MANNEQUIN)], 4, s=0.2, pad=25.0)
    for t in xwall(h, g, KKF("table_medium"), ["back", "front"], 3, s=1.0, gap=3.0):
        xtop(h, t, [KKF("pillow_A"), KKF("pillow_B"), P + "Bag", KKD("box_small")], 3, s=0.5)
    xwall(h, g, BD + "Wardrobe", ["left", "right"], 2, gap=3.0, tag="tall")
    xscatter(h, g, [P + "Basket" if Bm.have(P + "Basket") else KKF("pillow_A"), KKF("chair_A_wood")], 4, pad=10.0)


def dressing_room(h, g):
    xwall(h, g, BD + "Wardrobe", ["back", "front", "left"], 4, gap=2.0, tag="tall")
    xmount(h, g, KF("bathroomMirror"), ["right"], 140.0, every=260.0, s=2.0)
    xwall(h, g, KKF("shelf_B_small"), ["front", "back"], 2, gap=2.0)
    xwall(h, g, KKF("couch"), ["right", "back"], 1, gap=4.0)
    xscatter(h, g, [KKD("trunk_small_A"), KKD("box_large")], 3, s=0.6, pad=10.0)


def picture_gallery(h, g):
    gallery_run(h, g)
    xmount(h, g, PPM("Painting_rsZqX75a8x"), ["front", "back", "left"], 230.0, every=420.0, s=1.2)
    xscatter(h, g, [PPM(STATUE), PPM(ANGEL)], 3, s=0.6, pad=40.0, yaw_any=False)


def crypt(h, g):
    xwall(h, g, KKH("coffin_decorated"), ["back", "front"], 4, gap=10.0)
    xscatter(h, g, [KKH("candle_triple")], 4, s=0.8, pad=8.0, collide=False)
    xscatter(h, g, [PPM(COFFIN)], 1, s=1.0, pad=30.0, yaw_any=False)
    xscatter(h, g, [KKH("skull_candle"), KKH("bone_A"), KKH("bone_B"), KKH("candle_melted")], 10, pad=6.0, collide=False)
    xwall(h, g, KKH("shrine_candles"), ["left", "right"], 2, gap=6.0)
    xmount(h, g, QD("Cobweb"), ["front", "back", "left", "right"], 210.0, every=400.0)
    p = xat(h, g, QD("Pedestal2"), 19.0, -12.0, s=0.6)
    xon(h, p, P + "Vase_4", 0.0, 0.0, s=1.0)                       # the reliquary (secret chore)


def observatory(h, g):
    xat(h, g, PPM(TELESCOPE), 48.5, -40.2, 150.0, s=1.6)
    xscatter(h, g, [PPM(GLOBE)], 1, s=3.0, pad=20.0)
    t = reading_table(h, g, 1, table=KKF("table_medium"), s=1.0)
    xtop(h, t, [P + "Scroll_1", P + "Scroll_2", KKD("candle_lit"), P + "Book_7"], 4, s=0.8)
    xwall(h, g, KKD("bed_floor"), ["front", "left"], 1, gap=3.0)
    xwall(h, g, KF("bookcaseOpen"), ["back", "right"], 2, s=2.0, gap=2.0, tag="tall")
    xmount(h, g, KKF("pictureframe_large_A"), ["front", "left"], 180.0, every=300.0)


def wine_catacombs(h, g):
    for sh in xwall(h, g, KKD("shelf_large"), ["front", "back", "left"], 8, s=0.9, gap=1.0, tag="tall"):
        xtop(h, sh, [KKD("bottle_A_brown"), KKD("bottle_B_green"), KKD("bottle_C_brown"), KKD("bottle_A_labeled_green")],
             5, s=0.55)
    xscatter(h, g, [KKD("barrel_large"), KKD("barrel_small_stack"), KKD("keg"), QD("Barrel")], 8, s=0.8, pad=15.0)
    xscatter(h, g, [KKD("candle_lit"), KKD("candle_melted")], 6, pad=5.0, collide=False)
    xmount(h, g, QD("Cobweb2"), ["front", "back", "left", "right"], 210.0, every=380.0)


def vault(h, g):
    xwall(h, g, KKD("chest_gold"), ["front", "back", "left"], 4, s=0.5, gap=3.0)
    xwall(h, g, QD("Chest_Gold"), ["front", "back", "right"], 2, s=0.9, gap=3.0)
    for sh in xwall(h, g, KKD("shelf_large"), ["front", "back"], 4, s=0.8, gap=2.0, tag="tall"):
        xtop(h, sh, [P + "Scroll_1", P + "Scroll_2", KKD("coin_stack_medium"), P + "Book_7"], 4, s=0.6)
    xscatter(h, g, [QD("Coin_Pile"), QD("Bag_Coins"), KKD("coin_stack_large")], 6, pad=6.0, collide=False)
    xmount(h, g, KKD("torch_mounted"), ["left", "right"], 150.0, every=500.0, s=0.5)


def ossuary(h, g):
    for sh in xwall(h, g, KKD("shelves"), ["front", "back", "left"], 8, s=0.8, gap=1.0, tag="tall"):
        xtop(h, sh, [KKH("skull"), QD("Skull"), KKH("bone_C"), KKH("skull_candle")], 5, s=0.7)
    xscatter(h, g, [KKH("ribcage"), KKH("bone_A"), KKH("bone_B"), KKH("candle_melted")], 10, pad=6.0, collide=False)
    xwall(h, g, KKH("pillar"), ["left", "right"], 2, gap=10.0, tag="tall")


def steps(h, g):
    xscatter(h, g, [KKH("lantern_standing"), KKD("crates_stacked"), QD("Barrel")], 3, s=0.8, pad=10.0)


def guest_bath(h, g):
    ext_bath(h, g)
    xwall(h, g, PPM(BATHTUB), ["back", "right", "front"], 1, s=1.2, gap=4.0)


NEW_ROOMS = [
    ("music_room", music_room, 50), ("green_salon", salon(KKF("rug_rectangle_stripes_A")), 51),
    ("blue_salon", salon(KKF("rug_oval_B"), KKF("couch")), 52), ("staircase_hall", staircase_hall, 53),
    ("yellow_salon", salon(KKF("rug_rectangle_B")), 54), ("card_room", salon(KKF("rug_rectangle_A"), KKF("couch")), 55),
    ("trophy_room", east_room("gun"), 56), ("long_gallery", gallery_run, 57),
    ("west_passage", gallery_run, 58), ("servants_hall", servants_hall, 59), ("housekeeper", housekeeper, 60),
    ("silver_room", silver_room, 61), ("boiler_room", boiler_room, 62), ("east_hall", gallery_run, 63),
    ("smoking_room", east_room("smoking"), 64), ("gun_room", east_room("gun"), 65), ("map_room", east_room("map"), 66),
    ("games_room", east_room("games"), 67), ("orangery", east_room("orangery"), 68),
    ("morning_room", east_room("morning"), 69), ("north_corridor", gallery_run, 70), ("grand_landing", staircase_hall, 71),
    ("lilac_room", guest_bed("Red"), 72), ("sewing_room", sewing_room, 73), ("chinese_room", chinese_room, 74),
    ("dressing_room", dressing_room, 75), ("upper_west", gallery_run, 76), ("picture_gallery", picture_gallery, 77),
    ("east_corridor", gallery_run, 78), ("green_room", guest_bed("Blue"), 79), ("gold_room", guest_bed("Red"), 80),
    ("rose_room", guest_bed("Red"), 81), ("ivory_room", guest_bed("Blue"), 82), ("guest_bath", guest_bath, 83),
    ("observatory", observatory, 84), ("wine_catacombs", wine_catacombs, 85), ("catacomb_steps", steps, 86),
    ("vault", vault, 87), ("ossuary", ossuary, 88), ("ossuary_steps", steps, 89), ("crypt", crypt, 90),
]
# lit by a neighbour's light (shadowless lights reach through the walls) - keeps the manor within 70 lights
NO_LIGHT = {"silver_room", "catacomb_steps", "ossuary_steps", "upper_west", "gun_room", "games_room", "grand_landing",
            "rose_room", "west_passage"}


EXT = {"great_hall": ext_great_hall, "vestibule": ext_vestibule, "dining": ext_dining, "kitchen": ext_kitchen,
       "pantry": ext_pantry, "laundry": ext_laundry, "chapel": ext_chapel, "library": ext_library,
       "ballroom": ext_ballroom, "servants_corridor": ext_servants_corridor, "wine_cellar": ext_wine_cellar,
       "spark_room": ext_spark_room, "cistern": ext_cistern, "smugglers_tunnel": ext_smugglers_tunnel,
       "boathouse": ext_boathouse, "gallery": ext_gallery, "corridor_w": ext_corridor, "corridor_e": ext_corridor,
       "blue_room": ext_bedroom, "red_room": ext_bedroom, "master": ext_bedroom, "bath": ext_bath,
       "studio": ext_studio, "nursery": ext_nursery, "study": ext_study, "billiard": ext_billiard,
       "storm_terrace": ext_storm_terrace, "attic": ext_attic, "clock_room": ext_clock_room,
       "storm_tower": ext_storm_tower, "tower_top": ext_tower_top, "courtyard": ext_courtyard,
       "service_yard": ext_service_yard, "greenhouse": ext_greenhouse, "graveyard": ext_graveyard,
       "cliff_path": ext_cliff_path}


# =================================================================================================== entry
def run(builder, kg_interiors):
    global Bm, KI, SMHouse
    Bm, KI = builder, kg_interiors
    SMHouse = make_house_class()
    DRESSED.clear()
    jobs = [great_hall, vestibule, dining, kitchen, pantry, laundry, chapel, library, ballroom, servants_corridor,
            wine_cellar, spark_room, cistern, smugglers_tunnel, boathouse, gallery,
            lambda: corridor("corridor_w", False), lambda: corridor("corridor_e", True),
            lambda: bedroom_room("blue_room", "Blue", 40),
            bath,
            lambda: bedroom_room("red_room", "Red", 41, fire=("left", -2.0), hero=True),
            studio, nursery, study,
            lambda: bedroom_room("master", "Red", 42, fire=("front", 26.0), extra=master_extra),
            billiard, storm_terrace, attic, clock_room, storm_tower, tower_top, roof_walk]
    # SPRINT-040: the new wings before the grounds (the grounds' lamp posts are the first to go when the 70-light
    # budget runs out: moonlight and lightning light the outside)
    for rid, fn, style in NEW_ROOMS:
        def job(rid=rid, fn=fn, style=style):
            new_room(rid, fn, style, light=rid not in NO_LIGHT)
        job.__name__ = rid
        jobs.append(job)
    jobs += [courtyard, service_yard, greenhouse, graveyard, cliff_path]
    for job in jobs:
        name = getattr(job, "__name__", "job")
        try:
            job()
        except Exception:
            import traceback
            Bm.log(f"dress {name} FAILED:\n{traceback.format_exc()}")
            Bm.stats.setdefault("dress_failed", []).append(name)
    Bm.SHIM.room = None
    Bm.log(f"dress: {len(DRESSED)} rooms, {sum(DRESSED.values())} meshes; per room {DRESSED}")
