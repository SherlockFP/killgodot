# SPRINT-041 code audit — Trapper role, mimic chests, role-ability framework

Read-only audit (no engine access; C++ read + reasoning only) against `Docs/Iterations/SPRINT-041-TrapperRole-Mimics.md`
and CLAUDE.md's non-negotiable code rules. Scope: `Source/KillGodot/Abilities/*` (except `KGTrapperBot.*`, owned by
another agent) and `Source/KillGodot/Traps/*`. Role catalog / reveal-card / role-list-generator files were read for
evidence but not edited (outside the writable scope for this audit).

## Acceptance coverage

| # | Item | Status | Evidence |
|---|---|---|---|
| 1 | Role-ability framework (MVP): server-authoritative, charges/cooldowns/phase, owner-only UI, data-driven, tests incl. no-leak | **Covered** | `FKGAbilityRules::Validate/Commit/Refill` (`Abilities/KGAbilityTypes.cpp:17-69`) is pure and server-called only from `AKGAbilityHolder::AuthUse` (`Abilities/KGAbilityHolder.cpp:272-367`), which runs under `!HasAuthority()` early-out. `RoleId`/`States`/`MyTraps` all replicate `COND_OwnerOnly` (`KGAbilityHolder.cpp:104-106`) and the holder is `bOnlyRelevantToOwner=true`, `bAlwaysRelevant=false` (`KGAbilityHolder.cpp:89-90`). Bar + targeting prompt: `KGAbilityHUD.cpp:102-219`. Data-driven from role catalog: `FKGAbilityCatalog::ForRole` (`KGAbilityTypes.cpp:133-147`) reads `FKGRoleInfo::AbilityIds`. Tests: `KillGodot.Abilities.Rules`, `.Catalog`, `.NoLeak` in `Private/Tests/KGAbilityTests.cpp:16-184`. |
| 2 | The Trapper + Mimic/Snare/Tripwire, reveal card EN/TR ≤12 words, lore flavour | **Covered** | Catalog entry + weights: `Roles/KGRoleListGenerator.cpp:68-71`. Reveal card + flavour: `UI/Reveal/KGRoleCardText.cpp:165-169,295`. Numbers match `Docs/02_Roles.md` §"Tuzakçı" exactly (2/20s/3.2m/12m unseen for Mimic, 2/30s/3.5m/25dmg/5HP-floor/4s for Snare, 2/10s/3.5m/8s rearm for Tripwire) against `KGTrapperTuning` in `Abilities/KGAbilityTypes.h:127-142` and `FKGAbilityCatalog::GetAll()` (`KGAbilityTypes.cpp:103-126`). Mimic bite: heavy damage clamped above `BiteMinHealthLeft`, 3 s hold via `KGTrapHold`, 30 m scream, `BiteMarks` wound, reverts to a normal chest with permanent teeth marks (`Traps/KGMimicTrap.cpp:282-324`). Telegraph (breath/lid/drool) and flinch: `KGMimicTrap.cpp:490-644`, `ServerWatch` throw-test `KGMimicTrap.cpp:326-389`. 2 s public reveal then owner-only: `AKGFieldTrap::IsShownLocally` (`Traps/KGFieldTraps.cpp:86-93`) + `MyTraps` owner-only replication. Dawn expiry: `UKGAbilitySubsystem::OnDawn` (`Abilities/KGAbilitySubsystem.cpp:175-210`). |
| 3 | Deduction hooks: distinct wounds, event log, throw-test flinch, bot avoidance/route-arming | **Covered / Partial** | Distinct, replicated, server-added wounds: `Abilities/KGWoundComponent.cpp` (`BiteMarks`/`SnareWound`, public — correctly *not* owner-only, since a wound is visible evidence). Event log: `UKGTrapSubsystem::Record` (`Traps/KGTrapSubsystem.cpp:85-91`, the SPRINT-040 log reused, not a TODO — the acceptance's "else a TODO" branch doesn't apply). Throw-test flinch: `KGMimicTrap.cpp:344-368` (physics-overlap speed check) plus a scripted case in the `-KGTrapperSmoke` (`KGAbilitySubsystem.cpp:537-575`). Bot avoidance bookkeeping (`NoteMimicScream`/`IsKnownMimic`/`CountKnownMimics`, `KGAbilitySubsystem.cpp:212-241`) is in scope and looks correct; the bot *behaviour* itself (avoiding known mimics, arming along chore routes) lives in `KGTrapperBot.cpp`, explicitly out of this audit's scope — **not independently verified here**. |
| 4 | Balance: GDD numbers, ~1/3 frequency at N≥8, forced-Trapper smoke reaches a winner, Town win rate in band | **Covered (framework side) / Not verifiable (match-level)** | Weighted pick uses `FKGRng` (seeded, no `FMath::Rand`) in `PickWeighted` (`Roles/KGRoleListGenerator.cpp:339-364`); `KillGodot.Abilities.TrapperFrequency` asserts 25–45% per N and 28–40% overall for N≥8, and that a forced Trapper always lands exactly once (`Private/Tests/KGAbilityTests.cpp:187-241`). The 20-seeded-bot-match win-rate and "forced Trapper still reaches a winner" match smoke cannot be run or verified from this container (no engine). |
| 5 | Evidence: screenshots, 2-process network smoke, invariants green | **Not verifiable here / smoke code present** | No engine access, so screenshots and `run_invariants.ps1` can't be produced or run in this session. The two-process smoke exists and is thorough: `-KGTrapperSmoke` in `UKGAbilitySubsystem::TickSmoke` (`KGAbilitySubsystem.cpp:266-685`) drives host (Trapper) + client (victim) through setup → unseen-rule check → arm → throw-test flinch → bite → chest reset with teeth marks → tripwire cross → snare catch, all through the real `AuthUse`/`Interact` paths, not shortcuts. |

