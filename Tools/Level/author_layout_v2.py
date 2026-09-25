"""Authoring script for Tools/Level/morrowmere_layout_v2.json ("Morrowmere v2 - The Amphitheatre Cove", final).

    python Tools/Level/author_layout_v2.py          # writes morrowmere_layout_v2.json
    python Tools/Level/validate_layout.py           # must print PASS
    python Tools/Level/render_layout.py             # writes Docs/Level/Morrowmere_v2.png

The JSON is the builders' source of truth (kg_build_terrain.py, kg_build_village.py, kg_dress_common.py read it).
This script is the design tool that produced it: the town is laid out in a polar frame around the harbour basin
centre O, which sits on the Processional Axis (bell tower -> ... -> pagoda at (45, 150)). Terraces are rings around
O, streets follow the rings (contours), stairs and opes run down the radials. Edit here and re-run rather than
hand-editing the JSON (hand edits are overwritten).

Frame: metres, UE axes (x east, +y toward the sea, z up). UE cm = m * 100. Blender = (x, -y).
Building convention (same as v1 kg_build_village.build_house): `size` = [w, d] with w = frontage along local X,
d = depth along local Y, the front (door) is local -Y and looks at `face`; UE actor yaw = face_deg + 90.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "morrowmere_layout_v2.json")

# ------------------------------------------------------------------------------------------------ frame
PAGODA = (45.0, 150.0)
_ax = (-51.0, -213.0)                      # pagoda -> bell tower (the Processional Axis, pointing inland)
_n = math.hypot(*_ax)
DIN = (_ax[0] / _n, _ax[1] / _n)           # unit vector inland along the axis
E = (-DIN[1], DIN[0])                      # unit vector "east" across the axis
_t = (66.0 - PAGODA[1]) / DIN[1]
O = (PAGODA[0] + _t * DIN[0], 66.0)        # amphitheatre centre = basin centre, on the axis


def rad(th):
    c, s = math.cos(math.radians(th)), math.sin(math.radians(th))
    return (c * DIN[0] + s * E[0], c * DIN[1] + s * E[1])


def tan_(th):
    c, s = math.cos(math.radians(th)), math.sin(math.radians(th))
    return (-s * DIN[0] + c * E[0], -s * DIN[1] + c * E[1])


def P(r, th):
    u = rad(th)
    return (O[0] + r * u[0], O[1] + r * u[1])


def A(lat, r):
    """Axial frame: r metres inland from O along the axis, lat metres to the east of it."""
    return (O[0] + r * DIN[0] + lat * E[0], O[1] + r * DIN[1] + lat * E[1])


def ang(v):
    return math.degrees(math.atan2(v[1], v[0]))


def rd(p, k=2):
    return [round(p[0], k), round(p[1], k)]


def arc(r, th0, th1, step=2.0):
    n = max(2, int(math.ceil(abs(th1 - th0) * math.pi / 180.0 * r / step)) + 1)
    return [P(r, th0 + (th1 - th0) * i / (n - 1)) for i in range(n)]


def sector(r0, r1, th0, th1):
    return arc(r1, th0, th1) + arc(r0, th1, th0)


def clear(w_lane, r, extra=1.0):
    """Angular half-clearance (deg) around a radial walkway of width w_lane at radius r."""
    return math.degrees(math.asin((w_lane / 2.0 + extra) / r)) + 0.35


def poly(pts):
    return [rd(p, 2) for p in pts]


# ------------------------------------------------------------------------------------------------ buildings
BUILD = []          # every footprint (homes, infill, civic, towers, special)


def bldg(kind, at, w, d, face_deg, name, district, terrace, z, storeys=2, block=None, **kw):
    f = math.radians(face_deg)
    rec = {"id": None, "kind": kind, "name": name, "district": district, "terrace": terrace, "z": z,
           "at": rd(at), "size": [w, d], "face_deg": round(face_deg % 360.0, 2), "yaw": round((face_deg + 90.0) % 360.0, 2),
           "face": rd((at[0] + math.cos(f) * (d / 2.0 + 4.0), at[1] + math.sin(f) * (d / 2.0 + 4.0))),
           "storeys": storeys}
    if block:
        rec["block"] = block
    rec.update(kw)
    BUILD.append(rec)
    return rec


def parse(spec):
    """'h6x8^3:Name' -> (kind, w, d, storeys, name). kinds: h home, i infill, c civic."""
    name = None
    if ":" in spec:
        spec, name = spec.split(":", 1)
    storeys = None
    if "^" in spec:
        spec, s = spec.split("^")
        storeys = int(s)
    kind = {"h": "home", "i": "infill", "c": "civic"}[spec[0]]
    w, d = (int(v) for v in spec[1:].split("x"))
    return kind, w, d, storeys, name


def _blocks(units, block_max):
    blocks, cur, cl = [], [], 0
    for u in units:
        if cur and cl + u[1] > block_max:
            blocks.append(cur)
            cur, cl = [], 0
        cur.append(u)
        cl += u[1]
    if cur:
        blocks.append(cur)
    return blocks


def _need(units, r_front, facing, block_max, min_gap):
    blocks = _blocks(units, block_max)
    need, rb = 0.0, r_front
    for b in blocks:
        L = sum(u[1] for u in b)
        dmax = max(u[2] for u in b)
        if facing == "in":
            need += 2 * math.degrees(math.atan((L / 2.0) / r_front))
        else:
            rl = math.sqrt(r_front ** 2 - (L / 2.0) ** 2)
            need += 2 * math.degrees(math.atan((L / 2.0) / (rl - dmax)))
            rb = r_front - dmax
    return need + (len(blocks) - 1) * (math.degrees(min_gap / rb) + 0.25)


def pack(label, district, terrace, z, r_front, facing, th0, th1, specs, block_max=18.0, min_gap=1.15,
         storeys=(2, 3, 2, 2), fill=True):
    """Fill the ring segment th0..th1: take the longest prefix of `specs` that fits, then (fill=True) keep adding
    4 m infill shells and widening 4 m units to 6 m until nothing more fits, so the street wall is continuous.
    Units are grouped into straight party-wall blocks of <= block_max m, >= min_gap apart."""
    parsed = [parse(s) for s in specs]
    avail = th1 - th0
    fits = lambda us: _need(us, r_front, facing, block_max, min_gap) <= avail  # noqa: E731
    n = len(parsed)
    while n > 0 and not fits(parsed[:n]):
        n -= 1
    units = parsed[:n]
    added = widened = 0
    while fill and units:
        cand = units + [("infill", 4, units[-1][2], None, None)]
        if fits(cand):
            units, added = cand, added + 1
            continue
        done = False
        for i in reversed(range(len(units))):
            k, w, d, st, nm = units[i]
            if w == 4:
                cand = units[:i] + [(k, 6, d, st, nm)] + units[i + 1:]
                if fits(cand):
                    units, widened, done = cand, widened + 1, True
                    break
        if not done:
            break
    code = {"home": "h", "infill": "i", "civic": "c"}
    grouped = [[f"{code[k]}{w}x{d}" + (f"^{st}" if st else "") + (f":{nm}" if nm else "") for k, w, d, st, nm in b]
               for b in _blocks(units, block_max)]
    print(f"  pack {label}: {n}/{len(specs)} specs + {added} added, {widened} widened, {len(grouped)} blocks")
    return row(label, district, terrace, z, r_front, facing, th0, th1, grouped, storeys)


def along(pts, s, side, w, d, lane_w, kind, name, district, terrace, setback=1.2, storeys=2, block=None, shift=0.0):
    """Building fronting a polyline at arc length s (m), on side +1 (left of travel) / -1 (right)."""
    from shapely.geometry import LineString as _LS
    ln = _LS(pts)
    p, q = ln.interpolate(max(0.0, s - 1.0)), ln.interpolate(min(ln.length, s + 1.0))
    tx, ty = q.x - p.x, q.y - p.y
    tn = math.hypot(tx, ty)
    tx, ty = tx / tn, ty / tn
    nx_, ny_ = -ty * side, tx * side
    c = ln.interpolate(s)
    off = lane_w / 2.0 + setback + d / 2.0
    at = (c.x + nx_ * off + tx * shift, c.y + ny_ * off + ty * shift)
    return bldg(kind, at, w, d, ang((-nx_, -ny_)), name, district, terrace, None, storeys=storeys, block=block)


def row(label, district, terrace, z, r_front, facing, th0, th1, blocks, storeys=(2, 3, 2, 2)):
    """Straight terrace blocks along the ring r_front between angles th0 < th1. facing 'in' = fronts toward O
    (seaward), 'out' = fronts away from O (inland). Units inside a block share party walls (touching); blocks are
    separated by the leftover angle, spread evenly (single blocks are centred)."""
    parsed = [[parse(u) for u in b] for b in blocks]
    info = []
    for b in parsed:
        L = sum(u[1] for u in b)
        dmax = max(u[2] for u in b)
        if facing == "in":
            r_line = r_front
            phi = math.degrees(math.atan((L / 2.0) / r_line))
        else:
            r_line = math.sqrt(r_front ** 2 - (L / 2.0) ** 2)
            phi = math.degrees(math.atan((L / 2.0) / (r_line - dmax)))
        info.append((L, r_line, phi))
    need = sum(2 * i[2] for i in info)
    left = (th1 - th0) - need
    if left < -1e-6:
        print(f"  WARNING row {label}: blocks need {need:.1f} deg, only {th1 - th0:.1f} available")
    else:
        print(f"  row {label}: spare {left:.1f} deg ({math.radians(left) * r_front:.1f} m at r {r_front})")
    gap = left / (len(info) - 1) if len(info) > 1 else 0.0
    cur = th0 if len(info) > 1 else th0 + left / 2.0
    out, k = [], 0
    for bi, (b, (L, r_line, phi)) in enumerate(zip(parsed, info)):
        thc = cur + phi
        n, t = rad(thc), tan_(thc)
        s = -L / 2.0
        for (kind, w, d, st, name) in b:
            sc = s + w / 2.0
            if facing == "in":
                c = (O[0] + n[0] * (r_line + d / 2.0) + t[0] * sc, O[1] + n[1] * (r_line + d / 2.0) + t[1] * sc)
                fd = ang((-n[0], -n[1]))
            else:
                c = (O[0] + n[0] * (r_line - d / 2.0) + t[0] * sc, O[1] + n[1] * (r_line - d / 2.0) + t[1] * sc)
                fd = ang(n)
            out.append(bldg(kind, c, w, d, fd, name, district, terrace, z,
                            storeys=st or storeys[k % len(storeys)], block=f"{label}{bi + 1}",
                            _ring=f"{r_front}:{facing}"))
            s += w
            k += 1
        cur = thc + phi + gap
    return out


# ================================================================================================ TERRAIN
# Terrace heights (m). Every step is a multiple of one kit storey (3 m) so retaining walls are whole
# Wall_UnevenBrick_Straight courses (312 cm incl. coping) and stairs are whole Stairs_Exterior_Straight risers (1 m).
Z_BASIN, Z_QUAY, Z_HEART, Z_UPPER, Z_CROWN, Z_GARDEN = -3.0, 2.0, 5.0, 8.0, 14.0, 8.0

R_QUAY_EDGE, R_HARBOUR_WALL, R_UPPER_WALL, R_CROWN_WALL, R_CROWN_BACK = 26.0, 44.5, 88.0, 112.0, 146.0
TH_QUAY = (-80.0, 58.0)
TH_HEART = (-50.0, 34.0)
TH_GARDEN = (34.0, 64.0)
R_GARDEN_BACK = 72.0
TH_UPPER = (-42.0, 34.0)
TH_CROWN = (-34.0, 14.0)

# Land (coast) polygon: everything inside is land, outside is sea. West shingle beach -> brook mouth -> quay wall
# around the basin -> sea cliffs of Lighthouse Point -> east coast; closed far inland.
coast = ([(-320.0, 74.0), (-200.0, 71.0), (-140.0, 68.0), (-104.0, 66.0), (-80.0, 66.5), (-60.0, 67.0), (-46.0, 68.0),
          (-37.0, 70.5), (-31.0, 70.5), (-24.0, 69.5), P(R_HARBOUR_WALL, TH_QUAY[0])]
         + arc(R_QUAY_EDGE, TH_QUAY[0], TH_QUAY[1], 2.5)
         + [(46.0, 54.0), (48.8, 64.0), (50.6, 76.0), (51.6, 88.0), (53.0, 97.0), (56.5, 104.5), (63.0, 109.0),
            (73.0, 110.5), (85.0, 106.0), (97.0, 99.5), (110.0, 93.0), (132.0, 88.0), (200.0, 86.0), (320.0, 88.0),
            (320.0, -320.0), (-320.0, -320.0)])

terraces = [
    {"name": "quay", "district": "harbour_row", "z": Z_QUAY, "surface": "stone_flags",
     "polygon": poly(sector(R_QUAY_EDGE, R_HARBOUR_WALL, *TH_QUAY)),
     "note": "Quay / Harbour Row. Basin face = quay wall (top 2, foot -3). Back = Harbour Wall (5) and Sakura Wall (8)."},
    {"name": "heart", "district": "heart", "z": Z_HEART, "surface": "cobble",
     "polygon": poly(sector(R_HARBOUR_WALL, R_UPPER_WALL, *TH_HEART)),
     "note": "The Heart: Fountain Square and three concentric rows. West edge blends soft into Brookside."},
    {"name": "garden", "district": "sakura_garden", "z": Z_GARDEN, "surface": "gravel",
     "polygon": poly(sector(R_HARBOUR_WALL, R_GARDEN_BACK, *TH_GARDEN)),
     "note": "Sakura Garden terrace on the lip above the east quay; pavilion on the island axis x = 45."},
    {"name": "upper", "district": "upper_town", "z": Z_UPPER, "surface": "cobble",
     "polygon": poly(sector(R_UPPER_WALL, R_CROWN_WALL, *TH_UPPER)),
     "note": "Upper Town: Balcony Lane with a parapet over the Heart, Well Court, Pilgrim Garden."},
    {"name": "crown", "district": "crown_hill", "z": Z_CROWN, "surface": "grass",
     "polygon": poly(sector(R_CROWN_WALL, R_CROWN_BACK, *TH_CROWN)),
     "edge": "soft", "soft_m": 10.0,
     "note": "Crown Hill: belvedere, church, bell tower, graveyard. Flanks fall as grassy banks (soft 10 m)."},
]


def profile(origin, direction, stops):
    n = math.hypot(*direction)
    return {"origin": list(origin), "dir": [round(direction[0] / n, 5), round(direction[1] / n, 5)],
            "stops": [list(s) for s in stops]}


def prof_z(pr, p):
    s = (p[0] - pr["origin"][0]) * pr["dir"][0] + (p[1] - pr["origin"][1]) * pr["dir"][1]
    st = pr["stops"]
    if s <= st[0][0]:
        return st[0][1]
    for (s0, z0), (s1, z1) in zip(st, st[1:]):
        if s <= s1:
            return z0 + (z1 - z0) * (s - s0) / (s1 - s0)
    return st[-1][1]


BROOK_PROFILE = profile((0.0, 66.0), (0.0, -1.0), [(0, 1.2), (6, 2.0), (24, 5.0), (46, 8.0), (88, 14.0), (180, 18.0)])
ORCHARD_PROFILE = profile((35.0, 0.0), (0.55, -0.835), [(0, 8.0), (40, 8.5), (100, 10.0), (170, 11.0)])
HEADLAND_PROFILE = profile((0.0, 30.0), (0.0, 1.0), [(-10, 8.0), (0, 8.0), (25, 9.5), (50, 12.5), (62, 14.0), (95, 14.0)])

slope_raw = {
    "brookside": [(-130.0, -118.0), (-92.0, -118.0), P(R_CROWN_BACK, TH_CROWN[0]), P(R_CROWN_WALL, TH_CROWN[0]),
                  P(R_CROWN_WALL, TH_UPPER[0]), P(R_UPPER_WALL, TH_UPPER[0]), P(R_UPPER_WALL, TH_HEART[0]),
                  P(R_HARBOUR_WALL, TH_HEART[0]), P(R_HARBOUR_WALL, TH_QUAY[0]), (-130.0, 72.0)],
    "orchard": [P(R_CROWN_BACK, TH_CROWN[1]), P(R_CROWN_WALL, TH_CROWN[1]), P(R_CROWN_WALL, TH_UPPER[1]),
                P(R_GARDEN_BACK, TH_GARDEN[0]), P(R_GARDEN_BACK, TH_GARDEN[1]), (130.0, 21.0), (130.0, -118.0),
                (20.0, -118.0)],
    "headland": [P(R_QUAY_EDGE, TH_QUAY[1]), P(R_HARBOUR_WALL, TH_QUAY[1]), P(R_HARBOUR_WALL, TH_GARDEN[1]),
                 P(R_GARDEN_BACK, TH_GARDEN[1]), (130.0, 21.0), (130.0, 120.0), (46.0, 120.0)],
}
SLOPE_META = {
    "brookside": ("brookside", BROOK_PROFILE, "grass", "Brook valley: tilted plane falling to the shingle beach, the Morrow Brook cut 1.2 m into it."),
    "orchard": ("orchard_upland", ORCHARD_PROFILE, "grass", "Orchard Upland: apple rows, walled fields, windmill knoll (mound)."),
    "headland": ("lighthouse_point", HEADLAND_PROFILE, "grass", "Lighthouse Point: ridge rising south to 14, sea cliffs on three sides."),
}

# ================================================================================================ STREETS
lanes = []


def lane(name, width, pts, kind="lane", surface="cobble", z="terrain", dead_end_ok=False, note=None):
    rec = {"name": name, "width": width, "kind": kind, "surface": surface, "z": z,
           "points": [rd(p, 2) for p in pts]}
    if dead_end_ok:
        rec["dead_end_ok"] = True
    if note:
        rec["note"] = note
    lanes.append(rec)
    return rec


# Radial connectors (opes / stairs) - angles.
TH_CAT, TH_NET, TH_CHAPEL = -31.0, 31.5, -31.0
W_OPE = 2.4

# --- Quay ---
lane("quay_promenade", 7.0, arc(29.5, TH_QUAY[0] + 1.5, 54.3, 3.0), kind="quay", surface="stone_flags", z=Z_QUAY,
     note="Main street 1: the curved quay, Harbour Row fronts on its inland side, bollards on the basin side.")
lane("tide_alley", 2.3, arc(43.2, TH_CAT, TH_NET, 3.0), kind="alley", z=Z_QUAY,
     note="Back alley under the Harbour Wall, between Cat Steps and Net Stairs (murder corridor, one lamp per end).")
lane("cat_ope_quay", W_OPE, [P(32.5, TH_CAT), P(41.3, TH_CAT)], kind="ope", z=Z_QUAY)
lane("net_ope_quay", W_OPE, [P(32.5, TH_NET), P(41.3, TH_NET)], kind="ope", z=Z_QUAY)
# --- Heart ---
lane("rope_walk", 4.0, arc(57.0, -9.8, TH_HEART[0] + 0.5, 3.0), kind="main", z=Z_HEART,
     note="Main street 2 (west): ring street at r 57, Lip Row fronts on the sea side, Mid Row on the inland side.")
lane("market_street", 4.0, arc(57.0, 9.8, 29.6, 3.0), kind="main", z=Z_HEART,
     note="Main street 2 (east): ring street to the Garden Stair.")
lane("back_lane_west", 3.0, arc(72.5, -8.9, -47.2, 3.0), kind="lane", z=Z_HEART)
lane("back_lane_east", 3.0, arc(72.5, 8.9, 30.5, 3.0), kind="lane", z=Z_HEART)
lane("cat_ope_heart", W_OPE, [P(47.4, TH_CAT), P(55.5, TH_CAT)], kind="ope", z=Z_HEART)
lane("net_ope_heart", W_OPE, [P(47.4, TH_NET), P(55.5, TH_NET)], kind="ope", z=Z_HEART)
lane("chapel_wynd", W_OPE, [P(58.5, TH_CHAPEL), P(71.5, TH_CHAPEL)], kind="ope", z=Z_HEART)
lane("chapel_wynd_upper", W_OPE, [P(73.5, TH_CHAPEL), P(82.6, TH_CHAPEL)], kind="ope", z=Z_HEART)
# --- Upper ---
lane("balcony_lane", 3.6, arc(91.0, TH_UPPER[0] - 4.0, TH_UPPER[1], 3.0), kind="main", z=Z_UPPER,
     note="Main street 3: contour lane at r 91 with a parapet on its seaward side over the Heart.")
# --- Crown ---
lane("crown_walk", 3.0, arc(124.5, -32.0, 11.0, 3.0), kind="lane", surface="gravel", z=Z_CROWN)
lane("graveyard_path", 2.0, [A(-26.0, 122.4), A(-26.0, 137.6)], kind="path", surface="gravel", z=Z_CROWN,
     dead_end_ok=True, note="Between the grave rows to the mausoleum (future catacomb stair).")
# --- Brookside (terrain-following lanes) ---
BROOK_PATH = [P(57.0, TH_HEART[0] + 0.5), (-33.0, 50.0), (-35.5, 57.5), (-30.0, 64.0), (-19.5, 66.6), P(29.5, TH_QUAY[0] + 1.5)]
lane("brook_path", 3.0, BROOK_PATH, kind="lane", surface="dirt",
     note="Rope Walk -> down the brook's east bank -> west quay (5 -> 2 on the valley slope, 1:8).")
MILL_LANE = [(-35.5, 57.5), (-42.3, 62.2), (-48.0, 63.4), (-62.0, 60.5), (-76.0, 52.0), (-85.0, 38.0), (-89.0, 22.0),
             (-89.5, 14.5)]
lane("mill_lane", 3.0, MILL_LANE, kind="lane", surface="dirt",
     note="West-bank lane from the Beach Bridge up to the smithy yard.")
lane("mill_bridge_way", 4.0, [P(91.0, TH_UPPER[0] - 4.0), (-63.0, 18.2), (-80.5, 16.5), (-89.5, 14.5)], kind="main",
     note="Balcony Lane continues west over the Old Stone Bridge to the smithy yard.")
WOODS = [(-89.5, 14.5), (-97.0, -4.0), (-104.0, -18.0), (-106.0, -36.0), (-102.0, -48.0), (-92.0, -52.0), (-80.0, -44.0)]
lane("woods_path", 2.6, WOODS, kind="path", surface="dirt", note="Smithy -> woodcutter clearing -> mill pond -> Hollow Way.")
lane("hollow_way", 2.8, [(-80.0, -44.0), (-72.0, -36.0), P(126.5, -32.0)], kind="path", surface="dirt",
     note="Sunken path from the woods to the top of the Graveyard Ramp (crown west gate).")
# --- Orchard / farm ---
ORCH_LANE = [P(91.0, TH_UPPER[1]), (66.0, -26.0), (74.0, -36.0), (80.0, -47.0), (86.5, -55.5), (90.5, -60.5)]
lane("orchard_lane", 3.6, ORCH_LANE, kind="main", surface="dirt",
     note="Balcony Lane continues NE over the upland to the windmill knoll.")
lane("mill_track", 3.0, [(90.5, -60.5), (84.0, -69.0), (72.0, -70.5), (63.0, -70.5)], kind="lane", surface="dirt",
     note="Windmill -> pen -> barn -> farmyard.")
lane("farm_track", 3.0, [(70.5, -31.0), (64.0, -42.0), (58.5, -56.0), (57.0, -63.0)], kind="lane", surface="dirt")
lane("field_lane", 3.0, [(59.0, -77.0), (61.5, -88.0), (61.5, -104.0)], kind="path", surface="dirt", dead_end_ok=True,
     note="Between the walled fields (field gates).")
lane("orchard_walk", 2.6, [(63.5, 5.8), (72.0, -4.0), (76.0, -16.0), (71.0, -28.0)], kind="path", surface="dirt",
     note="Top of the Orchard Ramp through the apple rows to Orchard Lane.")
# --- Garden / headland ---
lane("sakura_walk", 2.6, [(48.4, 13.9), (54.0, 13.2), (60.0, 16.0), (64.5, 21.5), (69.0, 30.5)], kind="path",
     surface="gravel", note="Garden Stair top -> through the Sakura Garden -> Headland Road.")
lane("koi_path", 2.2, [P(45.4, 54.3), (57.0, 25.0), (60.0, 16.0)], kind="path", surface="gravel",
     note="Cliff Stair top -> Sakura Walk.")
HEAD_RD = [(69.0, 30.5), (73.0, 44.0), (73.5, 58.0), (71.5, 72.0), (69.0, 84.0), (68.5, 90.2)]
lane("headland_road", 3.0, HEAD_RD, kind="lane", surface="dirt", dead_end_ok=True,
     note="Ridge road to the lighthouse: the long lonely trip.")
lane("orchard_link", 2.6, [(69.0, 30.5), (70.0, 22.5), (66.2, 12.5), (64.2, 7.5)], kind="path", surface="dirt",
     note="Headland Road <-> top of the Orchard Ramp (closes the east loop).")
# --- Sea ---
JETTY_ROOT, JETTY_END = P(R_QUAY_EDGE + 0.2, 0.0), P(-10.0, 0.0)
lane("long_jetty", 3.0, [P(24.0, 0.0), JETTY_END], kind="pier", surface="timber", z=1.2, dead_end_ok=True,
     note="On the axis. Pirate pier / JettySection deck at 1.2, ramp up to the quay at the root.")
lane("jetty_head", 3.0, [A(-6.5, -10.0), A(6.5, -10.0)], kind="pier", surface="timber", z=1.2, dead_end_ok=True)
lane("fish_pier", 3.0, [P(R_QUAY_EDGE + 0.2, 44.0), P(15.0, 44.0)], kind="pier", surface="timber", z=1.2, dead_end_ok=True)
MOLE_PTS = arc(35.5, -82.0, -160.0, 6.0)
lane("mole_walk", 3.5, MOLE_PTS, kind="mole", surface="stone_flags", z=2.5, dead_end_ok=True,
     note="Breakwater walk to the harbour light (exposed dead end on purpose).")

# Squares / yards (walkable areas).
SQ = [A(-9.0, 45.6), A(9.0, 45.6), A(10.0, 54.6), A(11.2, 59.4), A(11.6, 70.6), A(11.2, 76.4), A(-11.2, 76.4),
      A(-11.6, 70.6), A(-11.2, 59.4), A(-10.0, 54.6)]
squares = [
    {"name": "fountain_square", "district": "heart", "z": Z_HEART, "surface": "cobble_fan", "polygon": poly(SQ),
     "note": "The heart: ~22 x 31 m, enclosed on four sides; the south edge is the balustrade over the Grand Stair."},
    {"name": "well_court", "district": "upper_town", "z": Z_UPPER, "surface": "cobble",
     "polygon": poly(sector(92.6, 104.0, -26.5, -18.0))},
    {"name": "pilgrim_garden", "district": "upper_town", "z": Z_UPPER, "surface": "gravel",
     "polygon": poly([A(-8.0, 92.6), A(8.0, 92.6), A(8.0, 101.8), A(-8.0, 101.8)])},
    {"name": "belvedere", "district": "crown_hill", "z": Z_CROWN, "surface": "stone_flags",
     "polygon": poly([A(-11.0, 112.4), A(11.0, 112.4), A(11.0, 122.8), A(-11.0, 122.8)])},
    {"name": "west_quay_apron", "district": "harbour_row", "z": Z_QUAY, "surface": "stone_flags",
     "polygon": poly([P(26.5, -80.0), P(33.5, -80.0), P(33.5, -73.0), P(26.5, -73.0)])},
]

# ================================================================================================ STAIRS / RAMPS
stairs = []


def stair(name, frm, to, width, z0, z1, landings, note=None):
    rec = {"name": name, "from": rd(frm), "to": rd(to), "width": width, "z0": z0, "z1": z1,
           "risers": int(round(abs(z1 - z0))), "landings": landings, "kit": "Stairs_Exterior_Straight (+_L/_R edges)",
           "collision": "hidden ramp box (NavMesh + physics)"}
    if note:
        rec["note"] = note
    stairs.append(rec)


stair("grand_stair", A(0.0, 46.6), A(0.0, 34.0), 8.0, Z_HEART, Z_QUAY, 2,
      "THE window: square balustrade -> 3 flights / 2 landings -> quay, flanked by the two gatehouses.")
stair("scala", A(0.0, 77.6), A(0.0, 90.0), 6.0, Z_HEART, Z_UPPER, 2,
      "Square -> Balcony Lane between the Town Hall and the Clock Tower, on the axis.")
stair("pilgrim_stair", A(0.0, 101.8), A(0.0, 119.6), 4.0, Z_UPPER, Z_CROWN, 2,
      "Pilgrim Garden -> Belvedere. The last flight tops out on the panorama.")
stair("cat_steps", P(47.6, TH_CAT), P(41.2, TH_CAT), 2.0, Z_HEART, Z_QUAY, 0, "Stepped ope through the Harbour Wall.")
stair("net_stairs", P(47.6, TH_NET), P(41.2, TH_NET), 2.0, Z_HEART, Z_QUAY, 0, "Stepped ope beside the koi cascade.")
stair("chapel_steps", P(82.6, TH_CHAPEL), P(89.4, TH_CHAPEL), 2.0, Z_HEART, Z_UPPER, 0, "Stepped ope up the Upper Wall.")
stair("garden_stair", P(57.0, 29.6), P(57.0, 37.5), 4.0, Z_HEART, Z_GARDEN, 1, "Market Street -> Sakura Garden (vermilion rails).")
stair("cliff_stair", P(29.8, 54.3), P(45.4, 54.3), 2.0, Z_QUAY, Z_GARDEN, 1, "East quay -> garden lip, cut into the cliff; rope rail.")

ramps = []


def ramp(name, pts, width, z0, z1, note=None):
    L = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))
    rec = {"name": name, "points": [rd(p) for p in pts], "width": width, "z0": z0, "z1": z1,
           "grade": f"1:{L / max(1e-6, abs(z1 - z0)):.1f}"}
    if note:
        rec["note"] = note
    ramps.append(rec)


ramp("well_ramp", arc(72.5, -47.2, -50.2, 1.0) + [P(80.0, -50.8), P(88.0, -49.6), P(91.0, TH_UPPER[0] - 4.0)], 3.4,
     Z_HEART, Z_UPPER, "Cart ramp Heart -> Upper Town past the NW corner (embankment, kit wall on the low side).")
ramp("orchard_ramp", arc(72.5, 30.5, 46.5, 2.0), 3.0, Z_HEART, Z_UPPER,
     "Cart ramp Heart -> Orchard Upland in a cutting behind the garden (kit walls on both sides).")
ramp("graveyard_ramp", [P(91.0, TH_UPPER[0] - 4.0), (-57.0, 8.0), (-61.0, -6.0), (-64.0, -26.5), P(126.5, -32.0)],
     3.4, Z_UPPER, Z_CROWN, "Upper Town west end -> Crown Hill west gate, up the valley side.")
ramp("hilltop_track", [(57.0, -66.0), (46.0, -64.5), (36.0, -63.0), P(124.5, 11.0)], 3.0, 9.3, Z_CROWN,
     "Farmyard -> Crown Hill east end (closes the north loop).")
ramp("jetty_ramp", [P(29.0, 0.0), P(24.0, 0.0)], 3.0, Z_QUAY, 1.2, "Timber ramp from the quay down to the jetty deck.")
ramp("fish_pier_ramp", [P(31.0, 44.0), P(R_QUAY_EDGE + 0.2, 44.0)], 3.0, Z_QUAY, 1.2)

# ================================================================================================ WALLS
walls = []


def wall(name, pts, top, bottom, parapet=False, note=None):
    if sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:])) < 2.0:
        return   # sliver between two cuts (e.g. a stair next to the terrace end)
    rec = {"name": name, "points": [rd(p) for p in pts], "top_z": top, "bottom_z": bottom,
           "courses": int(round((top - bottom) / 3.0)) if bottom > -1 else None,
           "kit": "Wall_UnevenBrick_Straight per 2 m (+ Corner_Exterior_Brick pilaster at kinks)", "parapet": parapet}
    if note:
        rec["note"] = note
    walls.append(rec)


def cut(r, th0, th1, gaps):
    """Split an arc at radius r between th0..th1, leaving gaps [(th, half_width_m)]."""
    segs, cur = [], th0
    for th, hw in sorted(gaps):
        dth = math.degrees(math.asin(min(0.99, hw / r)))
        if th - dth > cur:
            segs.append((cur, th - dth))
        cur = max(cur, th + dth)
    if cur < th1:
        segs.append((cur, th1))
    return segs


for i, (a, b) in enumerate(cut(R_HARBOUR_WALL, TH_HEART[0], TH_HEART[1], [(0.0, 4.4), (TH_CAT, 1.4), (TH_NET, 1.4)])):
    wall(f"harbour_wall_{i + 1}", arc(R_HARBOUR_WALL, a, b, 2.0), Z_HEART, Z_QUAY, parapet=True,
         note="Heart / quay. Parapet = balustrade (kit Prop_ExteriorBorder / new Balustrade prop).")
for i, (a, b) in enumerate(cut(R_HARBOUR_WALL, TH_GARDEN[0], TH_QUAY[1], [(54.3, 1.9)])):
    wall(f"sakura_wall_{i + 1}", arc(R_HARBOUR_WALL, a, b, 2.0), Z_GARDEN, Z_QUAY, parapet=True,
         note="Garden lip over the east quay: 2 courses; the koi cascade falls down it at x 45.")
for i, (r0, r1) in enumerate([(R_HARBOUR_WALL, 55.0), (59.0, 71.0), (74.0, R_UPPER_WALL)]):
    wall(f"garden_west_wall_{i + 1}", [P(r0, TH_HEART[1]), P(r1, TH_HEART[1])], Z_UPPER, Z_HEART,
         note="Heart / garden + orchard (3 m), interrupted by the Garden Stair and the Orchard Ramp.")
for i, (a, b) in enumerate(cut(R_UPPER_WALL, TH_UPPER[0], TH_UPPER[1], [(0.0, 3.6), (TH_CHAPEL, 1.4)])):
    wall(f"upper_wall_{i + 1}", arc(R_UPPER_WALL, a, b, 2.0), Z_UPPER, Z_HEART, parapet=True,
         note="Heart / Upper Town. Parapet on Balcony Lane.")
for i, (a, b) in enumerate(cut(R_CROWN_WALL, TH_CROWN[0], TH_CROWN[1], [(0.0, 2.4)])):
    wall(f"crown_wall_{i + 1}", arc(R_CROWN_WALL, a, b, 2.0), Z_CROWN, Z_UPPER, parapet=True,
         note="Upper Town / Crown: 2 courses, the Belvedere parapet on top.")
wall("upper_east_wall", [P(R_UPPER_WALL, TH_UPPER[1]), P(R_CROWN_WALL, TH_UPPER[1])], Z_UPPER, Z_UPPER,
     note="Marker only (upper and orchard meet level): low garden wall, 1 m, kit fence.")
walls[-1]["courses"] = 0

quay = {"edge": [rd(p) for p in arc(R_QUAY_EDGE, TH_QUAY[0], TH_QUAY[1], 2.0)], "top_z": Z_QUAY, "bottom_z": Z_BASIN,
        "kit": "QuayWall prop (new) or Wall_UnevenBrick_Straight x2 courses sunk to -3; Bollard every 8 m, steps every 30 m",
        "water_steps": [rd(P(R_QUAY_EDGE, -40.0)), rd(P(R_QUAY_EDGE, 25.0))]}
basin = {"center": rd(O), "radius": 30.0, "floor_z": Z_BASIN, "calm_mask": True,
         "note": "Dredged round basin; ocean waves scaled x0.25 inside the mole (wave mask polygon = water inside coast hull)."}
mole = {"points": [rd(p) for p in MOLE_PTS], "width": 7.0, "walk_width": 3.5, "top_z": 2.5,
        "kit": "rock armour (Rock_Medium_*, SeaStack) on the outer face, stone flags on top",
        "light": rd(MOLE_PTS[-1])}
cliffs = [
    {"name": "basin_east_cliff", "points": [rd(p) for p in [P(R_QUAY_EDGE, TH_QUAY[1]), (46.0, 54.0), (48.8, 64.0), (50.6, 76.0),
                                                                (51.6, 88.0), (53.0, 97.0)]],
     "top_z": "terrain", "bottom_z": -3.0, "material": "rock",
     # SPRINT-022 acceptance 6 (user): the face seen from the basin is lowered. The land side drops to a ragged rock lip
     # at ~5 m, a grassy bench at ~7.5 m, then climbs back to the headland (Tools/Blender/kg_build_terrain_v2.py).
     "shoulder": {"edge_z": 5.0, "ledge_z": 7.5, "ledge_at_m": 4.0, "bench_to_m": 6.5, "width_m": 13.0, "fade_m": 9.0, "fade_end_m": 4.0,
                  "wobble_m": 0.9}},
    {"name": "point_sea_cliff", "points": [[53.0, 97.0], [56.5, 104.5], [63.0, 109.0], [73.0, 110.5], [85.0, 106.0],
                                           [97.0, 99.5], [110.0, 93.0]], "top_z": "terrain", "bottom_z": -4.0, "material": "rock"},
    {"name": "fish_market_cliff", "points": [rd(P(R_QUAY_EDGE, TH_QUAY[1])), rd(P(R_HARBOUR_WALL, TH_QUAY[1]))],
     "top_z": "terrain", "bottom_z": Z_QUAY, "material": "rock"},
]

# ================================================================================================ WATER
BROOK = [(-104.0, -96.0), (-101.0, -70.0), (-99.5, -56.0), (-94.0, -32.0), (-83.0, -6.0), (-75.0, 14.0), (-68.0, 33.0),
         (-56.0, 50.0), (-42.5, 62.0), (-34.0, 71.5)]
_bed, _last = [], 99.0
for p in BROOK:
    zb = min(_last - 0.3, prof_z(BROOK_PROFILE, p) - 1.2)
    _bed.append([p[0], p[1], round(max(-0.5, zb), 2)])
    _last = zb
stream = {"name": "morrow_brook", "points": _bed, "width": 3.2, "depth": 0.5, "bank_cut": 1.2,
          "material": "M_KG_PondWater (ribbon mesh, flow along points)",
          "ponds": [{"name": "mill_pond", "center": [-99.0, -60.0], "radius": 6.5, "z": 14.8}],
          "cascades": [{"at": [-75.0, 14.0], "drop": 0.8, "note": "weir under the Old Stone Bridge"},
                       {"at": [-56.0, 50.0], "drop": 1.0}],
          "note": "Wadeable (60% speed). Point z = bed height. Bots cross at the bridges and the ford."}
koi = {"name": "koi_spill", "points": [[45.0, 20.0], [45.0, 27.0], [45.0, 33.0]], "width": 0.8,
       "note": "Koi pond overflow runs under the pavilion and falls 6 m down the Sakura Wall onto the quay (splash pool)."}

bridges = [
    {"name": "old_stone_bridge", "at": [-75.4, 16.8], "along": [-1.0, 0.1], "span": 7.0, "width": 4.0, "deck_z": 8.4,
     "kit": "stone arch (new prop) or Floor_Brick deck + Wall_Arch sides", "lane": "mill_bridge_way"},
    {"name": "beach_bridge", "at": [-42.3, 62.2], "along": [-1.0, 0.16], "span": 5.2, "width": 2.5, "deck_z": 1.9,
     "kit": "SM_KG_WoodBridge (KG_DressWilds)", "lane": "mill_lane"},
    {"name": "mill_bridge", "at": [-100.5, -47.0], "along": [0.98, -0.2], "span": 5.2, "width": 2.5, "deck_z": 15.4,
     "kit": "SM_KG_WoodBridge", "lane": "woods_path"},
    {"name": "ford_stones", "at": [-68.0, 33.0], "along": [-0.95, 0.3], "span": 4.0, "width": 1.6, "deck_z": 5.2,
     "kit": "SM_KG_SteppingStone x6", "lane": None},
]

# ================================================================================================ BUILDINGS
Q, H, U = "quay", "heart", "upper"
RF_HR, RF_LIP, RF_MID, RF_BACK, RF_UP, RF_CR = 34.1, 53.9, 60.1, 75.1, 93.9, 127.1
# ---- Harbour Row (quay, fronts on the promenade, pastel stucco) ----
TH_BOAT = -65.0
bldg("civic", P(RF_HR + 4.0, TH_BOAT), 6, 8, ang(rad(180.0 + TH_BOAT)), "Boathouse + slipway", "harbour_row", Q, Z_QUAY, 2,
     legacy="boathouse")
pack("HW", "harbour_row", Q, Z_QUAY, RF_HR, "in", TH_BOAT + 6.6, TH_CAT - clear(W_OPE, RF_HR),
     ["h4x6", "i4x6", "h6x6^3", "i4x6", "h4x6", "i6x6"], block_max=16.0)
pack("HG", "harbour_row", Q, Z_QUAY, RF_HR, "in", TH_CAT + clear(W_OPE, RF_HR), -clear(8.0, RF_HR, 1.2),
     ["h4x6", "i6x6^3:West Gatehouse", "i4x6"], block_max=16.0)
pack("HE", "harbour_row", Q, Z_QUAY, RF_HR, "in", clear(8.0, RF_HR, 1.2), TH_NET - clear(W_OPE, RF_HR),
     ["i6x6^3:East Gatehouse", "i4x6", "i4x6"], block_max=16.0)
bldg("civic", P(RF_HR + 3.0, 43.0), 8, 6, ang(rad(180.0 + 43.0)), "Fish Market Hall (open arcade)", "harbour_row", Q,
     Z_QUAY, 1, open=True, legacy="fish_market", top_z=8.5)

# ---- The Heart: square frame ----
bldg("civic", A(-15.8, 63.8), 6, 6, ang(E), "Bakery", "heart", H, Z_HEART, 2, legacy="bakery")
bldg("civic", A(16.8, 63.8), 6, 8, ang((-E[0], -E[1])), "Inn - The Latecomer", "heart", H, Z_HEART, 3, legacy="inn")
bldg("civic", A(-10.5, 81.6), 8, 8, ang((-DIN[0], -DIN[1])), "Town Hall", "heart", H, Z_HEART, 3, legacy="town_hall")
bldg("tower", A(6.8, 81.2), 4, 4, ang((-DIN[0], -DIN[1])), "Clock Tower", "heart", H, Z_HEART, 4, legacy="clock_tower",
     ladder=True, top_z=Z_HEART + 4 * 3 + 7.5)
bldg("infill", A(13.6, 82.2), 6, 6, ang((-DIN[0], -DIN[1])), "Guild House", "heart", H, Z_HEART, 3)
# Lip Row: fronts face inland onto Rope Walk / Market Street, backs = hanging gardens over the Harbour Wall.
pack("LW", "heart", H, Z_HEART, RF_LIP, "out", TH_HEART[0] + 1.0, TH_CAT - clear(W_OPE, RF_LIP - 6.0),
     ["i4x6", "h6x6", "i4x6", "i4x6", "i6x6", "i4x6"])
pack("LC", "heart", H, Z_HEART, RF_LIP, "out", TH_CAT + clear(W_OPE, RF_LIP - 6.0), -13.4,
     ["h6x6^3", "i4x6^3", "i4x6^3", "i4x6"])
pack("LE", "heart", H, Z_HEART, RF_LIP, "out", 13.4, TH_NET - clear(W_OPE, RF_LIP - 6.0),
     ["i4x6^3", "h6x6^3", "i4x6", "i4x6"])
# Mid Row: fronts face the sea onto Rope Walk / Market Street, back yards behind.
pack("MW", "heart", H, Z_HEART, RF_MID, "in", TH_HEART[0] + 1.5, TH_CHAPEL - clear(W_OPE, RF_MID),
     ["i4x8", "h6x8", "h4x8", "i6x8", "i4x8", "i4x8"])
pack("MC", "heart", H, Z_HEART, RF_MID, "in", TH_CHAPEL + clear(W_OPE, RF_MID), -19.8, ["i4x8", "i4x8", "i4x6"])
pack("ME", "heart", H, Z_HEART, RF_MID, "in", 21.6, TH_HEART[1] - 1.5, ["h6x8", "i4x8", "i4x8"])
# Back Row: fronts face the sea onto Back Lane, yards up to the Upper Wall.
pack("BW", "heart", H, Z_HEART, RF_BACK, "in", -44.3, TH_CHAPEL - clear(W_OPE, RF_BACK),
     ["i4x8", "h6x8", "i4x8", "i4x8", "i6x8"])
pack("BC", "heart", H, Z_HEART, RF_BACK, "in", TH_CHAPEL + clear(W_OPE, RF_BACK), -12.3,
     ["i6x8", "h6x8^3", "i4x8", "i6x8", "i4x8"])
pack("BE", "heart", H, Z_HEART, RF_BACK, "in", 13.9, TH_HEART[1] - 1.3, ["i4x8", "h6x8", "i6x8^3", "i4x8", "i4x8"])

# ---- Upper Town (fronts face the sea onto Balcony Lane, gardens up to the Crown Wall) ----
pack("UW", "upper_town", U, Z_UPPER, RF_UP, "in", TH_UPPER[0] + 3.2, -27.4, ["i6x8", "h6x8", "i4x8", "i4x8"])
pack("UC", "upper_town", U, Z_UPPER, RF_UP, "in", -17.1, -5.9, ["h6x8", "i6x8", "i4x8", "i4x8"])
pack("UE", "upper_town", U, Z_UPPER, RF_UP, "in", 5.9, TH_UPPER[1] - 0.9,
     ["i6x8", "h6x8", "i4x8", "i6x8", "h6x8", "i4x8", "i6x8", "i4x8", "i6x8", "i4x8"])

# ---- Crown Hill ----
bldg("civic", A(-9.5, 134.0), 8, 12, ang((-DIN[0], -DIN[1])), "Church of the Morrow", "crown_hill", "crown", Z_CROWN, 2,
     legacy="church")
bldg("tower", A(0.0, 131.0), 4, 4, ang((-DIN[0], -DIN[1])), "Bell Tower", "crown_hill", "crown", Z_CROWN, 4,
     legacy="bell_tower", ladder=True, top_z=Z_CROWN + 4 * 3 + 7.5)
bldg("special", A(-26.0, 141.0), 4, 4, ang((-DIN[0], -DIN[1])), "Mausoleum (future catacombs)", "crown_hill", "crown",
     Z_CROWN, 1, legacy="mausoleum", prop="SM_KG_Mausoleum")
pack("CR", "crown_hill", "crown", Z_CROWN, RF_CR, "in", 3.6, TH_CROWN[1] - 3.2, ["h6x6:Sexton's House", "i6x6:Parsonage"])

# ---- Brookside (along its lanes) ----
# SPRINT-022: the two Brookside homes are gone (homes 20 -> 18): the Brook Path home and the 4 m Tannery shell are
# merged into one 6 m Tannery workshop on the home's plot; the lone Mill Lane home is removed.
along(BROOK_PATH, 5.5, +1, 6, 6, 3.0, "infill", "Tannery", "brookside", "brookside")
along(MILL_LANE, 49.0, +1, 4, 6, 3.0, "infill", "Dyer's shed", "brookside", "brookside", storeys=1)
bldg("civic", (-97.8, 14.2), 4, 6, 0.0, "Smithy (open forge)", "brookside", "brookside", None, 1, legacy="smithy", open=True)
along(WOODS, 12.0, -1, 4, 4, 2.6, "infill", "Woodcutter's hut", "brookside", "brookside", storeys=1)
along(WOODS, 50.0, -1, 6, 6, 2.6, "infill", "Watermill", "brookside", "brookside")

# ---- Orchard upland / farm ----
bldg("home", (46.0, -71.0), 6, 8, 0.0, "Farmhouse", "orchard_upland", "orchard", None, 2)
bldg("civic", (68.5, -77.5), 8, 8, 180.0, "Barn", "orchard_upland", "orchard", None, 2, legacy="mill_barn")
along(ORCH_LANE, 30.0, -1, 6, 6, 3.6, "infill", "Orchard cottage", "orchard_upland", "orchard")
bldg("special", (97.0, -62.8), 8, 6, 162.0, "Windmill", "orchard_upland", "orchard", None, 1, legacy="windmill",
     prop="SM_KG_Windmill_Body + Windmill_Sails (mover)")

# ---- Garden / headland ----
bldg("special", (45.0, 21.2), 4, 4, 90.0, "Garden pavilion (on the island axis)", "sakura_garden", "garden", Z_GARDEN, 1,
     legacy="pavilion", open=True)
bldg("tower", (68.0, 95.0), 4, 4, -60.0, "Lighthouse", "lighthouse_point", "headland", 14.0, 5, legacy="lighthouse",
     ladder=True, top_z=14.0 + 5 * 3 + 7.5)
along(HEAD_RD, 50.0, +1, 4, 6, 3.0, "infill", "Keeper's hut", "lighthouse_point", "headland", storeys=1)

# SPRINT-022 ("you can reduce the number of houses very slightly"): the three weakest infill shells go, the lone
# single-unit blocks at the ends of the Back Row and the Upper Town (their neighbours keep the street wall).
S22_DROP_BLOCKS = {"BC2", "BE2", "UE3"}
BUILD[:] = [b for b in BUILD if b.get("block") not in S22_DROP_BLOCKS]

# ================================================================================================ STREET JOINS
# Close the street wall between consecutive blocks of one ring: a 2.4 m garden wall with a gate where nothing
# passes, a first-floor arch (sottoportego, kit Wall_Arch + Floor + roof strip) where an ope or lane runs through.
from shapely.geometry import LineString as _LS, Polygon as _Poly  # noqa: E402


def polar(p):
    dx, dy = p[0] - O[0], p[1] - O[1]
    return math.hypot(dx, dy), math.degrees(math.atan2(dx * E[0] + dy * E[1], dx * DIN[0] + dy * DIN[1]))


def corners(b):
    w, d = b["size"]
    yaw = math.radians(b["face_deg"] + 90.0)
    ux, uy = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))
    cx, cy = b["at"]
    front = [(cx + sx * w / 2 * ux[0] - d / 2 * uy[0], cy + sx * w / 2 * ux[1] - d / 2 * uy[1]) for sx in (-1, 1)]
    return front


walk_geo = [(l["name"], l["width"], _LS(l["points"]).buffer(l["width"] / 2.0, cap_style=2)) for l in lanes]
walk_geo += [(s["name"], s["width"], _LS([s["from"], s["to"]]).buffer(s["width"] / 2.0, cap_style=2)) for s in stairs]
walk_geo += [(r["name"], r["width"], _LS(r["points"]).buffer(r["width"] / 2.0, cap_style=2)) for r in ramps]
street_joins = []
rings = {}
for b in BUILD:
    if b.get("_ring"):
        rings.setdefault(b["_ring"], {}).setdefault(b["block"], []).append(b)
for ring, blocks in rings.items():
    ends = []
    for blk, units in blocks.items():
        fc = [c for u in units for c in corners(u)]
        fc.sort(key=lambda c: polar(c)[1])
        ends.append((polar(fc[0])[1], fc[0], fc[-1], blk))
    ends.sort()
    for (t1, a1, b1, k1), (t2, a2, b2, k2) in zip(ends, ends[1:]):
        seg = _LS([b1, a2])
        if seg.length > 9.5 or seg.length < 0.5:
            continue
        crossing = [(n, w) for n, w, g in walk_geo if g.intersects(seg.buffer(0.05))]
        if any(w > 4.5 for n, w in crossing):
            continue
        kind = "arch" if crossing else "garden_wall"
        street_joins.append({"kind": kind, "points": [rd(b1), rd(a2)], "between": [k1, k2],
                             "over": [n for n, w in crossing] or None,
                             "kit": ("Wall_Arch x ceil(span/2) at 3.0 m + Floor_WoodDark deck + Roof_RoundTile_2x1_Long"
                                     if kind == "arch" else "Wall_UnevenBrick_Straight scaled z 0.75 + Door_1_Flat gate")})
print(f"  street joins: {sum(j['kind'] == 'arch' for j in street_joins)} arches, "
      f"{sum(j['kind'] == 'garden_wall' for j in street_joins)} garden walls")
for b in BUILD:
    b.pop("_ring", None)

# ================================================================================================ ids, z, homes
def terrace_z_for(b):
    if b["z"] is not None:
        return b["z"]
    pr = {"brookside": BROOK_PROFILE, "orchard": ORCHARD_PROFILE, "headland": HEADLAND_PROFILE}[b["terrace"]]
    # Pad at front-door level (the door side decides), plinth on the downhill side.
    f = math.radians(b["face_deg"])
    door = (b["at"][0] + math.cos(f) * b["size"][1] / 2.0, b["at"][1] + math.sin(f) * b["size"][1] / 2.0)
    return round(prof_z(pr, door), 2)


homes, infill, civic = [], [], []
for b in BUILD:
    b["z"] = terrace_z_for(b)
    if b["kind"] == "home":
        homes.append(b)
    elif b["kind"] == "infill":
        infill.append(b)
    else:
        civic.append(b)
DIST_ORDER = ["heart", "harbour_row", "upper_town", "crown_hill", "brookside", "orchard_upland"]
homes.sort(key=lambda b: (DIST_ORDER.index(b["district"]), math.hypot(b["at"][0] - A(0, 64)[0], b["at"][1] - A(0, 64)[1])))
for k, b in enumerate(homes):
    b["id"] = f"H{k + 1:02d}"
    b["home_index"] = k
    b["name"] = b["name"] or f"Home {k + 1}"
    b["style"] = k
    b["interior"] = True
    b["chest"] = True
for k, b in enumerate(infill):
    b["id"] = f"I{k + 1:02d}"
    b["name"] = b["name"] or "Shell"
    b["shell"] = True
    b["style"] = 40 + k
INTERIOR_STYLE = {"town_hall": 20, "inn": 21, "bakery": 22, "boathouse": 23, "mill_barn": 24, "church": 25}  # kg_interiors.LANDMARKS
for k, b in enumerate(civic):
    b["id"] = f"C{k + 1:02d}"
    if b.get("legacy") in INTERIOR_STYLE:
        b["style"] = INTERIOR_STYLE[b["legacy"]]
        b["interior"] = True
    else:
        b["style"] = 30 + k
        b["interior"] = False

# SPRINT-022: house archetypes (Tools/Level/kg_archetypes_v2.py): no two neighbouring homes / shells look alike.
import sys as _sys  # noqa: E402
_sys.path.insert(0, HERE)
import kg_archetypes_v2 as ARCH  # noqa: E402
_pairs = ARCH.assign(homes + infill)
print(f"  archetypes: {len(homes) + len(infill)} buildings, {len(_pairs)} neighbour pairs, "
      f"{len({b['archetype'] for b in homes + infill})} archetypes in use")

houses_out = [{k: v for k, v in b.items() if k != "kind"} for b in homes]
infill_out = [{k: v for k, v in b.items() if k != "kind"} for b in infill]

landmarks = {}
for b in civic:
    key = b.get("legacy") or b["name"].split(" ")[0].lower()
    rec = {k: v for k, v in b.items() if k not in ("legacy",)}
    landmarks[key] = rec
landmarks["mausoleum"]["prop"] = "SM_KG_Mausoleum"


def point_lm(name, at, z, **kw):
    rec = {"at": rd(at), "z": z}
    rec.update(kw)
    landmarks[name] = rec


FOUNTAIN = A(0.0, 64.0)
point_lm("fountain", FOUNTAIN, Z_HEART, prop="SM_KG_Fountain", clear_radius=2.0, note="square centre, spawn ring r 7.5")
point_lm("gallows", A(-6.5, 72.8), Z_HEART, prop="SM_KG_Stage + gallows frame", face=rd(FOUNTAIN), tag="KG_Gallows",
         note="Moot stage in front of the Town Hall steps; everyone faces the hall.")
point_lm("notice_board", A(-3.6, 76.2), Z_HEART, prop="SM_KG_SignPost / notice board", face=rd(FOUNTAIN))
point_lm("dead_tree", A(6.5, 55.5), Z_HEART, prop="DeadTree_2 (scaled 1.4)", note="black silhouette against the sea window")
point_lm("market_stalls", A(8.0, 66.0), Z_HEART, prop="Stall_Empty x3 + Awning", face=rd(FOUNTAIN))
landmarks["well"] = {"at": rd(P(99.5, -22.2)), "z": Z_UPPER, "prop": "well (kit build) + cellar hatch (future)",
                     "note": "Old Well in Well Court beside the Old Oak."}
point_lm("old_oak", P(101.5, -25.0), Z_UPPER, prop="CommonTree_3 (scale 1.8)")
point_lm("belvedere_telescope", A(6.0, 113.6), Z_CROWN, prop="telescope on the parapet")
point_lm("graveyard", A(-24.0, 136.0), Z_CROWN, polygon=poly([A(-36.0, 127.0), A(-16.0, 127.0), A(-16.0, 145.0), A(-36.0, 145.0)]),
         gate=rd(A(-26.0, 126.0)))
point_lm("koi_pond", (45.0, 16.6), Z_GARDEN, radius=2.0, prop="SM_KG_KoiPondRim + pond plane + ArchBridge")
point_lm("garden_torii", (45.0, 13.4), Z_GARDEN, prop="SM_KG_Torii (scale 0.55)", face=[45.0, 150.0])
point_lm("giant_sakura", (58.0, 26.0), Z_GARDEN, prop="SM_KG_Sakura_A (scale 1.6)")
point_lm("sea_torii", (45.0 - 0.23944 * 31.0, 119.0), -1.5, prop="SM_KG_Torii", face=rd(FOUNTAIN), note="on the axis, in the shallows")
point_lm("pagoda", PAGODA, 5.55, prop="KG_Island + SM_KG_Pagoda")
point_lm("island", PAGODA, 0.0, radius=17.0)
point_lm("harbour_light", MOLE_PTS[-1], 2.5, prop="beacon lantern, green night light")
point_lm("waterwheel", (-80.0, 3.5), None, prop="waterwheel (new prop) on the brook, mover spin", face=[-96.0, 13.0])
point_lm("crane", P(30.5, 12.0), Z_QUAY, prop="SM_KG_CargoHoist")
point_lm("slipway", P(25.0, TH_BOAT), 0.0, note="boathouse slip into the basin")
point_lm("woodcutter", (-101.0, -4.0), None, radius=5.0)
point_lm("farmyard", (57.0, -70.0), None, radius=7.0)
point_lm("pen", (78.0, -60.0), None, polygon=poly([(73.0, -66.0), (84.0, -66.0), (84.0, -55.0), (73.0, -55.0)]))

# ================================================================================================ AREAS (legacy)
areas = [
    {"name": "square", "center": rd(FOUNTAIN), "radius": 12.0},
    {"name": "fish_market", "center": rd(P(34.0, 42.0)), "radius": 7.0},
    {"name": "church_yard", "center": rd(A(0.0, 117.5)), "radius": 9.0},
    {"name": "smithy_yard", "center": [-91.0, 11.0], "radius": 6.0},
    {"name": "farm", "center": [57.0, -70.0], "radius": 9.0},
    {"name": "woodcutter", "center": [-101.0, -4.0], "radius": 5.0},
    {"name": "lighthouse", "center": [68.0, 90.0], "radius": 5.0},
    {"name": "japan_garden", "center": [47.0, 18.0], "radius": 8.0},
    {"name": "well_court", "center": rd(P(98.5, -22.2)), "radius": 5.0},
    {"name": "quay_head", "center": rd(P(30.0, 0.0)), "radius": 5.0},
]

# ================================================================================================ FIELDS (walled)
fields = [
    {"name": "carrot_field", "polygon": [[36.0, -112.0], [57.0, -112.0], [57.5, -88.0], [38.0, -86.0]], "crop": "carrots",
     "wall": "SM_KG_StoneWall (4 m) + StoneWall_End, gate on the south side"},
    {"name": "wheat_field", "polygon": [[64.0, -112.0], [94.0, -110.0], [92.0, -86.0], [66.0, -88.0]], "crop": "wheat"},
    {"name": "apple_orchard", "polygon": [[80.0, -36.0], [112.0, -40.0], [114.0, 8.0], [88.0, 12.0], [84.0, -8.0]],
     "crop": "apple rows (CommonTree_2 scale 0.8, 5 m grid)"},
    {"name": "cabbage_plots", "polygon": [[98.0, -84.0], [116.0, -84.0], [116.0, -68.0], [104.0, -70.0]], "crop": "cabbages"},
]

# ================================================================================================ CHORES
def door_of(b, out=1.7):
    """World point `out` m in front of the door (same maths as kg_build_village.build_house + Frame.put)."""
    w, d = b["size"]
    cw = w // 2
    lx = -cw + 1 + 2 * (cw // 2)
    yaw = math.radians(b["face_deg"] + 90.0)
    c, s = math.cos(yaw), math.sin(yaw)
    ly = -d / 2.0 - out
    return (b["at"][0] + lx * c - ly * s, b["at"][1] + lx * s + ly * c)


def by_name(n):
    return [b for b in BUILD if b["name"] == n][0]


TASKS = [
    ("DrawWater", "Draw water from the well", P(97.2, -22.2), 4, "upper_town", None),
    ("PostNotice", "Post the notice at the board", A(-3.6, 74.6), 3, "heart", None),
    ("FileReports", "File the reports in the town hall", door_of(by_name("Town Hall")), 5, "heart", None),
    ("RingBell", "Ring the church bell", A(0.0, 131.0), 3, "crown_hill", "bell_tower"),
    ("LightCandles", "Light the church candles", door_of(by_name("Church of the Morrow")), 4, "crown_hill", None),
    ("TendGraves", "Tend the graves", A(-23.4, 132.0), 5, "crown_hill", None),
    ("ForgeNails", "Forge nails at the smithy", (-91.5, 10.5), 6, "brookside", None),
    ("SharpenTools", "Sharpen the tools", (-87.5, 9.0), 4, "brookside", None),
    ("BakeBread", "Bake bread at the bakery", door_of(by_name("Bakery")), 6, "heart", None),
    ("PourAle", "Pour ale at the inn", door_of(by_name("Inn - The Latecomer")), 3, "heart", None),
    ("StockStall", "Stock the market stall", A(7.2, 66.0), 4, "heart", None),
    ("MendNets", "Mend the fishing nets", P(29.0, -48.0), 6, "harbour_row", None),
    ("UnloadFish", "Unload the fish crates", A(3.5, -10.0), 5, "harbour_row", None),
    ("FuelLighthouse", "Refuel the lighthouse lamp", (68.0, 95.0), 7, "lighthouse_point", "lighthouse"),
    ("HarvestCarrots", "Harvest carrots at the farm", (57.8, -95.0), 5, "orchard_upland", None),
    ("FeedAnimals", "Fill the feed trough", (78.0, -66.7), 4, "orchard_upland", None),
    ("ChopWood", "Chop firewood at the camp", (-98.4, -2.4), 6, "brookside", None),
    ("FixBoat", "Tar the boat in the boathouse", P(30.0, TH_BOAT), 6, "harbour_row", None),
    # optional (new ids; gameplay can ignore them until wired)
    ("WindClock", "Wind the town clock", A(6.8, 81.0), 4, "heart", "clock_tower"),
    ("FeedKoi", "Feed the koi", (47.8, 16.6), 3, "sakura_garden", None),
    ("LightHarbourLamp", "Light the harbour lamp", MOLE_PTS[-2], 4, "harbour_row", None),
    ("GrindFlour", "Grind flour at the windmill", (92.6, -61.4), 5, "orchard_upland", None),
]
tasks = []
for tid, name, at, secs, dist, tower in TASKS:
    rec = {"id": tid, "name": name, "at": rd(at), "secs": secs, "district": dist}
    if tower:
        rec["tower"] = tower
        rec["note"] = "station on the lookout floor at the top of the ladder"
    if tid in ("WindClock", "FeedKoi", "LightHarbourLamp", "GrindFlour"):
        rec["optional"] = True
    tasks.append(rec)

# ================================================================================================ SIGHTLINES
AX = lambda y: (45.0 - 0.23944 * (150.0 - y), y)  # noqa: E731
sightlines = [
    {"name": "S1_postcard", "from": rd(A(0.0, 45.8)), "to": rd(PAGODA), "eye_z": Z_HEART + 1.7, "target_z": 12.0,
     "note": "From the square balustrade: Grand Stair, gatehouses as the frame, jetty as a leading line, harbour mouth, torii, pagoda."},
    {"name": "S2_pilgrim", "from": rd(A(0.0, -9.0)), "to": rd(A(0.0, 131.0)), "eye_z": 1.2 + 1.7, "target_z": Z_CROWN + 19.5,
     "note": "From the jetty head back up the axis: Grand Stair, fountain, Scala, Pilgrim Stair, bell tower."},
    {"name": "S3_belvedere", "from": rd(A(0.0, 121.5)), "to": rd(A(0.0, 20.0)), "eye_z": Z_CROWN + 1.7, "target_z": 2.0,
     "note": "Belvedere panorama down the stepped axis to the basin, the lighthouse on the left, the island beyond."},
    {"name": "S4_garden_axis", "from": [45.0, 24.3], "to": rd(PAGODA), "eye_z": Z_GARDEN + 1.7, "target_z": 12.0,
     "note": "Pavilion -> over the koi cascade and the fish market roof -> harbour mouth -> pagoda (x = 45)."},
    {"name": "S5_lighthouse_reveal", "from": [-24.0, 67.2], "to": [68.0, 95.0], "eye_z": 2.0 + 1.7, "target_z": 30.0,
     "note": "Coming round the brook bend onto the west quay: the whole basin and the lighthouse on its cliff."},
    {"name": "S6_bridge", "from": [-63.0, 18.2], "to": [-80.0, 3.5], "eye_z": 8.0 + 1.7, "target_z": 10.5,
     "note": "Approaching the Old Stone Bridge: brook, turning waterwheel, forge glow."},
    {"name": "S7_windmill", "from": rd(P(91.0, 33.0)), "to": [97.0, -62.0], "eye_z": Z_UPPER + 1.7, "target_z": 20.0,
     "note": "Balcony Lane east: the windmill rises over the orchard crest."},
]

map_labels = [
    {"text": "THE HEART  z 5", "at": rd(A(-47.0, 66.0)), "size": 15},
    {"text": "FOUNTAIN SQUARE", "at": rd(A(0.0, 50.5)), "size": 8},
    {"text": "HARBOUR ROW  quay z 2", "at": rd(P(38.0, -95.0)), "size": 12},
    {"text": "UPPER TOWN  z 8", "at": rd(P(107.0, 20.0)), "size": 13},
    {"text": "CROWN HILL  z 14", "at": rd(P(138.0, -24.0)), "size": 14},
    {"text": "BROOKSIDE", "at": [-112.0, 36.0], "size": 14},
    {"text": "ORCHARD UPLAND  z 8-11", "at": [96.0, -118.0], "size": 13},
    {"text": "SAKURA GARDEN  z 8", "at": [62.0, 4.0], "size": 11},
    {"text": "LIGHTHOUSE POINT  z 8-14", "at": [100.0, 58.0], "size": 12},
    {"text": "HARBOUR BASIN  -3", "at": [42.0, 78.0], "size": 10},
    {"text": "SHRINE ISLAND", "at": [45.0, 170.0], "size": 12},
    {"text": "the Long Ope", "at": rd(P(66.0, -34.5)), "size": 7, "rot_deg": 0},
]

# ================================================================================================ ZONES (dressing)
zones_src = {
    "square": [SQ],
    "harbour": [sector(R_QUAY_EDGE, R_HARBOUR_WALL, TH_QUAY[0] - 10.0, TH_QUAY[1]), [P(42.0, a) for a in range(0, 360, 10)]],
    "streets": [sector(R_HARBOUR_WALL, R_UPPER_WALL, *TH_HEART), sector(R_UPPER_WALL, R_CROWN_WALL, *TH_UPPER)],
    "church": [sector(R_CROWN_WALL, R_CROWN_BACK + 8.0, TH_CROWN[0] - 2.0, TH_CROWN[1] + 2.0)],
    "japan": [sector(R_HARBOUR_WALL, R_GARDEN_BACK, *TH_GARDEN)],
    "beckside": [slope_raw["brookside"]],
    "countryside": [slope_raw["orchard"]],
    "coast": [slope_raw["headland"]],
}
ZONE_MODULE = {"square": "dress_square", "harbour": "dress_harbour", "streets": "dress_streets", "church": "dress_church",
               "japan": "dress_japan", "beckside": "dress_beckside (new)", "countryside": "dress_countryside",
               "coast": "dress_coast"}

# ================================================================================================ DISTRICTS
districts = {
    "harbour_row": {"terrace": "quay", "walls": "pastel stucco: sea-glass teal, coral, butter, sky (Wall_Plaster_*, tinted MI)",
                    "roofs": "terracotta Roof_RoundTiles (warm red)", "accents": "blue shutters, orange buoys, nets, white trim",
                    "mood": "busy, noisy, many witnesses"},
    "heart": {"terrace": "heart", "walls": "cream limestone ground floor (Wall_UnevenBrick, pale MI) + white plaster",
              "roofs": "bright red tile", "accents": "red/white awnings, yellow-red bunting, white fountain, flower boxes",
              "mood": "the living room: meetings and trials"},
    "upper_town": {"terrace": "upper", "walls": "ochre half-timber (Wall_Plaster_WoodGrid) over brick", "roofs": "deep red-brown",
                   "accents": "geraniums, green shutters, laundry lines", "mood": "quiet, domestic, views"},
    "crown_hill": {"terrace": "crown", "walls": "blue-grey stone (UnevenBrick, cool MI)", "roofs": "slate-blue tint",
                   "accents": "gold bell, purple glass glow, cypress, lavender", "mood": "solemn, windy, highest ground"},
    "brookside": {"terrace": "brookside", "walls": "dark timber + mossy stone", "roofs": "dark shingle (brown MI)",
                  "accents": "forge glow, waterwheel, willow, reeds", "mood": "work and water noise, shade"},
    "orchard_upland": {"terrace": "orchard", "walls": "whitewash + barn red", "roofs": "straw/ochre tint",
                       "accents": "wheat gold, apple rows, windmill, dry-stone walls", "mood": "open, sunny, hard to hide"},
    "sakura_garden": {"terrace": "garden", "walls": "vermilion timber, white plaster", "roofs": "dark tile",
                      "accents": "sakura pink, raked gravel, lanterns, koi", "mood": "calm; framed island view"},
    "lighthouse_point": {"terrace": "headland", "walls": "white/red stripes, granite", "roofs": "red",
                         "accents": "yellow gorse, rope rails, oil barrels", "mood": "exposed, the edge of the world"},
}

# ================================================================================================ WRITE
from shapely.geometry import Polygon  # noqa: E402  (authoring only; consumers get plain polygons)
from shapely.ops import unary_union  # noqa: E402

LAND = Polygon(coast).buffer(0)
FLAT = unary_union([Polygon(t["polygon"]) for t in terraces]).buffer(0)


def largest(g):
    if g.geom_type == "MultiPolygon":
        parts = sorted(g.geoms, key=lambda p: -p.area)
        for p in parts[1:]:
            if p.area > 2.0:
                print(f"  note: dropped a {p.area:.1f} m2 sliver")
        g = parts[0]
    return [rd(p, 2) for p in list(g.exterior.coords)[:-1]]


for name, raw in slope_raw.items():
    dist, pr, surf, note = SLOPE_META[name]
    g = Polygon(raw).buffer(0).intersection(LAND).difference(FLAT.buffer(0.01)).buffer(0)
    terraces.append({"name": name, "district": dist, "z": None, "z_profile": pr, "surface": surf, "edge": "soft",
                     "soft_m": 6.0, "polygon": largest(g.simplify(0.05)), "note": note})

zones = []
for zname in ["square", "japan", "church", "harbour", "streets", "beckside", "countryside", "coast"]:
    g = unary_union([Polygon(p).buffer(0) for p in zones_src[zname]])
    if zname not in ("harbour",):
        g = g.intersection(LAND.buffer(3.0))
    polys = [g] if g.geom_type == "Polygon" else list(g.geoms)
    zones.append({"name": zname, "module": ZONE_MODULE[zname],
                  "polygons": [[rd(p, 1) for p in list(q.simplify(0.3).exterior.coords)[:-1]] for q in polys if q.area > 4.0]})
zones.append({"name": "japan_island", "module": "dress_japan", "polygons": [[rd((PAGODA[0] + 40 * math.cos(a / 16 * math.pi * 2),
                                                                                 PAGODA[1] + 40 * math.sin(a / 16 * math.pi * 2)), 1)
                                                                              for a in range(16)]]})

# Point props get a nominal footprint + facing too (prop=True: exempt from the building-gap rule, they stand inside
# squares/yards on purpose) and the terrace/z under them.
from shapely.geometry import Point as _Pt, Polygon as _Pg  # noqa: E402
PROP_SIZE = {"fountain": [4, 4], "gallows": [4, 4], "notice_board": [2, 2], "dead_tree": [4, 4], "market_stalls": [2, 8],
             "well": [2, 2], "old_oak": [6, 6], "belvedere_telescope": [2, 2], "graveyard": [20, 18], "koi_pond": [4, 4],
             "garden_torii": [4, 2], "giant_sakura": [6, 6], "sea_torii": [8, 2], "pagoda": [8, 8], "island": [34, 34],
             "harbour_light": [2, 2], "waterwheel": [4, 2], "crane": [4, 4], "slipway": [4, 8], "woodcutter": [10, 10],
             "farmyard": [14, 14], "pen": [10, 10]}
def finish_props():
  _terr = [(t["name"], _Pg(t["polygon"]), t) for t in terraces]
  for k, v in landmarks.items():
      if "size" in v and not v.get("is_prop"):
          continue
      v["size"] = PROP_SIZE.get(k, [2, 2])
      v["is_prop"] = True
      if "face" not in v:
          v["face"] = rd(FOUNTAIN) if math.dist(v["at"], FOUNTAIN) < 40 else rd((v["at"][0], v["at"][1] + 4.0))
      v["face_deg"] = round(ang((v["face"][0] - v["at"][0], v["face"][1] - v["at"][1])) % 360.0, 2)
      v["yaw"] = round((v["face_deg"] + 90.0) % 360.0, 2)
      hit = [(n, t) for n, g, t in _terr if g.covers(_Pt(v["at"]))]
      v["terrace"] = hit[0][0] if hit else "sea"
      if v.get("z") is None and hit:
          t = hit[0][1]
          v["z"] = t["z"] if t.get("z") is not None else round(prof_z(t["z_profile"], v["at"]), 2)


finish_props()

layout = {
    "_doc": ("Morrowmere v2 (final): 'The Amphitheatre Cove'. Shared by Tools/Blender/kg_build_terrain.py (terraces, "
             "walls, water, stairs, ramps), Tools/Unreal/kg_build_village.py (buildings, stairs, walls, quay, props, "
             "tasks) and Tools/Unreal/kg_dress_common.py (zones). Units: METRES, UE frame (x east, +y toward the sea, "
             "z up); UE cm = m*100; Blender uses (x, -y). Plan doc: Docs/Level/Morrowmere_v2_Plan.md. Generated by "
             "Tools/Level/author_layout_v2.py; checked by Tools/Level/validate_layout.py; drawn by render_layout.py."),
    "_schema": {
        "building": "at [x,y] centre; size [w,d] (w frontage along local X, d depth); face = point 4 m in front of the "
                    "door; face_deg = direction the front looks (0 east, 90 sea); yaw = UE actor yaw = face_deg+90; "
                    "z = pad height at the front door (plinth down to terrain behind); terrace; storeys; block = party-wall "
                    "group (members of one block touch, everything else keeps >= 1 m).",
        "terrace": "polygon (closed, CCW or CW); z flat, or z_profile {origin, dir, stops[[s,z]]} with s = (p-origin).dir; "
                   "edge 'hard' = retaining wall/cliff on its boundary, 'soft' = blend soft_m metres outward. "
                   "Slope terraces are computed as raw_polygon minus the flat terraces; validate/render do the same.",
        "stair": "from (top or bottom) -> to; z0 at from, z1 at to; risers = |z1-z0| kit risers of 1 m x 2.08 m run; "
                 "landings share the remaining length; width multiple of 2 m (kit piece width).",
        "ramp": "polyline, z linear with length from z0 to z1; terrain overrides to this grade inside width/2.",
        "retaining_wall": "polyline along a terrace edge; top_z / bottom_z; courses = kit wall courses (3 m).",
        "lane": "polyline + width; z number = flat, 'terrain' = follows the terrace/profile under it.",
        "landmark": "civic buildings/towers as buildings; point props (fountain, torii, well...) carry is_prop=true with a nominal footprint, facing and terrace/z; they may stand inside squares/yards.",
        "street_join": "arch (sottoportego over an ope) or garden_wall (with gate) closing the street wall between two party-wall blocks.",
    },
    "version": 2,
    "plateau_z": Z_HEART,
    "axis": {"from": rd(A(0.0, 131.0)), "to": rd(PAGODA), "origin_O": rd(O), "dir_inland": [round(DIN[0], 5), round(DIN[1], 5)],
             "note": "Processional Axis. Polar frame used by the plan: P(r, th) = O + r*(cos th*dir_inland + sin th*east)."},
    "terraces": terraces,
    "coast": {"land_polygon": poly(coast), "beach_z": 0.8, "sea_floor": {"near_z": -3.0, "far_z": -14.0, "far_at_m": 120.0},
              "note": "Inside = land. Outside = sea. The basin is the dredged water inside the quay/mole/headland."},
    "basin": basin,
    "quay": quay,
    "mole": mole,
    "cliffs": cliffs,
    "stream": stream,
    "koi_spill": koi,
    "bridges": bridges,
    "retaining_walls": walls,
    "stairs": stairs,
    "ramps": ramps,
    "lanes": lanes,
    "squares": squares,
    "areas": areas,
    "houses": houses_out,
    "infill": infill_out,
    "landmarks": landmarks,
    "fields": fields,
    "mounds": [{"name": "windmill_knoll", "center": [97.0, -62.0], "radius": 16.0, "height": 2.5}],
    "tasks": tasks,
    "spawn": {"center": rd(FOUNTAIN), "radius": 7.5, "count": 20, "face": "centre"},
    "street_joins": street_joins,
    "sightlines": sightlines,
    "map_labels": map_labels,
    "zones": zones,
    "_zones_doc": "Ordered: kg_dress_common.zone_of() returns the FIRST zone whose polygons contain the point; 'wilds' otherwise.",
    "districts": districts,
    "core": {"polygons": ["quay", "heart", "upper"], "min_coverage": 0.20},
    "district_hubs": {"harbour_row": "quay_promenade", "heart": "rope_walk", "upper_town": "balcony_lane", "crown_hill": "crown_walk", "brookside": "mill_lane", "orchard_upland": "orchard_lane", "sakura_garden": "sakura_walk"},
    "_district_hubs_doc": "validate_layout checks two edge-disjoint routes from the square to the middle of each listed street; Lighthouse Point is a deliberate single spur.",
    "nav_bounds": {"min": [-120.0, -120.0, -4.0], "max": [120.0, 112.0, 45.0]},
    "boats": [rd(A(-3.5, -2.0)), rd(A(3.8, 6.0)), rd(A(-3.8, 12.0)), rd(P(17.0, 52.0)), rd(P(18.0, -55.0)), rd(P(20.0, -70.0))],
}
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(layout, f, indent=1)
print(f"wrote {OUT}: {len(homes)} homes, {len(infill)} infill, {len(civic)} civic, {len(lanes)} lanes, "
      f"{len(stairs)} stairs, {len(ramps)} ramps, {len(walls)} walls, {len(tasks)} tasks")
