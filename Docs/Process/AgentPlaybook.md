# Agent playbook: how to develop KillGo with several AI agents

This is the working guide for whoever continues the project (any AI tool). `HANDOFF.md` has the current state and
queue; `LoopContract.md` has the sprint rules. This file covers how to organise the work.

## 1. Roles
| Role | Who | Job |
|---|---|---|
| **Director** (main session) | the strongest model, one only | Talks with the owner in Turkish, turns requests into sprint contracts, splits the work, launches and briefs workers, reviews their results, runs the final gates, commits. Writes little code itself: only small, cross-cutting fixes. |
| **Worker** | cheaper model (e.g. Fable, Sonnet), 2–3 in parallel | One sprint contract each, in its own writable scope. Builds, tests, captures evidence, writes the Result section. Does not commit. |
| **Reviewer** (optional) | one per big design | Reads a design or a finished sprint once and lists concrete problems. It never reviews its own work. |

The owner is the product owner: they decide on "Proposed" items and give look/feel feedback. They don't read code.

## 2. The loop (one iteration)
1. **Collect.** Batch the owner's messages into `Docs/Backlog.md` (one line each, their words translated). Don't start a
   new workflow per message.
2. **Contract.** For each item, write `Docs/Iterations/SPRINT-NNN-Name.md` with:
   - the user quote;
   - **Acceptance (fixed)**: numbered, testable, with the evidence expected (test names, smokes, shots);
   - **Writable scope**: folders and files the worker may touch;
   - **Limits**: 3 fix attempts per failing check, 2 look rounds, plateau stop; extras go in as proposals.
   Copy an existing contract (SPRINT-040, 041) as the template.
3. **Split.** Give each worker a disjoint scope (see §3). If two items need the same file, run them one after another
   in the same worker.
4. **Launch.** Use the brief template in §4. Run workers in the background, 2–3 at a time.
5. **Integrate.** When a worker reports, the director reads its diff (`git diff --stat`, then the risky files), runs
   `run_invariants.ps1` under the lock, and commits with a message listing what changed and the evidence.
6. **Show.** Tell the owner in Turkish what is now visible in the game, with shot paths. Add unseen features to
   `VerificationDebt.md`. When the debt passes ~15 items, ask for a play session before new features.

## 3. Ownership map (avoid collisions)
| Area | Paths |
|---|---|
| Character / movement / carry | `Source/KillGodot/Character/*`, `Combat/*` |
| Roles / abilities / traps | `Abilities/*`, `Traps/*`, `Roles/*` |
| Chores | `Chores/*`, `WorldChores/*`, chore data |
| UI / HUD / menus | `UI/*` (KGHUD.cpp is one big shared file: one owner at a time) |
| Village map | `Tools/Level/author_layout_v2.py`, `Tools/Unreal/kg_build_v2_all.ps1`, `dressing/v2/*.py`, `L_Morrowmere_v2.umap` |
| Manor map | `Tools/Level/*stormmanor*`, `Tools/Unreal/kg_*stormmanor*`, `kg_sm_dress.py`, `Manor/*` |
| Tabletop | `Tabletop/*`, `dressing/v2/dress_tabletop.py` |
| Voice / social | `Voice/*`, `Emote/*`, `Chat/*` |
| Build and gates | `Tools/Gauntlet/*`: the director only |

Shared hot spots:
- `KGCharacter.cpp`, `KGHUD.cpp`, `KGGameMode.cpp`, `KillGodot.Build.cs`: surgical edits only, and tell the director.
- `.umap` files: only through the build pipelines, never by hand. Rebuilds overwrite manual edits.

## 4. Worker brief template
```
You implement Docs/Iterations/SPRINT-NNN-Name.md in the UE 5.8 project at D:\Kill Godot. Read the contract,
Docs/Process/LoopContract.md and HANDOFF.md first. <2–4 lines of context: what exists to build on, file names>.
HARD RULES (other agents work in parallel):
- Every UE use (Build.bat, run_gates/run_invariants, -game/-nullrhi processes, commandlets, pipeline steps) runs under
  `python Tools/Gauntlet/kg_ue_lock.py "<name>: <what>" -- powershell -NoProfile -ExecutionPolicy Bypass -File ...`.
  Keep each hold short (one build, one smoke, <= 4 matches).
- Stay in the writable scope. Do not edit <other workers' paths>.
- Headless only: no windows, no editor GUI. Never kill processes you did not start.
- Don't commit. At the end, write the "## Result" section and add proposals to Docs/Backlog.md.
- Final report (max 15 lines): results with numbers, changed files, evidence paths, open items.
```

## 5. Verification rules (what "done" means)
- The C++ builds (`run_gates.ps1` G1) and all automation tests pass (G2). New logic gets pure, unit-tested helpers
  (see `UKGHealthComponent::RegenStep`, `FKGVoiceRules`).
- Networked features get a two-process headless smoke (listen server + client, `-nullrhi`, optional PktLag) that
  prints `KG_*` log markers the script checks.
- Visual features get offscreen captures (`-game -RenderOffScreen` + HighResShot, or named `shot:` captures) and
  up to 2 look rounds: look at the PNGs, fix, capture again.
- Map changes: `verify_v2_build.py` / `verify_stormmanor_build.py` plus the placement check (0 floating, 0 sunk,
  stairs fit).
- The whole game: `run_invariants.ps1` stays green (I4 = a full bot match reaches "Match decided:").
- Never call something done without evidence. If a check is skipped, say so.

## 6. How to grow the game (design direction)
- **Visible progress first.** The owner judges by what is on screen. Every sprint must add something they can see or
  play, with shots to prove it.
- **Pillars:** `Docs/Design/KillGo_Pillars.md`. Short matches with a visible day count; roles stay secret; deduction
  from evidence (bodies, wounds, event log, witnesses); physical, simple-but-fun chores; social play (voice, emotes,
  barks); two maps with their own identity.
- **Owner feedback so far:**
  - They reject boring or text-heavy UI ("SIKICI"): 6-second moments, 12 words or fewer.
  - They reject samey maps: varied houses and a real layout design, not props placed side by side.
  - They reject clunky chores: keep them simple but fun.
  - They reject puppet-like models: keep the vibrant Quaternius villagers.
- **Balance** through seeded bot matches (`kg_match_smoke.ps1`, `kg_trapper_balance.ps1`): the Town win rate stays in
  0.35–0.65.
- **Assets:** free packs under `/Game/KillGodot/Env/Ext` (catalogue: `kg_asset_catalog.py`). Dedupe against what
  exists before downloading. Record licences.
- **Performance** is a requirement ("everyone should be able to play"). New content must respect the budgets in
  `Docs/Process/Performance_Plan.md`.

## 7. Usage budget
- The owner has a limited plan. Use one director and 2–3 cheap-model workers. No fan-out workflows with many critics.
- Keep briefs and reports short.
- When a usage limit cuts agents off:
  1. Check the build.
  2. Commit a "WIP checkpoint" listing what is unfinished.
  3. Relaunch finishers with that context. Cut-off agents cannot be resumed.
