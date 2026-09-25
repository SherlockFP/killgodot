# SPRINT-042 — Performance and minimum spec: "it must not stutter, everyone should be able to play" (5 slices, 042a–042e)

Status: **Proposed (needs approval)**, 2026-09-25.
Plan, tiers, budgets, baseline and hypotheses: `Docs/Process/Performance_Plan.md` (Turkish). Section numbers below
(§) refer to that plan.

User request (2026-09-25, translated): "You can also download extra assets and decorate the map, but the game must be
optimized and must not stutter, because everyone should be able to play it. Can we port it to the web later?"

## Baseline (2026-09-25, reference PC RTX 5070 + Ryzen 7 7800X3D, 1080p, editor binary `-game -RenderOffScreen`)
| | Frame ms | Game thread ms | GPU ms | Draw calls | Tris/frame |
|---|---|---|---|---|---|
| Epic, 8 spots, no bots | 8.85–10.31 | 3.31–4.24 | 8.36–9.83 | 176–1338 | 1.46–6.84 M |
| Low (`scalability 0`), 4 spots | 3.57–4.47 | 3.53–3.83 | 2.11–2.40 | 102–832 | 0.61–2.27 M |
| Epic + 19 bots, 3 spots in view | 9.37–11.90 | 5.80–6.57 | 8.83–11.34 | 1275–1845 | 5.97–8.61 M |
Hitches: 19 bots added in one frame = 315 ms; a quality change = 293 ms; 67 graphics and 9 compute PSO misses. VRAM:
1.99–2.04 GB at Epic, 1.75 GB at Low. Ticks: 304 with no players, 570 with 20 characters. Estimate for a GTX 1050 Ti
(§3.5): 11–13 fps at Epic and 43–50 fps at Low; the 20-character game thread would take about 12.5–16.5 ms.

## Dependencies and order (read first)
- 042a first: every other slice is judged by its gate.
- Then 042b and 042c can run in parallel, because their files are disjoint (042b = level tools/content, 042c = C++).
- 042d follows 042a. 042e needs the user (a real low-tier device).
- **Verification debt is 13 (> ~10).** 042a and 042d are tooling and infrastructure (no new gameplay feature) and
  may start. 042b changes what the map looks like, so it waits for a verification session.
- Serial files touched here: `Character/KGCharacter.cpp` (042c only), `UI/Menu/SKGSettingsMenu.cpp` (042a only).
  Never at the same time as another sprint that writes them.
- Timing measurements use the GPU for about 4 minutes. They never run while the user is playing a game. If a game
  process is detected, the run is marked UNRELIABLE and does not count.
- **The user may be gaming:**
  - no editor GUI and no windows, ever;
  - measurements are headless (`-game -RenderOffScreen`), one process at a time;
  - header or UPROPERTY changes are batched into one editor close cycle, at a time the user chooses.

---

## SPRINT-042a — Perf gate, GPU breakdown, real scalability presets, first-run auto-detect (M)
**Objective (visible):**
- A new player on a weak PC gets a playable preset on first launch.
- The Low / Medium / High / Epic presets now change our own content: grass, trees, clouds, sun shadow.
- Every later sprint is measured by the same automatic gate.

### Acceptance (fixed)
1. **Gate:**
   - `Tools/Gauntlet/kg_perf_gate.ps1` runs `Tools/Unreal/kg_perf_tour.py`: one windowless process, ≤ 4 min.
   - The tour covers the 7 named spots of §3 × Low / High / Epic, then 19 bots at plaza, belvedere and jetty_back.
   - It writes `Saved/Gauntlet/perf-<stamp>.json`. Per segment: avg, p95 and p99 frame, game thread, render thread,
     GPU, draw calls, triangles, ticks, VRAM, and frames over 50 ms.
   - Budgets and the baseline live in `Tools/Gauntlet/perf_budget.json`.
   - It is added as **I5** to `run_invariants.ps1` and can be skipped with `-Skip I5`.
   - Hard gates (draws, triangles, ticks, frames over 50 ms) apply on any machine. Timing gates apply only on the
     reference machine (GPU and CPU names match): WARN at +15 % against the baseline, FAIL at +30 % or over budget.
   - Two back-to-back runs agree within ±10 % on GPU and game-thread averages at every spot. The unreliable
     measurement of QA-002-08 is gone.
