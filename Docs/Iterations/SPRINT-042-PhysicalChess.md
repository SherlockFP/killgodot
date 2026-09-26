# SPRINT-042 — Physical 3D chess/draughts at the table (Source/GMod-style hold-E)

User (2026-09-26): "Chess shouldn't be 2D. It should be a table in the 3D game, and you hold E there to move the pieces
like in Source engine games."

## Acceptance (fixed)
1. The v2 tables (dressing/v2/dress_tabletop.py: the Latecomer garden + Fountain Square) and the manor billiards/study
   table (optional) carry a real 3D board with 32 chess or 24 draughts piece actors, in the palette style.
2. **Hold E to grab a piece** using the existing physics carry (KGCharacter hold-E). Release over the board, and the piece
   snaps to the nearest square.
   - The server validates the move with the existing rules engine (Tabletop/KGChessRules, KGDraughtsRules).
   - An illegal move or a move out of turn slides the piece back.
   - A capture knocks the taken piece off to the table's side tray.
   - Promotion swaps the mesh.
   - Castling or a draughts multi-jump moves the partner pieces automatically.
   - Only the two seated players can move their own colour; others can pick pieces up only when no game is running
     (free-play mode, just for fun).
3. Replicated smoothly: pieces are server-owned with interpolated clients. Spectators see every move live. The 2D panel
   becomes an optional overlay (move list + clock) toggled with Tab, no longer the main UI.
4. **Evidence.**
   - A two-process network smoke: the scripted hold-E moves of a scholar's mate agree on both machines, and an illegal
     move returns the piece.
   - Offscreen shots: the board mid-game, a piece in hand, the capture tray.
   - Invariants green.

## Writable scope
- Source/KillGodot/Tabletop/ (the piece actors and board actor)
- a surgical hook in the KGCharacter carry (release-onto-board)
- dressing/v2/dress_tabletop.py
- Blender --background piece meshes (or kit pieces if they exist in /Env/Ext; Kenney has a chess set?)
- tests, smokes, docs

## Limits
- 3 fix attempts per failing check
- 2 look rounds
- plateau stop
