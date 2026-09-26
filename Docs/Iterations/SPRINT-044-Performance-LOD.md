# SPRINT-044 — Performance: distant things are culled or low-poly (LOD/HLOD), so everyone can play

User (2026-09-26): "Things that are very far away shouldn't be rendered, or should look very low-poly, for optimisation."
Earlier the user said: "it must not stutter, everyone should be able to play."

## Acceptance (fixed)
1. **Baseline first.** A headless perf capture (windowless -game -RenderOffScreen, `stat unit`/`stat rhi`/`stat scenerendering`
   logged) at 8 fixed camera spots on L_Morrowmere_v2 and 6 on L_StormManor. It records frame/game/draw/GPU ms, draw
   calls, primitives and triangles. Stored as `Saved/Perf/baseline.json` + a table in Docs/Process/Performance_Plan.md.
2. **Distance culling by size class** on every placed prop (the builder + the dressing helpers + the manor dressing):
   - small ≤ 0.5 m: 25–35 m
   - medium ≤ 2 m: 60 m
   - large ≤ 6 m: 150 m
   - buildings and landmarks: never, but with HLOD
   Instanced foliage/grass cull ranges re-tuned. Lights: attenuation limits and max draw distance.
3. **LODs.** Every static mesh we place gets 3 LODs via a commandlet: auto reduction (60/30/12%) + screen sizes; skip meshes
   under ~300 tris. Villager/character skeletal meshes get 2 LODs. Nanite stays off (vertex colour).
4. **HLOD / far village.** Bake HLOD proxies (or merged far-LOD proxies) for house clusters and districts, so the village
   seen from the lighthouse or the island draws as a few low-poly proxies. Also the manor exterior from the sea.
5. **Settings.** Shadow distance and cascades tuned per scalability level. The graphics settings Low/Medium/High/Epic
   map to real cvars (view distance scale, foliage density, shadow quality, LOD bias) and are documented.
6. **Result.** The same capture after the changes, showing a draw-call and GPU-ms reduction, with before/after
   screenshots at 3 spots to prove there is no visible popping close up. The target is the plan's Low tier (60 fps at
   1080p on a GTX 1050 Ti-class GPU, estimated from the ms budgets).
   A perf gate is added to run_invariants.ps1 (I5: the frame-time budget at the fixed spots, with tolerance).
   Invariants stay green.

## Writable scope
- LOD/HLOD commandlet tools
- cull settings in kg_build_village_v2.py / kg_dress_common_v2.py / kit.py / kg_sm_dress.py / kg_build_stormmanor.py
- DefaultScalability.ini / DefaultEngine.ini perf cvars
- the perf capture tools
- docs
Coordinate with the agents owning those builders; run this sprint when they're idle.

## Limits
- 3 fix attempts per failing check
- 2 look rounds for popping
- plateau stop
