"""Lighthouse Point (zone "coast", ridge z 8-14 m, sea cliffs): exposed, the edge of the world.

  * The lighthouse beam: a slowly turning pair of light cones (M_KG_LighthouseBeam, faint by day, strong at night)
    on an AKGSpinner hub in the lantern room - the navigation beacon over the basin.
  * The keeper's yard at the foot of the tower: oil barrels (FuelLighthouse clear), a woodpile, a bench looking out,
    a vegetable bed, washing, a lantern; the old signal cannon on the brow pointing out to sea with its shot pile.
  * The Headland Road: a rope rail on posts along the cliff side of the last climb, gorse and heather drifts, waymark.
  * Below the cliffs: a shipwreck broken on the rocks off the point, a mast in the surf, tide pools and driftwood.
"""
import importlib
import math
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

V, N, P, PIR, WP, DV, DH, DW, DX, IP = C.V, C.N, C.P, C.PIR, C.WP, C.DV, C.DH, C.DW, C.DX, C.IP
R = C.rng


def m(xm, ym):
    return xm * C.M, ym * C.M


def spot(x, y, r, reach=300.0, flat=50.0, **kw):
    for _ in range(4):
        s = C.find(x, y, r, reach=reach, **kw)
        if not s:
            return None
        if C.slope(s[0], s[1], min(r, 150.0)) <= flat:
            return s
        C.claim(s[0], s[1], r * 0.6)
    return None


def beam():
    lh = C.building("lighthouse")
    x, y = lh.x, lh.y
    z = lh.z + 4 * C.FLOOR_H + 175.0
    hub = C.mover(C.E + "Sphere", x, y, z, yaw=0.0, scale=0.45, spin=(0.0, 22.0, 0.0), sub="Lighthouse",
                  material="/Game/KillGodot/Materials/M_KG_JapanGlow")
    L, rad = 3400.0, 260.0
    for phi in (0.0, 180.0):
        dx, dy = C.fwd(phi, L * 0.5)
        cone = C.place(DX + "SM_KG_BeamCone", x + dx, y + dy, z - 80.0, yaw=phi, pitch=90.0 - 3.0,
                       scale=(rad / 50.0, rad / 50.0, L / 100.0), collide=False, sub="Lighthouse", shadow=False,
                       movable=True, overhead=True)
        C.attach(cone, hub)
    C.stats["beam"] = 2


