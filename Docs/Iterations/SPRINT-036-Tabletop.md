# SPRINT-036 — Tabletop games: chess and checkers as social fun (hotfix + 5 slices, 036h, 036a–036e)

Status: **Proposed (needs approval)**, 2026-09-25.
Design: `Docs/Design/Tabletop_Games.md` v0.2. Roadmap place: M12 "Living village"; the detailed form of
`01b_Village_Life_Fun.md` §2.

User request (2026-09-25, translated): "Chess, checkers and so on, not as a game mode but for social fun."

## Dependencies and order (read first)
- **Verification debt is 12 (> ~10).** Exceptions: **036h** is a bug fix, and **036a** writes only new files and adds
  no in-game feature, so both may run before the verification session.
- MVP scope: English checkers + full-rules chess, 4 tables (square and the Late Arrival inn) during warm-up and the
  epilogue; in a match at most floor(floor((N − threats) / 3) / 2) tables (N ≤ 7: 0, N 8–12: 1, N 16–20: 2).
- 036d needs Roadmap **021** (bell) and **023** (bot mind); 036e needs Roadmap **020b**, **021** and **024**.
- Recommended global order (same block in 033–036):
  0. Verification session (user). 1. SPRINT-016 finisher. 2. Roadmap 020a + **036h**, with 033a in parallel.
  3. Roadmap 020b, with **036a** in parallel. 4. 035a. 5. 035b. 6. 033b, **036b**. 7. 034a, 033c, **036c**. 8. 034b.
  9. Verification session. 10. 035c, 035d, **036d** and 035e (after 023), 035f and **036e** (after 024), 033d.
- Serial files: `Core/KGGameMode.cpp`, `AI/KGBotController.cpp`, `Dev/KGDevCommands.cpp`, `UI/KGHUD.cpp`,
  `Tools/Gauntlet/run_invariants.ps1`.

---

## SPRINT-036h — The meeting teleport stands seated players up (XS, hotfix)
**Objective:** today `AKGGameMode::StartMeeting` teleports a seated character without standing it up; `AKGSeat::Tick`
keeps disabling movement, so the player can freeze in the meeting ring and snap back to the chair later. This affects
the 71 seats that exist now.

### Acceptance (fixed)
1. **Test:** a character seated at meeting start is stood up (`AKGSeat::StandUpCharacter`) before the teleport, ends in
   the ring, can move, and does not snap back to the seat after the meeting.
2. The match smoke (I4) and `run_invariants.ps1` pass.

### Writable scope
- `Core/KGGameMode.cpp` (`StartMeeting` only), tests

### Limits
- 3 fix attempts; runs in the same serial window as Roadmap 020a.

---

## SPRINT-036a — Rules core and a 2D panel (M, new files only)
**Objective (visible):** a complete chess and checkers engine you can play against the bot in a 2D panel.

### Acceptance (fixed)
1. **Test** `KillGodot.Tabletop.Perft`: chess start d1–4 = 20 / 400 / 8 902 / 197 281; Kiwipete d1–3 = 48 / 2 039 /
   97 862; position 3 d1–4 = 14 / 191 / 2 812 / 43 238; position 5 d1–3 = 44 / 1 486 / 62 379; English checkers start
   d1–5 = 7 / 49 / 302 / 1 469 / 7 361 (checked against an independent source at sprint start).
2. **Test:** ≥ 20 rule cases: castling through check refused, en passant only right after the double step, knight
   underpromotion, stalemate, threefold repetition (Zobrist), 50-move rule, insufficient material, forced capture,
   multi-jump continuation, crowning ends the move, 80-ply no-progress draw in checkers.
3. **Bot:** Apprentice / Journeyman / Master levels; search time-sliced ≤ 0.5 ms per frame with node budgets (chess
   20k, checkers 50k) in a headless benchmark; deterministic with an `FKGRng` seed.
4. A 2D panel (`FKGMgPainter`) plays a full game against the bot, opened by a console command registered inside
   `Tabletop/`; UIShot of a mid-game with legal-move dots. `run_invariants.ps1` passes.

### Writable scope
- new `Source/KillGodot/Tabletop/` (`FKGChessRules`, `FKGDraughtsRules`, `FKGTableBot`, `FKGBoardState`, the panel)
- new `Private/Tests/KGTabletopTests.cpp`
No existing file changes; may run in parallel with Roadmap 020b.

### Limits
- 3 fix attempts per check; 2 look rounds for the panel; plateau stop.
- Out of scope → proposals: Turkish draughts (decision T1), Liar's Dice, backgammon.

---

## SPRINT-036b — Networked board tables (M)
**Objective (visible):** two players sit at a table on the square or in the Late Arrival and play; in a match the game
spans days on a small daily thinking allowance, freezes at the bell, the meeting and night, writes an **unnamed** line
on the chalk slate, and can carry a 0–10 Bakır stake.

### Acceptance (fixed)
1. **Test:** allowance 0.12 × Day (+1 s per move), sitting cap 0.3 × Day, continuous focus ≤ 45 s → auto-adjourn;
   running out adjourns and never loses; clocks advance 0 s in Meeting, Trial and Night; the in-match table cap
   (N = 6 → 0, 8 and 12 → 1, 16 and 20 → 2); SaveGame round trip is identical (FEN, clocks, allowances, escrow, slate).
