# SPRINT-043 — Carry (almost) anything: Garry's Mod style physics props

User (2026-09-26): "Let us carry the other physical objects too by pressing E, like in Garry's Mod."

The hold-E physics carry already exists (KGCharacter: grab, rotate with R, release/throw), but most map props are static
meshes. This sprint makes the small and medium props physical and carriable on both maps. It runs right after SPRINT-042
in the same agent, because both touch the carry code.

## Acceptance (fixed)
1. **AKGCarriable** is a lightweight replicated physics prop.
   - Mass from bounds/material class.
   - Physics sleeps when resting (cheap), wakes on touch or grab.
   - Movement replicates only while awake.
   - Resets to its home spot at dawn if moved more than 30 m away or lost (in the sea, out of the world).
   It builds on or shares code with KGBreakable, so breakables stay carriable too.
2. **Conversion rules**, a table in the dressing helpers. Carriable: crates, barrels, buckets, pots, bottles, mugs,
   baskets, sacks, chairs, stools, small tables, books, lanterns (hand), fruit/goods, tools, cushions, etc. Size/mass
   limit: one person carries up to ~40 kg; heavier props (tables, chests) are two-person carries, reusing SPRINT-016's
   two-carry. Static on purpose: buildings, walls, large furniture, beds, stalls, anything structural or needed by nav
   and chores.
   Applied in the v2 dressing (dressing/v2) and the manor dressing (kg_sm_dress.py). Budget: ≤ 600 carriables per map;
   the rest stay static.
3. **Feel.**
   - Grab and hold at a distance with a gentle spring. R rotates; mouse wheel changes the distance.
   - Left click throws, with strength by mass.
   - Props knock other props, with impact sounds by material.
   - A thrown prop hitting a player does small damage and a stagger (a GMod classic). It doesn't hurt teammates in
     meetings.
   - Held props block the view slightly, so there is a tactical cost.
   - Bots ignore carriables unless blocked; then they push through.
4. **Deduction and fairness.**
   - Moved props stay where they are left, so they can be evidence (barricading a door, hiding a body behind crates) and
     can enter the event log if it exists.
   - Carriables can't be used to climb into out-of-bounds (a max stack height/overlap rule).
   - Anti-grief: a per-player throw cooldown, and no prop may block a door for more than 20 s.
5. **Evidence.**
   - A network smoke: grab → carry → throw → rest, positions agree on 2 machines.
   - A perf capture: frame time with 600 carriables at rest ≤ +0.5 ms.
   - Shots.
   - Invariants green, both maps' match smokes reach a winner.

## Writable scope
- Source/KillGodot/World/KGCarriable.*
- surgical hooks in KGCharacter's carry and KGBreakable
- dressing/v2/kit.py + kg_dress_common_v2.py (the conversion)
- kg_sm_dress.py
- tests, smokes, docs

## Limits
- 3 fix attempts per failing check
- 2 feel rounds
- plateau stop
