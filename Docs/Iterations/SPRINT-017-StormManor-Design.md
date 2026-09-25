# SPRINT-017 — Map 2 "Storm Manor": design only

The user chose the theme on 2026-09-25: **Fırtınalı Malikâne**, a vast manor on a rock off the coast in a storm. It is
an enclosed map in the Among Us sense, with many named rooms and regions, tied to Morrowmere's lore (Docs/Lore). It is
built with the same pipeline as v2 (layout JSON → builder → dressing), so the "houses are reskinnable units" idea holds.

This sprint is DESIGN ONLY. It ends with a plan the user approves, then SPRINT-018 builds it.

## Acceptance (fixed)
1. `Docs/Level/StormManor_Plan.md` (Turkish, like the other design docs):
   - Concept and lore hook.
   - 14–20 named rooms over 2–3 floors plus a cellar: ballroom, kitchen, library, wine cellar, conservatory,
     chapel, servants' corridors, tower, boathouse, guest wings, study, gallery, etc.
   - Circulation loops, secret passages (vent-like), sightlines, chokepoints.
   - Meeting room. Spawn. Travel-time estimates.
   - 15+ physical chores that fit the SPRINT-016 framework (carry, deliver, use).
   - Storm gameplay (lightning reveals, power outage sabotage, windows).
   - Kit and prop needs: interior walls and floors from the existing kit and Furniture packs, plus new Blender props.
   - Performance budget.
2. `Tools/Level/stormmanor_layout.json` (rooms as polygons per floor, doors, stairs, secret passages, windows, chores,
   spawns, meeting room). Validated by an extended `validate_layout.py`: connectivity, no overlaps, every chore reachable,
   meeting within 30 s.
3. `Docs/Level/StormManor.png`: a floor-by-floor top-down plan rendered from the JSON (`render_layout.py` extension).
4. No code, level or asset changes.

## Limits
- 2 design iterations.
- Then stop and present to the user for approval.
