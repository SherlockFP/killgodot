"""Zone "japan": the Japanese garden in the village (r 15 m around (2600, 2000)) and the shrine island across the bay
(r 40 m around (4500, 15000)).

Already built by kg_build_village.py (do not duplicate): garden koi pond + arched bridge + koi, 4 garden lamps, 2 toro
lanterns, sakura/maple/bamboo/bonsai, 2 rocks, 7 stepping stones from (2400, 1100); island mesh, pagoda at the centre
(yaw 180), 4 toro lanterns, 3 sakura/bonsai/maple, the big torii in the shallows at (4200, 11900).

This module adds the stories:
  garden  - senbon-torii tunnel entrance between the two southern houses, lantern-lined stepping path, tea pavilion with
            a tea set, karesansui rock garden around the bonsai (raked lines + rock islands), bamboo grove with a hidden
            hokora + chest, koinobori poles, paper-lantern strings over the lane (matsuri), a yatai food stall, benches.
  island  - jetty + moored boat under the big torii, stone steps climbing through a senbon-torii tunnel to the pagoda,
            shrine plaza (offering box, ema board, omikuji rack, incense burner, shrine shop), sakura groves with petals,
            hokora, koinobori, lantern strings, shore rocks, bamboo, the priest's store + loot chest behind the pagoda.
"""
import math
import random

import unreal

import kg_dress_common as C

JP, N, P, V, WP, PIR = C.JP, C.N, C.P, C.V, C.WP, C.PIR
SPHERE = "/Engine/BasicShapes/Sphere"
CYL = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"
MAT = "/Game/KillGodot/Materials/"
KITCHEN = "/Game/KillGodot/Env/Furniture/KG_Kitchen/StaticMeshes/"
INTERIOR = "/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/"

GX, GY = C.GARDEN
IX, IY = C.ISLAND
R = random.Random(2026)

# Lantern material candidates (the Japan props all use M_KG_JapanGlow); picked after a visual test.
# (tested: M_KG_TaskMarker / M_KG_Embers render black on the sphere, the default material reads flat grey)
LANTERN_MATS = [MAT + "M_KG_JapanGlow"]
LANTERN_MAT = MAT + "M_KG_JapanGlow"


# ============================================================================================ small helpers
class Batch:
    """Collects repeated meshes and flushes them through C.instanced (one HISM per mesh/collision/cull)."""

    def __init__(self):
        self.d = {}

    def add(self, path, x, y, z, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, collide=False, cull=6000.0):
        self.d.setdefault((path, collide, cull), []).append((x, y, z, yaw, scale, pitch, roll))

    def flush(self):
        n = 0
        for (path, collide, cull), tr in self.d.items():
            n += C.instanced(path, tr, collide=collide, cull=cull, sub="Instanced")
        self.d = {}
        return n


B = Batch()


def set_mat(actor, path):
    if actor is None or not path or path == "default":
        return
    m = unreal.load_asset(path)
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    if m and comp:
        comp.set_material(0, m)


def fwd(yaw):
    return math.cos(math.radians(yaw)), math.sin(math.radians(yaw))


def local(ox, oy, yaw, lx, ly):
    """World point of local (lx, ly) in a frame at (ox, oy) rotated by yaw (local +X = yaw direction)."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return ox + lx * c - ly * s, oy + lx * s + ly * c


def rod(path, a, b, thick, base_len, collide=False, cull=7000.0, centred=False):
    """A thin mesh (long along +Z, `base_len` cm; pivot at its base, or its middle when centred) stretched from
    point a to b (x, y, z)."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    if L < 1.0:
        return
    h = math.degrees(math.atan2(dy, dx))
    e = math.degrees(math.asin(max(-1.0, min(1.0, dz / L))))
    o = (a[0] + dx * 0.5, a[1] + dy * 0.5, a[2] + dz * 0.5) if centred else a
    B.add(path, o[0], o[1], o[2], yaw=h + 180.0, pitch=90.0 - e, scale=(thick, thick, L / base_len), collide=collide,
          cull=cull)


def beam(a, b, thick=1.0, cull=9000.0, collide=False):
    """Wooden beam/log between two points (the village kit's exterior wood corner post, laid along a to b)."""
    rod(V + "Corner_Exterior_Wood", a, b, thick, 300.0, collide=collide, cull=cull)


def line(a, b, thick=0.055, z=None, cull=5000.0):
    """Raked-gravel ridge: a thin grey cylinder lying from a to b."""
    rod(CYL, a, b, thick, 100.0, cull=cull, centred=True)


def seat_bench(x, y, z, face_yaw, sub="Seats"):
    """Bench seat (E to sit) whose sitter faces `face_yaw`."""
    s = C.seat(P + "Bench", x, y, z, yaw=face_yaw, seat_height=52.0, sub=sub)
    try:
        s.set_editor_property("mesh_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        s.set_seat_mesh(C.mesh(P + "Bench"))
    except Exception:
        pass
    return s


# ============================================================================================ island surface
_ISL = []
_izc = {}


def _island_comp():
    if not _ISL:
        for a in C.actors.get_all_level_actors():
            if a.get_actor_label() == "ShrineIsland":
                _ISL.append(a.get_component_by_class(unreal.StaticMeshComponent))
                break
        else:
            _ISL.append(None)
    return _ISL[0]


def iz(x, y):
    """Island surface height (traced against the island mesh only; the sea floor elsewhere)."""
    key = (int(round(x)), int(round(y)))
    if key in _izc:
        return _izc[key]
    z = None
    comp = _island_comp()
    if comp:
        try:
            loc, nrm, _bone, _hit = comp.line_trace_component(unreal.Vector(x, y, 3000.0), unreal.Vector(x, y, -1500.0),
                                                            True, False, False)
            if abs(nrm.z) > 0.05 or abs(nrm.x) > 0.05 or abs(nrm.y) > 0.05:
                z = loc.z
        except Exception:
            z = None
    if z is None:
        z = C.ground(x, y)
    _izc[key] = z
    return z


def iz_min(x, y, r):
    return min(iz(x + dx, y + dy) for dx in (-r, 0.0, r) for dy in (-r, 0.0, r))


def iz_max(x, y, r):
    return max(iz(x + dx, y + dy) for dx in (-r, 0.0, r) for dy in (-r, 0.0, r))


def islope(x, y, r=80.0):
    return iz_max(x, y, r) - iz_min(x, y, r)


def island_free(x, y, r):
    """Island spot: in the zone, on land, not claimed yet."""
    return C.in_zone(x, y) and iz(x, y) > 45.0 and not C.overlaps(x, y, r)


# ============================================================================================ shared set pieces
def torii(x, y, z, yaw, scale, sub="Torii"):
    return C.place(JP + "Torii", x, y, z, yaw=yaw, scale=scale, sub=sub, claim_r=0.0)


def paper_lantern(x, y, z, size=1.0, mat=None, sway=True, sub="Lanterns"):
    """A round chochin: glowing sphere + dark cap, gently swaying."""
    sc = (0.30 * size, 0.30 * size, 0.38 * size)
    if sway:
        a = C.mover(SPHERE, x, y, z, yaw=R.uniform(0, 360), scale=sc, sway=5.0, sway_hz=0.4, sub=sub)
    else:
        a = C.place(SPHERE, x, y, z, yaw=0.0, scale=sc, sub=sub, collide=False, cull=9000.0, shadow=False)
    set_mat(a, mat or LANTERN_MAT)
    try:
        comp = a.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_editor_property("cast_shadow", False)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    except Exception:
        pass
    # caps top and bottom (dark wood) + the cord up to the string
    B.add(CYL, x, y, z + 19.0 * size, scale=(0.2 * size, 0.2 * size, 0.04), cull=7000.0)
    B.add(CYL, x, y, z - 19.0 * size, scale=(0.2 * size, 0.2 * size, 0.04), cull=7000.0)
    return a


def post(x, y, z, height=320.0):
    """Wooden pole (the pirate flag pole, thinned)."""
    return C.place(PIR + "FlagLow_0", x, y, z - 10.0, scale=(0.55, 0.55, (height + 10.0) / 756.0), sub="Lanterns",
                   cull=12000.0)


def lantern_string(a, b, n, sag=45.0, mat=None, posts=(True, True), sway=True, zfun=None):
    """Rope between two points (x, y, z) sagging in the middle, with n paper lanterns hanging from it.
    Posts (when wanted) stand on zfun(x, y) and reach the rope end."""
    zf = zfun or C.ground
    for end, want in zip((a, b), posts):
        if want:
            base = zf(end[0], end[1])
            post(end[0], end[1], base, end[2] - base + 12.0)
    pts = []
    k = 8
    for i in range(k + 1):
        t = i / k
        pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
                    a[2] + (b[2] - a[2]) * t - sag * 4.0 * t * (1.0 - t)))
    for p0, p1 in zip(pts, pts[1:]):
        rod(INTERIOR + "SM_KG_RailPost", p0, p1, 0.18, 108.0)
    for i in range(n):
        t = (i + 0.5) / n
        x = a[0] + (b[0] - a[0]) * t
        y = a[1] + (b[1] - a[1]) * t
        z = a[2] + (b[2] - a[2]) * t - sag * 4.0 * t * (1.0 - t)
        B.add(INTERIOR + "SM_KG_RailPost", x, y, z - 22.0, scale=(0.1, 0.1, 0.2), cull=6000.0)
        paper_lantern(x, y, z - 38.0, size=R.uniform(0.9, 1.1), mat=mat, sway=sway)


def koinobori(x, y, z, wind_yaw=60.0, height=760.0):
    """Carp streamer pole: three koi (big to small) flying from the top of a tall pole, fluttering."""
    C.place(PIR + "FlagLow_0", x, y, z - 15.0, scale=(0.5, 0.5, height / 756.0), sub="Koinobori", cull=15000.0)
    top = z - 15.0 + height
    ball = C.place(SPHERE, x, y, top + 8.0, scale=0.22, sub="Koinobori", collide=False, cull=12000.0)
    set_mat(ball, LANTERN_MAT)
    dx, dy = fwd(wind_yaw)
    for k, (drop, sc) in enumerate(((70.0, 5.2), (175.0, 4.4), (270.0, 3.6))):
        L = 50.0 * sc
        cx, cy = x + dx * (L * 0.5 + 12.0), y + dy * (L * 0.5 + 12.0)
        C.mover(JP + "Koi", cx, cy, top - drop, yaw=wind_yaw + 180.0, pitch=-6.0, scale=(sc, sc * 0.9, sc * 1.1),
                sway=14.0 - k * 2.0, sway_hz=0.55 + 0.1 * k, sub="Koinobori")
    # the wind sock streamer (fukinagashi): a long banner cloth at the top
    C.mover(P + "Banner_1_Cloth", x + dx * 10.0, y + dy * 10.0, top - 5.0, yaw=wind_yaw, scale=(0.5, 0.6, 0.45),
            sway=8.0, sway_hz=0.5, sub="Koinobori")