## Findings

### Medium

1. **Iris-readiness gap: no FastArraySerializer / push-model on the new replicated arrays.** CLAUDE.md's non-negotiable
   rules require "Iris-ready replication (registered subobject lists, FastArray, push-model macros)". `AKGAbilityHolder::States`
   and `::MyTraps` (`Abilities/KGAbilityHolder.h:104-109`) replicate as plain `TArray<>` with whole-array `DOREPLIFETIME_CONDITION`
   and manual `ForceNetUpdate()` calls, not `FFastArraySerializer` + `MARK_PROPERTY_DIRTY`. Several sibling systems in the same
   module already use the FastArray/push-model pattern (e.g. `Chat/KGChatComponent.cpp`, `Inventory/KGInventoryComponent.cpp`,
   `Emote/KGEmoteComponent.cpp`, `UI/Reveal/KGRevealComponent.cpp`), so this is a real deviation from the established
   convention, not just an MVP shortcut. Impact today is low (both arrays are ≤3 elements, updated rarely — ability use and
   dawn refill), so it is not a functional bug, but it is worth a Backlog "Proposed (needs approval)" item before Iris
   migration, since converting `States`/`MyTraps` to FastArray means adding a wrapper `USTRUCT` with `FFastArraySerializer`,
   which is a header/class-layout change requiring a full close-cycle rebuild — out of scope to do inside this audit.
   *Left as a proposal, not fixed.*

### Low / informational

2. **Stray empty file `KGAbilityTests.cpp` at the repo root.** `git log` shows it was added empty (`e69de29`, the empty
   blob) in the SPRINT-041 WIP checkpoint commit `8aedc92`, alongside the real, populated test file at
   `Source/KillGodot/Private/Tests/KGAbilityTests.cpp`. It is **not** a duplicate with different content — it is 0 bytes —
   and it is not referenced by any `.Build.cs`/module, so it does not compile into anything; it looks like an accidental
   `touch`/wrong-path artifact from whatever tool wrote the real test file. It sits outside this audit's writable scope
   (repo root, not `Abilities/`/`Traps/`), so it was left alone. *Proposal: delete `KGAbilityTests.cpp` at the repo root
   (`git rm KGAbilityTests.cpp`) the next time someone is touching that area — nothing depends on it.*
3. **Magic number instead of a named tuning constant (fixed in this audit).** `AKGSnareTrap::OnFire` computed the
   "never below N HP" floor as a bare `5.0f` (`Traps/KGFieldTraps.cpp`), unlike the Mimic's identical floor which is the
   named `KGTrapperTuning::BiteMinHealthLeft`. Since the sprint's own comment on `FKGAbilityCatalog::GetAll()` says to
   keep the GDD and the code numbers in sync, an un-named constant duplicated between the doc and the code is a small
   drift risk. **Fixed**: added `KGTrapperTuning::SnareMinHealthLeft = 5.0f` next to the other Snare constants in
   `Abilities/KGAbilityTypes.h`, and `KGFieldTraps.cpp`'s `OnFire` now uses it. Purely additive (a new `constexpr` in an
   existing namespace, not a `UPROPERTY`/class-layout change), matches the existing `BiteMinHealthLeft` idiom exactly, and
   the file already included `Abilities/KGAbilityTypes.h`, so no new include was needed.
4. **Personal (owned/locked) chests: lock check runs before the mimic-bite check.** `AKGStorageChest::Interact_Implementation`
   checks `CanPlayerUseContainer(PS)` (the lock) before `AKGMimicTrap::TryBite` (`World/KGStorageChest.cpp:229-256`, read
   for context, not owned by this audit). A non-owner who is blocked by the lock never reaches the bite check, so a
   Trapper mimicking someone else's *locked personal* chest can only ever bite the owner, not a third party who is turned
   away at the lock. This matches the design intent ("the next person who opens it") for the common case (unowned world
   chests/crates/barrels) and is not something to change without a design call on locked containers, so it is noted only,
   not treated as a bug.
