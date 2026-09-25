"""House archetypes for Morrowmere v2 (SPRINT-022: "every house is the same, it feels copy-paste").

Pure Python (no UE, no shapely), shared by:
  - Tools/Level/author_layout_v2.py      assigns `archetype` + `details` to every home and infill shell (neighbours differ)
  - Tools/Unreal/kg_build_village_v2.py  builds the facades, roofs, balconies, porches, jetties, stairs from the table
  - Tools/Unreal/dressing/v2/kit.py      window_slots(): flower boxes sit under the real windows of each archetype
  - Tools/Level/verify_v2_build.py       the "same-looking neighbour" check

An archetype fixes the silhouette and the facade grammar:
  storeys (allowed 1/2/3), roof form (gable to the street | ridge along the street | hip | cross-gable), ground-floor and
  upper-floor wall material, timber pattern, window rhythm, a balcony / porch canopy / jetty / outside stair, chimneys,
  the door leaf. On top: the district palette (kg_build_village_v2.TINTS) and per-house details (shutter and door paint,
  flower boxes, a trade sign), all deterministic from the building id.

Facade cells: a side of w metres has n = w // 2 kit cells of 2 m; cell k sits at local t = -n*100 + 100 + k*200 cm.
The front door is always cell n // 2 of the ground floor (kg_build_village_v2.door_local).
"""
import hashlib

# kit piece names without the /Game path (the builder prefixes them)
W = "Wall_{m}_{p}"

ARCHETYPES = {
    # label, allowed storeys, roof, ground / upper wall material, window rhythm, extras
    "townhouse": dict(label="Plaster townhouse", storeys=(2, 3), roof="gable", ground="Plaster", upper="Plaster",
                      g_front="wide_round_odd", g_side="thin_round_odd", u_front="wide_round_all", u_side="thin_round_odd",
                      shutters="Wide_Round", balcony=None, porch=False, jetty=False, stair=False,
                      chimney="right", door="Door_1_Round", dormers=True),
    "timber": dict(label="Jettied half-timber", storeys=(2, 3), roof="gable", ground="UnevenBrick", upper="Plaster",
                   g_front="wide_flat_odd", g_side="straight", u_front="grid_flat_alt", u_side="grid",
                   shutters=None, balcony=None, porch=False, jetty=True, stair=False,
                   chimney="left", door="Door_8_Round", dormers=False),
    "merchant": dict(label="Merchant house, hip roof", storeys=(2, 3), roof="hip", ground="Plaster", upper="Plaster",
                     g_front="wide_round_all", g_side="thin_round_odd", u_front="wide_flat_all", u_side="wide_flat_odd",
                     shutters="Wide_Flat", balcony="centre", porch=False, jetty=False, stair=False,
                     chimney="both", door="Door_4_Round", dormers=False),
    "crossgable": dict(label="Cross-gable house", storeys=(2,), roof="cross", ground="UnevenBrick", upper="Plaster",
                       g_front="thin_round_odd", g_side="thin_round_odd", u_front="grid_centre_thin", u_side="grid",
                       shutters="Thin_Round", balcony=None, porch=True, jetty=False, stair=False,
                       chimney="right", door="Door_2_Round", dormers=False),
    "stone": dict(label="Tall stone house", storeys=(2, 3), roof="gable", ground="UnevenBrick", upper="UnevenBrick",
                  g_front="thin_round_odd", g_side="thin_round_odd", u_front="thin_round_odd", u_side="thin_round_odd",
                  shutters="Thin_Round", balcony="top_centre", porch=False, jetty=False, stair=False,
                  chimney="left_tall", door="Door_2_Flat", dormers=True),
    "balcony": dict(label="Balcony house", storeys=(2, 3), roof="ridge", ground="Plaster", upper="Plaster",
                    g_front="thin_round_odd", g_side="straight", u_front="wide_round_all", u_side="thin_round_odd",
                    shutters="Wide_Round", balcony="full", porch=False, jetty=False, stair=False,
                    chimney="back", door="Door_1_Flat", dormers=True),
    "cottage": dict(label="Stone cottage", storeys=(1, 2), roof="hip", ground="UnevenBrick", upper="Plaster",
                    g_front="wide_flat_odd", g_side="thin_round_odd", u_front="thin_round_odd", u_side="straight",
                    shutters="Wide_Flat", balcony=None, porch=True, jetty=False, stair=False,
                    chimney="both", door="Door_4_Flat", dormers=False),
    "stairhouse": dict(label="Outside-stair house", storeys=(2,), roof="ridge", ground="UnevenBrick", upper="Plaster",
                       g_front="wide_round_odd", g_side="straight", u_front="grid_flat_alt", u_side="grid",
                       shutters=None, balcony=None, porch=False, jetty=False, stair=True,
                       chimney="right", door="Door_8_Flat", dormers=True),
}