def hokora(x, y, z, yaw, kind="wood", light=False, zfun=None):
    """Small wayside shrine on a stone plinth, a mini torii in front, offerings, two tiny lamps.
    yaw = the direction the shrine faces (its front)."""
    zf = zfun or (lambda px, py: C.ground(px, py))
    base = z
    C.place(V + "Stairs_Exterior_Platform", x, y, base - 30.0, yaw=yaw, scale=(0.72, 0.72, 0.72), sub="Hokora",
            cull=12000.0)
    top = base - 30.0 + 72.0
    if kind == "wood":
        C.place(JP + "Noren_Stall", x, y, top, yaw=yaw + 90.0, scale=0.42, sub="Hokora", cull=12000.0)
    else:
        C.place(JP + "Pagoda", x, y, top, yaw=yaw + 90.0, scale=0.17, sub="Hokora", cull=12000.0)
    fx, fy = fwd(yaw)
    # offerings on the plinth lip
    ox, oy = x + fx * 55.0, y + fy * 55.0
    for (lx, ly, path, sc) in ((0, -18, KITCHEN + "Kitchen_Cup", 1.4), (0, 18, KITCHEN + "Kitchen_Bowl", 1.2),
                               (8, 0, P + "Coin_Pile", 1.0), (-4, -34, P + "Candle_2", 1.0), (-4, 34, P + "Candle_2", 1.0)):
        px, py = local(ox, oy, yaw, lx, ly)
        B.add(path, px, py, top, yaw=R.uniform(0, 360), scale=sc, cull=4500.0)
    # a sake bottle and a vase of flowers
    px, py = local(x, y, yaw, 60.0, -48.0)
    B.add(P + "Bottle_1", px, py, top, scale=1.2, cull=4500.0)
    # mini torii in front
    tx, ty = x + fx * 190.0, y + fy * 190.0
    torii(tx, ty, zf(tx, ty) - 4.0, yaw - 90.0, 0.16, sub="Hokora")
    # little lamps either side
    for side in (-1, 1):
        px, py = local(x, y, yaw, 120.0, side * 95.0)
        B.add(JP + "GardenLamp", px, py, zf(px, py) + 10.0, scale=0.8, collide=False, cull=7000.0)
    # pots to smash
    px, py = local(x, y, yaw, 40.0, 95.0)
    C.breakable(P + "Vase_4", px, py, zf(px, py) + 3.0, yaw=R.uniform(0, 360), sub="Hokora")
    if light:
        C.light(x + fx * 80.0, y + fy * 80.0, top + 60.0, intensity=5.0, radius=450.0, color=(255, 160, 90))
    C.claim(x, y, 150.0)


def ema_board(x, y, z, yaw, zfun):
    """Wooden votive board on two posts, a small tiled ridge, rows of colourful wish plaques on both faces.
    yaw = direction the front face looks."""
    for side in (-1, 1):
        px, py = local(x, y, yaw, 0.0, side * 85.0)
        C.place(V + "Corner_Exterior_Wood", px, py, zfun(px, py) - 5.0, yaw=yaw, scale=(1.0, 1.0, 0.62), sub="Ema",
                cull=9000.0)
    # the board: a wood floor tile stood up (normal along the yaw direction)
    C.place(V + "Floor_WoodDark", x, y, z + 125.0, yaw=yaw + 90.0, roll=90.0, scale=(0.82, 0.48, 1.0), sub="Ema",
            cull=9000.0)
    C.place(V + "Roof_Modular_RoundTiles", x, y, z + 178.0, yaw=yaw, scale=(0.8, 0.95, 0.7), sub="Ema", cull=9000.0)
    fx, fy = fwd(yaw)
    cols = [P + "Book_Simplified_Single"]
    for face in (1, -1):
        for row, zz in enumerate((150.0, 122.0, 94.0)):
            for i in range(6):
                lat = -62.0 + i * 24.8 + (6.0 if row == 1 else 0.0)
                px, py = local(x, y, yaw, face * 5.0, lat)
                B.add(cols[0], px, py, z + zz - 12.0, yaw=yaw + (0.0 if face > 0 else 180.0) + R.uniform(-8, 8),
                      scale=(1.0, 0.95, 0.85), roll=R.uniform(-6, 6), cull=4000.0)
    C.claim(x, y, 110.0)


def omikuji_rack(x, y, z, yaw, zfun):
    """Fortune rack: two posts and three cords with little white paper slips tied on."""
    for side in (-1, 1):
        px, py = local(x, y, yaw, 0.0, side * 90.0)
        C.place(V + "Corner_Exterior_Wood", px, py, zfun(px, py) - 5.0, yaw=yaw, scale=(0.9, 0.9, 0.55), sub="Ema",
                cull=9000.0)
    for zz in (80.0, 115.0, 150.0):
        a = local(x, y, yaw, 0.0, -90.0) + (z + zz,)
        b = local(x, y, yaw, 0.0, 90.0) + (z + zz,)
        rod(INTERIOR + "SM_KG_RailPost", a, b, 0.12, 108.0, cull=5000.0)
        for i in range(9):
            lat = -76.0 + i * 19.0
            px, py = local(x, y, yaw, R.uniform(-2, 2), lat)
            B.add(P + "Scroll_1", px, py, z + zz - 14.0, yaw=yaw + 90.0 + R.uniform(-10, 10), pitch=90.0,
                  scale=(1.2, 1.3, 1.3), cull=4000.0)
    C.claim(x, y, 110.0)


def offering_box(x, y, z, yaw):
    """Saisen-bako: slatted wooden box with coins on top, a bell rope hanging above it."""
    C.place(P + "Crate_Wooden", x, y, z + 3.0, yaw=yaw, scale=(1.35, 0.72, 0.66), sub="Shrine", cull=9000.0)
    top = z + 3.0 + 93.0 * 0.66 - 4.0
    for (lx, ly) in ((-25, 5), (15, -8), (30, 12)):
        px, py = local(x, y, yaw, lx, ly)
        B.add(P + "Coin_Pile_2", px, py, top, yaw=R.uniform(0, 360), cull=4000.0)
    for (lx, ly) in ((-8, 20), (6, -20)):
        px, py = local(x, y, yaw, lx, ly)
        B.add(P + "Coin", px, py, top + 1.0, yaw=R.uniform(0, 360), pitch=90.0, cull=3000.0)
    C.claim(x, y, 80.0)


def tea_set(x, y, z, yaw):
    """Low table with teapot, cups and a bowl."""
    C.place(KITCHEN + "Kitchen_Square_Table", x, y, z, yaw=yaw, scale=(1.0, 1.0, 0.55), sub="Tea", cull=7000.0)
    top = z + 67.0 * 0.55
    B.add(KITCHEN + "Kitchen_SmallPotLid", x + 5.0, y - 5.0, top, yaw=yaw + 30.0, scale=0.8, cull=4000.0)
    for (lx, ly) in ((-24, 22), (24, 22), (-24, -24), (26, -20)):
        px, py = local(x, y, yaw, lx, ly)
        B.add(KITCHEN + "Kitchen_Cup", px, py, top, yaw=R.uniform(0, 360), scale=1.3, cull=3500.0)
    px, py = local(x, y, yaw, 0.0, 30.0)
    B.add(KITCHEN + "Kitchen_Bowl", px, py, top, scale=1.1, cull=3500.0)
    # cushions (zabuton) around the table
    for (lx, ly) in ((0, 70), (0, -70), (70, 0), (-70, 0)):
        px, py = local(x, y, yaw, lx, ly)
        B.add(INTERIOR + "SM_KG_Rug_Round", px, py, z + 1.5, scale=(0.36, 0.36, 6.0), cull=5000.0)


def petals_under(x, y, r, n, zfun, allow=None):
    """Pink sakura petals on the ground under a tree."""
    for _ in range(n):
        a, d = R.uniform(0, 2 * math.pi), r * math.sqrt(R.uniform(0.02, 1.0))
        px, py = x + math.cos(a) * d, y + math.sin(a) * d
        if allow and not allow(px, py):
            continue
        B.add(N + f"Petal_{R.randint(1, 5)}", px, py, zfun(px, py) - 1.0, yaw=R.uniform(0, 360),
              scale=R.uniform(0.8, 1.4), cull=5000.0)


# ============================================================================================ garden
def gz(x, y):
    return C.ground(x, y)


def garden_ok(x, y, r, lane_gap=40.0):
    return C.free(x, y, r, lane_gap=lane_gap)


def garden():
    torii_tunnel_garden()
    tea_pavilion()
    zen_garden()
    bamboo_grove()
    pond_edges()
    yatai_stall()
    garden_lantern_strings()
    koinobori(2960.0, 1150.0, gz(2960.0, 1150.0), wind_yaw=40.0)
    koinobori(3900.0, 2350.0, gz(3900.0, 2350.0), wind_yaw=40.0, height=680.0)
    floating_lanterns()
    hanami_picnic()
    pergola()
    lane_side()
    lantern_corner()
    for (x, y, yaw) in ((2385.0, 640.0, 0.0), (2935.0, 640.0, 180.0)):
        nobori(x, y, gz(x, y), yaw)
    garden_planting()
    C.ambient("A_Birds_Loop", GX - 700.0, GY + 900.0, gz(GX - 700.0, GY + 900.0) + 300.0, 0.6)