5. **`AKGMimicTrap::FitOverlays()` keeps the teeth-marks decal visible while a spent mimic is re-armed.** `bTeethMarks`
   is never cleared, so `bShowMarks = bTeethMarks && !bBite` stays true through `Idle`/`Armed`/`Cooldown` once a chest has
   ever bitten someone — including a fresh re-arm of the *same* chest. This reads as consistent with the GDD's "diş izleri
   kalır" (the teeth marks remain) being a **permanent** tell once a container has bitten, so a careful player recognizing
   a previously-bitten chest again seems intentional rather than a bug; flagged here in case that permanence was meant to
   be reset when a chest is deliberately re-armed for a fresh ambush (a design question, not a code defect).
6. **`FindContainer` (`Abilities/KGAbilityHolder.cpp:23-59`) is an O(all actors) scan on every Mimic use/aim tick.**
   Fine for how rarely the ability fires (a handful of times a night, plus the HUD's per-frame aiming-preview call while
   a player is holding Alt+1), but worth knowing if a future ability with a tighter latency budget reuses it.

## Rule compliance checked and passing (no issues found)

- Randomness: `PickWeighted` (role-list weighting) and any Trapper-adjacent randomness use `FKGRng`; no `FMath::Rand` in
  `Abilities/`, `Traps/`, or the touched parts of `Roles/`.
- Time: all trap/ability durations are `FKGMatchClock` (`FKGAbilityState::Cooldown`, `FKGTrapMachine::Clock`); no
  `TimerManager` usage found in either folder.
- PUID keying: `AKGTrap::ArmedByPuid`/`IsArmer`, `UKGAbilitySubsystem::NotifyArmer/IsKnownMimic`, and
  `AKGAbilityHolder::AuthSetup`'s dev-PUID fallback all key off `AKGPlayerState::Puid`, never a pointer/PlayerId.
- Secret replication: `AKGAbilityHolder::RoleId/States/MyTraps` are `COND_OwnerOnly`; `AKGTrap::ArmedByPuid` is
  `SaveGame` only (no `UPROPERTY(Replicated...)` at all) — both directly asserted by
  `KillGodot.Abilities.NoLeak` via reflection, including a `TFieldIterator` sweep of every Mimic/Snare/Tripwire
  property name for the words "Armed"+"By"/"Role"/"Puid".
- Snapshot: `AKGAbilityHolder` and `AKGTrap` both own a `UKGSnapshotComponent`; server-only fields (`RoleId`, `States`,
  `ArmedByPuid`, `Machine`, `LockedDoorNames`, `bTeethMarks`) are `SaveGame`.
- World Partition: both actor classes set `bIsSpatiallyLoaded = false` under `WITH_EDITORONLY_DATA`.
- Server validation of the ability RPC: `AuthUse` re-measures phase, alive state, line-of-sight/unseen, and target
  distance from server-side data; the client's `ViewStart` is clamped to within 200 cm of the body's actual eye
  location (`KGAbilityHolder.cpp:292-296`) rather than trusted outright. Dead callers, wrong phase, spent charges,
  live cooldowns, out-of-range/no-target and seen-by-others are all refused with the correct `EKGAbilityDeny`, matching
  `KillGodot.Abilities.Rules`.

## Fixes applied

- `Source/KillGodot/Abilities/KGAbilityTypes.h`: added `KGTrapperTuning::SnareMinHealthLeft = 5.0f`.
- `Source/KillGodot/Traps/KGFieldTraps.cpp`: `AKGSnareTrap::OnFire` now uses `KGTrapperTuning::SnareMinHealthLeft`
  instead of a bare `5.0f`.

Both are additive/behaviour-preserving (same numeric value, matches an existing idiom in the same file), touch no
`UPROPERTY`/`UFUNCTION`/class layout, and need no rebuild beyond a normal Live Coding `.cpp`-body recompile.

## Left as proposals (not fixed)

- Convert `AKGAbilityHolder::States`/`MyTraps` (and, more broadly, `AKGTrap::Witnesses` from SPRINT-040) to
  `FFastArraySerializer` + push-model (`MARK_PROPERTY_DIRTY`) for Iris readiness — needs new header-level wrapper
  structs, so it needs a full close-cycle rebuild and is a poor fit for a code-only audit pass.
- Delete the empty, untracked-by-any-build stray file `KGAbilityTests.cpp` at the repo root (outside this audit's
  writable scope).
- Whether a re-armed, previously-bitten mimic should hide its teeth-marks decal while `Armed` again (design question,
  §Finding 5).
