# SPRINT-022 — Morrowmere v2 visual pass (the user's review of 2026-09-25 in the editor)

User feedback, looking at L_Morrowmere_v2 in the editor:
- Every house looks the same, so the village feels copy-pasted. The house count can drop *slightly*.
- Some Japanese pieces are placed with wrong geometry. In the screenshot: a torii on top of the stair, lanterns on the stair
  treads, a garden rock floating in the air, the arch bridge over a sunken pit.
- The water looks bad; make it realistic.
- Make the towers better (clock tower etc.) and add "many things like that".

## Acceptance (fixed)
1. **House variety.** At least 6 clearly distinct home archetypes, and no two neighbouring homes share the same one.
   - Archetypes differ in storeys (1/2/3), roof form (gable/hip/cross-gable/jettied), facade material mix, timber
     pattern, window rhythm, balcony/porch/bay/outside stair, and chimney.
   - A per-district palette on top, plus per-house details: shutters colour, door colour, flower boxes, signs.
   - Home count reduced by about 10% (e.g. 20 → 18) by merging or removing the weakest infill; the town-core coverage
     target stays ≥ 20%.
   - Evidence: an automated "same-looking neighbour" check in verify_v2_build.py, plus before/after eye-level renders of
     3 streets.
2. **Placement correctness.** A new geometric validator in verify_v2_build.py, zero violations over the whole map
   (all dressing and builder props). Nothing may be:
   - floating (> 8 cm gap under its footprint, sampled with traces in a headless commandlet);
   - sunk more than its tolerance;
   - intersecting stairs, ramps, bridges or door aprons;
   - standing on a stair tread (unless a whitelisted stair prop).
   The Japanese garden is re-laid-out with correct geometry: torii on flat ground framing the path, not on the stair;
   lanterns on plinths beside paths; rocks grounded; the bridge over real water. Evidence: the validator report plus
   renders of the garden from 3 angles.
3. **Water.** A new water material family (sea, harbour basin, koi pond, brook) that reads as realistic within the
   stylised look:
   - depth-based colour and absorption, visible shoreline depth fade
   - normal-mapped ripples at two scales
   - fresnel reflection of the sky
   - refraction and distortion of what is underneath (pond/basin)
   - soft foam at contact lines (shore, piers, mole)
   - specular sun glint
   Keep the wave WPO and C++ FKGWaves in sync (sea + calm basin). Evidence: before/after renders (quay, koi pond,
   brook, open sea at dusk) and no frame-time regression in the headless perf capture (≤ +0.5 ms at 1080p).
4. **Towers and landmarks.**
   - The clock tower is rebuilt as a proper landmark: taller, a clock stage, a belfry, a spire, animated hands driven
     by the match clock, a readable silhouette from the harbour.
   - The bell tower gets a real bell (it rings on the RingBell chore).
   - The lighthouse gets a gallery/railing and a lamp room.
   - Plus at least 4 further "iconic small landmarks" from the plan's proposal list: waterwheel, gate arch, retaining
     wall props, Belvedere telescope, and whichever fit.
   All climbable towers keep their ladders; the navmesh test stays 100%.
5. **Invariants:** run_invariants.ps1 passes; the nav test reaches all targets.
6. **Lighthouse Point cliff** (added 2026-09-25 with the user's approval): the huge flat smooth brown cliff / retaining
   face at Lighthouse Point seen from the harbour basin is broken up: lowered, with layered rocks, ledges, vegetation
   and a natural silhouette. Evidence: the cliff-face check in verify_v2_build.py plus before/after renders from the
   basin.

## Writable scope
- Tools/Level/author_layout_v2.py + morrowmere_layout_v2.json (house count/archetype fields only)
- Tools/Unreal/kg_build_village_v2.py, Tools/Level/prep_v2_placements.py, dressing/v2/*, kg_dress_common_v2.py
- Tools/Level/verify_v2_build.py
- ocean/water material scripts (kg_make_ocean.py or new) + KGWaves.h (sync only)
- new Blender prop packs, new materials
- docs (v2_Build_Report.md)

Coordinate with SPRINT-016, which owns chore station placement hooks in the builder: add, don't reorder.

## Limits
- 2 look rounds per acceptance item
- 3 fix attempts per failing check
- plateau stop

## Rules
- The editor may be open for the user: never rebuild or save L_Morrowmere_v2 while they have it open. Build headless into the level only after the director says the editor is closed; before that, work on scripts, props and materials.