def torii_tunnel_garden():
    """Senbon torii: a tight row of vermilion gates leading from the east market between the houses into the garden."""
    x0 = 2660.0
    ys = [575.0, 710.0, 845.0, 980.0]
    for y in ys:
        z = min(gz(x0 - 200.0, y), gz(x0 + 200.0, y)) - 6.0
        torii(x0, y, z, 0.0, 0.42, sub="Garden/Torii")
    C.claim(x0, 780.0, 60.0)
    # stone lanterns flanking the tunnel mouth
    for x in (2395.0, 2925.0):
        C.place(JP + "ToroLantern", x, 520.0, gz(x, 520.0) - 4.0, yaw=0.0, scale=0.7, sub="Garden/Torii", cull=12000.0)
    C.light(x0, 780.0, gz(x0, 780.0) + 330.0, intensity=6.0, radius=650.0, color=(255, 120, 70), sub="Lights")
    # stepping stones through the tunnel and on to the old path
    pts = [(x0 + R.uniform(-15, 15), y) for y in (540.0, 630.0, 720.0, 810.0, 900.0, 990.0)]
    pts += [(2650.0, 1080.0), (2630.0, 1170.0), (2615.0, 1250.0)]
    for (x, y) in pts:
        B.add(JP + "SteppingStone", x, y, gz(x, y) - 3.0, yaw=R.uniform(0, 360), scale=R.uniform(1.0, 1.25), cull=7000.0)
    # gravel edging along the tunnel
    for y in range(520, 1060, 45):
        for side in (-1, 1):
            x = x0 + side * R.uniform(150.0, 175.0)
            B.add(N + f"Pebble_Round_{R.randint(1, 5)}", x, y, gz(x, y) - 2.0, yaw=R.uniform(0, 360),
                  scale=R.uniform(0.6, 1.0), cull=4000.0)
    # garden lamps lining the old stepping-stone path
    for (x, y) in ((2330.0, 1170.0), (2530.0, 1100.0), (2470.0, 1330.0), (2680.0, 1250.0), (2610.0, 1480.0),
                   (2830.0, 1400.0), (2760.0, 1620.0), (2930.0, 1560.0)):
        B.add(JP + "GardenLamp", x, y, gz(x, y) + 12.0, scale=0.9, collide=True, cull=8000.0)
        C.claim(x, y, 30.0)


PAVILION_TOP = []


