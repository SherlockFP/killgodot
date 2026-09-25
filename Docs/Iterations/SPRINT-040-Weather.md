# SPRINT-040 — Weather and atmosphere + a graphics "notch" (3 slices, 040a–040c)

Status: **Proposed (needs approval)**, 2026-09-25. Nothing here is in the active queue until the user says so
(LoopContract "Scope changes need the user"). The number is provisional (037–039 are left free for parallel design
work; 042 is Performance).
Design: `Docs/Design/Weather_Atmosphere.md` v0.1 (Turkish). Section numbers (§) below refer to it.
Performance frame: `Docs/Process/Performance_Plan.md` + `SPRINT-042-Performance.md` (tiers, budgets, the I5 gate,
`Config/DefaultScalability.ini`).

User requests (2026-09-25, translated):
- "You can make the visual design and the maps a notch better. You can add a wind system, rain, snow and so on;
  lightning can strike now and then."
- "Improve the graphics a notch: the shadows, the lighting, and the water too." (The water material itself is
  SPRINT-022. This contract only drives its weather parameters.)

## Dependencies and order (read first)
- **Verification debt is 13 (> ~10).** No slice starts before a verification session. 040a changes what the map looks
  like, so it is a content slice, not tooling.
- **040a needs SPRINT-042a** (it owns `DefaultScalability.ini`, the I5 perf gate and `SKGSettingsMenu.cpp`) and
  **SPRINT-022** finished (the water material and the v2 builder). 040a never edits the builder: every new light, volume
  and sky object is spawned at runtime.
- **040b needs 040a** (the atmosphere director and profile) and **Roadmap 020a** (phase checks; forecast lines ride the
  phase machine).
- **040c needs Roadmap 020b** (Event Ledger + witness filter) and **026a** (physical traces). The Mist/torch/campfire hooks
  are wired only if **SPRINT-033b** is merged; otherwise they ship as pure-function tests only and a note in the report.
  Outdoor-candle blowouts need SPRINT-016's world chores (merged or not, the candles are world state, not chores).
- Recommended place in the global order (the block in SPRINT-033/034/035/036):
  after the verification session and 042a: **040a** (can run in parallel with 020a/020b: disjoint files) →
  **040b** (after 020a) → verification session → **040c** (after 020b and 026a; after 033b if the forest ships first).
- **Serial files** (never two sprints at once): `Core/KGGameState.cpp` (040a: one delegation line),
  `Character/KGCharacter.cpp` (040c: footstep multiplier and snow step sound, one hook), `Dev/KGDevCommands.cpp`,
  `UI/KGHUD.cpp` (040b: forecast card), `AI/KGBotController.cpp` (040c), `UI/Menu/SKGSettingsMenu.cpp` and
  `Core/KGGameUserSettings.*` (040c: photosensitivity option; header change, batched into one editor close cycle at a time
  the user chooses).
- **The user may be gaming:** no editor GUI, no windows. Renders, perf and smokes are headless (`-game -RenderOffScreen`),
  one process at a time. Perf runs are marked UNRELIABLE if a game process is running (Performance_Plan §4).
- Assets are written to **new paths** only (LoopContract rollback rule). Re-parenting shipped material instances onto the
  new wind masters is done by a script with a `--revert` mode.

---

## SPRINT-040a — Look pass: shadows, night light, phase grading, sky (M)
**Objective (visible):**
- The village's shadows sit on the ground (contact shadows, GTAO/SSAO), each phase has its own colour grade, and the
  night is readable and beautiful: the Lighthouse Point cliff is no longer a flat black slab, the night sky is navy with
  stars instead of a grey haze.

### Acceptance (fixed)
1. **One source for the look.** `UKGAtmosphereProfile` holds the phase looks (today in `KGGameState.cpp` `GetLook`) plus
   fog colour/density, sky luminance factor, exposure bias, white balance, saturation/contrast, fill lights and local
   exposure per phase (§9.1). `AKGGameState::UpdatePhaseLighting` delegates to the runtime `AKGAtmosphereDirector`; the
   render tour's night look (`kg_capture_v2.py` `NIGHT`) reads the same profile. Test `KillGodot.Weather.Profile`: the
   director's resolved Day/Dusk/Night values equal the profile; the old day look numbers are unchanged.
