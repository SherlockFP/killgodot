# SPRINT-033 — Forest threats: wolves and the Mist (4 slices, 033a–033d)

Status: **Proposed (needs approval)**, 2026-09-25. Nothing here is in the active queue until the user says so
(LoopContract "Scope changes need the user").
Design: `Docs/Design/Forest_Threats_Chores.md` v0.2 (§2–§6, §8–§12). Sister contract: `SPRINT-034-ForestChores-Vigil.md`.

User request (2026-09-25, translated): "If you wander too deep into the forest, wolves can attack, or a mysterious fog
follows you and you die if it catches you." The user left balance and content to the designers.

## Dependencies and order (read first)
- **Verification debt is 12 (> ~10).** No new feature slice starts before a verification session, except **033a**
  (tools and data only; it adds no unverified feature).
- Roadmap core loop first: **020a** before 033b (phase refusals); **020b** (Event Ledger) before the forest is ever
  enabled in a real match (charter Ç2). Until then every slice ships behind `kg.Forest.Enabled 0` (dev-only).
- Needs **SPRINT-035b** (shared `UKGStatusComponent`: Wet, Limp) before 033b.
- 033c needs the user's **download approval** for the wolf model (O2).
- 033d needs Roadmap **021, 023, 026a, 027b**.
- Recommended global order (same block in 033–036):
  0. Verification session (user). 1. SPRINT-016 finisher. 2. Roadmap 020a + 036h, with **033a** in parallel.
  3. Roadmap 020b, with 036a in parallel. 4. 035a. 5. 035b. 6. **033b**, 036b. 7. 034a, **033c**, 036c. 8. 034b.
  9. Verification session. 10. 035c (after 020b), 035d (after 021, 022, FPArms2 check), 036d and 035e (after 023),
  035f and 036e (after 024), **033d** (after 021, 023, 026a, 027b).
  A verification session after every ~2 content slices (wolves, Mist, board view and pistol clips each add debt).
- Serial files (never two slices at once): `Character/KGCharacter.cpp`, `Core/KGGameMode.cpp`,
  `AI/KGBotController.cpp`, `Dev/KGDevCommands.cpp`, `UI/KGHUD.cpp`.

---

## SPRINT-033a — Forest band raster and geometry measurement (S, spike)
**Objective:** know, with numbers, whether the v2 forest ring can hold the Edge/Middle/Deep bands, 4 sectors, dens and
a vigil camp **before** any wolf or Mist code is written. (`nav_bounds` stops at x/y ≈ ±120 m and z 45 m; the forest
floor climbs to 40–55 m beyond r ≈ 95 m, so the walkable forest may be a ~25 m ring.)

### Acceptance (fixed)
1. `Tools/Level/gen_forest_bands.py` writes `Tools/Level/morrowmere_forest_v2.json` (2 m grid, 1 byte band + 1 byte
   sector per cell; safe cells from lanes, zones, trail-lantern points and fixed light radii). Deterministic: two runs
   give byte-identical output. Evidence: the tool's own check.
2. Report `Saved/KG_ForestBands_Report.json` plus a top-down image `KG_Cap_forest_bands.png`: walkable m² per band ×
   sector, area lost to `nav_bounds` and the z clip, and the largest distance from any walkable forest cell to a safe
   cell.
3. Lint (exit code ≠ 0 on violation): every walkable forest cell ≤ `KG_FOREST_SAFE_MAX` (45 m) from a safe cell; the
   vigil camp candidate ≤ 110 m path from the square and ≥ 15 m from a village lane; den candidates exist in ≥ 2
   sectors.
4. A decision note at the end of the report with the two options and their numbers: (a) rescale `KG_FOREST_EDGE` /
   `KG_FOREST_DEEP` to the area that exists, or (b) ask the SPRINT-022 v2 owner to widen `nav_bounds` and rebuild
   (I2). The user picks (design O10).
5. `run_invariants.ps1` unaffected (no Source or Content change).

### Writable scope
- new `Tools/Level/gen_forest_bands.py`, new `Tools/Level/morrowmere_forest_v2.json`
- `Saved/` report and image
- `Docs/Design/Forest_Threats_Chores.md` §2 numbers only
Walkability comes from the same inputs `Tools/Level/verify_v2_build.py` reads; a headless commandlet dump of the
navmesh is allowed. No editor window, no level edits.

### Limits
- 3 fix attempts per failing check; plateau stop.
- Read-only for `Tools/Unreal/dressing/v2/*` and `kg_build_village_v2.py` (SPRINT-022 owns them).