def tea_pavilion():
    """Open tea pavilion (azumaya) on a stone plinth: tatami, low tea table, cushions, railings, hanging lanterns."""
    cx, cy = 2080.0, 1195.0
    half = 200.0
    samples = [gz(cx + dx, cy + dy) for dx in (-half, 0, half) for dy in (-half, 0, half)]
    top = max(samples) + 30.0
    PAVILION_TOP[:] = [top]
    C.clear_grass(cx, cy, 330.0)
    for dx in (-100.0, 100.0):
        for dy in (-100.0, 100.0):
            C.place(V + "Stairs_Exterior_Platform", cx + dx, cy + dy, top - 101.0, sub="Garden/Tea", cull=15000.0)
            C.place(V + "Floor_WoodDark", cx + dx, cy + dy, top + 1.0, sub="Garden/Tea", cull=15000.0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            C.place(V + "Corner_Exterior_Wood", cx + sx * 188.0, cy + sy * 188.0, top, sub="Garden/Tea", cull=15000.0)
    C.place(V + "Roof_RoundTiles_4x4", cx, cy, top + 300.0 + 40.0, yaw=0.0, sub="Garden/Tea")
    # railings on the back (west) and south sides
    for i in range(4):
        C.place(INTERIOR + "SM_KG_Railing_1m", cx - 195.0, cy - 150.0 + i * 100.0, top, yaw=90.0, scale=(1.0, 1.0, 0.6),
                sub="Garden/Tea", cull=9000.0)
        if i in (0, 3):
            C.place(INTERIOR + "SM_KG_Railing_1m", cx - 150.0 + i * 100.0, cy - 195.0, top, yaw=0.0,
                    scale=(1.0, 1.0, 0.6), sub="Garden/Tea", cull=9000.0)
    # tatami + tea set
    C.place(INTERIOR + "SM_KG_Rug_Rect", cx + 10.0, cy + 10.0, top + 2.5, yaw=90.0, sub="Garden/Tea", collide=False,
            cull=7000.0)
    tea_set(cx + 10.0, cy + 10.0, top + 3.0, 15.0)
    # engawa benches looking out over the pond (sit!)
    seat_bench(cx + 150.0, cy + 60.0, top + 2.0, 0.0, sub="Garden/Tea")
    # stone step up on the north side
    B.add(N + "RockPath_Square_Wide", cx + 60.0, cy + half + 60.0, top - 32.0, yaw=5.0, scale=(0.8, 0.5, 1.0),
          collide=True, cull=8000.0)
    # lanterns hanging from the four eaves
    for sx in (-1, 1):
        for sy in (-1, 1):
            paper_lantern(cx + sx * 245.0, cy + sy * 245.0, top + 265.0, size=1.1)
    C.light(cx, cy, top + 230.0, intensity=5.0, radius=550.0, color=(255, 170, 100))
    # tsukubai (stone wash basin) + ladle by the step
    bx, by = cx + 230.0, cy + 250.0
    C.place(V + "Stairs_Exterior_Platform", bx, by, gz(bx, by) - 20.0, yaw=40.0, scale=(0.45, 0.45, 0.45),
            sub="Garden/Tea", cull=8000.0)
    C.place(P + "Vase_2", bx, by, gz(bx, by) + 24.0, scale=(0.9, 0.9, 0.55), sub="Garden/Tea", cull=7000.0)
    B.add(KITCHEN + "Kitchen_Laddle", bx + 12.0, by, gz(bx, by) + 53.0, yaw=30.0, scale=1.6, cull=3500.0)
    for k in range(5):
        a = k * 1.3
        B.add(N + f"Pebble_Round_{k + 1}", bx + math.cos(a) * 70.0, by + math.sin(a) * 70.0, gz(bx, by) - 2.0,
              yaw=k * 70.0, scale=1.3, cull=4000.0)
    # sake barrels + firewood behind (smashable)
    for (lx, ly) in ((-265.0, -120.0), (-270.0, -40.0)):
        C.breakable(P + "Barrel", cx + lx, cy + ly, gz(cx + lx, cy + ly) + 2.0, yaw=R.uniform(0, 360), sub="Garden/Tea")
    C.place(INTERIOR + "SM_KG_Firewood", cx - 260.0, cy + 70.0, gz(cx - 260.0, cy + 70.0), yaw=90.0, sub="Garden/Tea",
            cull=7000.0)
    C.claim(cx, cy, 300.0)


def zen_garden():
    """Karesansui: a pale raked-gravel bed with stone kerbs, three rock islands (one is the old bonsai pine) ringed by
    raked circles, straight raked lines elsewhere, moss at the rock feet, a viewing bench."""
    x0, x1, y0, y1 = 1990.0, 2640.0, 2610.0, 3010.0
    cx, cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    zs = [gz(x, y) for x in range(int(x0), int(x1) + 1, 50) for y in range(int(y0), int(y1) + 1, 50)]
    top = max(zs) + 3.0
    C.clear_grass(cx, cy, 420.0)
    bed = C.place(CUBE, cx, cy, top - 30.0, scale=((x1 - x0) / 100.0, (y1 - y0) / 100.0, 0.6), sub="Garden/Zen",
                  collide=True, cull=0.0)
    set_mat(bed, None)
    # stone kerbs centred on the bed edges (the kerb mesh extends +Y 70 cm from its pivot line)
    for (edge, yaw) in (("S", 180.0), ("N", 0.0), ("W", 90.0), ("E", -90.0)):
        if edge in "SN":
            a0, a1 = x0 - 35.0, x1 + 35.0
        else:
            a0, a1 = y0 - 35.0, y1 + 35.0
        L = a1 - a0
        n = int(math.ceil(L / 200.0))
        sc = L / (200.0 * n)
        for i in range(n):
            t = a0 + (i + 0.5) * L / n
            px, py = {"S": (t, y0 + 35.0), "N": (t, y1 - 35.0), "W": (x0 + 35.0, t), "E": (x1 - 35.0, t)}[edge]
            B.add(V + "Prop_ExteriorBorder_Straight1", px, py, top - 26.0, yaw=yaw, scale=(sc, 1.0, 2.6),
                  collide=True, cull=9000.0)
    # rock islands (x, y, ring radius); the bonsai pine at (2600, 2900) is the third
    islands = [(2150.0, 2770.0, 95.0), (2380.0, 2900.0, 70.0), (2590.0, 2890.0, 80.0)]
    B.add(JP + "GardenRock", 2150.0, 2770.0, top - 35.0, yaw=20.0, scale=1.1, collide=True, cull=12000.0)
    B.add(JP + "GardenRock", 2105.0, 2730.0, top - 30.0, yaw=150.0, scale=0.6, collide=True, cull=9000.0)
    B.add(N + "Rock_Medium_2", 2195.0, 2800.0, top - 25.0, yaw=70.0, scale=0.28, collide=True, cull=9000.0)
    B.add(JP + "GardenRock", 2380.0, 2900.0, top - 30.0, yaw=260.0, scale=0.8, collide=True, cull=12000.0)
    B.add(N + "Rock_Medium_1", 2350.0, 2930.0, top - 20.0, yaw=10.0, scale=0.2, collide=True, cull=9000.0)
    for (ix, iy, rr) in islands:
        for k in range(10):
            a = k / 10.0 * 2 * math.pi + R.uniform(-0.2, 0.2)
            d = rr * R.uniform(0.35, 0.7)
            B.add(N + "Plant_7", ix + math.cos(a) * d, iy + math.sin(a) * d, top - 3.0, yaw=R.uniform(0, 360),
                  scale=R.uniform(0.35, 0.55), cull=5000.0)
    # raked rings around the islands
    ring_step = 22.0
    for (ix, iy, rr) in islands:
        for k in range(4):
            rad = rr + 18.0 + k * ring_step
            segs = max(10, int(2 * math.pi * rad / 26.0))
            for s in range(segs):
                a0, a1 = s / segs * 2 * math.pi, (s + 1) / segs * 2 * math.pi
                p0 = (ix + math.cos(a0) * rad, iy + math.sin(a0) * rad)
                p1 = (ix + math.cos(a1) * rad, iy + math.sin(a1) * rad)
                if not (x0 + 8 < p0[0] < x1 - 8 and y0 + 8 < p0[1] < y1 - 8 and x0 + 8 < p1[0] < x1 - 8 and y0 + 8 < p1[1] < y1 - 8):
                    continue
                line((p0[0], p0[1], top - 1.0), (p1[0], p1[1], top - 1.0))

    def in_ring(x, y):
        return any(math.hypot(x - ix, y - iy) < rr + 18.0 + 3 * ring_step + 12.0 for (ix, iy, rr) in islands)

    # straight raked lines along X
    y = y0 + 16.0
    while y < y1 - 10.0:
        run = None
        x = x0 + 10.0
        while x <= x1 - 10.0:
            inside = not in_ring(x, y)
            if inside and run is None:
                run = x
            if (not inside or x + 12.0 > x1 - 10.0) and run is not None:
                end = x if not inside else x1 - 10.0
                if end - run > 20.0:
                    line((run, y, top - 1.0), (end, y, top - 1.0))
                run = None
            x += 12.0
        y += ring_step
    # viewing bench on the south side, a rake leaning on the kerb
    seat_bench(2470.0, 2535.0, gz(2470.0, 2535.0), 90.0, sub="Garden/Zen")
    C.place(WP + "SM_KG_Shovel", x1 + 30.0, 2700.0, top + 5.0, yaw=95.0, pitch=0.0, sub="Garden/Zen", collide=False,
            cull=5000.0)
    C.claim(cx, cy, 330.0)


def bamboo_grove():
    """Dense bamboo in the north-west corner with a stepping-stone path to a hidden hokora and chest."""
    hokora(1790.0, 3120.0, gz(1790.0, 3120.0), -90.0, kind="wood", light=True)
    C.loot_chest(1680.0, 3200.0, gz(1680.0, 3200.0), yaw=-60.0, table="Chest", name="Shrine Offerings")
    C.claim(1680.0, 3200.0, 70.0)
    spots = [(1560.0, 2560.0), (1520.0, 2870.0), (1620.0, 3330.0), (1980.0, 3330.0), (2000.0, 3180.0),
             (2180.0, 3300.0), (1560.0, 3060.0), (1900.0, 2620.0), (1540.0, 2740.0), (2200.0, 3120.0),
             (1640.0, 2960.0), (1940.0, 2850.0)]
    placed = 0
    for (x, y) in spots:
        if not C.in_zone(x, y) or C.building_hit(x, y, 150.0) or C.lane_distance(x, y) < 180.0 or C.overlaps(x, y, 90.0):
            continue
        B.add(JP + "Bamboo", x, y, gz(x, y) - 10.0, yaw=R.uniform(0, 360), scale=R.uniform(0.8, 1.2), collide=True,
              cull=0.0)
        C.claim(x, y, 140.0)
        placed += 1
    # grove floor: ferns, fallen leaves (petals), mushrooms
    for _ in range(60):
        x, y = R.uniform(1400, 2250), R.uniform(2400, 3400)
        if not C.in_zone(x, y) or C.building_hit(x, y, 60.0) or C.lane_distance(x, y) < 60.0:
            continue
        if 1950 < x < 2680 and 2560 < y < 3050 or C.overlaps(x, y, 35.0):
            continue
        B.add(R.choice([N + "Fern_1", N + "Plant_1", N + "Mushroom_Common", N + "Clover_1"]), x, y, gz(x, y) - 2.0,
              yaw=R.uniform(0, 360), scale=R.uniform(0.5, 0.9), cull=5000.0)
    # stepping stones from the lamp at (1950, 2420) into the grove
    path = [(1900.0, 2500.0), (1850.0, 2590.0), (1790.0, 2680.0), (1760.0, 2780.0), (1750.0, 2880.0), (1770.0, 2980.0)]
    for (x, y) in path:
        B.add(JP + "SteppingStone", x, y, gz(x, y) - 3.0, yaw=R.uniform(0, 360), scale=1.1, cull=6000.0)
    # a small torii at the grove mouth
    torii(1905.0, 2470.0, gz(1905.0, 2470.0) - 5.0, -30.0, 0.24, sub="Garden/Grove")
    C.ambient("A_Wind_Loop", 1700.0, 2900.0, gz(1700.0, 2900.0) + 250.0, 0.35)


def pond_edges():
    """Irises and ferns along the pond rim, rim rocks, a stone path loop, benches facing the water."""
    cx, cy = 2640.0, 2010.0
    # flowers hugging the rim (the rim is ~800 x 500)
    for i in range(34):
        a = i / 34.0 * 2 * math.pi + R.uniform(-0.05, 0.05)
        x, y = cx + math.cos(a) * 455.0, cy + math.sin(a) * 300.0
        if abs(y - 2000.0) < 140.0 and (x < 2350.0 or x > 2930.0):   # bridge landings stay clear
            continue
        B.add(R.choice([N + "Flower_4_Single", N + "Flower_3_Single", N + "Fern_1", N + "Flower_4_Group",
                        N + "Plant_1"]), x, y, gz(x, y) - 2.0, yaw=R.uniform(0, 360), scale=R.uniform(0.45, 0.75),
              cull=6000.0)
    # a loop of flat path stones around the pond
    for i in range(40):
        a = i / 40.0 * 2 * math.pi
        x, y = cx + math.cos(a) * 590.0, cy + math.sin(a) * 420.0
        if C.lane_distance(x, y) < 30.0 or C.overlaps(x, y, 40.0):
            continue
        B.add(N + f"RockPath_Round_Small_{R.randint(1, 3)}", x, y, gz(x, y) - 4.0, yaw=math.degrees(a) + 90.0,
              scale=0.85, cull=6000.0)
    # benches facing the water
    seat_bench(2130.0, 2150.0, gz(2130.0, 2150.0), -10.0, sub="Garden/Seats")
    seat_bench(2470.0, 1620.0, gz(2470.0, 1620.0), 80.0, sub="Garden/Seats")
    C.claim(2130.0, 2150.0, 150.0)
    C.claim(2470.0, 1620.0, 150.0)
    # rim boulders and a heron-less fishing kid's bucket
    for (x, y, sc) in ((2230.0, 2240.0, 0.5), (3040.0, 1790.0, 0.45), (2280.0, 1760.0, 0.4), (3010.0, 2230.0, 0.55)):
        B.add(JP + "GardenRock", x, y, gz(x, y) - 12.0, yaw=R.uniform(0, 360), scale=sc, collide=True, cull=8000.0)
    B.add(P + "Bucket_Wooden_1", 2560.0, 1690.0, gz(2560.0, 1690.0), yaw=30.0, cull=4500.0)
    B.add(WP + "SM_KG_FishingRod", 2540.0, 1705.0, gz(2540.0, 1705.0) + 4.0, yaw=75.0, cull=4500.0)
    B.add(KITCHEN + "Kitchen_Bowl", 2590.0, 1700.0, gz(2590.0, 1700.0), cull=3500.0)


def yatai_stall():
    """A little food stall by the lane (east side): grilled fish, stools, sake barrels, lantern string."""
    x, y, yaw = 3790.0, 1720.0, 90.0          # stall front faces west (toward the lane)
    z = min(gz(x - 100.0, y), gz(x + 100.0, y), gz(x, y - 80.0), gz(x, y + 80.0)) - 3.0
    C.clear_grass(x - 80.0, y, 260.0)
    C.place(JP + "Noren_Stall", x, y, z, yaw=yaw + 180.0, sub="Garden/Yatai", cull=15000.0)
    # counter goods: fish on plates, bowls, a pot
    top = z + 100.0
    for i, fish in enumerate((WP + "SM_KG_Fish_Mackerel", WP + "SM_KG_Fish_Salmon", WP + "SM_KG_Fish_Mackerel",
                              WP + "SM_KG_Fish_Cod")):
        B.add(fish, x - 70.0, y - 75.0 + i * 45.0, top, yaw=R.uniform(-20, 20), cull=4000.0)
    B.add(P + "Cauldron", x + 20.0, y + 150.0, gz(x + 20.0, y + 150.0), scale=0.6, collide=True, cull=7000.0)
    B.add(INTERIOR + "SM_KG_Firewood", x + 25.0, y + 225.0, gz(x + 25.0, y + 225.0), yaw=40.0, scale=0.8, cull=6000.0)
    # stools in front (sit, facing the counter = east)
    for i in range(3):
        sx, sy = x - 175.0, y - 70.0 + i * 70.0
        C.seat(P + "Stool", sx, sy, gz(sx, sy), yaw=0.0, sub="Garden/Yatai")
    # sake barrels / crates to smash
    for (lx, ly, path) in ((30.0, -150.0, P + "Barrel"), (-40.0, -165.0, P + "Barrel_Apples"),
                           (40.0, -230.0, P + "Crate_Wooden")):
        C.breakable(path, x + lx, y + ly, gz(x + lx, y + ly) + 2.0, yaw=R.uniform(0, 360), sub="Garden/Yatai")
    C.light(x - 120.0, y, z + 230.0, intensity=5.0, radius=500.0, color=(255, 150, 80))
    C.claim(x, y, 250.0)


def garden_lantern_strings():
    """Matsuri lantern strings: two crossing the fish alley, one over the stepping path, one from the tea pavilion."""
    tests = LANTERN_MATS
    s = [((3040.0, 1760.0), (3520.0, 1880.0)), ((3000.0, 2300.0), (3470.0, 2480.0)),
         ((2400.0, 1260.0), (2880.0, 1470.0))]
    for k, ((ax, ay), (bx, by)) in enumerate(s):
        za, zb = gz(ax, ay) + 330.0, gz(bx, by) + 330.0
        lantern_string((ax, ay, za), (bx, by, zb), 5, sag=40.0)
    # from the tea pavilion eave to a post by the pond
    t = PAVILION_TOP[0] if PAVILION_TOP else gz(2080.0, 1195.0) + 30.0
    lantern_string((2300.0, 1415.0, t + 285.0), (2690.0, 1700.0, gz(2690.0, 1700.0) + 320.0), 4, sag=35.0,
                   posts=(False, True))
    C.light(3270.0, 2090.0, gz(3270.0, 2090.0) + 280.0, intensity=4.0, radius=600.0, color=(255, 140, 90))


def garden_planting():
    """Extra trees, petals under the sakura, flower clumps, moss and pebbles in the gaps."""
    for (name, x, y, sc) in (("Maple", 3080.0, 1180.0, 0.5), ("Sakura_B", 1880.0, 1750.0, 0.55),
                             ("Maple", 2200.0, 3290.0, 0.55), ("Sakura_A", 3950.0, 1450.0, 0.6),
                             ("Maple", 3820.0, 2050.0, 0.45)):
        if C.building_hit(x, y, 120.0) or C.lane_distance(x, y) < 140.0:
            continue
        C.place(JP + name, x, y, gz(x, y) - 10.0, yaw=R.uniform(0, 360), scale=sc, sub="Garden/Trees")
        C.claim(x, y, 120.0)
    for (x, y, r) in ((1700.0, 1300.0, 300.0), (3550.0, 1250.0, 320.0), (1880.0, 1750.0, 200.0),
                      (3950.0, 1450.0, 220.0)):
        petals_under(x, y, r, 35, gz, allow=lambda px, py: C.in_zone(px, py) and not C.building_hit(px, py, 20.0))
    # flower beds along the house backs and lane edge
    for _ in range(90):
        a, d = R.uniform(0, 2 * math.pi), 1500.0 * math.sqrt(R.uniform(0.1, 1.0))
        x, y = GX + math.cos(a) * d, GY + math.sin(a) * d
        if not C.free(x, y, 45.0, lane_gap=25.0):
            continue
        C.claim(x, y, 45.0)
        B.add(R.choice([N + "Flower_3_Group", N + "Flower_4_Group", N + "Bush_Common_Flowers", N + "Fern_1",
                        N + "Plant_1", N + "Flower_4_Single", N + "Clover_2"]), x, y, gz(x, y) - 3.0,
              yaw=R.uniform(0, 360), scale=R.uniform(0.45, 0.8), cull=6500.0)


# ============================================================================================ island
def island():
    if _island_comp() is None:
        unreal.log_warning("KG_DRESS japan: ShrineIsland not found, island skipped")
        return
    jetty()
    shrine_path()
    shrine_plaza()
    behind_pagoda()
    island_groves()
    bell_pavilion(5560.0, 15280.0, 90.0, iz)
    sake_wall(3860.0, 14230.0, iz_max(3860.0, 14230.0, 120.0), 10.0, iz)
    beach_camp()
    tea_stall_island()
    sea_shrine()
    driftwood()
    pagoda_ring_path()
    meditation_rock()
    island_shore()
    island_extras()


PATH_A = (4210.0, 12880.0)       # foot of the steps (jetty end on land)
PATH_B = (4480.0, 14180.0)       # top of the steps (plaza edge)


def jetty():
    """Wooden jetty on the torii axis, a moored bobbing rowboat, fishing gear, lanterns."""
    x = 4210.0
    deck = 70.0
    for yc in (12860.0, 12460.0, 12060.0):
        C.place(JP + "WoodWalk", x, yc, deck, yaw=90.0, sub="Island/Jetty", cull=0.0)
    C.mover(WP + "SM_KG_Rowboat", x + 190.0, 12250.0, 16.0, yaw=95.0, bob=6.0, sway=2.5, sway_hz=0.25, sub="Island/Jetty")
    B.add(WP + "SM_KG_Oar", x + 150.0, 12300.0, 30.0, yaw=100.0, cull=5000.0)
    # mooring posts
    for (px, py) in ((x + 85.0, 12150.0), (x - 85.0, 12150.0), (x + 85.0, 12550.0), (x - 85.0, 12550.0)):
        C.place(INTERIOR + "SM_KG_RailPost", px, py, deck - 40.0, scale=(1.6, 1.6, 1.0), sub="Island/Jetty",
                cull=8000.0)
    B.add(P + "Rope_2", x + 60.0, 12180.0, deck + 1.0, yaw=30.0, scale=0.7, cull=4000.0)
    # the fisherman's corner on the deck
    s = C.seat(P + "Stool", x - 35.0, 12000.0, deck, yaw=-90.0, sub="Island/Jetty")
    C.place(WP + "SM_KG_FishingRod", x - 40.0, 11990.0, deck + 45.0, yaw=-95.0, pitch=25.0, sub="Island/Jetty",
            collide=False, cull=6000.0)
    B.add(P + "Bucket_Wooden_1", x + 30.0, 12020.0, deck, cull=4000.0)
    B.add(WP + "SM_KG_Fish_Mackerel", x + 30.0, 12060.0, deck + 2.0, yaw=40.0, cull=3500.0)
    B.add(WP + "SM_KG_Fish_Cod", x + 45.0, 12080.0, deck + 2.0, yaw=-20.0, cull=3500.0)
    C.breakable(PIR + "crates_0", x + 30.0, 12640.0, deck + 1.0, yaw=15.0, sub="Island/Jetty")
    C.breakable(P + "Barrel", x - 40.0, 12700.0, deck + 1.0, yaw=0.0, sub="Island/Jetty")
    # lanterns on posts at the jetty end
    for side in (-1, 1):
        B.add(JP + "GardenLamp", x + side * 60.0, 11890.0, deck, scale=1.0, cull=9000.0)
    C.light(x, 12000.0, deck + 180.0, intensity=5.0, radius=600.0, color=(255, 160, 90))
    C.ambient("A_Lapping_Loop", x, 12300.0, 80.0, 0.7)
    C.claim(x, 12460.0, 120.0)


def _path_point(t):
    ax, ay = PATH_A
    bx, by = PATH_B
    return ax + (bx - ax) * t, ay + (by - ay) * t


def shrine_path():
    """Stone steps from the beach up to the plaza, framed by a senbon-torii tunnel and lamp posts."""
    ax, ay = PATH_A
    bx, by = PATH_B
    L = math.hypot(bx - ax, by - ay)
    h = math.degrees(math.atan2(by - ay, bx - ax))
    # stone steps every ~70 cm
    n = int(L / 64.0)
    for i in range(n + 1):
        x, y = _path_point(i / n)
        z = iz(x, y)
        if z < 30.0:
            continue
        px, py = local(x, y, h, -35.0, 0.0)      # kerb pivot sits on its front edge, depth runs uphill
        B.add(V + "Prop_ExteriorBorder_Straight1", px, py, z - 24.0, yaw=h - 90.0, scale=(1.0, 1.0, 2.6),
              collide=True, cull=12000.0)
        C.claim(x, y, 90.0)
        # pebble edging
        for side in (-1, 1):
            px, py = local(x, y, h, R.uniform(-20, 20), side * R.uniform(112.0, 130.0))
            B.add(N + f"Pebble_Round_{R.randint(1, 5)}", px, py, iz(px, py) - 2.0, yaw=R.uniform(0, 360),
                  scale=R.uniform(0.7, 1.1), cull=5000.0)
    # nobori banners lining the lower steps
    for i in range(4):
        t = 0.06 + i * 0.07
        x, y = _path_point(t)
        side = -1 if i % 2 else 1
        px, py = local(x, y, h, 0.0, side * 175.0)
        nobori(px, py, iz(px, py), h + 90.0 * side)
    # the tunnel: 9 torii over the upper two thirds
    k = 9
    for i in range(k):
        t = 0.28 + i * (0.68 / (k - 1))
        x, y = _path_point(t)
        lx, ly = local(x, y, h, 0.0, 205.0)
        rx, ry = local(x, y, h, 0.0, -205.0)
        z = min(iz(lx, ly), iz(rx, ry)) - 8.0
        torii(x, y, z, h - 90.0, 0.45, sub="Island/Torii")
    # lamps between the pillars, outside the tunnel
    for i in range(6):
        t = 0.1 + i * 0.16
        x, y = _path_point(t)
        for side in (-1, 1):
            px, py = local(x, y, h, 0.0, side * 290.0)
            z = iz(px, py)
            if z > 30.0:
                B.add(JP + "GardenLamp", px, py, z + 8.0, scale=1.1, collide=True, cull=9000.0)
    # big stone lanterns at the foot of the steps
    for side in (-1, 1):
        px, py = local(ax, ay, h, 80.0, side * 330.0)
        C.place(JP + "ToroLantern", px, py, iz_min(px, py, 60.0) - 5.0, yaw=h - 90.0, sub="Island/Path", cull=15000.0)
    mx, my = _path_point(0.62)
    C.light(mx, my, iz(mx, my) + 330.0, intensity=6.0, radius=800.0, color=(255, 120, 70))


def shrine_plaza():
    """Paved forecourt of the pagoda: offering box, ema board, omikuji rack, incense burner, shrine shop, lantern strings."""
    # sando: a paved approach from the top of the steps to the offering box, gravel either side
    ax, ay = PATH_B
    bx, by = 4500.0, 14540.0
    L = math.hypot(bx - ax, by - ay)
    h = math.degrees(math.atan2(by - ay, bx - ax))
    n = max(2, int(round(L / 68.0)))
    for i in range(n):
        x, y = ax + (bx - ax) * (i + 0.5) / n, ay + (by - ay) * (i + 0.5) / n
        z = iz_max(x, y, 60.0)
        px, py = local(x, y, h, -35.0, 0.0)
        B.add(V + "Prop_ExteriorBorder_Straight1", px, py, z - 22.0, yaw=h - 90.0, scale=(1.0, L / n / 70.0 + 0.03, 2.2),
              collide=True, cull=12000.0)
        for _ in range(6):
            side = R.choice((-1, 1))
            px, py = local(x, y, h, R.uniform(-L / n / 2, L / n / 2), side * R.uniform(115.0, 190.0))
            B.add(N + f"Pebble_Round_{R.randint(1, 5)}", px, py, iz(px, py) - 2.0, yaw=R.uniform(0, 360),
                  scale=R.uniform(0.6, 1.0), cull=4500.0)
    C.claim(4500.0, 14370.0, 150.0)
    # offering box in front of the pagoda door
    offering_box(4500.0, 14640.0, iz_max(4500.0, 14640.0, 50.0), 0.0)
    # incense burner (jokoro) in the middle of the court
    jz = iz_max(4500.0, 14420.0, 50.0)
    B.add(N + "RockPath_Square_Wide", 4500.0, 14420.0, jz - 10.0, yaw=45.0, scale=(0.45, 0.45, 1.6), collide=True,
          cull=9000.0)
    C.place(P + "Cauldron", 4500.0, 14420.0, jz + 36.0, yaw=0.0, scale=0.75, sub="Island/Shrine", cull=9000.0)
    for a in (0, 120, 240):
        px, py = 4500.0 + math.cos(math.radians(a)) * 18.0, 14420.0 + math.sin(math.radians(a)) * 18.0
        B.add(P + "Candle_2", px, py, jz + 76.0, scale=(0.5, 0.5, 1.8), cull=4000.0)
    C.light(4500.0, 14420.0, jz + 170.0, intensity=5.0, radius=650.0, color=(255, 140, 70))
    # ema + omikuji either side
    ema_board(4120.0, 14600.0, iz(4120.0, 14600.0), -60.0, iz)
    omikuji_rack(4880.0, 14600.0, iz(4880.0, 14600.0), -120.0, iz)
    # shrine shop (omamori) to the east of the court
    sx, sy = 5280.0, 14470.0
    C.place(JP + "Noren_Stall", sx, sy, iz_min(sx, sy, 110.0) - 3.0, yaw=180.0 - 25.0, sub="Island/Shrine", cull=15000.0)
    for i in range(5):
        px, py = local(sx, sy, 155.0, -60.0 + i * 30.0, -60.0)
        B.add(P + "Pouch_Large", px, py, iz(sx, sy) + 98.0, yaw=R.uniform(0, 360), scale=1.2, cull=3500.0)
    C.claim(sx, sy, 170.0)
    # lantern strings over the court (pagoda side posts to the step-top posts)
    for (a, b) in (((4150.0, 14250.0), (4860.0, 14250.0)), ((4150.0, 14250.0), (4200.0, 14760.0)),
                   ((4860.0, 14250.0), (4810.0, 14760.0))):
        lantern_string((a[0], a[1], iz(*a) + 330.0), (b[0], b[1], iz(*b) + 330.0), 5, sag=35.0, posts=(True, True),
                       zfun=iz)
    # benches facing the court under the trees
    seat_bench(3900.0, 14700.0, iz(3900.0, 14700.0), -20.0, sub="Island/Seats")
    seat_bench(5150.0, 14750.0, iz(5150.0, 14750.0), -160.0, sub="Island/Seats")


def behind_pagoda():
    """The priest's store behind the pagoda: loot chest, crates and barrels to smash, firewood, a broom."""
    x, y = 4500.0, 15430.0
    z = iz(x, y)
    C.loot_chest(x, y, z, yaw=90.0, table="Chest", name="Shrine Treasury")
    C.claim(x, y, 90.0)
    for (lx, ly, path) in ((-140.0, 20.0, PIR + "crates_1"), (-150.0, 110.0, PIR + "crates_0"), (140.0, 30.0, P + "Barrel"),
                           (210.0, 90.0, P + "Barrel_Apples")):
        C.breakable(path, x + lx, y + ly, iz(x + lx, y + ly) + 2.0, yaw=R.uniform(0, 360), sub="Island/Store")
    B.add(INTERIOR + "SM_KG_Firewood", x - 60.0, y + 150.0, iz(x - 60.0, y + 150.0), yaw=15.0, cull=6000.0)
    B.add(P + "Bag", x + 80.0, y + 140.0, iz(x + 80.0, y + 140.0), yaw=40.0, cull=5000.0)
    B.add(P + "Bag", x + 130.0, y + 170.0, iz(x + 130.0, y + 170.0), yaw=-20.0, scale=0.9, cull=5000.0)
    C.light(x, y + 60.0, z + 220.0, intensity=4.0, radius=450.0, color=(255, 160, 90))
    # little paper lanterns on the store wall line
    for i in range(3):
        paper_lantern(x - 120.0 + i * 120.0, y - 110.0, z + 250.0, size=0.9)


def island_groves():
    """Sakura groves on the west and north-east slopes, maples, bamboo on the north shore, petals everywhere."""
    trees = [("Sakura_A", 3450.0, 14350.0, 0.9), ("Sakura_B", 3250.0, 15050.0, 0.85), ("Sakura_B", 3700.0, 16650.0, 0.8),
             ("Sakura_A", 4350.0, 16900.0, 0.75), ("Sakura_B", 5300.0, 16650.0, 0.8), ("Sakura_A", 5950.0, 15550.0, 0.85),
             ("Sakura_B", 6050.0, 14350.0, 0.8), ("Maple", 5700.0, 13850.0, 0.7), ("Maple", 3350.0, 13800.0, 0.65),
             ("Sakura_A", 3900.0, 13700.0, 0.6), ("Maple", 6300.0, 15050.0, 0.6)]
    for (name, x, y, sc) in trees:
        z = iz_min(x, y, 60.0)
        if z < 60.0 or not C.in_zone(x, y):
            continue
        C.place(JP + name, x, y, z - 12.0, yaw=R.uniform(0, 360), scale=sc, sub="Island/Trees")
        C.claim(x, y, 150.0)
        if name.startswith("Sakura"):
            petals_under(x, y, 330.0 * sc, 40, iz, allow=lambda px, py: iz(px, py) > 40.0)
    for (x, y) in ((3400.0, 15600.0), (5650.0, 14600.0), (3900.0, 16300.0)):
        C.claim(x, y, 150.0)
        petals_under(x, y, 330.0, 35, iz, allow=lambda px, py: iz(px, py) > 40.0)
    # bamboo on the north shore
    for (x, y) in ((4550.0, 17150.0), (4850.0, 17050.0), (4250.0, 17100.0), (5100.0, 16900.0)):
        z = iz(x, y)
        if z > 40.0:
            B.add(JP + "Bamboo", x, y, z - 12.0, yaw=R.uniform(0, 360), scale=R.uniform(0.8, 1.1), collide=True, cull=0.0)
            C.claim(x, y, 140.0)
    # hokora on the east and west shoulders
    hokora(6000.0, 15050.0, iz_max(6000.0, 15050.0, 60.0), 180.0, kind="stone", light=False, zfun=iz)
    hokora(3150.0, 14700.0, iz_max(3150.0, 14700.0, 60.0), 20.0, kind="wood", light=False, zfun=iz)


def island_shore():
    """Rocks along the waterline, beach grass, driftwood, an upturned boat on the sand."""
    for i in range(46):
        a = i / 46.0 * 2 * math.pi + R.uniform(-0.04, 0.04)
        # walk outwards from the centre until the surface drops to the water line
        d = 1200.0
        while d < 4000.0 and iz(IX + math.cos(a) * d, IY + math.sin(a) * d) > 25.0:
            d += 60.0
        x, y = IX + math.cos(a) * d, IY + math.sin(a) * d
        if math.hypot(x - 4210.0, y - 12800.0) < 400.0:   # the jetty foot stays clear
            continue
        sc = R.uniform(0.25, 0.55)
        B.add(N + f"Rock_Medium_{R.randint(1, 3)}", x, y, iz(x, y) - 40.0 * sc, yaw=R.uniform(0, 360), scale=sc,
              collide=True, cull=15000.0)
        # beach grass a bit further up
        gx, gy = IX + math.cos(a) * (d - 220.0), IY + math.sin(a) * (d - 220.0)
        if R.random() < 0.7:
            B.add(R.choice([N + "Grass_Wispy_Tall", N + "Grass_Common_Tall", N + "Grass_Wispy_Short"]), gx, gy,
                  iz(gx, gy) - 3.0, yaw=R.uniform(0, 360), scale=R.uniform(0.7, 1.1), cull=7000.0)
    # upturned boat + oars + net floats on the beach east of the jetty
    bx, by = 4800.0, 12930.0
    C.place(WP + "SM_KG_Rowboat", bx, by, iz_min(bx, by, 150.0) + 45.0, yaw=20.0, roll=180.0, sub="Island/Beach",
            cull=12000.0)
    B.add(WP + "SM_KG_Oar", bx - 80.0, by - 120.0, iz(bx - 80.0, by - 120.0) + 3.0, yaw=60.0, cull=5000.0)
    B.add(P + "Rope_3", bx + 150.0, by - 60.0, iz(bx + 150.0, by - 60.0) + 1.0, yaw=10.0, cull=4000.0)
    koinobori(4600.0, 13050.0, iz(4600.0, 13050.0), wind_yaw=-20.0, height=720.0)


def island_extras():
    """Flowers, ferns and pebbles on the slopes, a few small lamps along the plaza ring path."""
    placed = 0
    for _ in range(600):
        if placed >= 140:
            break
        a, d = R.uniform(0, 2 * math.pi), R.uniform(700.0, 3400.0)
        x, y = IX + math.cos(a) * d, IY + math.sin(a) * d
        z = iz(x, y)
        if z < 80.0 or C.overlaps(x, y, 60.0):
            continue
        C.claim(x, y, 50.0)
        B.add(R.choice([N + "Flower_3_Group", N + "Flower_4_Group", N + "Bush_Common_Flowers", N + "Fern_1",
                        N + "Plant_1", N + "Clover_1", N + "Flower_4_Single", N + "Grass_Wispy_Short"]), x, y, z - 3.0,
              yaw=R.uniform(0, 360), scale=R.uniform(0.5, 0.9), cull=6500.0)
        placed += 1
    # small lamps ringing the pagoda
    for i in range(10):
        a = i / 10.0 * 2 * math.pi + 0.3
        x, y = IX + math.cos(a) * 620.0, IY + math.sin(a) * 620.0
        if C.overlaps(x, y, 40.0):
            continue
        B.add(JP + "GardenLamp", x, y, iz(x, y) + 8.0, scale=1.0, collide=True, cull=8000.0)


# ============================================================================================ round-2 set pieces
def nobori(x, y, z, yaw):
    """Tall shrine banner on a thin pole (the cloth hangs from a top arm and sways a little)."""
    C.place(PIR + "FlagLow_0", x, y, z - 10.0, scale=(0.4, 0.4, 420.0 / 756.0), sub="Nobori", cull=12000.0)
    fx, fy = fwd(yaw)
    C.mover(P + "Banner_2_Cloth", x + fx * 22.0, y + fy * 22.0, z + 395.0, yaw=yaw + 90.0, scale=(0.55, 1.0, 1.35),
            sway=3.0, sway_hz=0.3, sub="Nobori")


def floating_lanterns():
    """Toro-nagashi: little glowing lanterns bobbing on the koi pond (either side of the bridge)."""
    water = C.ground(GX, GY) + 55.0 - 18.0
    for (x, y) in ((2420.0, 1905.0), (2540.0, 1880.0), (2780.0, 1890.0), (2890.0, 1920.0), (2450.0, 2110.0),
                   (2640.0, 2125.0), (2840.0, 2105.0)):
        a = C.mover(CUBE, x, y, water + 7.0, yaw=R.uniform(0, 90), scale=(0.16, 0.16, 0.14), bob=2.5, sub="Garden/Pond")
        set_mat(a, LANTERN_MAT)
        try:
            a.get_component_by_class(unreal.StaticMeshComponent).set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        except Exception:
            pass
        B.add(CUBE, x, y, water + 0.5, yaw=R.uniform(0, 90), scale=(0.24, 0.24, 0.02), cull=5000.0)


def hanami_picnic():
    """Blossom-viewing picnic under the sakura across the lane: mat, food, sake, a paper umbrella."""
    x, y, yaw = 3760.0, 1150.0, 80.0
    z = gz(x, y)
    C.clear_grass(x, y, 160.0)
    C.place(INTERIOR + "SM_KG_Rug_Rect", x, y, z + 1.0, yaw=yaw, sub="Garden/Picnic", collide=False, cull=7000.0)
    goods = [(KITCHEN + "Kitchen_Plate", -30, -20, 1.0), (P + "Table_Plate", 30, 25, 1.0), (P + "Mug", -45, 30, 1.1),
             (P + "Mug", 45, -35, 1.1), (P + "Bottle_1", 5, 45, 1.0), (KITCHEN + "Kitchen_Bowl", 10, -40, 1.2)]
    for (path, lx, ly, sc) in goods:
        px, py = local(x, y, yaw, lx, ly)
        B.add(path, px, py, z + 2.0, yaw=R.uniform(0, 360), scale=sc, cull=4000.0)
    for (path, lx, ly) in ((INTERIOR + "SM_KG_Bread", -30, -20), (INTERIOR + "SM_KG_Cheese", 30, 25)):
        px, py = local(x, y, yaw, lx, ly)
        B.add(path, px, py, z + 4.0, yaw=R.uniform(0, 360), cull=3500.0)
    px, py = local(x, y, yaw, 90.0, 0.0)
    B.add(P + "Bag", px, py, z, yaw=yaw + 20.0, scale=0.7, cull=5000.0)
    px, py = local(x, y, yaw, -70.0, -60.0)
    B.add("/Game/KillGodot/Items/Melee/Closed_Umbrella_o0CUgpt8pm/StaticMeshes/SM_KG_Closed_Umbrella", px, py, z + 16.0,
          yaw=yaw + 10.0, pitch=90.0, scale=1.6, cull=5000.0)
    C.claim(x, y, 140.0)


def pergola():
    """Wisteria pergola by the west houses: posts, beams, hanging vines, lanterns and a bench looking at the pond."""
    x0, x1, y0, y1 = 1610.0, 1770.0, 2150.0, 2450.0
    z = max(gz(x0, y0), gz(x1, y0), gz(x0, y1), gz(x1, y1))
    top = z + 250.0
    for (x, y) in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
        C.place(V + "Corner_Exterior_Wood", x, y, gz(x, y) - 5.0, scale=(1.0, 1.0, (top - gz(x, y) + 15.0) / 300.0),
                sub="Garden/Pergola", cull=12000.0)
    for x in (x0, x1):
        beam((x, y0 - 40.0, top - 10.0), (x, y1 + 40.0, top - 10.0), 1.0)
    y = y0 - 20.0
    while y <= y1 + 20.0:
        beam((x0 - 40.0, y, top + 12.0), (x1 + 40.0, y, top + 12.0), 0.6, cull=8000.0)
        y += 50.0
    for k, y in enumerate((y0 + 20.0, y0 + 110.0, y0 + 200.0, y0 + 280.0)):
        for x in (x0, x1):
            B.add(V + ("Prop_Vine1" if (k + int(x)) % 2 else "Prop_Vine2"), x, y, top + 8.0, yaw=90.0 + R.uniform(-8, 8),
                  scale=(0.6, 1.0, 0.45), cull=8000.0)
    seat_bench((x0 + x1) * 0.5 + 10.0, (y0 + y1) * 0.5, gz((x0 + x1) * 0.5, (y0 + y1) * 0.5), 0.0, sub="Garden/Pergola")
    for y in (y0, y1):
        paper_lantern((x0 + x1) * 0.5, y, top - 45.0, size=0.9)
    C.claim((x0 + x1) * 0.5, (y0 + y1) * 0.5, 190.0)


def lane_side():
    """The fish alley where it crosses the garden: stone lamps and shrubs on both verges, a fallen-leaf strip."""
    for (x, y, dx, dy) in C.lane_samples("fish_alley", 420.0):
        if not C.in_zone(x, y):
            continue
        for side in (-1, 1):
            px, py = x - dy * side * 205.0, y + dx * side * 205.0
            if not C.free(px, py, 40.0, lane_gap=20.0):
                continue
            C.claim(px, py, 40.0)
            if R.random() < 0.55:
                B.add(JP + "GardenLamp", px, py, gz(px, py) + 12.0, scale=1.0, collide=True, cull=8000.0)
            else:
                B.add(R.choice([N + "Bush_Common_Flowers", N + "Bush_Common", N + "Plant_1_Big"]), px, py, gz(px, py) - 5.0,
                      yaw=R.uniform(0, 360), scale=R.uniform(0.45, 0.65), collide=False, cull=8000.0)


def bell_pavilion(x, y, yaw, zfun):
    """Shoro: a temple bell (an upturned cauldron) in an open tiled pavilion on a stone base, with its striking log."""
    zt = max(zfun(x + dx, y + dy) for dx in (-140.0, 0.0, 140.0) for dy in (-140.0, 0.0, 140.0)) + 20.0
    zb = min(zfun(x + dx, y + dy) for dx in (-140.0, 0.0, 140.0) for dy in (-140.0, 0.0, 140.0))
    C.place(V + "Stairs_Exterior_Platform", x, y, zb - 20.0, yaw=yaw, scale=(1.35, 1.35, (zt - zb + 20.0) / 100.0),
            sub="Island/Bell", cull=15000.0)
    for sx in (-1, 1):
        for sy in (-1, 1):
            px, py = local(x, y, yaw, sx * 112.0, sy * 112.0)
            C.place(V + "Corner_Exterior_Wood", px, py, zt, yaw=yaw, scale=(1.4, 1.4, 1.0), sub="Island/Bell", cull=15000.0)
    C.place(V + "Roof_RoundTiles_4x4", x, y, zt + 300.0 + 32.0, yaw=yaw, scale=(0.62, 0.6, 0.62), sub="Island/Bell")
    a = local(x, y, yaw, -125.0, 0.0) + (zt + 288.0,)
    b = local(x, y, yaw, 125.0, 0.0) + (zt + 288.0,)
    beam(a, b, 1.2, cull=12000.0)
    C.place(P + "Cauldron", x, y, zt + 272.0, yaw=yaw, roll=180.0, scale=(1.05, 1.05, 1.25), sub="Island/Bell",
            cull=15000.0)
    rod(INTERIOR + "SM_KG_RailPost", (x, y, zt + 270.0), (x, y, zt + 290.0), 0.6, 108.0, cull=8000.0)
    # the striking log (shumoku) on two ropes
    l0 = local(x, y, yaw, 58.0, 0.0) + (zt + 205.0,)
    l1 = local(x, y, yaw, 160.0, 0.0) + (zt + 205.0,)
    beam(l0, l1, 1.9)
    for lx in (75.0, 145.0):
        px, py = local(x, y, yaw, lx, 0.0)
        rod(INTERIOR + "SM_KG_RailPost", (px, py, zt + 205.0), (px, py, zt + 288.0), 0.12, 108.0, cull=6000.0)
    C.claim(x, y, 190.0)


def sake_wall(x, y, z, yaw, zfun):
    """Kazaridaru: a wall of sake barrels stacked on their sides in a wooden frame under a little tiled roof."""
    kinds = [PIR + f"Barrel_{i}" for i in (0, 1, 3, 4, 10, 11)]
    for side in (-1, 1):
        px, py = local(x, y, yaw, -30.0, side * 205.0)
        C.place(V + "Corner_Exterior_Wood", px, py, zfun(px, py) - 5.0, yaw=yaw, scale=(1.2, 1.2, 0.92),
                sub="Island/Sake", cull=12000.0)
    C.place(V + "Stairs_Exterior_Platform", *local(x, y, yaw, -10.0, 0.0), z - 80.0, yaw=yaw,
            scale=(0.55, 2.1, 0.85), sub="Island/Sake", cull=12000.0)
    for row in range(3):
        for col in range(5 - (1 if row == 2 else 0)):
            lat = -150.0 + col * 75.0 + (37.0 if row == 2 else 0.0)
            px, py = local(x, y, yaw, -45.0, lat)
            B.add(R.choice(kinds), px, py, z + 5.0 + 37.0 + row * 74.0, yaw=yaw + 180.0, pitch=90.0, scale=0.74,
                  collide=True, cull=9000.0)
    a = local(x, y, yaw, -30.0, -215.0) + (z + 270.0,)
    b = local(x, y, yaw, -30.0, 215.0) + (z + 270.0,)
    beam(a, b, 1.1)
    C.place(V + "Roof_Modular_RoundTiles", *local(x, y, yaw, -30.0, 0.0), z + 272.0, yaw=yaw, scale=(1.1, 2.05, 0.9),
            sub="Island/Sake", cull=12000.0)
    C.claim(x, y, 230.0)


def beach_camp():
    """A fisherman's camp on the south-east beach: fire, drying rack of fish, stool, crates, nets, oars."""
    x, y = 5460.0, 13330.0
    for _ in range(12):
        if iz(x, y) > 40.0:
            break
        x, y = x - 40.0, y + 60.0
    z = iz(x, y)
    # fire
    for k in range(3):
        B.add(INTERIOR + "SM_KG_Firewood", x, y, z - 6.0, yaw=k * 120.0, scale=0.7, cull=7000.0)
    for k in range(9):
        a = k / 9.0 * 2 * math.pi
        px, py = x + math.cos(a) * 55.0, y + math.sin(a) * 55.0
        B.add(N + f"Pebble_Round_{k % 5 + 1}", px, py, iz(px, py) - 2.0, yaw=k * 40.0, scale=1.3, cull=5000.0)
    ember = C.place(CUBE, x, y, z + 6.0, yaw=20.0, scale=(0.35, 0.35, 0.12), sub="Island/Camp", collide=False,
                    cull=7000.0, shadow=False)
    set_mat(ember, LANTERN_MAT)
    C.place(PIR + "Torch_1", x - 40.0, y + 190.0, iz(x - 40.0, y + 190.0) - 5.0, yaw=30.0, sub="Island/Camp",
            cull=12000.0)
    C.light(x, y, z + 90.0, intensity=6.0, radius=550.0, color=(255, 120, 50))
    C.ambient("A_Fire_Loop", x, y, z + 50.0, 0.5)
    # drying rack with fish
    rx, ry, ryaw = x + 170.0, y + 120.0, 30.0
    for side in (-1, 1):
        px, py = local(rx, ry, ryaw, 0.0, side * 110.0)
        C.place(V + "Corner_Exterior_Wood", px, py, iz(px, py) - 8.0, scale=(0.7, 0.7, 0.63), sub="Island/Camp",
                cull=9000.0)
    rz = iz(rx, ry) + 175.0
    beam(local(rx, ry, ryaw, 0.0, -125.0) + (rz,), local(rx, ry, ryaw, 0.0, 125.0) + (rz,), 0.45, cull=8000.0)
    fishes = [WP + "SM_KG_Fish_Mackerel", WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Salmon", WP + "SM_KG_Fish_Mackerel",
              WP + "SM_KG_Fish_Cod", WP + "SM_KG_Fish_Mackerel"]
    for i, f in enumerate(fishes):
        px, py = local(rx, ry, ryaw, 0.0, -85.0 + i * 34.0)
        B.add(f, px, py, rz - 32.0, yaw=ryaw + R.uniform(-15, 15), pitch=-88.0, cull=5000.0)
    # seat + gear
    sx, sy = x - 95.0, y - 50.0
    C.seat(P + "Stool", sx, sy, iz(sx, sy), yaw=math.degrees(math.atan2(y - sy, x - sx)), sub="Island/Camp")
    for (lx, ly, path, sc) in ((-60.0, 110.0, P + "FarmCrate_Empty", 1.2), (-20.0, 150.0, P + "Bucket_Wooden_1", 1.0),
                               (80.0, -110.0, P + "Rope_3", 1.0), (140.0, -40.0, WP + "SM_KG_Oar", 1.0)):
        px, py = x + lx, y + ly
        B.add(path, px, py, iz(px, py) + 1.0, yaw=R.uniform(0, 360), scale=sc, cull=5000.0)
    for (lx, ly) in ((-60.0, 105.0), (-50.0, 118.0)):
        B.add(WP + "SM_KG_Fish_Cod", x + lx, y + ly, iz(x + lx, y + ly) + 22.0, yaw=R.uniform(0, 360), cull=3500.0)
    C.breakable(PIR + "crates_1", x + 30.0, y + 170.0, iz(x + 30.0, y + 170.0) + 2.0, yaw=20.0, sub="Island/Camp")
    C.breakable(P + "Barrel", x + 110.0, y + 190.0, iz(x + 110.0, y + 190.0) + 2.0, yaw=0.0, sub="Island/Camp")
    C.claim(x + 60.0, y + 60.0, 220.0)


def pagoda_ring_path():
    """Stepping stones circling the pagoda (joining the lanterns, the bell, the store and the shrines)."""
    for i in range(34):
        a = i / 34.0 * 2 * math.pi
        x, y = IX + math.cos(a) * 470.0, IY + math.sin(a) * 470.0
        if C.overlaps(x, y, 25.0):
            continue
        B.add(JP + "SteppingStone", x, y, iz(x, y) - 3.0, yaw=R.uniform(0, 360), scale=1.2, cull=6000.0)


def meditation_rock():
    """A flat rock on the west shore with a cushion, a candle and a view back to the village."""
    a = math.radians(200.0)
    d = 1200.0
    while d < 3800.0 and iz(IX + math.cos(a) * d, IY + math.sin(a) * d) > 70.0:
        d += 50.0
    x, y = IX + math.cos(a) * (d - 80.0), IY + math.sin(a) * (d - 80.0)
    z = iz_min(x, y, 90.0)
    B.add(N + "Rock_Medium_2", x, y, z - 25.0, yaw=30.0, scale=(0.5, 0.55, 0.3), collide=True, cull=12000.0)
    top = z - 25.0 + 190.0 * 0.3
    B.add(INTERIOR + "SM_KG_Rug_Round", x + 10.0, y, top - 2.0, scale=(0.4, 0.4, 6.0), cull=5000.0)
    B.add(P + "Candle_2", x + 45.0, y - 30.0, top - 3.0, cull=3500.0)
    B.add(P + "Candle_1", x + 50.0, y - 12.0, top - 3.0, cull=3500.0)
    C.claim(x, y, 120.0)


# ============================================================================================ round-3 set pieces
def find_flat(x, y, r, max_slope, search=500.0, min_z=60.0):
    """Nearest island spot to (x, y) that is flat enough, dry and unclaimed (spiral search)."""
    for k in range(0, 60):
        d = search * math.sqrt(k / 60.0)
        a = k * 2.39996
        px, py = x + math.cos(a) * d, y + math.sin(a) * d
        if iz(px, py) > min_z and islope(px, py, r) < max_slope and not C.overlaps(px, py, r) and C.in_zone(px, py):
            return px, py
    return None


def tea_stall_island():
    """Chaya: a dango/tea stand on the north-east plateau with a red bench, tea set and a paper lantern."""
    spot = find_flat(5150.0, 16150.0, 150.0, 60.0)
    if not spot:
        return
    x, y = spot
    z = iz_min(x, y, 110.0)
    yaw = math.degrees(math.atan2(IY - y, IX - x))            # counter faces the pagoda
    C.place(JP + "Noren_Stall", x, y, z - 3.0, yaw=yaw - 90.0, sub="Island/Chaya", cull=15000.0)
    fx, fy = fwd(yaw)
    top = z + 100.0
    for i in range(3):
        px, py = local(x, y, yaw, -60.0, -50.0 + i * 50.0)
        B.add(KITCHEN + "Kitchen_Cup", px, py, top, scale=1.3, cull=3500.0)
    px, py = local(x, y, yaw, -60.0, 70.0)
    B.add(KITCHEN + "Kitchen_SmallPotLid", px, py, top, yaw=yaw, scale=0.8, cull=3500.0)
    bx, by = x + fx * 220.0, y + fy * 220.0
    seat_bench(bx, by, iz(bx, by), yaw + 180.0, sub="Island/Chaya")
    paper_lantern(x + fx * 130.0, y + fy * 130.0, z + 235.0, size=1.0)
    C.breakable(P + "Vase_2", *local(x, y, yaw, 40.0, 150.0), z + 2.0, yaw=R.uniform(0, 360), sub="Island/Chaya")
    C.claim(x, y, 200.0)
    C.claim(bx, by, 150.0)


def sea_shrine():
    """North tip: a stone hokora on the rocks facing the open sea, a small torii standing in the water before it."""
    a = math.radians(90.0)
    d = 1500.0
    while d < 3800.0 and iz(IX, IY + d) > 60.0:
        d += 40.0
    x, y = IX + 60.0, IY + d - 140.0
    z = iz_max(x, y, 80.0)
    B.add(N + "Rock_Medium_3", x, y + 60.0, z - 70.0, yaw=200.0, scale=(0.55, 0.5, 0.45), collide=True, cull=15000.0)
    hokora(x, y, z + 5.0, 90.0, kind="stone", light=False, zfun=iz)
    tx, ty = x, y + 520.0
    torii(tx, ty, -120.0, 0.0, 0.38, sub="Island/SeaShrine")
    for k in range(6):
        rx, ry = x + R.uniform(-300, 300), y + R.uniform(100, 380)
        B.add(N + f"Rock_Medium_{R.randint(1, 3)}", rx, ry, iz(rx, ry) - 30.0, yaw=R.uniform(0, 360),
              scale=R.uniform(0.25, 0.4), collide=True, cull=12000.0)


def driftwood():
    """Weathered planks, a lost barrel and rope washed up along the waterline; a second boat pulled up on the west."""
    for i in range(14):
        a = R.uniform(0, 2 * math.pi)
        d = 1400.0
        while d < 4000.0 and iz(IX + math.cos(a) * d, IY + math.sin(a) * d) > 45.0:
            d += 50.0
        x, y = IX + math.cos(a) * (d - 60.0), IY + math.sin(a) * (d - 60.0)
        if C.overlaps(x, y, 60.0) or math.hypot(x - 4210.0, y - 12800.0) < 450.0:
            continue
        C.claim(x, y, 60.0)
        B.add(PIR + f"Planks_{R.randint(0, 3)}", x, y, iz(x, y) - 2.0, yaw=R.uniform(0, 360), roll=R.uniform(-6, 6),
              scale=R.uniform(0.5, 0.8), cull=7000.0)
    # west beach boat on its side + oar + a lantern on a post
    a = math.radians(165.0)
    d = 1500.0
    while d < 3800.0 and iz(IX + math.cos(a) * d, IY + math.sin(a) * d) > 50.0:
        d += 40.0
    x, y = IX + math.cos(a) * (d - 120.0), IY + math.sin(a) * (d - 120.0)
    C.place(WP + "SM_KG_Rowboat", x, y, iz_min(x, y, 120.0) + 20.0, yaw=75.0, roll=-22.0, sub="Island/Beach",
            cull=12000.0)
    B.add(WP + "SM_KG_Oar", x + 90.0, y - 120.0, iz(x + 90.0, y - 120.0) + 3.0, yaw=30.0, cull=5000.0)
    B.add(P + "Rope_1", x - 80.0, y + 110.0, iz(x - 80.0, y + 110.0) + 1.0, yaw=0.0, cull=4000.0)
    C.breakable(PIR + "Barrel_7", x + 150.0, y + 60.0, iz(x + 150.0, y + 60.0) + 2.0, yaw=0.0, sub="Island/Beach")
    C.claim(x, y, 200.0)


def lantern_corner():
    """North-east corner of the garden by the fish alley: a stone lantern among rocks, ferns and a young maple."""
    x, y = 2800.0, 2660.0
    C.place(JP + "ToroLantern", x, y, gz(x, y) - 5.0, yaw=200.0, scale=0.75, sub="Garden/Corner", cull=12000.0)
    for (dx, dy, sc) in ((110.0, -40.0, 0.45), (-90.0, 70.0, 0.35), (60.0, 110.0, 0.3)):
        B.add(JP + "GardenRock", x + dx, y + dy, gz(x + dx, y + dy) - 15.0, yaw=R.uniform(0, 360), scale=sc,
              collide=True, cull=8000.0)
    for k in range(8):
        a = k / 8.0 * 2 * math.pi
        px, py = x + math.cos(a) * 150.0, y + math.sin(a) * 150.0
        if C.lane_distance(px, py) < 40.0:
            continue
        B.add(R.choice([N + "Fern_1", N + "Plant_7", N + "Flower_3_Single", N + "Plant_1"]), px, py, gz(px, py) - 2.0,
              yaw=R.uniform(0, 360), scale=R.uniform(0.45, 0.7), cull=6000.0)
    C.place(JP + "Maple", 2880.0, 2470.0, gz(2880.0, 2470.0) - 8.0, yaw=40.0, scale=0.42, sub="Garden/Corner")
    C.claim(x, y, 170.0)


# ============================================================================================ entry
def dress():
    garden()
    island()
    B.flush()
