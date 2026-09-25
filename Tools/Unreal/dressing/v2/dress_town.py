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


def dress():
    K.reset()
    try:
        glow_windows()
    except Exception:
        import traceback
        C.stats.setdefault("errors", []).append(f"windows: {traceback.format_exc()[-900:]}")
    K.flush()