# District preferences (first = most typical). Every district can use any archetype that fits the storeys.
DISTRICT_PREF = {
    "heart": ["townhouse", "merchant", "timber", "balcony", "stone", "crossgable"],
    "harbour_row": ["balcony", "townhouse", "crossgable", "merchant", "timber", "stone"],
    "upper_town": ["timber", "stairhouse", "crossgable", "townhouse", "balcony", "stone"],
    "crown_hill": ["stone", "cottage", "timber", "merchant"],
    "brookside": ["cottage", "stairhouse", "timber", "crossgable", "stone"],
    "orchard_upland": ["cottage", "crossgable", "stairhouse", "timber"],
    "lighthouse_point": ["cottage", "stone"],
}

# Per-house paint (sRGB-ish linear multipliers for M_KG_PaintedWood): shutters and doors.
PAINTS = {
    "sage": (0.36, 0.55, 0.36), "sky": (0.30, 0.52, 0.80), "oxblood": (0.55, 0.12, 0.10), "mustard": (0.85, 0.62, 0.16),
    "navy": (0.10, 0.18, 0.42), "teal": (0.10, 0.48, 0.48), "lilac": (0.55, 0.42, 0.70), "forest": (0.10, 0.30, 0.16),
    "cream": (0.90, 0.84, 0.66), "natural": None,
}
SHUTTER_PAINTS = ["sage", "sky", "oxblood", "mustard", "navy", "teal", "lilac", "forest"]
DOOR_PAINTS = ["oxblood", "forest", "navy", "teal", "natural", "mustard", "sky", "natural"]
SIGNS = ["Bread", "Ale", "Fish", "Herb", "Anvil"]


def _h(s, salt=""):
    return int(hashlib.md5((s + salt).encode()).hexdigest()[:8], 16)


def fits(name, storeys):
    return storeys in ARCHETYPES[name]["storeys"]