def keeper_yard():
    lh = C.building("lighthouse")
    fx, fy = C.task_spots()["FuelLighthouse"]
    face = lh.front_yaw()
    for (t, out), what in (((-280.0, 260.0), "oil"), ((260.0, 200.0), "wood"), ((0.0, 650.0), "bench"),
                           ((-500.0, 500.0), "veg"), ((520.0, 520.0), "laundry"), ((-150.0, 380.0), "lantern")):
        x, y = lh.at("front", t, out)
        r = {"oil": 90.0, "wood": 90.0, "bench": 140.0, "veg": 200.0, "laundry": 230.0, "lantern": 40.0}[what]
        s = spot(x, y, r)
        if not s or math.hypot(s[0] - fx, s[1] - fy) < 220.0:
            continue
        if what == "oil":
            K.barrels(s[0], s[1], 4, 55.0, pirate=False, breakable=1)
        elif what == "wood":
            K.woodpile(s[0], s[1], lh.tyaw("front"), 4)
        elif what == "bench":
            K.bench(s[0], s[1], K.face_dir(s[0], s[1], *C.O_BASIN))
        elif what == "veg":
            K.veg_bed(s[0], s[1], face, 300.0, 200.0)
        elif what == "laundry":
            K.laundry_line(s[0], s[1], face + 90.0)
        else:
            K.lantern_post(s[0], s[1], face, light=6.0)
    # the signal cannon on the brow, pointing out to sea
    s = spot(*m(74.0, 100.0), 180.0, reach=500.0)
    if s:
        yaw = K.face_dir(s[0], s[1], s[0] + 300.0, s[1] + 800.0)
        K.prop(PIR + "cannon_0", s[0], s[1], C.ground_min(s[0], s[1], 120.0), yaw=yaw, sub="Cannon", claim=180.0)
        for k in range(4):
            K.clutter(PIR + "Cannon_Ball_0", s[0] - 150.0 + (k % 2) * 30.0, s[1] + 90.0 + (k // 2) * 25.0,
                      C.ground(s[0], s[1]) + (20.0 if k == 3 else 0.0), None, 0.35)


def headland_road():
    pts = C.lane("headland_road")
    L = C.polyline_len(pts)
    # rope rail along the seaward side of the last climb (east side of the road = towards the open sea)
    prev = None
    for x, y, ux, uy, s in C.resample(pts, 300.0, start=L * 0.45, end_trim=250.0):
        nx, ny = uy, -ux
        side = 1 if (x + nx * 100.0) > x else -1
        px, py = x + nx * side * 230.0, y + ny * side * 230.0
        if not C.free(px, py, 25.0, claims=False):
            prev = None
            continue
        top = K.post(px, py, 110.0, 0.6)
        if prev:
            K.rod(prev, (px, py, top - 10.0), 0.12, cull=9000.0)
        prev = (px, py, top - 10.0)
    # gorse (yellow) and heather drifts on the slopes either side
    n = 0
    for _ in range(500):
        if n >= 26:
            break
        s = R.uniform(0.0, L)
        x, y, ux, uy = C.at_len(pts, s)
        off = R.choice([-1, 1]) * R.uniform(350.0, 1400.0)
        px, py = x - uy * off, y + ux * off
        if C.zone_of(px, py) != "coast" or not C.free(px, py, 120.0) or C.ground(px, py) < 400.0:
            continue
        if R.random() < 0.5:
            for k in range(3):
                K.shrub(px + R.uniform(-90, 90), py + R.uniform(-90, 90), R.uniform(0.35, 0.55), flowers=True)
        else:
            K.flower_patch(px, py, 150.0, 6)
        C.claim(px, py, 150.0)
        n += 1
    st = C.anchor("headland_road:start")
    s = C.find(st["x"] + 300.0, st["y"] + 200.0, 40.0, reach=250.0)
    if s:
        K.signpost_arrows(s[0], s[1], [C.xy("lighthouse"), C.xy("pavilion")])


def wreck():
    """A broken hull on the rocks off the point and a lone mast in the surf (visible from the lighthouse)."""
    for (xm, ym), what in (((86.0, 118.0), "hull"), ((96.0, 113.0), "mast")):
        x, y = m(xm, ym)
        gz = C.ground(x, y)
        if what == "hull":
            K.prop(DH + "Shipwreck", x, y, max(gz, -260.0) - 60.0, yaw=35.0, roll=14.0, pitch=-6.0, sub="Wreck",
                   cull=0.0, claim=600.0)
            for k in range(3):
                K.solid(C.N + "Rock_Medium_%d" % (1 + k), x + R.uniform(-600, 600), y + R.uniform(-300, 300),
                        max(gz, -300.0) - 80.0, R.uniform(0, 360), R.uniform(0.9, 1.4))
        else:
            K.prop(DH + "ShipwreckMast", x, y, max(gz, -300.0) - 100.0, yaw=R.uniform(0, 360), roll=18.0, sub="Wreck",
                   cull=0.0)
    for xm, ym in ((60.0, 104.0), (67.0, 108.0), (78.0, 110.5)):
        x, y = m(xm, ym)
        if -120.0 < C.ground(x, y) < 80.0:
            K.prop(DH + "TidePool", x, y, C.ground(x, y) - 10.0, yaw=R.uniform(0, 360), sub="Shore", collide=False)
            K.clutter(DH + "ShellCluster", x + 120.0, y + 60.0, None, None, 1.0)
            K.clutter(DH + "SeaweedClump", x - 110.0, y + 40.0, None, None, 1.0)


def dress():
    K.reset()
    for name, fn in (("beam", beam), ("keeper", keeper_yard), ("road", headland_road), ("wreck", wreck), ("smash", lambda: K.smash_piles([C.building("lighthouse").at("front", 350.0, 500.0)]))):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
