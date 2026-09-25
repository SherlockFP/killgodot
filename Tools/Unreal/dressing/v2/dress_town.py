"""Town-wide pass (pseudo-zone "town", run last): lit windows in the infill shells.

The 36 infill shells have no interiors, so at night their windows stayed black while the homes glowed. Each shell
window now has a "lightbox" behind it: a card with M_KG_WindowGlow (SM_KG_GlowCard) 12 cm behind the wall's inner
face, seen only through the window opening. The material follows the sun (SkyAtmosphereLightIlluminance): a faint
warm tint at golden hour, a warm glow in the game's Night phase; per-instance random dims about a third of the panes.
Window slots come from the builder's exact wall rhythm (kit.window_slots), party walls are skipped.
"""
import importlib
import os
import sys

import kg_dress_common_v2 as C

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit as K  # noqa: E402
K = importlib.reload(K)

CARD = C.DX + "SM_KG_GlowCard"
R = C.rng


def glow_windows():
    n = 0
    for f in C.buildings():
        if f.kind != "infill":
            continue
        for side in ("front", "back", "left", "right"):
            for lvl in range(max(1, min(3, f.storeys))):
                if not K.exposed(f, side, lvl):
                    continue
                for t in K.window_slots(f, side, lvl):
                    if R.random() < 0.18:
                        continue                          # a few rooms stay dark
                    x, y = f.at(side, t, -22.0)
                    z = f.z + lvl * C.FLOOR_H + 170.0
                    K.B.add(CARD, x, y, z, f.nyaw(side) + 90.0, (1.6, 2.0, 1.0), 0.0, 90.0, collide=False, cull=25000.0)
                    n += 1
    C.stats["glow_windows"] = n


def house_signs():
    """SPRINT-022 per-house detail: the homes / shells whose details carry a trade sign (Tools/Level/kg_archetypes_v2
    .details) hang it beside the door when the front faces a street."""
    n = 0
    for f in C.buildings():
        sp = K.spec_of(f)
        sign = ((sp or {}).get("details") or {}).get("sign")
        if not sign or f.kind not in ("home", "infill") or "front" not in K.street_sides(f):
            continue
        t = (f.door_t if f.door else 0.0) + (150.0 if f.cells("front") >= 3 else 110.0)
        if abs(t) > f.half("front") - 40.0:
            t = -t
        K.hang_sign(sign, f, "front", t, z_above=265.0)
        n += 1
    C.stats["house_signs"] = n


def dress():
    K.reset()
    for name, fn in (("windows", glow_windows), ("signs", house_signs)):
        try:
            fn()
        except Exception:
            import traceback
            C.stats.setdefault("errors", []).append(f"{name}: {traceback.format_exc()[-900:]}")
    K.flush()