2. **GPU breakdown:**
   - One `-csvGpuStats` / `ProfileGPU` capture at Epic and at Low, at plaza and forest.
   - Per-pass ms go into Performance_Plan §3 (a new §3.6).
   - Each H1 suspect gets a measured cost: TSR, VolumetricCloud, SSR, AO, outline, ocean, CSM.
3. **Presets:**
   - A new `Config/DefaultScalability.ini` implements the §5.1 table.
   - The Low A/B choices (TSR q0 against FXAA, `r.Nanite` 0 against 1) are decided by measurement and recorded.
   - Low keeps a 1-cascade sun shadow.
   - `KGGrassField` and `KGFoliageField` scale density and cull distance by `sg.FoliageQuality`. Measured: Low grass
     instances rendered ≤ 40 % of Epic at plaza.
4. **Numbers on the reference PC, 1080p, no bots, all 7 spots:**
   - Low GPU avg ≤ **1.5 ms**, High ≤ **4.0 ms**, Epic ≤ **6.5 ms**;
   - Low VRAM ≤ **1.6 GB**.
5. **First run:**
   - With no `GameUserSettings.ini`, the game runs `RunHardwareBenchmark` once, applies the result capped at High,
     and falls back to Medium if the benchmark fails.
   - A headless test starts with an empty settings dir and asserts the chosen levels are ≠ Epic. The line
     `KG_SETTINGS autodetect` is logged.
   - Applying a quality change shows "Applying…" for the hitch frame.
   - The test lives in `KillGodot.Settings.AutoDetect`.
6. `run_invariants.ps1` passes (I1–I5).

### Writable scope
- new: `Config/DefaultScalability.ini`, `Tools/Gauntlet/kg_perf_gate.ps1`, `Tools/Gauntlet/perf_budget.json`,
  `Tools/Unreal/kg_perf_tour.py`, the test file `Private/Tests/KGPerfSettingsTests.cpp`
- `Tools/Gauntlet/run_invariants.ps1`: the I5 line and the header comment only
- `Config/DefaultEngine.ini`: `[/Script/Engine.RendererSettings]` and `[SystemSettings]` lines only
- `Core/KGGameUserSettings.*`: first-run auto-detect (header change, so batch it into one close cycle)
- `UI/Menu/SKGSettingsMenu.cpp`: first-run hook and the "Applying…" state (serial)
- `World/KGGrassField.*`, `World/KGFoliageField.*`: foliage quality scaling
- the Low cloud fallback: a new material under `/Game/KillGodot/Env/Sky/` (new path) plus a Low switch on the
  VolumetricCloud actor, driven from C++ in `World/` or `KGMapInfo` (no level edit)
- `Docs/Process/Performance_Plan.md` (results sections only)

### Limits
- 3 fix attempts per failing check; 2 look rounds for the cloud fallback.
- Plateau stop: two rounds without GPU gain on an acceptance spot.
- No downloads.
- Out of scope, as proposals: World Partition/HLOD conversion; DLSS/FSR plugins (these would need a download).

---

## SPRINT-042b — Draw calls and triangles: batch the static village, cull, LOD (L)
**Objective (visible):**
- The same map, visually unchanged at eye level, draws about half as much.
- Decorating with new assets has room in the budget.

### Acceptance (fixed)
1. With 20 characters in view, at all 7 spots:
   - Epic and High ≤ **1500** draw calls, ≤ **300** shadow-depth draws, ≤ **6 M** triangles;
   - Low ≤ **700** draw calls, ≤ **1.5 M** triangles.
   - Measured by the I5 gate.
