# Kill Godot — agent guide

First-person, voice-driven social-deduction game (Town of Salem × GMod Murder × Lockdown Protocol × R.E.P.O.,
CS2-quality viewmodel) set in the coastal fishing village of Morrowmere. Unreal Engine **5.8.3**
(`D:\Program Files\Epic Games\UE_5.8`), Blender **5.2.2** (`C:\Program Files\Blender Foundation\Blender 5.2`).

## Talk to the user in Turkish
The user writes Turkish (often without diacritics). Design docs are Turkish; **code, comments, identifiers and
research reports are English**. In-game text goes through String Tables (EN/TR/RU at launch).

## Where things are
| Path | What |
|---|---|
| `Docs/00_Vision.md` … `Docs/09_Roadmap_Gauntlet.md` | The design. Read the relevant doc before building a system. |
| `Docs/Backlog.md` | Work queue for the Gauntlet loop (milestones M0–M18). |
| `Docs/Research/TechResearch.md` | Engine-verified UE 5.8 facts: host migration, EOS, voice, FP rendering, perf, legal. |
| `Docs/Research/AssetResearch.md` | Free asset candidates + licenses (download only with user permission). |
| `Docs/Prompts/` | Reusable prompts: the loop prompt, milestone prompts, art-generation prompts. |
| `Source/KillGodot/` | C++ runtime module (prefix `KG`). |
| `Tools/Blender/` | Headless Blender pipeline (palette, puppet builder, variant sheets). |
| `Tools/Gauntlet/run_gates.ps1` | Quality gates G1–G4. |
| `Art/` | Generated textures, concept renders, exported meshes. |

## The Gauntlet loop (how to make progress)
SELECT next `[ ]` in `Docs/Backlog.md` → PLAN (`Docs/Iterations/ITER-NNN.md`) → BUILD → run gates
(`powershell -File Tools/Gauntlet/run_gates.ps1`) → update docs/backlog → report. A task is done only when its
gates pass. Max 3 fix attempts per failing gate, then ask the user. Commit only when the user asks.

## Non-negotiable code rules (host migration depends on them)
- Gameplay durations come from `FKGMatchClock` (remaining seconds). No `TimerManager` for gameplay-critical time.
- Randomness comes from `FKGRng` (seeded, snapshot-able). No `FMath::Rand` in gameplay code.
- Every gameplay-relevant actor has a `UKGSnapshotComponent`; server-only state is `UPROPERTY(SaveGame)`.
- Players are keyed by EOS PUID, never by PlayerId/NetGUID/pointers in persistent state.
- Secret role data replicates `COND_OwnerOnly`; the public `AKGPlayerState` never exposes it.
- Gameplay actors are not spatially loaded (World Partition must never stream them out).
- Write Iris-ready replication (registered subobject lists, FastArray, push-model macros).
- Voice/chat features ship with mute/block/report.

## Art rules
Characters: Quaternius villagers (UBC + Modular Outfits, CC0) tuned vibrant via `M_KG_Character` — the procedural
puppet direction was REJECTED by the user (never reintroduce primitive-built characters). Environment: vibrant toon look,
TF2 warm/cool touch, one 8×8 palette texture (`Art/Textures/T_KG_Palette.png`,
`Tools/Blender/kg_common.py::PALETTE`). Budgets in `Docs/06_Art_Direction.md §8`.

## Commands
```powershell
# Blender (headless)
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python Tools/Blender/kg_make_palette.py -- Art/Textures
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python Tools/Blender/kg_puppet_prototype.py -- Art/Concept/Proto
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python Tools/Blender/kg_variant_sheet.py -- Art/Concept/Variants 1 2

# UE (VS 2026 Build Tools at D:\VS2026\BuildTools) — only when the editor is CLOSED
powershell -File Tools/Gauntlet/run_gates.ps1            # G1 build + G2 automation tests
```