2. **Test:** slate lines carry no player names (colours and 30 s buckets only); a wipe leaves the "wiped" mark.
3. **Two-process smoke** `Tools/Unreal/kg_table_smoke.ps1` (windowless): the client plays Scholar's mate over RPC;
   final FEN and "mate" match on both ends; invalid, out-of-turn and unseated moves are rejected (G7.2); a 5 Bakır
   stake is paid to the winner exactly once; a dead player's stake stays on the table as a pickup.
4. **Performance (headless):** 4 tables + 2 searching bots at N = 20: tabletop server cost mean ≤ 0.3 ms/frame, p99
   ≤ 1.0 ms.
5. Shot `KG_Cap_table_square.png` (T1; placeholder pieces allowed) and a slate with 3 lines. The table smoke joins I3;
   `run_invariants.ps1` passes.

### Writable scope
- `Tabletop/` (`AKGBoardTable`, `UKGTabletopRPCComponent`, `UKGTabletopSubsystem`)
- layout JSON `tables[]` (data only; runtime spawn, the v2 build line is untouched)
- `Tools/Unreal/kg_table_smoke.ps1`, `Tools/Gauntlet/run_invariants.ps1` (add the smoke to I3, serial)
- tests, a new section in `Docs/05_Tech_Architecture.md`
`World/KGSeat.*` is read, not changed.

### Limits
- 3 fix attempts per check; 2 look rounds; plateau stop; header changes in one user-timed batch.

**Depends on:** 036a, 036h.

---

## SPRINT-036c — The 3D board view and pieces (M)
**Objective (visible):** sitting down moves the camera over a real 3D board with the opponent's face at the top of the
screen; you drag pieces in the world.

### Acceptance (fixed)
1. Camera transition 0.35 s (pitch −52°, FOV 55°), 15% vignette; right-click look-around moves **only the camera**
   (test: the narrow witness flag stays set while look-around is held).
2. Pieces from `Tools/Blender/kg_make_tabletop.py` (headless, palette texture, 150–400 tris per piece) unless the user
   picks the KayKit download (T2); ≤ 32 draw calls for 4 boards; pieces hidden at 25 m, LOD1 at 8 m.
3. Input: drag and drop, legal-move dots, promotion radial, draw offer, resign by holding R 1.5 s; UIShots of a
   mid-game, check, promotion and the adjourn toast.
4. **Test:** hearing a gun cock within 8 m breaks board focus for 3 s (dev-fired cock event).
5. 2 look rounds; `run_invariants.ps1` passes.

### Writable scope
- `Tabletop/` view code, new `UI/Tabletop/`, `Tools/Blender/kg_make_tabletop.py` + new content paths, tests

### Limits
- 2 look rounds; 3 fix attempts per check; plateau stop. Any download needs the user's approval first.

**Depends on:** 036b, decision T2.

---

## SPRINT-036d — The table bot in the world (S)
### Acceptance (fixed)
1. **[U]** an invited bot (look + E within 12 m) sits within 15 s and finishes a checkers game (also bot vs bot via
   `kg.Table.Start Checkers Bot Bot`); Town and Impatient bots accept with the same seeded chance (0.6); logs
   `KG_TABLE act=accept|decline|sit|move|stand|resign reason=…`.
2. The bot stands up at the bell within 1–3 s and walks to the gathering; it stands up on a scream within
   `KG_HEAR_SCREAM`.
3. Bots never stake. `run_invariants.ps1` passes.

### Writable scope
- `AI/KGBotController.cpp` (invite hook, serial), `Tabletop/` bot glue, `Dev/KGDevCommands.cpp` (`kg.Table.*`
  verbs, serial), tests

### Limits
- 3 fix attempts per check; plateau stop. Bots sitting down on their own for alibis is out of scope (proposal).

**Depends on:** 036b, Roadmap 021, 023.

---

## SPRINT-036e — Tables in the deduction loop (S)
### Acceptance (fixed)
1. **Test:** for the whole seated time, and with look-around held, a seated player does not see a backstab 5 m behind
   at 60° (hears it), and sees and identifies the opponent and anyone within the ±30° / 8 m cone.
2. **Test:** the six table events (`TableGameStart/Stand/End/Spectate`, `SlateWiped`, `StakeTaken`) are written with
   witness lists; none is mirrored to the Town Board; at the bell every seated or watching player lands in one
   `TableStand{bell}` batch; a player's own table events do not count as perceived events for S1.1.
3. **Test:** an "I WAS THERE" claim built from a table event is shared through `ClaimRef`; the opponent can confirm or
   deny it; a claim that contradicts the slate logs `KG_CONTRA`.
4. **[U]** bots use overlapping table events as claims and log them. T-M1, T-M2, T-M3 (gun and no-gun matches
   separately) and T-M5 are reported (not gates). `run_invariants.ps1` passes.

### Writable scope
- `Tabletop/`, `Evidence/` (event types and the perception hook only; 020b owns the ledger), `AI/` (claims), tests

### Limits
- 3 fix attempts per check; plateau stop.
- Out of scope → proposals: table whisper mode, flipping the table, spectator side bets, a card game.

**Depends on:** 036b, Roadmap 020b, 021, 024.