def details(b):
    """Deterministic per-house details from the id: shutter / door paint, flower boxes, a sign (shops)."""
    bid = b.get("id") or b.get("name", "")
    h = _h(bid)
    sh = SHUTTER_PAINTS[h % len(SHUTTER_PAINTS)]
    dr = DOOR_PAINTS[(h // 7) % len(DOOR_PAINTS)]
    if dr == sh:
        dr = DOOR_PAINTS[(h // 7 + 3) % len(DOOR_PAINTS)]
    return {"shutters": sh, "door": dr, "flower_boxes": (h // 49) % 3 != 0,
            "shutters_closed": (h // 97) % 4 == 0, "sign": SIGNS[(h // 131) % len(SIGNS)] if (h // 29) % 5 == 0 else None}


def neighbours(bs, gap=3.5):
    """Pairs (i, j) of neighbouring residential buildings: consecutive units of one party-wall block, or footprints
    whose centres are closer than half their widths + depths + `gap` metres (facing / diagonal neighbours)."""
    out = set()
    for i, a in enumerate(bs):
        for j in range(i + 1, len(bs)):
            b = bs[j]
            if a.get("block") and a.get("block") == b.get("block"):
                d = ((a["at"][0] - b["at"][0]) ** 2 + (a["at"][1] - b["at"][1]) ** 2) ** 0.5
                if d <= (a["size"][0] + b["size"][0]) / 2.0 + 0.3:
                    out.add((i, j))
                    continue
            d = ((a["at"][0] - b["at"][0]) ** 2 + (a["at"][1] - b["at"][1]) ** 2) ** 0.5
            ra = (a["size"][0] + a["size"][1]) / 4.0
            rb = (b["size"][0] + b["size"][1]) / 4.0
            if d < ra + rb + gap:
                out.add((i, j))
    return sorted(out)


def assign(bs, fixed=None):
    """Give every building (homes + infill) an archetype so no two neighbours share one: greedy colouring in order of
    most constrained first, preferring the district's typical archetypes; deterministic. Writes b['archetype'] and
    b['details']; may lower a free-standing 2-storey unit to a 1-storey cottage (storeys field) when that is the only
    way out. Returns the neighbour pairs."""
    fixed = fixed or {}
    pairs = neighbours(bs)
    adj = {i: set() for i in range(len(bs))}
    for i, j in pairs:
        adj[i].add(j)
        adj[j].add(i)
    order = sorted(range(len(bs)), key=lambda i: (-len(adj[i]), bs[i].get("id", "")))
    got = {}
    for i in order:
        b = bs[i]
        if b.get("id") in fixed:
            got[i] = fixed[b["id"]]
            continue
        st = int(b.get("storeys", 2))
        pref = DISTRICT_PREF.get(b.get("district"), list(ARCHETYPES))
        cands = [a for a in pref if fits(a, st)] + [a for a in ARCHETYPES if a not in pref and fits(a, st)]
        used = {got[j] for j in adj[i] if j in got}
        free = [a for a in cands if a not in used]
        if not free:
            raise RuntimeError(f"no archetype left for {b.get('id')} ({st} storeys, neighbours {sorted(used)})")
        # balance the village (least used first), then the district's taste, then a stable per-id jitter
        count = {}
        for a in got.values():
            count[a] = count.get(a, 0) + 1
        got[i] = min(free, key=lambda a: (count.get(a, 0) + 0.45 * cands.index(a)
                                          + (_h(b.get("id", ""), a) % 100) / 400.0))
    for i, b in enumerate(bs):
        b["archetype"] = got[i]
        b["details"] = details(b)
    return pairs


# ============================================================================================ facade grammar
def _win(pattern, k, n, lvl, mat):
    """Piece name for one wall cell from a rhythm pattern (mat = Plaster | UnevenBrick)."""
    odd = k % 2 == 1
    if pattern == "straight":
        return W.format(m=mat, p="Straight")
    if pattern == "grid":
        return "Wall_Plaster_WoodGrid" if mat == "Plaster" else W.format(m=mat, p="Straight")
    if pattern == "wide_round_odd":
        return W.format(m=mat, p="Window_Wide_Round" if odd else "Straight")
    if pattern == "wide_flat_odd":
        return W.format(m=mat, p="Window_Wide_Flat" if odd else "Straight")
    if pattern == "thin_round_odd":
        return W.format(m=mat, p="Window_Thin_Round" if odd else "Straight")
    if pattern == "wide_round_all":
        return W.format(m=mat, p="Window_Wide_Round")
    if pattern == "wide_flat_all":
        return W.format(m=mat, p="Window_Wide_Flat")
    if pattern == "grid_flat_alt":
        if (k + lvl) % 2 == 0:
            return "Wall_Plaster_WoodGrid" if mat == "Plaster" else W.format(m=mat, p="Straight")
        return W.format(m=mat, p="Window_Wide_Flat")
    if pattern == "grid_centre_thin":
        if k == n // 2:
            return W.format(m=mat, p="Window_Thin_Round")
        return "Wall_Plaster_WoodGrid" if mat == "Plaster" else W.format(m=mat, p="Straight")
    return W.format(m=mat, p="Straight")


def arch_of(b):
    return ARCHETYPES.get(b.get("archetype") or "", None)


def piece(b, side, lvl, k, n, shell=False):
    """Kit wall piece (no path prefix) of one cell of a residential building, or None when the archetype does not
    apply (civic buildings keep the builder's own style)."""
    A = arch_of(b)
    if A is None:
        return None
    if lvl == 0:
        mat = A["ground"]
        if side == "front" and k == n // 2:
            return W.format(m=mat, p="Door_Flat" if (shell or "Flat" in A["door"]) else "Door_Round")
        pat = A["g_front"] if side in ("front", "back") else A["g_side"]
        if side == "back" and pat.endswith("_all"):
            pat = pat.replace("_all", "_odd")
        return _win(pat, k, n, lvl, mat)
    mat = A["upper"]
    if A.get("stair") and side == stair_side_hint(b) and lvl == 1 and k == (0 if side == "right" else n - 1):
        return W.format(m=mat, p="Door_Flat")          # the outside stair lands at a first-floor door (front end)
    pat = A["u_front"] if side in ("front", "back") else A["u_side"]
    return _win(pat, k, n, lvl, mat)


def stair_side_hint(b):
    """Side the outside stair climbs (set by the builder when a free side exists): 'left' | 'right' | None."""
    return b.get("_stair_side")


def is_window(name):
    return name is not None and "Window" in name


def window_slots(b, side, lvl):
    """Local t (cm) of the window cells of one storey of one side, or None when b has no archetype."""
    if arch_of(b) is None:
        return None
    w, d = b["size"]
    n = int((w if side in ("front", "back") else d) // 2)
    return [-n * 100.0 + 100.0 + k * 200.0 for k in range(n) if is_window(piece(b, side, lvl, k, n))]


def signature(b):
    """What a passer-by sees: used by the same-looking-neighbour check."""
    A = arch_of(b) or {}
    return (b.get("archetype"), A.get("roof"), A.get("ground"), A.get("upper"), int(b.get("storeys", 2)),
            A.get("balcony"), A.get("porch"), A.get("jetty"), A.get("stair"))