2. **Actors:**
   - StaticMeshActor count ≤ **2,500** (baseline 7,215). Repeated kit pieces are ISM/HISM per 32 m cell × mesh ×
     material.
   - Interiors are per-house ISM with a 40 m cull.
   - Every placement has a class cull distance.
   - Point lights have MaxDrawDistance 60–80 m.
   - Gameplay actors (doors, seats, chests, stations, breakables) stay separate actors, never spatially loaded.
3. **Level checks unchanged:**
   - `verify_v2_build.py` PASS;
   - real-navmesh check 79/79;
   - `kg_placement_check_v2` clear;
   - the new budgets (actor count, cull distance set on every placement) are added to `verify_v2_build.py`.
4. **Screenshots:**
   - Before/after pairs of `plaza`, `belvedere`, `jetty_back`, `ropewalk` and `night_square` with
     `kg_capture_v2.ps1`, in a contact sheet `Docs/Level/v2_Perf_BeforeAfter.png`.
   - Nothing is missing at eye level. Pop-in beyond 40 m is acceptable.
5. The asset gate of §2.4 is enforced by `kg_import_dress_pack.py`: it warns on a triangle or material budget breach
   and on a missing LOD or cull distance. `run_invariants.ps1` passes.

### Writable scope
- new `Tools/Unreal/kg_batch_static_v2.py`, called from `kg_build_village_v2.py` as a build step
- `Tools/Unreal/kg_build_village_v2.py`: batching hook, cull distances and light MaxDrawDistance only
- `Tools/Unreal/kg_dress_common_v2.py`: cull defaults only
- `Tools/Unreal/kg_import_dress_pack.py`: budget warnings
- `Tools/Level/verify_v2_build.py`: the new budget checks
- **Content:**
  - new merged or ISM assets go under `/Game/KillGodot/Env/Batched/` (new path);
  - `L_Morrowmere_v2.umap` is rebuilt by the builder only after its `.umap` is copied to
    `D:\KillGodot_Snapshots\<stamp>\Content\`.

### Limits
- 3 fix attempts per check; 2 look rounds for the before/after sheet; plateau stop.
- The map is rebuilt headless with the editor closed, or not at all. Never inside the user's open editor.
- Out of scope, as proposals: WP/HLOD, new decoration (a separate sprint within this budget).

---

## SPRINT-042c — CPU for 20 players: ticks, remote characters, animation, character draws (M)
**Objective (visible):**
- A 20-player match on a 4-core PC holds 60 fps.
- The host's own frame is not eaten by bots and replication.

### Acceptance (fixed)
1. **Tick audit:**
   - `dumpticks` and an Insights `-trace=cpu` capture of the tour are summarised in Performance_Plan §3 (new §3.7),
     with a cost per tick class.
   - Ticks per frame: ≤ **150** with no players and ≤ **300** with 20 characters (baseline 304 / 570).
2. **Game thread, reference PC, all tour spots:**
   - ≤ **2.5 ms** avg with no players (baseline 3.3–4.2);
   - ≤ **4.8 ms** avg and p99 ≤ 7 ms with host + 19 bots (baseline 5.0–6.6).
3. **Remote characters:**
   - no Viewmodel, Camera or PhysicsHandle tick;
   - Fishing, Mouth and Emote tick only while active;
   - animation URO on non-local characters beyond 15 m.
   - A test (`KillGodot.Perf.RemoteCharacterTicks`) asserts the tick-enabled component set of a bot.
   - Hit and shove validation on the host does not depend on the animation tick.
4. Draws per character ≤ **12** across all passes (baseline ~27). The outline custom-depth pass only runs within 30 m.
   Measured at bots_plaza as (draws with bots − draws without) / 19.
5. **Invariants:**
   - chat, emote, fish, chore and dig smokes plus the whole-match bot smoke pass;
   - the 20-bot soak `kg_bot_test_v2.ps1` still reaches 20/20 at the gather;
   - `run_invariants.ps1` passes.

### Writable scope
- `World/KGSpinner.*`, `World/KGBuoyancyComponent.*`, `World/KGDoor.cpp`, `World/KGTaskStation.cpp`,
  `World/KGFishSchool.cpp`
- `Chores/WorldChores/KGWorldChoreWorld.*` (AKGChoreSpot tick only)
- `Character/KGCharacter.cpp`: component tick settings only (serial)
- `Character/KGViewmodelComponent.cpp`, `Voice/KGMouthComponent.cpp`
- the fishing and emote component `.cpp` files: tick enable/disable only
- the character outfit assembly, for merging the mesh components
- new test file `Private/Tests/KGPerfTickTests.cpp`

### Limits
- 3 fix attempts per check; plateau stop.
- Header changes go into one batched close cycle.
- No gameplay rule changes (FKGMatchClock, FKGRng, snapshot rules untouched). Findings outside scope go to
  proposals.

---

## SPRINT-042d — Stutter: spawns, PSO cache, sync loads, memory in a cooked build (M)
**Objective (visible):** no hitch when players join, when a new area or material appears for the first time, or when
the match changes phase.

### Acceptance (fixed)
1. **Cooked build:**
   - A cooked Development build of the game (`BuildCookRun`, headless) runs the I5 tour.
   - After a 10 s warm-up, **0 frames over 50 ms** during the tour, excluding the one explicit quality change.
2. **Spawns:**
   - `kg.Bot.Add 19` and 19 simulated joins spawn at most one character per frame.
   - No frame over 50 ms, at most one over 33 ms per join.
   - Outfit and material assets load async beforehand.
   - A `-trace` capture shows no `FlushAsyncLoading` or sync `LoadPackage` during a match.
3. **PSO:**
   - PSO precaching is verified on, and a bundled PSO cache is recorded from the tour and shipped.
   - On a second run with the cache, `PSO/PSOMisses` after warm-up = 0.
   - The QA-002-08 ~430 ms hitch has its source named and fixed, or a proposal if it is engine-side.
4. **Memory in the cooked build:**
   - process RAM ≤ 3.0 GB at Low and ≤ 4.5 GB at Epic;
   - VRAM ≤ 1.6 GB at Low;
   - recorded in `perf_budget.json` and checked by I5.
   - `run_invariants.ps1` passes.

### Writable scope
- `Core/KGGameMode.cpp`: spawn queue only (serial)
- the bot add path in `Dev/KGDevCommands.cpp` (serial)
- the outfit/character asset preload (new file)
- `Config/DefaultGame.ini` / `DefaultEngine.ini`: PSO and cook settings only
- `Tools/Gauntlet/kg_perf_gate.ps1`: the `-Cooked` mode
- `Build/` pipeline files: PSO cache (`.upipelinecache`) under a new path

### Limits
- 3 fix attempts per check; plateau stop.
- A cook takes long: at most 2 cooks per session.
- No downloads.

---

## SPRINT-042e — Real low-tier device check [U] (S)
**Objective:** the tier conversion factors (§1.1) are measured, not guessed.

### Acceptance (fixed)
1. The user, or a friend with the user's OK, runs the cooked build's `kg_perf_gate.ps1 -Device` on one Low-tier
   machine (GTX 1050 Ti / RX 560 class or a Steam Deck) at the Low preset.
2. **Low preset on that device:**
   - p95 ≤ 16.7 ms and p99 ≤ 25 ms at all 7 spots, 1080p output (Deck: 1280×800);
   - game thread ≤ 12 ms with 19 bots.
3. Performance_Plan §1.1 factors are replaced by the measured ratios. Budgets that no longer hold become proposals.

**Depends on:** 042a–042d, and the user's time and hardware. It is added to `Docs/Process/VerificationDebt.md` when
042d closes.

---

## Out of scope (proposals, need approval)
- **World Partition + HLOD conversion of v2.** Only if 042b cannot reach the budgets.
- **DLSS / FSR / XeSS upscaler plugins.** A download; TSR and FXAA are used until then.
- **Web version:** see Performance_Plan §7. Only Pixel Streaming or third-party UE5→WebGPU exist; there is no native
  UE5 web export. Revisit after 042e.
- **Mobile (ES3.1) preview pass** for the master materials (`05_Tech_Architecture §8.1`).
