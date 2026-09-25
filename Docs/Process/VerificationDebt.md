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

Headless coverage that exists: `Tools/Gauntlet/run_invariants.ps1` (build, tests, v2 verify, chat/emote/fish/chore
smokes, whole-match bot smoke).