2. **Night renders, measured ([R]).** New `night_*` renders (`night_lighthouse`, `night_square`, `night_harbour`,
   `night_town`) with the same camera specs as today:
   - cliff ROI in `night_lighthouse`: mean sRGB luminance 0.06–0.20 and std ≥ 0.015;
   - sky ROI (top 20%): mean luminance ≤ 0.12 and mean blue ≥ 1.25 × mean red;
   - clear-night sky ROI: ≥ 30 star points (local-maximum count).
   The script `Tools/Unreal/kg_look_metrics.py` computes these from the PNGs and writes `Saved/KG_LookMetrics.json`. A
   before/after contact sheet `Docs/Level/v2_Look_BeforeAfter.png` (dawn, day, dusk, night × plaza, quay, lighthouse).
3. **Graphics additions on top of 042a's presets** (§8.1–§8.3): contact shadows (High+), SSAO (Medium) / GTAO (High+),
   sun source angle, cascade distribution, sky-light lower hemisphere, shadowless bounce and moon-fill lights, local
   exposure, film tonemapper settings, interior PP on runtime `AKGShelterVolume`s spawned from the layout JSON's
   `"interior": true` houses. Evidence: before/after day renders of 3 streets (`balcony_lane`, `long_ope`, `plaza`) at
   High, judged in the fixed 2 look rounds.
4. **Perf ([R], I5 gate of 042a, reference machine, 1080p, p95 GPU, 7 spots):** against the post-042a baseline, clear
   day: Low ≤ +0.05 ms, Medium ≤ +0.15, High ≤ +0.30, Epic ≤ +0.60; and every preset stays inside Performance_Plan §1.1
   absolute budgets. If a gate fails, the cut order of §8.3 applies (Epic extras → High contact shadow → GTAO to SSAO);
   night light is never cut.
5. **Tier fairness test** `KillGodot.Weather.TierFairness`: at scalability 0–3 the height-fog density, night fill
   lights, local exposure and flash strength are identical (H7).
6. `run_invariants.ps1` passes (I1–I5).

### Writable scope
- new `Source/KillGodot/Weather/`: `KGAtmosphereProfile.*`, `KGAtmosphereDirector.*`, `KGShelterVolume.*`
- `Core/KGGameState.cpp`: `UpdatePhaseLighting` delegates to the director (one call; `GetLook` moves to the profile)
- `Config/DefaultScalability.ini`: **append** the §6.5/§8.1 lines only (after 042a merged; 042a owns the file)
- new `Private/Tests/KGWeatherTests.cpp`
- new `Tools/Unreal/kg_make_atmosphere_assets.py` (headless commandlet: `M_KG_NightSky`, star-dome material params,
  profile asset) and new `Tools/Blender/kg_make_weather_meshes.py` (star dome only in this slice)