### Live editor workflow (default — the user keeps the editor open and watches the map)
The editor runs the UE MCP server (`http://localhost:8000/mcp`, `.mcp.json`) and Python remote execution.
Never close the user's editor; iterate live:
```powershell
python Tools/Unreal/kg_live.py --shot KG_Look       # StopPIE → Live Coding → StartPIE → HighResShot
python Tools/Unreal/kg_live.py --no-compile -p Tools/Unreal/kg_pie_probe.py   # restart PIE + run a probe
python Tools/Unreal/kg_remote.py -f some_script.py  # run Python inside the editor / PIE world
python Tools/Unreal/kg_mcp.py call StartPIE '{"options":{"bSimulate":false,"playMode":"PlayMode_InViewPort","warmupSeconds":2}}' EditorToolset.EditorAppToolset
```
Screenshots land in `Saved/Screenshots/WindowsEditor/` (a name with a dot gets no `.png` extension).
Editor-camera shots without PIE: `python Tools/Unreal/kg_capture.py aerial plaza street house harbour`
(→ `Saved/Screenshots/KG_Cap_*.png`). The village map is generated: `kg_build_village.py` (live, PIE stopped).
`unreal.Rotator(a, b, c)` is (roll, pitch, yaw) and `unreal.Color(a, b, c, d)` is BGRA — always pass keywords. Spawned lights must be Movable (nothing baked).
Live Coding gotchas: `.cpp` bodies and member default values are fine; new/removed members, UPROPERTY/UFUNCTION
or class layout changes need a batched close cycle: StopPIE → `EditorLoadingAndSavingUtils.save_dirty_packages`
→ `SystemLibrary.quit_editor()` → `run_gates.ps1` → relaunch `UnrealEditor.exe KillGodot.uproject` (user authorized
autonomous cycles 2026-09-24; batch header changes so it happens rarely). Live++ **keeps old
`static` locals**, so a `static ConstructorHelpers::FObjectFinder` with a changed path keeps its stale result —
use non-static finders for assets you may re-path. Assets can't be saved while PIE runs.
Editor Python gotchas: never `MaterialEditingLibrary.delete_all_material_expressions` on a loaded material (editor
crash) — rebuild materials headless (`UnrealEditor-Cmd ... -run=PythonScript -Script="x.py args"`; `__name__` is not
`"__main__"` there). Functions called from editor Python run under the editor script guard, so **RPCs execute
locally** — drive networked actions with the `kg.Act attack|shove|interact|blade` console command instead
(`Tools/Unreal/kg_net_test.py`). Multiplayer PIE is set through MCP `ConfigSettingsToolset`
(Editor/LevelEditor/PlayIn: playNetMode=PIE_ListenServer, playNumberOfClients=2). The user may be playing in PIE:
check the log for actions you did not trigger before restarting PIE.
Parallel agents share one editor: `kg_remote.py`, `kg_capture.py` and the `kg_mcp.py` CLI hold a cross-process lock
(`Tools/Unreal/kg_lock.py`, info in `Saved/kg_editor.lock.info`); wrap any new editor-driving tool in `kg_lock.editor()`.
Captures for parallel work: `kg_capture.py "shot:<name>:X,Y,Z,PITCH,YAW"` → `KG_Cap_<name>.png`.
Map content ("set dressing") lives in zone modules `Tools/Unreal/dressing/dress_<zone>.py` on top of
`kg_dress_common.py`; run live with `kg_dress.run(['zone'])` (see `Tools/Unreal/dressing/README.md`); the village
build calls them all. Asset list with sizes: `Tools/Unreal/dressing/asset_catalog.md` (`kg_asset_catalog.py`).
New Blender prop packs import with `kg_import_dress_pack.py` (manifest-driven materials/collision).
After an unclean editor exit the relaunch blocks on a "Restore Packages" dialog (remote exec never answers): close it,
copy the newer `Saved/Autosaves/...` files over `Content/` (back up first) and move `PackageRestoreData.json` aside.