---

## SPRINT-033b — The Mist: Mist tongue, Mist Wall, one Wet state, AFK rule, lethality gate (M)
**Objective (visible):** a player alone deep in the forest sees frost at the screen edge, hears whispers, and a rolling
Mist tongue spawns behind them and follows. Walking to a path or a light always escapes. Walking into the Mist Wall at
the forest edge puts you back on the nearest path, dripping wet. At N ≤ 9 and in the endgame the Mist never kills.

### Acceptance (fixed)
1. **Test** `KillGodot.Forest.Rules` (pure `FKGForestRules`, seeded), ≥ 15 table cases: notice fill day 40 s / night
   20 s / torch ×0.5 / frozen by `KG_FOREST_AFK`; spawn 40 m behind, opposite the nearest safe cell, outside the view
   cone; speed 2.4 → 4.2 m/s at +0.08 m/s²; core 3 m / 3 s; first frost → death ≥ 25 s; `KG_PVE_LETHAL` false at
   N ≤ 9 and when alive ≤ max(5, 2·threats + 1) → a catch is a Wall return; day 1 and night 1 no notice; Meeting,
   Trial and Epilogue freeze; tongue caps 1/2/3 by N.
2. **Test (slow states escape):** simulated kinematics from `KG_FOREST_SAFE_MAX` for plain walk, Limp (walk
   unchanged), carrying a world item (dropped at spawn, `CarryDropped{mist}` logged) and a bandage in progress
   (cancelled, cannot start while targeted): the player always reaches safety walking.
3. **Test:** Mist Wall entry → at a safe cell within 3 s, Wet 60 s on `UKGStatusComponent`, stamina 0. AFK in
   Middle/Deep: 10 s → both meters frozen, 20 s → returned to a safe cell.
4. **Two-process smoke** `Tools/Gauntlet/kg_forest_smoke.ps1` (windowless, `kg_chat_smoke` pattern), with
   `kg.Forest.Enabled 1`: the tongue actor replicates to both ends; the player's own stage is `COND_OwnerOnly` (the
   second client never receives it).
5. **Performance:** forest subsystem server cost ≤ 0.35 ms/frame p95 at N = 20 with 3 tongues (headless stat); [R]
   client GPU for tongue + wall ≤ 0.5 ms p95 at 1080p (offscreen render pass).
6. Offscreen shot `KG_Cap_forest_mist.png` (tongue approaching, frost edge); 2 look rounds. `run_invariants.ps1`
   passes; `kg.Forest.Enabled` defaults to 0.

### Writable scope
- new `Source/KillGodot/Forest/`: `UKGForestSubsystem` (server only), `FKGForestRules`, `KGForestTypes.h`,
  `AKGMistTongue`, `AKGMistWall`, own console commands `kg.Forest.*` registered inside `Forest/` (no
  `KGDevCommands.cpp` edit)
- `Character/KGCharacter.cpp`: hooks only (force-drop carry, damage type "Mist"), serial
- new `UI/Forest/` (frost, subtitles, compass arrows) + one hook line in `UI/KGHUD.cpp`, serial
- tests, `Tools/Gauntlet/kg_forest_smoke.ps1`, the forest design doc
Reads `Tools/Level/morrowmere_forest_v2.json` (033a). Uses `UKGStatusComponent` (035b); does not edit it.

### Limits
- 3 fix attempts per failing check; 2 look rounds for the Mist visuals; plateau stop.
- Header changes are collected and applied in **one** batch at a time the user chooses (Live Coding cannot reload
  them). Never close or relaunch the user's editor; everything headless.
- Out of scope → proposals: Mist corpse returning at the pier, Wall exit at the pier, Mist Siege (GDD §10).

