# Chat history summary (Claude Code sessions, 2026-09-24 → 2026-09-26)

A readable summary of what the owner asked for and what was decided. The raw transcripts are in `Handoff/History/`
(local only, not in git):
- `*.jsonl`: the three sessions, one JSON event per line; the owner's messages are the `"type":"user"` lines.
- `session-export-2026-09-26.zip`: the last session, with the subagent transcripts.

## Where it stopped (2026-09-26 ~11:40)
- The owner stopped all agents: the weekly limit was at 99 %.
- Last commits:
  - `f7255e5`: health feel + forest rebuild
  - `3d8bfab`: HANDOFF
  - `b73bfe4`: AgentPlaybook
  - plus this history
- Invariants on the last run: I1–I3 PASS. I4 (bot match smoke) was interrupted, so run it first.
- Uncommitted: SPRINT-041 Trapper work (5 files, see `HANDOFF.md`). The balance run was never done.
- Stopped before they started real work (nothing written): the chess+carry agent (SPRINT-042/043) and the performance
  agent (SPRINT-044).
- Last open question from the owner: "is health working? it doesn't seem to".
  - Answer: the code worked but gave no feedback.
  - Fix, committed: hit flinch, a hit marker, second wind up to 60 HP.
  - Not yet seen in a real game (VerificationDebt #21).

## The owner's requests in order (their words, translated)
### Base and maps
- Fill the map with lots of content. Map design must be excellent, not props placed side by side. This became v2
  "Amphitheatre Cove".
- Add a minimap with location names (Lockdown Protocol style). Hold M for a big map.
- Every house looks the same, so vary them. Fix the wrong Japanese-garden geometry, make the water realistic, improve
  the clock tower, use slightly fewer houses.
- Lower the Lighthouse Point wall, move the lighthouse to a corner, fix the broken stairs.
- Make the forest bigger: wolves, a mysterious mist, survival chores in the forest.
- A second map, closed like Among Us: Storm Manor. Then: "Make the manor much bigger. Huge and detailed inside, a very
  rich house that goes on and on. Secret rooms, hidden compartments, secret and manor-only chores, extra traps."
- Better graphics (shadows, lighting, water), weather (wind, rain, snow, lightning).
- Things far away should not render, or should be very low poly, for optimisation.

### Social / UI
- Chat panel with emojis, then emotes.
- An admin/dev panel with bot add/remove.
- A real lobby finder / server browser that fits the screen. Recording/replay later.
- Role reveal at match start. First feedback: "BORING, nothing is readable, not fun". Result: a 6 s punchy moment.
- Streamer mode, random names.
- Voice chat with moving mouths, TF2-style social emotes and voice commands.
- The UI is bad: improve it, and make chores easy to see.

### Gameplay
- The game is too slow: speed it up, advance day by day, show the day count.
- Chores: original and physical (fetch water from the well and carry it to town, etc.). Later feedback: "not fun,
  keep it simple but fun".
- Movement: bunny hop tied to stamina, strafe-based, not automatic.
- Many character models, random per player: women, men, witches, superheroes.
  - Decision: per player, never per role, so roles stay secret.
- The viewmodel hands and animations are too big and janky.
- Breakable boxes with loot, weapon parts that combine into a gun (Murder style). Chess and draughts.
- Small drivable boats: find a shovel near town, sail to the Japanese island, dig up a treasure chest from a random map.
  It must not be too hard.
- A new killer class that sets traps. Chests can be mimics. This became the "Trapper".
- Chess should not be 2D: a 3D table where you hold E to move pieces (Source style).
- Carry other physical objects with E like Garry's Mod.
- "Is health working? It doesn't seem to."

### Process
- Work in the background (the owner plays games): no windows, don't show the editor, don't relaunch it if closed.
- After the Reddit critique of the loop: a git repo, commits, a bounded loop with sprint contracts. Name the game
  **KillGo**.
- Download free assets (itch.io included, commercial licence doesn't matter), and make/generate your own if needed.
  - Feedback on the asset approval question: "these already existed, why ask again; install the missing ones and
    research more".
- The usage limit burned in ~40 min, so manage it better.
  - Decisions: 2 main-model agents plus cheap Fable 5.1 agents, one reviewer, batched requests.
  - Later: "use multiple Fable agents".
- Web port: asked about, not planned (documented as a later option).

## Standing decisions
- Listen-server P2P now. EOS (free lobbies/voice) later, when the owner sets up credentials.
- Appearance per player, never per role. Role data is owner-only (COND_OwnerOnly).
- Chores: 1–3 steps, 25–50 s. The reveal: at most 12 words, about 6 s.
- The Town win rate must stay in 0.35–0.65 over seeded bot matches.
- Second wind stops at 60 HP; bandages (SPRINT-035b) heal higher.
- Never push; local commits only.
