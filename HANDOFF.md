# HANDOFF — KillGo (2026-09-26)

For the next AI assistant (any tool). Read this first, then `CLAUDE.md` (project rules),
`Docs/Process/AgentPlaybook.md` (how to work: director + parallel workers, ownership map, brief template,
verification, design direction), then `Docs/Backlog.md`.

**Where it stopped and what the owner asked for over time:** `Handoff/ChatHistory.md`. Raw chat transcripts (local, not
in git): `Handoff/History/`.

## What this is
**KillGo** (internal/code name KillGodot): first-person online social deduction game (Town of Salem × GMod Murder ×
Among Us), 6–20 players, listen-server P2P (EOS later). **UE 5.8.3**, C++ in `Source/KillGodot/`, Blender 5.2 for
generated assets. Maps: `L_Morrowmere_v2` (village, main map) and `L_StormManor` (56-room manor).
The owner speaks Turkish, reply in Turkish. They delegate everything and judge progress by what is visible in the game.

## Owner's rules (keep them)
- Work in the background: nothing may open a window or steal focus (they play games meanwhile). Headless only
  (`UnrealEditor-Cmd`, `-game -nullrhi -RenderOffScreen`, Blender `--background`). If they closed the editor, do not
  relaunch it.
- Git: local commits are fine, end the message with a co-author line of your own. Remote since 2026-09-26:
  `origin` = https://gitlab.com/sologp-group/sherkillgodot (private, Git LFS). The real branch is `master`; the remote
  `main` only holds GitLab's initial commit. Push only when the owner asks; never force-push.
- Free assets are allowed (any licence, record the risk in the docs); no purchases, accounts or logins.
- Role secrecy: appearance is per player, never per role.
- "Keep it simple but fun": chores 1–3 steps, 25–50 s.

## How to build and check (all headless)
- Build + automation tests: `powershell -File Tools/Gauntlet/run_gates.ps1` (G1 build, G2 tests; last: 93/93).
- Full invariants: `powershell -File Tools/Gauntlet/run_invariants.ps1`
  (I1 build+tests, I2 v2 map verify, I3 smokes chat/emote/fish/chore/dig/voice/partner, I4 bot match smoke).
  Last run 2026-09-26: I1–I3 PASS, I4 interrupted by the owner (run it again first).
- Village rebuild: `Tools/Unreal/kg_build_v2_all.ps1 [-From n -To m]` (9 steps: terrain → village → dressing →
  navmesh → material check → capture → placement check). Manor: `Tools/Unreal/kg_build_stormmanor*.ps1`.
- **Parallel agents:** wrap every UE use in `python Tools/Gauntlet/kg_ue_lock.py "<who: what>" -- <command>`.
  A running `-game` process locks the DLLs (LNK1104 in the next build) and two commandlets saving the same .umap fail
  silently (this broke the forest navmesh once).
- After an unclean editor exit, see the "autosave restore" procedure in `CLAUDE.md`.

## Process
`Docs/Process/LoopContract.md`: one sprint contract per feature in `Docs/Iterations/SPRINT-xxx-*.md` with fixed
acceptance, writable scope and limits (3 fix attempts, 2 look rounds, plateau stop). Write a "## Result" section when
done. Features built but never seen by a human go into `Docs/Process/VerificationDebt.md` (21 items now; the owner
should do one play session to clear them).

## State at handoff (last commit f7255e5)
Done and committed: map v2 + dressing + forest ring (wolves, the Mist, forest chores), underground/digging, 10 world
chores + 22 minigames, UI/HUD pass, hold-M big map, movement/bunny hop (0 net corrections), 38 villager looks, reveal
ceremony, streamer mode, proximity voice + mouths + barks + partner emotes, 2D tabletop chess/draughts rules, Storm
Manor expansion (secrets, compartments, traps, manor chores), health feel (hit flinch, hit marker, second wind to 60 HP).

**Uncommitted work in progress — SPRINT-041 Trapper role + mimic chests** (`Docs/Iterations/SPRINT-041-TrapperRole-Mimics.md`):
the code is in, but the balance run is missing. Uncommitted: `Source/KillGodot/Abilities/{KGAbilityHUD,KGAbilitySubsystem,KGTrapperBot}.cpp`,
`Tools/Gauntlet/kg_trapper_balance.ps1` (-ForceRole / -OutName / -PortBase for Enforcer control runs),
`Tools/Unreal/kg_trapper_shots.py`. Review with `git diff`, build, then:
1. 20 seeded bot matches with a forced Trapper, plus the same seeds with `-ForceRole Enforcer`. The Town win rate
   must be 0.35–0.65. The 3 partial runs in `Saved/Logs/TrapperBalance` were all "Impatient win", so the Trapper may
   be too strong.
2. Two-process network smoke (arm → victim opens → bite → reset) and the shots.
3. Write the Result section, then commit.

## Queue (in the owner's priority order)
1. Finish SPRINT-041 (above).
2. SPRINT-042 `Docs/Iterations/SPRINT-042-PhysicalChess.md`: a 3D board at the tables, hold-E pieces (Source style),
   the server validates with `Tabletop/KGChessRules` and `KGDraughtsRules`.
3. SPRINT-043 `SPRINT-043-CarryAnything.md`: carry almost any physics object with E (GMod style).
4. SPRINT-044 `SPRINT-044-Performance-LOD.md`: distance culling, LOD/HLOD, scalability, perf gate I5. Put the settings
   in the dressing framework or builders so rebuilds keep them.
5. Then: weather/graphics pass (`SPRINT-040-Weather.md`), boats + treasure voyage, loot/bandage/flintlock
   (`SPRINT-035`, bandage heals above the 60 HP second-wind cap), viewmodel animation polish (027b), retargeting the
   59 external Quaternius characters, spectator/replay, event ledger / body reporting (020/021).

## Known pitfalls
- Don't kill processes you didn't start. Capture one view per editor call (multi-view captures hung the editor).
- `Docs/Backlog.md` "Proposed" lists extras that need owner approval before they are built.
