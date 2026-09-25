# SPRINT-015 — Match flow: lobby → role reveal → match, streamer mode

User request (2026-09-25):
- improve lobby creation;
- the lobby list is the first thing you see;
- player names show in the lobby;
- when the match starts, roles are dealt first on a special, beautiful screen before the map loads;
- a streamer mode where the role is hidden after the reveal and player names are randomised.

Spectating and recording (replays) are a SEPARATE later sprint (SPRINT-016), not part of this one.

## Acceptance (fixed)
1. **Play → server list first.** Play opens the server browser directly; Host is a button there.
   - Hosting opens the lobby room.
   - Joined players appear by name within 1 s on every machine: two-process headless smoke.
2. **Role reveal ceremony.** Starting from the lobby goes to a full-screen, dedicated reveal sequence, Town of Salem style:
   - the dealer's table: cards shuffle, your card flips;
   - role name, alignment colour, goal, 1–2 ability lines, the flavour line from Docs/Lore;
   - Impatient see their teammates.
   The match world is revealed only after that. It must not jump straight to a loading map.
   - Server-timed (FKGMatchClock), 8–12 s, skippable per player with a "ready" press.
   - Replicated: owner-only role data (`COND_OwnerOnly`); other players never receive your role.
   - Evidence: offscreen UIShots of every reveal stage for a Town, an Impatient and a Neutral role.
3. **Streamer mode** (Settings → Gameplay → Streamer mode, saved in UKGGameUserSettings):
   - (a) after the reveal the role is hidden everywhere in the HUD: role chip, chore FAKING tag, role card. Hold a key (default Tab) to peek.
   - (b) other players' names are replaced by stable per-match pseudonyms (e.g. "Villager 7" / village-themed names) everywhere this client shows names: nameplates, chat, lobby, meeting tally, epilogue, kill feed.
   - (c) the join code and host address are masked in the lobby and browser.
   Unit test for the pseudonym mapping (stable within a match, different across matches, never the real name).
4. Invariants: `Tools/Gauntlet/run_invariants.ps1` passes afterwards.

## Writable scope
- Source/KillGodot/UI/Menu/* (browser, lobby room, settings page)
- Source/KillGodot/Core/KGLobbyState.*
- Source/KillGodot/Core/KGGameUserSettings.*
- a new Source/KillGodot/UI/Reveal/ folder
- minimal hooks in KGGameMode (StartFromLobby → reveal phase) and KGHUD.cpp (streamer masking, role chip)
- the chat name rendering in Chat/KGChatUI.cpp (name masking only)
- tests, docs (08_UI_UX.md, Backlog)

## Limits
- max 3 fix attempts per failing check
- 2 look-and-iterate rounds per screen
- plateau stop

## Out of scope → proposals
- spectator mode and replays (SPRINT-016)
- voice anonymisation
- cosmetics in the lobby