- `Tools/Unreal/kg_capture_v2.py`: the `NIGHT` constant reads the profile (one block; coordinate with the SPRINT-022
  owner, add don't reorder)
- new `Tools/Unreal/kg_look_metrics.py`
- `Tools/Unreal/kg_perf_tour.py` (042a's): no change in this slice (it already covers the 7 spots × presets)
- `Docs/06_Art_Direction.md` (a new "Atmosphere and grading" subsection), `Docs/Level/v2_Look_BeforeAfter.png`

### Limits
- 3 fix attempts per failing check; 2 look rounds per acceptance render; plateau stop.
- No Lumen, no VSM, no distance fields, no new shadowed lights below Epic (§8.1).
- Out of scope, as proposals: cliff rock material strata (art, SPRINT-022 owner), moon path on the water (022's material).

---

## SPRINT-040b — Weather core: plan, forecast, wind, rain, fog, network (L)
**Objective (visible):**
- A match now has weather that the town crier forecasts: calm first day, then windy days, rain that makes the stones
  shine and puddles ripple, fog rolling in from the sea. One wind drives the trees, the grass, the flags, the clouds and
  the sea. Everyone sees the same weather, and the rules use exactly what they see.

### Acceptance (fixed)
1. **Plan and rules tests** (`KillGodot.Weather.*`, pure `FKGWeatherRules`, seeded `FKGRng` sub-stream
   `MatchSeed ^ Hash("Weather")`):
   - determinism: same seed → byte-identical plan; adding weather does not change the role list, homes or chores of
     10 fixed existing seeds;
   - over 10,000 seeds: day 1 is only Clear/Windy; Storm ≤ 1 per match (≤ 2 if ≥ 6 days); no severity-≥2 state 3 phases
     in a row; the day before a Storm night is never Clear; mean severity of days ≥ 3 > days 1–2 (§3.2);
   - every non-baseline phase has a forecast published ≥ 20 s before it starts (S10.6);
   - the sight table (§4.2), hearing multipliers with wind carry and the [0.30, 1.30] clamp (§4.1), and footstep
     multipliers (§4.3) return exactly the tabled numbers.
2. **Network smoke** `Tools/Unreal/kg_weather_smoke.ps1` (listen server + 1 client, two headless processes, the
   `kg_chat_smoke` pattern): forced sequence Clear → Windy → Rain → Fog via `kg.Weather.Set`. Both processes log
   `KG_WEATHER clock=… kind=… int=… wind=… wet=… swell=…` at 1 Hz. PASS when, after each blend, kind is equal, intensity
   and wetness within 0.02, wind yaw within 1°, `SwellScale` equal; and the client's replicated data never contains the
   future plan (only the two announced forecasts). Added to `run_invariants.ps1` I3.
3. **Renders per state ([R])** with a new headless tour `Tools/Unreal/kg_capture_weather.ps1/.py` (forces weather with
   `kg.Weather.Preview <Kind> <Intensity> <Phase>` under the plain GameModeBase): Clear, Windy, Rain, Fog × day and
   night × `plaza`, `quay`, `forest edge` (24 PNGs) → contact sheet `Docs/Level/Weather_States.png`. Plus:
   - **interior dry:** camera inside a house during Rain: mean absolute difference to the Clear render inside a mask that
     excludes the window/door opening ≤ 1%;
   - **fog calibration:** a villager mannequin at `KG_SIGHT_DAY`-in-Fog × 0.9 has Weber contrast ≥ 0.10 and at × 1.3
     ≤ 0.04 (§4.2), same for night lit/unlit.
4. **One wind.** `MPC_KG_Weather` drives new wind masters (`M_KG_FoliageWx`, `M_KG_GrassWx`, `M_KG_JapanFoliageWx`, cloth
   and hanging-sway functions), cloud advection and `MPC_KG_Water.SwellScale`. Test: `FKGWaves` reads the same
   `SwellScale` as the MPC at the same clock (sync test next to the existing wave sync note). Evidence: two renders with
   wind yaw 0° and 90° where grass lean, flag direction and cloud drift agree.
5. **Forecast is readable and fun:** the dawn/dusk crier line (ST_Lore keys of §3.3, EN/TR) and the forecast card
   (big icon, ≤ 6-word effect line, ≤ 14-word crier joke, 4 s, then a clock-side icon). UIShots at 1080p and 720p.
   **Perf ([R], I5 tour + weather segments):** Rain vs Clear, same preset: Low ≤ +0.10 ms, Medium ≤ +0.20, High ≤ +0.30,
   Epic ≤ +0.50, and every preset inside its Performance_Plan §1.1 absolute budget while raining.
6. `run_invariants.ps1` passes (I1–I5).

### Writable scope
- `Source/KillGodot/Weather/` (new): `KGWeatherTypes.h`, `KGWeatherRules.*` (pure), `KGWeatherManager.*` (server
  authority, replicated, not spatially loaded, `UKGSnapshotComponent`, SaveGame plan), `KGWeatherFX.*` (local rain box,
  audio layers), plus 040a's director/profile
- `World/KGWaves.h`: `SwellScale` setter and use in `HeightAt` only (keep in sync with `kg_make_ocean.py`)
- `Core/KGGameMode.cpp`: spawn the weather manager and publish forecasts at phase changes (hook lines only; serial file)
- `UI/KGHUD.cpp`: forecast card and clock icon only (serial file)
- `Dev/KGDevCommands.cpp`: `kg.Weather.Set|Plan|Preview|Season|Off` (serial file)
- `Chat/KGChatSubsystem.cpp`: crier lines only
- new `Tools/Unreal/kg_make_weather_materials.py` (MPC, MF_KG_Wind/ClothWind/HangSway/Wetness, M_KG_RainStreaks, the Wx
  masters) with a `--revert` mode for the re-parenting; `Tools/Unreal/kg_make_ocean.py`: `SwellScale` and
  `WhitecapAmount` parameters only (after SPRINT-022 merged)
- `Tools/Blender/kg_make_weather_meshes.py`: rain boxes
- `Tools/Audio/kg_synth_sfx.py`: **append** `S_Wx_*` rain/wind/gust/fog-horn sounds at the end (shared rng: nothing
  earlier may move); import via `kg_import_audio.py --only S_Wx_`
- new `Tools/Unreal/kg_capture_weather.ps1/.py`, new `Tools/Unreal/kg_weather_smoke.ps1`
- `Tools/Unreal/kg_perf_tour.py`: weather segments only (after 042a merged)
- `Tools/Gauntlet/run_invariants.ps1`: the weather smoke line in I3 only
- `Config/DefaultScalability.ini`: append the `kg.Wx.Quality` lines only
- ST_Lore rows (crier lines), `Private/Tests/KGWeatherTests.cpp`, `Docs/05_Tech_Architecture.md` (a weather section),
  `Docs/08_UI_UX.md` (forecast card)

### Limits
- 3 fix attempts per failing check; 2 look rounds per state render; plateau stop.
- No Niagara in this slice. No gameplay effect is wired to the Event Ledger here (that is 040c); hearing/sight numbers
  exist as tested pure functions only.
- Snow, Storm and lightning are 040c.

---

## SPRINT-040c — Lightning, snow, evidence and bot hooks, photosensitivity (L)
**Objective (visible):**
- Storm nights: lightning announced by a flicker, a flash that shows silhouettes for a heartbeat, thunder that arrives
  later the farther the bolt, and a few seconds when nobody can hear anything. Winter matches: snow that settles on roofs
  and keeps every footprint. Rain washes blood and footprints away; snow keeps them. Bots use all of it by their own
  senses. Players sensitive to flashes can dim or turn them off without losing the information.

### Acceptance (fixed)
1. **Evidence-decay and perception tests** (`KillGodot.Weather.*`):
   - trace lifetimes (§4.6), open sky vs sheltered: footprint 60–120 s → Rain 30–60, Storm 21–42, Snow falling 90–180,
     snow cover 180–360, sheltered 60–120; ground stain 45–60 → 23–30 / 16–21 / 68–90 / 90–120; drag 60 → 30 / 21 / 90 /
     180; a weather change mid-life rescales the remaining time; the 256 trace-pool cap holds in snow, oldest footprint
     evicted first;
   - rain never applies the Wet status (`KG_WET`);
   - lightning: strike gaps 20–45 s, pre-flicker 1.5 s, flash window 0.35 s, bolts 250–1200 m from the centre and never in
     the playable area; thunder arrival = distance / 343 m/s per listener; thunder mask ×0.5 for 3 s;
   - accessibility limiter: ≤ 3 pulses per second, ≥ 20 s between sequences, Reduced = 25% single pulse with 200 ms
     ramps, Off = no screen flash.
2. **Witness-filter scenario** (extends 020b's `KillGodot.Evidence.Witness`): a night-storm kill in an unlit area with
   observers at 3 m, 7 m, 15 m, 30 m (all LOS, facing; storm unlit sight is 4 m) and one upwind at `KG_HEAR_KILL` × 0.5 behind a wall.
   Outside the flash window: 3 m sees and identifies, the others do not see. Inside the window: 3 m and 7 m identify, 15 m
   sees a silhouette (`bByFlash`, not identified), 30 m nothing; the wall observer's hearing follows §4.1 (clamped).
   The `Lightning` ledger event lists the `bByFlash` witnesses.
3. **Bots ([U]) in a forced-Storm and a forced-Snow acceptance soak** (N 8 and 20, fixed seeds, frozen parameters of
   §4.7 in the soak header): each weather bot path runs and logs `KG_WX_BOT bot=… reason=thunder|fog|flash|lee|track`.
   S2.1, S2.2 and S3.3 are written per weather state, report only.
4. **Network smoke extension** (`kg_weather_smoke.ps1`): forced Storm night with ≥ 3 strikes and ≥ 2 gusts. Both processes
   log identical strike serials, clock times and positions (≤ 1 cm); the client's flash happens within ±150 ms of the
   server clock time; the client never receives a strike before its pre-flicker (no seed, no schedule replicated); a
   torch blown out by a gust (running, exposed holder) is out on both sides, a walking holder's is not.
5. **Renders and perf ([R]):** storm-night flash sequence (pre-flicker, pulse, after) at `plaza` and `night_lighthouse`;
   Reduced and Off photosensitivity frames showing the outline silhouettes; Snow falling, settled snow cover on roofs and
   terrain, a footprint trail across a stone street → added to `Docs/Level/Weather_States.png`. Perf: Storm and Snow vs
   Clear, same preset: Low ≤ +0.10 ms, Medium ≤ +0.20, High ≤ +0.30, Epic ≤ +0.50, and inside the Performance_Plan §1.1
   absolute budgets.
6. `run_invariants.ps1` passes (I1–I5).

### Writable scope
- `Source/KillGodot/Weather/`: lightning, gusts, snow cover, accessibility limiter, ledger emitters
- `Evidence/` (020b's): the witness filter reads `FKGWeatherRules` sight/hearing multipliers and the flash window
  (a call site, not a rewrite); `Evidence/Traces/` (026a's): lifetime multiplier and the snow soft-ground rule
- `Character/KGCharacter.cpp`: footstep radius multiplier and `S_Wx_Step_Snow_*` choice only (serial file)
- `AI/KGBotController.cpp`, `AI/KGBotView.*` (023's, if merged): the §4.7 weather behaviours through perceived events only
  (G6.1 grep stays at 0)
- Forest hooks (only if SPRINT-033b merged): `KG_MIST_NOTICE` multiplier, torch burn/gust rule, campfire fuel multiplier
  (the campfire numbers need the forest owner's OK, design W4; without it they ship as tests only)
- `Core/KGGameUserSettings.*`, `UI/Menu/SKGSettingsMenu.cpp`: the "Lightning flash: Full / Reduced / Off" option and the
  first-launch photosensitivity notice (header change: one batched editor close cycle; serial files, after 042a)
- `Tools/Unreal/kg_make_weather_materials.py`: `M_KG_Snowfall`, `MF_KG_Snow`, `M_KG_Bolt`; High/Epic rain-occlusion
  capture material; `Tools/Blender/kg_make_weather_meshes.py`: snow boxes, 6 bolt variants
- `Tools/Audio/kg_synth_sfx.py`: append `S_Wx_Thunder_*`, `S_Wx_Step_Snow_*`, `S_Wx_Torch_Out`
- `Tools/Unreal/kg_capture_weather.py`, `kg_weather_smoke.ps1`, `kg_perf_tour.py` (weather segments),
  `Tools/Gauntlet/kg_pillar_soak.ps1` (a `-Weather` switch that forces a state; report columns per weather)
- lobby season option (`Mevsim: Auto / Autumn / Winter`) in the lobby settings code, tests, `Docs/01_GDD_Core.md` §6
  (weather rows in the evidence table), `Docs/08_UI_UX.md` §8 (accessibility)

### Limits
- 3 fix attempts per failing check; 2 look rounds per render; plateau stop.
- One time-boxed attempt at Niagara depth-collision splashes (High/Epic) by duplicating an engine template and setting
  user parameters from Python. If it fails, material ripples stay and the report says so.
- Weather never kills, never reveals a role, never flips a coin during the match (H2, H5). Any tuning beyond the tabled
  numbers goes to the Backlog as a proposal.
- Storm Manor's permanent storm profile (§10) is wired by SPRINT-018, not here.

---

## Out of scope → proposals
Umbrellas, cold/warmth status, lightning-started fires, rainbows, seasonal village dressing, weather-changing roles,
weather effects on fishing, VSM/Lumen experiments at Epic, distance-field AO.

## Verification debt this adds (to `Docs/Process/VerificationDebt.md` when a slice lands)
- 040a: the new night and phase look seen in a real session (editor PIE or a match), including interiors.
- 040b: a real match with a forecast and a rain/fog phase, two players on two machines agreeing on the weather.
- 040c: a storm night and a snow day felt in a real match (flash readability, thunder timing, footprint trails),
  and the Reduced/Off flash options checked by a person.
