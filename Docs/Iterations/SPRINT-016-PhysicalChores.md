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

## Result (2026-09-25, finishing agent)
Mid-sprint user feedback (the user played it in the editor): "some chores aren't fun: KEEP IT SIMPLE BUT FUN". The
coordinator's brief: 1–3 simple steps, one clear verb each, 25–50 s including the walk, a juicy payoff every step, cheap
playful touches, cut anything that feels like work. The acceptance text above stays as written. The chores below are
the simplified versions, and each change says what was cut and why.

| Chore | Steps (verbs) | Route | Time* | Simplified |
|---|---|---|---|---|
| Water run (fountain / bakery / inn) | crank a full bucket at the well → carry it (walk, don't run) and pour | 61–66 m | 43–45 s | Cut "fetch an empty bucket first", which was a 60 m walk with nothing to do. Farm trough → inn horse trough (the farm was 61 s away) |
| Fish to market | lift a crate of the catch → haul it to Madam Brine's (slow alone, full speed with two) | 28 m | 29 s | Pickup moved from the far jetty (61 s) to the quay head |
| Bread delivery (3 variants) | grab the steaming basket → knock at 3 marked doors | 99–124 m | 40–47 s | — |
| Lamplighter | take a lit taper → light 5 lamps (the taper burns out at the last one) | 109 m | 46 s | Cut the walk back to return the taper |
| Bell and clock | climb, wind the clock → climb, pull the bell rope (heard village-wide) | 56 m + 2 ladders | 41 s | Work times shortened |
| Mend and hang the nets | grab the torn net → mend it at the rack (short panel) → hang it on the quay posts | 66 m | 43 s | Cut the separate "bring it to the rack" step (4 → 3 steps) |
| Firewood | chop 3 logs (E per swing) → feed the bundle to the smithy forge (tossing it in counts) | 18 m | 52 s | Inn hearth (122 m of slow carrying) → the smithy forge next to the camp |
| Letters (3 variants) | take the sealed letters → post each in the right named box | 96–136 m | 38–51 s | — |
| Grain to the windmill | shoulder a sack at the barn → tip it into the hopper | 34 m | 49 s | Cut the lever panel and the 95 m flour trip back to the bakery (4 → 2 steps) |
| Lighthouse oil | take an oil can at the lighthouse door → climb and fill the lamp (the beam blazes) | 2 m + 12 m ladder | ≈51 s | The oil moved from the boathouse (84 m away) to the lighthouse door |

\* Time = the navmesh route at walking/carrying speed + work + ladders, plus the walk from the square
(`kg.WorldChore.Routes`, `Saved/KG_WorldChoreRoutes.json`). Sprinting the empty-handed approach is faster.

Payoffs and playful touches:
- Every step: a gold sparkle at the spot and a chime for everyone near. The owner gets the HUD tick pop plus a chime, and
  a bigger flourish when the whole chore is done.
- The spots change for good: trough levels, lit lamps, hung nets, loaves on doormats, mail flags, logs in the woodbox.
- Crank, chop and rope cues make the spot move: the bell rope bounces and swings, the axe hops, the spare buckets
  rattle.
- The water sloshes harder the faster you go before it spills. A fish flops on the crate. The bread steams.
- A delivery thrown in from 3 m or more counts too, with "Nice throw!".

Social twists (acceptance 3):
- The fish crate is a two-person carry.
- The Impatient can poison a filled trough (the next villager has to dump it first) or snuff a lit lamp (soot adds 2 s).
- Fakes look identical and count for nothing. Tested in `KillGodot.WorldChores.FakeSabotageTwoCarry`.

Bots:
- Bots walk the real steps (their navmesh goals now use the spots' stand points) and side-step head-height snags.
- They put their chore items down at night and in meetings.
- The Impatient only fake a world chore for 25 s, then drop the item and get back to hunting (fix for the I4 no-winner
  regression).

Network:
- The water-run smoke failed because a wall lantern at head height, which the 144 cm navmesh agent can't see, stopped
  the carrier.
- The smoke autopilot now side-steps like a player and faces the spot on arrival.
- Replication waits were added for the fill and the trough level.
- Panel stations that stood on a world chore spot (DrawWater on the well kerb, MendNets at the rack, ChopWood at the
  block) now slide their E box aside, so they no longer swallow the E.