**Depends on:** 033a (and the user's O10 decision), 035b, Roadmap 020a, verification session.

---

## SPRINT-033c — Wolves (M)
**Objective (visible):** alone and away from light in the forest, you hear a howl, then see glowing eyes circling, then
get bitten, with at least 20 s between your own howl stamp and the first bite. Paths, fixed lights, groups, shoving and
shouting drive them off. At N ≤ 9 bites only stagger.

### Acceptance (fixed)
1. **Test** (extends `KillGodot.Forest.Rules`), ≥ 15 cases: interest gains (Middle night +5, Deep day +3 / night +7,
   2-person ×0.5 at night, ≥ 3 ×0, torch ×0.75 day / ×0.5 night, raw fish ×1.5, HP < 50 ×1.25, safe −8/s); the
   **per-player telegraph stamp** (a second player walking into a live howl is never bitten earlier than 20 s after
   their own stamp); stage minima 12 / 8 / 20 s; the timing table (first bite 25 / 27 / 33.3 s, death 41 / 43 / 49 s,
   ±0.5 s); bite 20 and 0 when `KG_PVE_LETHAL` is false; a torch holder's first bite repels that wolf; the 2-player
   target rule (farther from the nearest light, then interest, then `PlayerKey`); wolves never path into terraces or a
   fixed light radius (server-side path filter from the band raster, no nav modifiers); flintlock shots have no effect.
2. **Test (escape):** running from the Eyes stage takes ≤ 1 bite from 45 m and 0 bites from ≤ 32 m (kinematic sim with
   `KG_WOLF_SPEED` and stamina).
3. **[U] soak** N = 8 and N = 20, 2 seeds, days 1–3, `kg.Forest.Enabled 1`: every bot reaching Eyes logs
   `KG_FOREST … action=run`; the wolf cap (2/3/6) is never exceeded; PvE damage on day 1 and night 1 = 0; PvE deaths
   at N = 8 = 0; the PvE death share and Town's PvE share are reported against the F1 gate (≤ 5%; Town share ≤ living
   share + 10 points).
4. **Performance:** forest total ≤ 0.35 ms/frame p95 at N = 20 with 6 wolves + 3 tongues; wolf net update 10 Hz, cull
   90 m, ≤ 2.4 KB/s per client (net profiler).
5. Offscreen shot `KG_Cap_forest_eyes_night.png` (eyes at 20 m at night); 2 look rounds. `run_invariants.ps1` passes;
   still default off.

### Writable scope
- `Forest/` (new `AKGWolf`, pack logic)
- new content path `/Game/KillGodot/Forest/Wolf/` (never overwrite shipped assets)
- `AI/KGBotController.cpp`: forest flee only, serial
- `Character/KGCharacter.cpp`: bite stagger + Limp through the status component, hooks only, serial
- `UI/Forest/`, tests

### Limits
- **Precondition:** ask the user to approve downloading Quaternius Ultimate Animated Animals (CC0; name, source, size).
  Without approval, stop and ask; no placeholder ships as the result.
- 3 fix attempts per check; 2 look rounds for the wolf; plateau stop; header changes in one user-timed batch.
- Out of scope → proposals: killable wolves, Hunter's trap, Shepherd's dog, Werewolf full-moon chorus.

**Depends on:** 033b, O2 approval.

---

## SPRINT-033d — Staging and forest evidence in the ledger (M)
**Objective (visible):** forest events show in the ledger and case file; an Impatient can "Maul" a fresh body to fake
a wolf kill, and the one public rule ("a wolf kill has a howl ≥ 20 s before in that sector and a den-ward drag with paw
prints") lets the village catch it.

### Acceptance (fixed)
1. **Test:** every forest event type (design §8.1) is written through the 020b witness filter (saw / heard /
   identified); the howl sector appears only in lines of witnesses ≤ 60 m; no Town Board mirror for howls or vigil
   signatures.
2. **Test:** Maul limits (threat side only, Middle/Deep band, body ≤ 60 s old, 6 s, `KG_HEAR_MAUL` 12 m); normal
   inspection says "Bite", the Coroner says "knife, then bite"; a bite corpse missing either public condition logs
   `KG_CONTRA`; every real wolf kill passes both (F6, 100% in [T]).
3. **Test:** a PvE death whose victim was grouped with a threat-side player in the prior 60 s spends that team's
   Kurban Hakkı (debt rule when none is left); nothing about it replicates.
4. **Test:** paw prints and the wolf drag use the 026a trace pool; the shared 256 cap is never exceeded.
5. **[U]** the Impatient bot's Maul path and the Town bot's contradiction suspicion are logged with
   `reason=<eventId>`. `run_invariants.ps1` passes. The forest may be enabled in real matches only if the F1 gate
   passed in the 033c soak.

### Writable scope
- `Forest/`, `Evidence/` (event types and hooks only; 020b owns the ledger), `AI/` (bot mind rules), tests

### Limits
- 3 fix attempts per check; plateau stop.
- Out of scope → proposals: sign turning, Mist corpse at the pier, carrying a body into the Wall (025b).

**Depends on:** 033c, Roadmap 020b, 021, 023, 026a, 027b.