### Dev panel / kg.* commands (test faster)
In-game **F1** (or `kg.Dev`) opens the dev panel (dev builds only; `Source/KillGodot/Dev/`, full table in
`Docs/05_Tech_Architecture.md` §12). Every panel button is also a console command, so drive tests headlessly with them
(`kg.Dev.Help` logs the list; `[host]` verbs run on the listen server, clients need `kg.Dev.AllowClients 1`):
- Match: `kg.Match.Start [Seed]`, `kg.Match.Restart`, `kg.Match.Phase Night`, `kg.Match.Freeze`, `kg.Match.Skip`,
  `kg.Match.Time 10`, `kg.Match.Speed 5`, `kg.Match.Win Town|Impatient|Neutral`, `kg.Match.Reveal`, `kg.Match.Seed`
- Bots: `kg.Bot.Add 5`, `kg.Bot.Fill 12`, `kg.Bot.Remove [N|Name]`, `kg.Bot.RemoveAll`, `kg.Bot.AI 0`, `kg.Bot.List`, `kg.Bot.Goto Name`
- Me: `kg.Me.Role Sheriff`, `kg.Me.God`, `kg.Me.Fly`, `kg.Me.Speed 3`, `kg.Me.Heal`, `kg.Me.Stamina`, `kg.Me.Kill`,
  `kg.Me.Revive`, `kg.Me.Blade`, `kg.Me.Give Pearl 3`, `kg.Me.Coins 100`, `kg.Me.Gold 500`
- Spawn: `kg.Spawn.Chest [Locked] [Mine]`, `kg.Spawn.Seat Chair`, `kg.Spawn.Pickup Coin 25`, `kg.Spawn.Loot Crate`, `kg.Spawn.Crate 5`
- World: `kg.World.Goto <Place|X Y Z>`, `kg.World.Places`, `kg.World.Look Night`, `kg.World.Hide Grass`, `kg.World.Stat unit`,
  `kg.World.NavMesh`, `kg.World.ChoreMarkers`
- Chores: `kg.Chore.List`, `kg.Chore.Done all`, `kg.Chore.Reset [all]`, `kg.Chore.Goto DrawWater`,
  `kg.Chore.Play BakeBread` (minigame anywhere, server-validated), `kg.Chore.AutoWin 1`; minigame PNGs headless:
  `Tools/Unreal/kg_chore_shots.ps1 -What all` (`kg.ChoreShot`), network check `Tools/Unreal/kg_chore_smoke.ps1`.
  Minigame code: `Source/KillGodot/Chores/` (a global `using namespace KGMg;` trips C4459 in engine templates - don't).
- Emotes: `kg.Emote wave|dance|sit|stop|list`, `kg.Emote.Bots dance|all|stop` [host], `kg.Emote.Cam 1` (look at your own body), `kg.Emote.Favorites` (Shift+1..4); headless net check `Tools/Unreal/kg_emote_smoke.ps1`
- Fishing: `kg.Fish.Give [Rod|Salmon 5]`, `kg.Fish.Cast [0.6|out]`, `kg.Fish.Bite [Species]`, `kg.Fish.Hook`, `kg.Fish.Land`,
  `kg.Fish.Sell`, `kg.Fish.Market`, `kg.Fish.BotCast`, `kg.Fish.Tension` (overlay); net smoke `Tools/Unreal/kg_fish_smoke.ps1`,
  pixels `Tools/Unreal/kg_fish_capture.ps1`, assets `Tools/Unreal/kg_import_fishing_assets.py` (GDD §15)
- Debug: `kg.Debug.HUDDemo 2`, `kg.Debug.VM FOV 62`, `kg.Debug.VMTune SwayHalfLife 0.1`, `kg.Debug.Dump` (KG_DUMP lines + clipboard), `kg.Debug.Net`
From C++/automation: `FKGDev::Execute({ServerWorld, nullptr}, TEXT("Bot.Add 3"))` (see `Private/Tests/KGDevPanelTests.cpp`).
Adding a verb = one entry in `KGDevPrivate::BuildVerbs()`; it becomes a console command and can back a panel button.

## Permissions / safety
- Never download assets or install software without the user's explicit OK (state name, source, size).
- Asset licenses go to `Docs/Credits.md` at import time.
- The Blender MCP server may be disconnected; the headless CLI pipeline above always works.
