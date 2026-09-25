# SPRINT-041 — New killer class "The Trapper" (Tuzakçı): mimic chests and traps

User (2026-09-26): "Add a new killer class that can set traps. There are chests in the game that randomly hold loot or
food; the killer should be able to turn those chests into MIMICS as traps, and things like that."

It depends on SPRINT-040's generic trap framework (Source/KillGodot/Traps/), so it starts after 040 lands. It is also
the first role with a real ability in code. The design audit found 0/51 roles had one, so this sprint adds a minimal,
reusable role-ability framework, and the Trapper is its first user.

## Acceptance (fixed)
1. **Role-ability framework (MVP).**
   - Server-authoritative abilities with charges, cooldowns and phase limits (e.g. only at night, or only while unseen).
   - Owner-only UI: an ability bar with charges, and a targeting prompt.
   - Data-driven from the role catalog.
   - Tests: the validation rules, and that no role data leaks to other clients.
2. **The Trapper (Impatient faction)** added to the role catalog. Reveal card text EN+TR in the one-line style (≤ 12
   words), and lore flavour from Docs/Lore.
   Abilities:
   - **Mimic:** turn any chest/crate/barrel container into a mimic, 2 charges per night. The next person who opens it
     is bitten: heavy damage and held in place for 3 s (not an instant kill), with a loud scream audible 30 m away. The
     mimic then turns back into a normal chest with teeth marks.
     Telegraph for sharp-eyed players: a faint breathing sound within 3 m, the lid slightly ajar, a drool drip.
   - **Snare:** a bear trap on forest paths/rooms; it roots and damages.
   - **Tripwire:** a silent alarm that tells only the Trapper who passed and where.
   Traps are visible for 2 s after arming, and all expire at dawn.
3. **Deduction hooks.**
   - Bite marks and snare wounds are distinct evidence on bodies and victims.
   - Mimic scream and trap events enter the event log if it exists (else a TODO with a proposal).
   - A careful player can "test" a chest from a distance by throwing an item at it: a mimic flinches.
   - Bots: they avoid chests they saw someone get bitten at, and the Trapper bot arms traps along chore routes.
4. **Balance.** Numbers in the GDD. Role-list generator weights so the Trapper appears in about 1 of 3 matches at N ≥ 8.
   A match smoke with a forced Trapper still reaches a winner, and the Town win rate over 20 seeded bot matches stays in
   the charter band.
5. **Evidence.**
   - Offscreen shots: a mimic mid-bite, a mimic at rest (the telegraph), the snare, the ability UI.
   - A two-process network smoke: arm → victim opens → the bite replicates → the chest resets.
   - Invariants green.

## Writable scope
- a new Source/KillGodot/Abilities/ folder
- Source/KillGodot/Traps/ (extend 040's framework)
- the role catalog entry + reveal text
- small hooks in AKGStorageChest / KGBreakable (the mimic state) and the bot controller
- new meshes and animations: the mimic chest (Blender --background: a teeth/tongue overlay on the existing chest)
- tests, smokes, docs

## Limits
- 3 fix attempts per failing check
- 2 look rounds
- plateau stop
