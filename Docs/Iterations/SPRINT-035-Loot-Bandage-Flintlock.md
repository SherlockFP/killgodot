# SPRINT-035 — Breakable boxes, bandages and the crafted flintlock (6 slices, 035a–035f)

Status: **Proposed (needs approval)**, 2026-09-25.
Design: `Docs/Design/Loot_Crafting_Weapon.md` v1.1 (§3–§9, §11).

User request (2026-09-25, translated): "Boxes around the map that you can break, with a chance of loot inside:
bandages, weapon parts. Combining the parts makes a weapon, like GMod Murder. Balance and contents are up to us."

## Dependencies and order (read first)
- **Verification debt is 12 (> ~10):** no slice starts before a verification session.
- 035a rides after Roadmap **020a** (same serial `KGGameMode.cpp` window). 035b follows 035a. Both give visible
  progress early and need no core-loop sprint.
- The gun needs the core loop, or charter Ç2/Ç8 fail: 035c after **020b**; 035d after **021** (wound class), **022**
  (side-true solver: who is "innocent") and the **FPArms2** viewmodel check (verification debt #1); 035e after **023**
  (bot mind); 035f after **024** (Town Board). The Kurban Hakkı rule (every gun kill by a threat-side player spends the
  team's kill right) is only a stub until Roadmap **027b** adds kill rights.
- Recommended global order (same block in 033–036):
  0. Verification session (user). 1. SPRINT-016 finisher. 2. Roadmap 020a + 036h, with 033a in parallel.
  3. Roadmap 020b, with 036a in parallel. 4. **035a**. 5. **035b**. 6. 033b, 036b. 7. 034a, 033c, 036c. 8. 034b.
  9. Verification session. 10. **035c** (after 020b), **035d** (after 021, 022, FPArms2 check), 036d and **035e**
  (after 023), **035f** and 036e (after 024), 033d.
- Serial files: `Character/KGCharacter.cpp`, `Core/KGGameMode.cpp`, `AI/KGBotController.cpp`,
  `Dev/KGDevCommands.cpp`, `UI/KGHUD.cpp`. No two 035 slices run at once.
- Code facts this contract relies on (verified by reading): placed containers have `LootSeed = 0`, so every match gives
  the same loot; `AKGBreakable` has no `UKGSnapshotComponent`; there is no item-use path
  (`KGInventoryRPCComponent` has only Transfer/Close/SetLocked/StandUp/Drop; `Heal()` is called only by the dev
  panel); the layout JSON has no `loot_spots` or `forge_bench` keys yet.

---

## SPRINT-035a — Breakable boxes v2 (S)
**Objective (visible):** crates, barrels and pots break in 1–2 punches, spill loot that now includes bandages and rusty
nails, and are whole again at every dawn with different contents.

### Acceptance (fixed)
1. **Test** `KillGodot.Loot.Containers`: container seed = hash(MatchSeed, Day, PersistentId); same seed + day → same
   loot, different match seed → different loot (the `LootSeed = 0` repeat is gone); table distribution over 100 000
   draws within ±1% of design §3.2; pot health 20 (1 punch), crate and barrel 35 (2 punches).
2. **Test:** dawn respawn by hide/show (no `SetLifeSpan(15)`), ≤ 6 respawns per frame spread over 12 s; broken/whole
   state and contents round-trip through `UKGSnapshotComponent` SaveGame.
3. **[U]** 8-bot x1 soak: every bot logs `KG_LOOT`; no container respawns outside Dawn.
4. Shot `KG_Cap_crate_burst.png`; UIShot of the inventory with Bandage and Rusty Nails. `run_invariants.ps1` passes.

### Writable scope
- `World/KGBreakable.*`, `Inventory/KGLoot.cpp`, `Inventory/KGItemCatalog.cpp` (Bandage, Scrap as plain items)
- `Core/KGGameMode.cpp`: Dawn hook only, serial
- `AI/KGBotController.cpp`: break path only, serial
- an empty `EmitLootEvent` hook (bound when 020b lands), tests

### Limits
- 3 fix attempts per check; 2 look rounds for the burst; plateau stop.
- Header changes in one batch at a time the user chooses; never close or relaunch the user's editor.
- Out of scope → proposals: runtime `loot_spots` top-up (035c), the Loot Director.

**Depends on:** verification session, Roadmap 020a.

---

## SPRINT-035b — Item use, the bandage, and a shared status component (M)
**Objective (visible):** hold to bandage yourself (+30 HP) or a standing friend (+40 HP); taking damage, being shoved,
attacking or sprinting cuts it short; a bloody bandage lies where it was used. One status component carries Wet, Limp
and slow effects for every system (the forest uses it too).

### Acceptance (fixed)
1. **Test** `KillGodot.Loot.Bandage`: self 1 s open + 5 s (+30); other 1 s + 6 s (+40), both stationary; cancel
   before the open keeps the item; after the open the item is spent and heal so far stays; interrupts: damage, shove,
   attack, sprint, start of the Meeting phase (the bell joins with 021), being targeted by a Mist tongue; HP never
   > 100; all timers from `FKGMatchClock`.
2. **Test** `KillGodot.Status`: Wet (60 s, 10 s within 3 m of any fire), Limp (sprint 5.8 → 4.0 m/s, walk unchanged),
   slow multipliers stack by the documented rule; private flags `COND_OwnerOnly`, public visuals (dripping) replicated;
   SaveGame round trip.
3. **Two-process smoke:** the client bandages the server player; both ends see HP rise; a client use request with no
   bandage or in the wrong phase is rejected (G7.2).
4. **[U]** bots with HP ≤ 60 and no threat within 15 m log `KG_BANDAGE`. Shots: the Bloody Bandage prop (90 s) and the
   heal shadow on the HP bar. `run_invariants.ps1` passes.

### Writable scope
- new `Inventory/KGItemUseComponent.*` and a new subsystem that attaches it (the `KGPlayerExtrasSubsystem`
  zero-integration pattern, separate file)
- new `Character/KGStatusComponent.*`, new `World/KGBloodyBandage.*`
- `Combat/KGHealthComponent.*` (heal over time)
- `UI/` HP-bar shadow + one hook in `UI/KGHUD.cpp`, serial
- `AI/KGBotController.cpp`: bandage path only, serial
- tests

### Limits
- 3 fix attempts per check; 2 look rounds; plateau stop; header changes in one user-timed batch.
- Out of scope → proposals: Mr. Thimble's shop (bandage price proposal 15 Bakır, max 1 per player per day), Doctor
  values (Roadmap 027a).

**Depends on:** 035a.

---

## SPRINT-035c — Loot Director, gun parts, the Forge bench, the belt (M)
**Objective (visible):** Barrel, Flintlock and Grip parts turn up in boxes on a small, day-ramped budget; with one of
each you forge a serial-numbered flintlock at the Brookside forge (8 s, anvil heard at 35 m); it hangs visibly on your
belt. It cannot fire yet.

### Acceptance (fixed)
1. **Test** `KillGodot.Loot.Director` (pure, 10 000 seeds): cap schedule G(N, d) exactly as design §3.3 (N ≤ 7: 0;
   8–11: 1 from day 3; 12–15: 1 from day 2, 2 from day 4; 16–20: 1 on day 2, 2 from day 3); stock P = 3·G(N, d+1) + 1;
   guns on belts + complete pocket sets ≥ G(N, d+1) → 0 parts; rarest-first counts only container stock; forest-band
   containers and containers within 8 m of a home are excluded; alive ≤ 4 → 0 parts; a gun not on a living belt at
   dawn expires; cap violations 0; the L1 stress case (Impatient pooling 100%, Town trading 80%) reproduces design §4.2
   gun-days within ±20%.
2. **Test:** one part of each type per pocket; every chest refuses the flintlock; forging takes 8 s server time and is
   refused before the forge day, at the cap, while banned, or when moving/hit; serials increment; a `Craft` event
   (saw / heard 35 m) reaches the 020b ledger.
3. **Two-process smoke:** the client forges; both ends agree on the serial and the belt; the other client sees the
   pistol on the belt in third person.
4. Shots: inventory with the belt slot and set tracker; the forge progress ring; the belt pistol in 3P at 5 m.
   `run_invariants.ps1` passes.

### Writable scope
- new `Loot/KGLootDirector.*`, new `World/KGForgeBench.*`, new `Weapons/KGBeltComponent.*`
- `Inventory/KGItemCatalog.cpp`, `Inventory/KGLoot.cpp`, `World/KGStorageChest.cpp` (refuse rule only)
- `Core/KGGameMode.cpp`: dawn → director hook, serial
- layout JSON keys `forge_bench`, `loot_spots` (data only; runtime spawn, the v2 build line is untouched)
- new Blender scripts and new content paths for the part and pistol meshes; tests

### Limits
- 3 fix attempts per check; 2 look rounds for meshes; plateau stop; header changes in one user-timed batch.
- Any mesh download needs the user's approval first (name, source, size).

**Depends on:** 035b, Roadmap 020b.

---

## SPRINT-035d — Flintlock rules, the private innocent penalty, first-person clips (L)
**Objective (visible):** draw (0.8 s), cock (0.4 s, click heard at 8 m), fire: 100 damage within 12 m, a flat 40 out
to 40 m. A shot that leads to a death cracks the gun. Killing an innocent blurs only your own screen for 30 s and bans
you from guns for the match, and nobody else can see it.

### Acceptance (fixed)
1. **Test** `KillGodot.Gun.Rules` (pure `FKGGunRules`), ≥ 20 cases: timings; damage step (100 ≤ 12 m, 40 at
   12–40 m, miss > 40 m); 30 s attribution (wounded victim finished by a punch or a wolf → the shooter gets the
   penalty); one shot, one gun (cracks on any death it causes, not on a protected target); ban blocks draw, forge and
   pickup; refusal in Meeting, Trial and Epilogue (S9.1); no random spread; the threat-side Kurban Hakkı hook is
   called (stub until 027b).
2. **Test (L7):** replication audit: no replicated property, multicast or HUD line reveals the penalty or the victim's
   side to anyone but the shooter (checked on a second client at runtime).
3. **Two-process smoke:** the client shoots a server bot at 10 m → dead, gun cracked on both ends; at 20 m → 40
   damage; lag-compensated through `ValidatedViewStart`.
4. First-person clips (draw, holster, idle, cock, aim, fire, misfire, reload, inspect) from `kg_make_fp_arms2.py` at
   new paths; shots `KG_Cap_flintlock_fp.png` and the aim pose; 2 look rounds.
5. `run_invariants.ps1` passes.

### Writable scope
- new `Weapons/` (`FKGGunRules`, `UKGGunComponent`)
- `Character/KGCharacter.cpp`: input hooks and the `KGFP2::Grips` row, serial
- `Combat/KGHealthComponent.*` (last gun-hit record for attribution)
- `Tools/Blender/kg_make_fp_arms2.py` (new clips only) + new content paths
- `UI/` (private penalty screen, cock icon), tests, `01_GDD_Core.md` §4 (weapon lines only)

### Limits
- 3 fix attempts per check; 2 look rounds; plateau stop; header changes in one user-timed batch.
- Out of scope → proposals: shotgun and harpoon (GDD §4, not recommended), gun skins, the Vigilante role gun.

**Depends on:** 035c, Roadmap 021, 022, FPArms2 verified.

---

## SPRINT-035e — Bot gun paths (S)
### Acceptance (fixed)
1. **[U]** soak N = 12, 2 seeds: 100% of Town bot shots log `KG_SHOT … reason=<eventId>`; `reason=random` 0; the
   Impatient `KG_SINK` path runs; trades use the bot-mind trust score; bot aim reaction ≥ 0.4 s with a frozen error.
2. **Grep test (G6.1):** no `AI/` call reads another player's inventory outside `KGBotView.cpp`.
3. L2, L3 and L4 reported from 40 bot matches (report only). `run_invariants.ps1` passes.

### Writable scope
- `AI/` (gun paths), tests. Serial with any other `KGBotController.cpp` writer.

### Limits
- 3 fix attempts per check; plateau stop.

**Depends on:** 035d, Roadmap 023.

---

## SPRINT-035f — Wet powder, sinking, disarming, the forge line on the Town Board (S)
### Acceptance (fixed)
1. **Test:** Wet from waist-deep water **or** a Mist Wall return → only a pan flash, 1.5 s recock; dries in 60 s or in
   10 s within 3 m of a fire; a front shove always drops a drawn gun, never a belted one; a sunk gun logs `GunSunk` and
   frees the cap at the next dawn.
2. **Test:** the Town Board line "Forge: No. N forged (Day d, hh:mm)" carries no name; streamer mode masks names in
   ledger lines.
3. **Two-process smoke:** push the client into the sea → the client sees the drip icon, the server has the wet flag;
   standing at a campfire dries it.
4. `run_invariants.ps1` passes.

### Writable scope
- `Weapons/`, `Fishing/` (water-entry hook only), the Town Board hook in Roadmap 024's files (hook only), tests

### Limits
- 3 fix attempts per check; plateau stop.

**Depends on:** 035d, Roadmap 024.
