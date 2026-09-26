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

---

## Implementation notes (2026-09-26)

### Voice (Source/KillGodot/Voice/)
- `FKGVoiceRules` (pure, tested): full volume <= 8 m, linear to silence at 25 m; ghosts hear ghosts anywhere and
  the living never receive ghost voice; ghosts hear the living; Meeting/Trial = everyone alive hears everyone;
  revenants, migration and the role-reveal curfew close the mic; whispering at night is allowed.
- `UKGVoiceComponent` (one per `AKGPlayerState`, attached by `UKGVoiceSubsystem` like the chat relay): the owning
  client captures the microphone through the engine **Voice** module (16 kHz mono, Opus, ~190 B per 60 ms packet,
  ~3 KB/s per talker) and sends `ServerVoice`; the listen server applies the rules **per receiver per packet** and
  forwards with the gain (`ClientVoice`), so a machine that may not hear a packet never gets it. Receivers decode into a
  `USoundWaveProcedural` attached to the speaker's Head (spatialised, volume = server gain x Voice slider), feed the
  amplitude to the speaker's mouth and remember who they heard (overlay marker). `/mute` (the chat list) drops a
  speaker's voice and barks on arrival. Push-to-talk **V** (Accuse moved to **middle mouse**), open mic in Settings ->
  Audio -> Voice chat.
- Headless: there is no microphone, so `-KGVoiceSmoke` injects a synthetic tone through the same encode -> send path
  (`InjectPCM`); everything after the capture is the shipping code. `kg.Voice.Tone 1` does the same in PIE.
- **EOS later:** EOS RTC replaces capture / wire / playback (lobby voice room). The seam is
  `UKGVoiceComponent::OnVoiceHeard(Speaker, Packet, Gain)`: the RTC unmixed-audio delegate feeds it per participant,
  `FKGVoiceRules::HearGain` becomes the per-participant volume set at 10 Hz (`SetPlayerVolume`), and the mouth, the
  indicator, the barks, the mute list and the smoke stay as they are (Docs/Research/TechResearch.md §4).

### Mouth
The Quaternius villager skeleton has `Head / neck_01 / spine_*` only, no jaw bone, and morph targets are not
imported, so `UKGMouthComponent` attaches a small dark oval (engine sphere, character material tinted near-black) to
the Head bone: a 0.7 cm slit at rest that opens to 3.2 cm with `JawOpen`. Placement/size: `kg.Mouth.Fwd/Up/Width/
OpenHeight`; `kg.Mouth.Force 0..1` pins it for shots. Drivers: live voice amplitude (dB gate, gamma, attack/release)
and the bark envelope (`FKGVoiceCommandCatalog::MouthEnvelope`, one sine hump per syllable, deterministic per line).

### Voice commands (Z / X / C)
`FKGVoiceCommandCatalog`: 3 radials x 8 lines (Calls / Deduction / Social). Each bark: server validates (phase =
reaction rules, 3-burst then 1 per 2 s limiter), fills `{place}` from `AKGMapInfo::FindRegionAt` (the minimap region
the speaker stands in), posts the line on **NEAR** (Dead channel for ghosts) through `UKGChatComponent::ServerSay`,
starts the gesture emote, and sends `ClientBark` to everyone who could see a reaction bubble: bark audio at the head
(`/Game/KillGodot/Audio/Barks/S_Bark_<Voice>_<Id>`, 4 placeholder voices from `Tools/Audio/kg_synth_barks.py`:
MaleLow / MaleYoung / Female / Old, picked from the villager look), the mouth envelope and the emoji bubble. Bots:
`kg.Bark.Bots <id|all>`.

### Partner emotes (Emote/)
`FKGPartnerCatalog`: high five, handshake, rock-paper-scissors, dance-off, built from shipped clips (cheer / point /
clap / dance) as placeholders. `UKGEmoteComponent` gained a replicated `FKGPartnerState` (kind, stage, partner,
serial, result): offer (8 s, rate-limited, same body rules as an emote) -> the acceptor presses **E** within 3 m
(`AKGCharacter::Interact` hook) -> the server snaps the acceptor to `Distance` in front of the offerer, turns both to
face each other and starts both clips; RPS / dance-off outcomes are one server byte rolled from the match seed
(`FKGRng`), revealed after the clips with a NEAR line + bubble. Moving, attacking, damage, death, a phase change or
a cancel end it for both.

### Tools
- `Tools/Unreal/kg_voice_smoke.ps1` (I3), `kg_partner_smoke.ps1` (I3), `kg_voice_shots.ps1` (offscreen renders to
  `Saved/Screenshots/Voice/`), `kg_import_barks.py`, `Tools/Audio/kg_synth_barks.py`.
- Dev verbs: `kg.Voice.Tone`, `kg.Voice.Mute`, `kg.Voice.Wheel <0|1|2|close> [slot]` (radial held open without the
  key, `kg.Voice.WheelPin`), `kg.Bark`, `kg.Bark.Bots`, `kg.Mouth.Pin`, `kg.Partner`, `kg.Partner.Bots`.

### Evidence run (2026-09-26, headless)
- `kg_voice_smoke.ps1` PASSED, `kg_partner_smoke.ps1` PASSED (the checker unrolled a single `KG_PARTNER_START` line to a
  string; `@()` fix), both in `run_invariants.ps1` I3.
- `Saved/Screenshots/Voice/`: `mouth_closed.png` / `mouth_open.png` (slit on the lip line vs a 3 cm oval under the lips;
  `kg.Mouth.Up` 4.0 -> 2.5 cm after look round 1, where the oval sat under the nostrils), `bark_1..3.png`, `bark_ui.png`
  (bubble + NEAR lines), `talking_ui.png` (mic pill), `radial_ui.png` (X Deduction wheel), `partner_highfive/rps/danceoff.png`,
  `partner_offer_ui.png`, `partner_rps_result_ui.png`.
- Open: the mouth offset is one number for all heads; male heads (Head bone 5 cm higher) carry it slightly lower than
  female heads. A per-skeleton offset is a proposal, not a blocker.
