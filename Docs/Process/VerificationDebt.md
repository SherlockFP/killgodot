# Verification debt

These features were built headless and pass their automated checks, but nobody has seen them in a real session yet.
New feature sprints pause while this list is long (LoopContract.md). One verification session, in the editor or a
windowed build with the user present, clears it.

| # | Feature | What to look at | Built |
|---|---|---|---|
| 1 | FPArms2 viewmodel (Desktop preset) | Size and position of idle/slash/stab/fists/carry; the new inspect clip | 2026-09-24 |
| 2 | Minimap + full map (M) + location toast | Readability, 38 m radius, label sizes, M key | 2026-09-24 |
| 3 | Chat panel + emojis + reaction bubbles | Emoji baseline, log position vs vitals, Enter/Esc input mode | 2026-09-24 |
| 4 | Emotes + G wheel + emote camera | 16 clips on villagers (sit foot slide, shrug width), the camera pull-out | 2026-09-24 |
| 5 | Dev panel (F1) | Fit at 720p–1440p, the client read-only mode, teleports landing on the ground | 2026-09-24 |
| 6 | Server browser + lobby room | Mouse/keyboard feel, a two-player lobby, map change from the lobby | 2026-09-24 |
| 7 | Morrowmere v2 map + dressing | Prop facings (stalls, pavilion, gate arch, bridges), stair trims, night look | 2026-09-24 |
| 8 | Fishing | Rod grip in first/third person, sounds, bobber size, Madam Brine's icon | 2026-09-25 |
| 9 | Chore minigames (22) | World effects (bell, smoke, lighthouse glow…) placement; real play times vs the 30–60 s target | 2026-09-25 |
| 10 | HUD overhaul | Hit direction, low-HP pulse, stamina TIRED state | 2026-09-24 |
| 11 | Interiors (stairs, rooms, 71 seats, 16 chests) | Tight upstairs walkway in 4 m houses, sitting poses | 2026-09-24 |
| 12 | Digging + underground | Shovel in first/third person (own FP rig), dig feel + dust, grave noise cue, well ladder down/up, mausoleum door, the underground map layer + toasts, underground light/fog in PIE | 2026-09-25 |
| 13 | World chores (SPRINT-016, 10 physical chores) | Hold-E carry feel of bucket/crate/sacks (slow walk, spill when sprinting), two players on the fish crate, shove knocks items out, compass/minimap waypoints + work ring, spot payoffs (trough level, lamps, bell, lighthouse glow, chimney smoke), "Nice throw!", poison/snuff sabotage, ladder climbs for the clock/bell/lighthouse chores | 2026-09-25 |
| 14 | Villager variety + safe role visuals (SPRINT-027a) | 38 seeded per-player looks in a real lobby (body swap on join, hair/gear on animated bodies, cape/apron/hats while running and sitting, shop hat/beard overriding the archetype), the owner-only cuff on the FP arms in play, the sash on a real corpse / at the trial; only offscreen sheets and tests so far | 2026-09-25 |

Headless coverage that exists: `Tools/Gauntlet/run_invariants.ps1` (build, tests, v2 verify, chat/emote/fish/chore
smokes, whole-match bot smoke).
| 15 | Storm Manor map (SPRINT-018) | Walk all 24 rooms + secret passages, room vignettes, storm look (reads grey dusk), a real match with players, white-mesh props | 2026-09-26 |
| 16 | Proximity voice + mouths + Z/X/C barks + partner emotes (SPRINT-023) | A real microphone end to end (capture device, Opus quality, latency, echo), spatial playback and the 8 m / 25 m feel, the mouth oval placement on every archetype (kg.Mouth.Fwd/Up), bark voices (placeholder synth: are they readable/annoying?), radial feel with the mouse, partner alignment on slopes, the offer prompt, RPS/dance-off result bubbles; only injected-tone smokes, tests and offscreen renders so far | 2026-09-26 |
| 17 | Forest ring + wolves + the Mist (SPRINT-033) | Walk the bigger forest (trails readable by day/night, trail-head lanterns, tree density vs. getting lost), the Quaternius wolf (facing, size, clip speed, eye placement on the head bone), howl/growl/snarl synth sounds, the Mist tongue look and the frost HUD, the arrow back to the path; only tests, a two-process smoke and offscreen shots so far | 2026-09-26 |
| 18 | Forest chores + camp fire + Camp Vigil (SPRINT-034) | Deadwood piles, tent pitch (canvas carry, pegs), trail lantern line from the village at night, herbs to the church table, lighting/feeding the fire, the vigil book on the square and a real vigil night | 2026-09-26 |
| 19 | Storm Manor expansion (SPRINT-040) | Walk the 56-room manor (north block, west/east wings, lower vaults) with real players; discover the 10 secrets (E on the seam) and see them appear on the minimap; open the 12 hidden compartments (loot, readable clues); a human Impatient arming the trapdoors / chandeliers / locking rooms / lamps; the creaking-floor minimap ping and the witness portraits; the 4 secret chores and their rewards; wing gates in a 6-8 player lobby; only a bot match smoke, tests and offscreen renders so far | 2026-09-26 |
