# SPRINT-023 — Voice, talking mouths and TF2-style social play

User request (2026-09-25):
- the mouth moves as you talk on voice chat;
- a social emote system like Team Fortress 2, and things of that kind.

## Acceptance (fixed)
1. **Proximity voice (works today without EOS credentials).**
   - UE VoIP over the current online subsystem (Null/LAN), push-to-talk (default V) and an open-mic option in Settings.
   - Proximity attenuation (full volume ≤ 8 m, silent at ~25 m), a "speaking" indicator over heads and in the HUD.
   - Mute per player (the chat's /mute and the scoreboard).
   - Channel rules from the chat charter: ghosts hear ghosts; the living don't hear ghosts; during meetings everyone at the
     square hears everyone.
   - EOS voice slots in later through the same interface (documented).
   - Evidence: a two-process headless smoke that sends a synthetic voice stream and checks the receiver's audibility,
     attenuation and ghost filtering.
2. **Talking mouths.** Villager faces animate a mouth from the speaker's live voice amplitude (a jaw/mouth blendshape,
   a mouth mesh/texture swap, or a head-bone approach — whatever the Quaternius villager supports; pick the most
   readable). Also driven by the voice-command barks below. Visible on bots when they "bark". Evidence: offscreen
   renders of a bot mid-bark (mouth open/closed frames).
3. **TF2-style voice command menus.**
   - Three quick radial menus (Z/X/C) with 8 lines each, social-deduction flavoured: "Over here!", "Help!", "I saw
     something!", "Suspicious…", "I was at the <place>", "Trust me", "Thanks!", "Yes/No", "Meeting at the fountain!",
     "Who's there?", "I'm doing my chore", "Follow me".
   - Each plays a spoken bark (a synthesised or procedurally processed placeholder voice, in several voice types),
     animates the mouth and a gesture, and posts a NEAR chat line.
   - "I was at <place>" fills in the current minimap region, as a claim.
   - Rate-limited; ghosts only in ghost channels.
4. **Partner emotes (TF2 partner taunts).**
   - High-five, handshake, rock-paper-scissors (server-resolved outcome with a result bubble), and a dance-off.
   - One player offers and waits with the prompt showing; a nearby player accepts with E.
   - Both play synced animations with aligned positions.
   - Cancelled by attack or damage; rate-limited.
   - Built on the existing emote system (Source/KillGodot/Emote/).
   - Evidence: a network smoke (offer → accept → both see the synced emote; RPS outcome identical on both machines)
     plus offscreen renders.
5. **Invariants:** run_invariants.ps1 passes, and the voice smoke is added to I3.

## Writable scope
- a new Source/KillGodot/Voice/ folder
- Emote/ (partner emotes)
- a Chat/ hook for barks
- the character face/mouth code, a minimal hook in KGCharacter
- new audio assets
- Settings page entries
- tests/smokes/docs

## Limits
- 3 fix attempts per failing check
- 2 look rounds for the mouth and emotes
- plateau stop
- extra ideas go in as proposals
