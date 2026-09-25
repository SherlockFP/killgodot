"""The wilds (everything outside the named zones: the forest ring and the mountain rim framing the bowl).

The builder already plants the groves; the wilds get points of interest, each a small story found off the paths:
  * a hunters' camp: tent, campfire (light), log seats, a drying rack with hides, a chest;
  * a standing-stone circle on open high ground with an offering stone;
  * a cave mouth in the rim with a lantern and a smuggler's chest inside;
  * a charcoal burner's clearing: log piles, a smouldering mound, tools;
  * a lookout rest: a log bench facing the town, a cairn;
plus fallen trees, rock clusters with ferns, mushroom rings and flower patches along the edges of the paths.
Spots are searched (not hard-coded): open (slope), inside the playable nav bounds, near but off a path, away from
the builder's trees.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

V, N, P, PIR, WP, DV, DH, DW, IP = C.V, C.N, C.P, C.PIR, C.WP, C.DV, C.DH, C.DW, C.IP
R = C.rng
NB = C.LAYOUT["nav_bounds"]
X0, X1 = NB["min"][0] * C.M + 400.0, NB["max"][0] * C.M - 400.0
Y0, Y1 = NB["min"][1] * C.M + 400.0, NB["max"][1] * C.M - 400.0


def open_spot(r, near=None, lane_max=2500.0, lane_min=500.0, flat=80.0, tries=4000, region=None):
    """A free, fairly flat spot in the wilds within [lane_min, lane_max] of a walkway (optionally in a region box)."""
    best = None
    for _ in range(tries):
        if region:
            x, y = R.uniform(region[0], region[1]), R.uniform(region[2], region[3])
        elif near:
            a, d = R.uniform(0, 6.283), R.uniform(0, 2500.0)
            x, y = near[0] + d * math.cos(a), near[1] + d * math.sin(a)
        else:
            x, y = R.uniform(X0, X1), R.uniform(Y0, Y1)
        if C.zone_of(x, y) != "wilds" or C.ground(x, y) < 150.0:
            continue
        ld = C.lane_distance(x, y)
        if ld < lane_min or not C.free(x, y, r) or C.slope(x, y, min(r, 200.0)) > flat:
            continue
        return x, y
    return best


def hunters_camp(x, y):
    face = K.face_dir(x, y, *C.O_BASIN)
    f = K.F(x, y, phi=face)
    K.campfire(x, y, light=16.0)
    tx, ty = f.w(0.0, 330.0)
    K.prop(DW + "Tent_A", tx, ty, C.ground_min(tx, ty, 150.0), yaw=face - 90.0 + 180.0, sub="Camp", claim=230.0)
    for a in (40.0, 160.0, 280.0):
        lx, ly = x + 200.0 * math.cos(math.radians(a)), y + 200.0 * math.sin(math.radians(a))
        K.log(lx, ly, C.ground(lx, ly), a + 90.0, 180.0, 1.7)
        C.claim(lx, ly, 80.0)
    ax, ay = f.w(-300.0, 60.0)
    bx, by = f.w(-300.0, -200.0)
    za, zb = K.post(ax, ay, 190.0, 0.6), K.post(bx, by, 190.0, 0.6)
    K.rod((ax, ay, za - 5.0), (bx, by, zb - 5.0), 0.3)
    for t in (0.3, 0.65):
        K.clutter(P + "Banner_2_Cloth", ax + (bx - ax) * t, ay + (by - ay) * t, za - 15.0, face, (0.6, 1.0, 0.35), cull=9000.0)
    cx, cy = f.w(260.0, 280.0)
    C.loot_chest(cx, cy, yaw=face, name="Hunter's Chest", path=PIR + "chest_common_0")
    K.clutter(P + "Bag", cx - 80.0, cy + 20.0, None, None, 0.9)
    K.clutter(WP + "Fish_Salmon", x + 40.0, y - 30.0, C.ground(x, y) + 20.0, None, 1.0, 0.0, 90.0)
    C.stats.setdefault("pois", []).append(f"camp@{x:.0f},{y:.0f}")


def stone_circle(x, y):
    n = 7
    for k in range(n):
        a = 2 * math.pi * k / n + R.uniform(-0.1, 0.1)
        px, py = x + 520.0 * math.cos(a), y + 520.0 * math.sin(a)
        K.prop(DW + "Menhir_%s" % "ABC"[k % 3], px, py, C.ground_min(px, py, 60.0) - 20.0, yaw=math.degrees(a) + 90.0,
               pitch=R.uniform(-4, 4), roll=R.uniform(-6, 6), sub="Stones", claim=90.0)
    K.solid(N + "Rock_Medium_2", x, y, C.ground_min(x, y, 120.0) - 30.0, R.uniform(0, 360), (0.8, 0.8, 0.5))
    for k in range(5):
        K.clutter(P + "Candle_2", x + R.uniform(-80, 80), y + R.uniform(-80, 80), C.ground(x, y) + 5.0, None, 1.5)
    K.flower_patch(x, y, 400.0, 10)
    C.light(x, y, C.ground(x, y) + 120.0, 4.0, 500.0, (255, 150, 90))
    C.claim(x, y, 600.0)
    C.stats.setdefault("pois", []).append(f"stones@{x:.0f},{y:.0f}")


def cave(x, y):
    face = K.face_dir(x, y, *C.O_BASIN)
    K.prop(DW + "CaveMouth", x, y, C.ground_min(x, y, 250.0) - 30.0, yaw=face - 90.0, sub="Cave", claim=450.0, cull=0.0)
    f = K.F(x, y, phi=face)
    cx, cy = f.w(0.0, 120.0)
    C.loot_chest(cx, cy, yaw=face, name="Smuggler's Cache", path=PIR + "chest_silver_0")
    K.barrels(*f.w(-160.0, 60.0), 2, 45.0)
    lx, ly = f.w(170.0, -120.0)
    K.lantern_post(lx, ly, face, light=6.0, radius=600.0, tall=0.8)
    C.stats.setdefault("pois", []).append(f"cave@{x:.0f},{y:.0f}")


def charcoal(x, y):
    face = K.face_dir(x, y, *C.O_BASIN)
    K.log_pile(x + 250.0, y, face + 90.0, 3, 340.0)
    K.clutter(WP + "DigMound", x - 150.0, y + 50.0, None, None, (2.4, 2.4, 1.6))
    K.clutter(N + "Grass_Wispy_Short", x - 150.0, y + 50.0, C.ground(x, y) + 40.0, None, 0.5)
    C.light(x - 150.0, y + 50.0, C.ground(x, y) + 60.0, 5.0, 450.0, (255, 110, 50))
    K.solid(P + "Anvil_Log", x, y - 180.0, None, R.uniform(0, 360))
    K.clutter(P + "Axe_Bronze", x, y - 180.0, C.ground(x, y) + 100.0, None, 1.0, 0.0, 20.0)
    K.clutter(WP + "Shovel", x - 60.0, y - 250.0, C.ground(x, y) + 10.0, face, 1.0, -75.0, 0.0)
    C.claim(x, y, 380.0)
    C.stats.setdefault("pois", []).append(f"charcoal@{x:.0f},{y:.0f}")


def lookout(x, y):
    face = K.face_dir(x, y, *C.O_BASIN)
    K.log(x, y, C.ground(x, y), face + 90.0, 220.0, 1.8)
    C.seat(P + "Stool", x, y, face, z=C.ground_max(x, y, 25.0) - 6.0, seat_height=45.0)
    bx, by = x + math.cos(math.radians(face + 180.0)) * 200.0, y + math.sin(math.radians(face + 180.0)) * 200.0
    for k in range(4):
        K.clutter(N + "Pebble_Round_%d" % (1 + k), bx, by, C.ground(bx, by) + k * 9.0, None, 2.0 - k * 0.3)
    C.claim(x, y, 250.0)
    C.stats.setdefault("pois", []).append(f"lookout@{x:.0f},{y:.0f}")


def pois():
    """The playable wilds are the wedge behind Crown Hill (x -85..25 m, y -120..-50 m) and the beach strip west of the
    harbour; the rim beyond the brook valley and the orchard (|x| > 130 m) is walkable too and seen from the town."""
    regions = {"N1": (-8200, -3500, -11800, -8200), "N2": (-3500, 2500, -11800, -9000), "Nrim": (-6000, 4000, -15500, -12200),
               "Wrim": (-16500, -13300, -8000, 3000), "Erim": (13300, 16500, -9000, 1000)}
    plan = [("camp", "N1", 420.0), ("stones", "N2", 650.0), ("cave", "Nrim", 460.0), ("charcoal", "N1", 400.0),
            ("lookout", "Erim", 260.0), ("camp", "Wrim", 420.0), ("lookout", "N2", 260.0), ("stones", "Erim", 650.0)]
    for kind, reg, r in plan:
        s = open_spot(r, region=regions[reg], lane_min=250.0, lane_max=9000.0, flat=160.0)
        if not s:
            C.stats.setdefault("poi_missed", []).append(f"{kind}/{reg}")
            continue
        C.clear_grass(s[0], s[1], {"camp": 650.0, "stones": 800.0, "cave": 500.0, "charcoal": 550.0, "lookout": 350.0}[kind])
        {"camp": hunters_camp, "stones": stone_circle, "cave": cave, "charcoal": charcoal, "lookout": lookout}[kind](*s)


def path_edges():
    """Fallen trees, rock clusters, mushroom rings and flowers a few metres off the country paths."""
    n = 0
    for l in C.LAYOUT["lanes"]:
        if l["kind"] not in ("path", "lane"):
            continue
        pts = C.cm(l["points"])
        for x, y, ux, uy, s in C.resample(pts, 1100.0, start=500.0, end_trim=300.0):
            side = R.choice([-1, 1])
            off = R.uniform(350.0, 700.0)
            px, py = x - uy * side * off, y + ux * side * off
            if C.zone_of(px, py) != "wilds" or not C.free(px, py, 150.0):
                continue
            roll = n % 4
            if roll == 0:
                K.solid(N + "DeadTree_%d" % R.choice([1, 2, 3]), px, py, C.ground(px, py) - 30.0,
                        math.degrees(math.atan2(uy, ux)) + R.uniform(-30, 30), 0.5, 86.0, 0.0)
                K.clutter(N + "Mushroom_Laetiporus", px + 60.0, py + 30.0, None, None, 0.8)
            elif roll == 1:
                K.rock_cluster(px, py, R.uniform(0.6, 1.0), 3)
            elif roll == 2:
                for k in range(9):
                    a = 2 * math.pi * k / 9
                    K.clutter(N + "Mushroom_Common", px + 110.0 * math.cos(a), py + 110.0 * math.sin(a), None, None,
                              R.uniform(0.6, 1.0))
            else:
                K.flower_patch(px, py, 160.0, 7)
            C.claim(px, py, 150.0)
            n += 1
    C.stats["path_edges"] = n


def wedge_scatter():
    """Forest-floor stories through the wedge behind Crown Hill and along the rims: fallen trees, rocks with ferns,
    mushroom rings, flower patches (never in a grove's trunk, never on a path)."""
    n = 0
    for _ in range(1500):
        if n >= 30:
            break
        x, y = R.uniform(-9000, 3000), R.uniform(-12500, -5500)
        if C.zone_of(x, y) != "wilds" or C.ground(x, y) < 150.0 or C.lane_distance(x, y) < 250.0:
            continue
        if not C.free(x, y, 150.0) or C.slope(x, y, 150.0) > 180.0:
            continue
        roll = n % 4
        if roll == 0:
            K.solid(N + "DeadTree_%d" % R.choice([1, 2, 3]), x, y, C.ground(x, y) - 30.0, R.uniform(0, 360), 0.5, 86.0, 0.0)
            K.clutter(N + "Mushroom_Laetiporus", x + 60.0, y + 30.0, None, None, 0.8)
        elif roll == 1:
            K.rock_cluster(x, y, R.uniform(0.6, 1.0), 3)
        elif roll == 2:
            for k in range(9):
                a = 2 * math.pi * k / 9
                K.clutter(N + "Mushroom_Common", x + 110.0 * math.cos(a), y + 110.0 * math.sin(a), None, None,
                          R.uniform(0.6, 1.0))
        else:
            K.flower_patch(x, y, 160.0, 7)
        C.claim(x, y, 150.0)
        n += 1
    C.stats["wedge_scatter"] = n


def dress():
    K.reset()
    for name, fn in (("pois", pois), ("edges", path_edges), ("scatter", wedge_scatter)):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
