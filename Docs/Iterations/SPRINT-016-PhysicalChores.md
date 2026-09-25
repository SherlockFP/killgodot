# SPRINT-016 — Original, physical chores (priority: the user calls this "very important")

User request (2026-09-25): "The games you added feel too much like Among Us, with no original games. Add them and
develop them. They must be fun. Tasks like *draw water from the well and carry it to the town* are very important."

The panel minigames from SPRINT-014 stay, but they are no longer the heart of chores. KillGo chores become
**physical, in-world jobs**: you carry real objects through the village (Hold-E physics carry already exists in
KGCharacter), use real props, and the result persists visibly. Walking routes cross each other, which creates alibis,
witnesses and ambushes.

## Acceptance (fixed)
1. **Framework: multi-step world chores.** Chain steps: pick up → carry → deliver → use. Each step has a world
   target (actor or tag) and a HUD waypoint on the minimap and compass. The carried object is a real replicated
   physics item. It can be dropped, stolen or knocked out of your hands by a shove, and water can spill.
   Server-validated; fake-able by the Impatient (they carry the thing but the result does not count, and it looks the
   same to others).
2. **At least 10 original chores on Morrowmere v2.** Each takes 30–75 s including travel, and each is shown by a
   headless route check (all steps reachable on the navmesh) plus an offscreen screenshot of the key moment:
   - **Water run:** fill a bucket at the well (short crank), carry it to the fountain / bakery / farm trough without
     running. Sprinting spills, and the fill level shows on the bucket.
   - **Fish to market:** take a crate from the jetty and deliver it to Madam Brine's table. It's heavy, so you walk
     slowly.
   - **Bread delivery:** collect the loaves at the bakery and deliver one to 3 marked doors (knock).
   - **Lamplighter:** carry the taper and light the street lamps along Balcony Lane (5 lamps), then return it to the town
     hall.
   - **Bell rope + clock:** wind the clock in the tower (climb), then ring the bell. Heard village-wide.
   - **Nets:** pick up the torn net at the Net Stairs, mend it at the rack (short panel game), hang it on the quay posts.
   - **Firewood:** chop 3 logs at the woodcutter camp, carry the bundle to the inn hearth.
   - **Letters:** take the sealed letters from the notice board and post each in the right house letterbox (names from
     the lore).
   - **Grain to the windmill:** carry a sack from the barn to the windmill, grind it (short lever game), and deliver the
     flour to the bakery.
   - **Lighthouse oil:** fetch an oil can from the boathouse, climb the lighthouse and fill the lamp. Its beam brightens.
3. **Fun pass.** At least 2 twists that make chores social:
   - the heavy crate needs two players to carry at full speed;
   - a delivered item can be sabotaged by the Impatient, so the next player finds the trough poisoned or the lamp out.
   Every chore gives juicy feedback: sounds, a visible result, a HUD tick.
4. **The chore list per match** mixes world chores with short panel ones: about 70% world, 30% panel.
5. **Invariants:** `run_invariants.ps1` passes. The chore smoke is extended with one world chore (water run, end to end
   across two processes).

## Writable scope
- Source/KillGodot/Chores/: new WorldChores subfolder + minimal hooks
- the carry code in KGCharacter.cpp: hooks only
- new carriable actors under World/
- chore data in layout_v2 or a new chores JSON
- small marked hooks in kg_build_village_v2.py for station/prop placement (coordinate with the v2 polish owner)
- UI waypoint drawing in UI/KGHUDMap.inl
- tests, smokes, docs

## Limits
- max 3 fix attempts per check
- 2 feel-and-look rounds per chore
- plateau stop
- ideas beyond the list go in as proposals
