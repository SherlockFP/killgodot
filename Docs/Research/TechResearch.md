# Kill Godot — Technical Research (UE 5.8.3)

**Date:** 2026-09-24
**Engine checked:** `D:\Program Files\Epic Games\UE_5.8`. `Engine\Build\Build.version` reports 5.8.3, CL 58210709, branch `++UE5+Release-5.8`.
- This is a launcher build with Win64 and Mac binaries only.
- **The Android and iOS target platforms are not installed.** `Engine\Build\Android` and `Engine\Build\IOS` are missing, and the EOS SDK folder has only Win64 libs.

**Decisions already fixed by the lead:**
- The backend is **EOS**.
- A match has **6–20 players**, and every system must scale with player count.

**Tags used below:**

| Tag | Meaning |
|---|---|
| **[ENGINE]** | Confirmed by reading the 5.8.3 source, config or `.uplugin` files. `$UE` = `D:\Program Files\Epic Games\UE_5.8`. |
| **[WEB]** | Taken from a web source; the link is given inline or in §13. |
| **UNVERIFIED** | Could not be confirmed. Treat it as a hypothesis. |
| **[EST]** | An engineering estimate. Measure it with Network Insights / Unreal Insights before relying on it. |

> Web searching hit the session limit near the end, so some Epic doc pages were only seen through search snippets. Those points are marked UNVERIFIED.

---

## 0. Executive summary

1. **Host migration has no engine support.** UE 5.8 has none for game state. The only related feature is EOS **lobby ownership** migration: `SETTING_HOST_MIGRATION` / `bDisableHostMigration` and the `EOS_LMS_PROMOTED` handling.
   - The practical CoD-style flow:
     - The EOS lobby survives the host leaving.
     - A pre-elected successor runs `open <Map>?listen` and restores from a snapshot.
     - The other clients run `ClientTravel("EOS:<SuccessorPUID>:GameNetDriver:<ch>", TRAVEL_Absolute)`.
   - Expect a **7–15 s** interruption on PC [EST], mostly map reload. Build the "snapshot-able match state" rule into the code from day one.
2. **Backend: use `OnlineSubsystemEOS` (OSSv1)** with Steam as `NativePlatformService`, plus `NetDriverEOS` for P2P and relays. The 5.8 docs still call Online Services (OSSv2) Beta and "not tested in shipping titles".
   - EOS Game Services are free, and a Steam game can use them via Connect with no Epic account.
   - **Blockers for mobile:**
     - Android/iOS targets are not installed.
     - OSS EOS / SocketSubsystemEOS / EOSVoiceChat do **not** list iOS in their 5.8.3 `.uplugin` files.
     - OSS EOS has no DeviceID or Google login mapping.
3. **Voice: use EOS RTC through the lobby room.** It survives host migration, uses no host upload and costs nothing.
   - Proximity at first is per-participant volume only, because `Set3DPosition` is *unimplemented* in `FEOSVoiceChatUser`.
   - True 3D comes later by rendering EOS **unmixed / manual-output** audio through UE audio components.
   - Ghost rules are applied per client from replicated alive/dead state.
   - Per-talker amplitude for mouths: `IVoiceChatUser::RegisterOnVoiceChatBeforeRecvUnmixedAudioRenderedDelegate` feeding `Audio::FEnvelopeFollower`.
   - Fallback is UE's built-in VOIP (`UVOIPTalker::GetVoiceLevel`), which can filter voice per packet on the server. But OSS EOS returns **no** `IOnlineVoice`, so it needs glue code.
4. **Replication: use the legacy system now.** In the 5.8.3 install, Iris is still a Beta plugin and switched off at runtime (`GUseIrisReplication = 0`), even though the 5.8 release notes call it production-ready for licensees.
   - Use dormancy, cull distances and adaptive update frequency.
   - Write code that will also work under Iris, and try Iris in a test branch with Gauntlet bots.
   - Physics props: `EPhysicsReplicationMode::PredictiveInterpolation`. Physics Prediction is labelled "(Experimental)" in 5.8.3.
   - Destruction: replicate the break event and let clients simulate the fragments cosmetically.
5. **Bandwidth at 20 players:** budget **~16 KB/s average and 32 KB/s peak per client** [EST]. The host then uploads ~2.5 Mbps typical and ~5 Mbps peak. Only PC peers with measured upload of at least 5 Mbps should host 13–20 player matches, and **mobile never hosts**.
6. **Rendering:**
   - Mobile forward renderer as the base, no Nanite or Lumen dependency.
   - Dynamic day-night from one movable sun plus preset ramps.
   - World Partition with HLOD for the 500 m town, with gameplay actors always loaded.
   - Native **First Person Rendering** (`FirstPersonPrimitiveType`, `FirstPersonFieldOfView`, `FirstPersonScale`) for the viewmodel. The mobile base pass applies the transform.
7. **Tooling:**
   - Visual Studio 2022 **17.14** with MSVC ≥ 14.44.35211, or Visual Studio 2026 **18.x** with MSVC ≥ 14.50.35723 (the UE 5.8 docs prefer VS 2026). Windows SDK 10.0.22621.
   - External agents can drive the editor through `-run=PythonScript`, `remote_execution.py`, or the new **Experimental "Unreal MCP" plugin** that ships with 5.8.
   - **Gauntlet** can run one listen server plus N clients and kill the host to test migration.
8. **Economy:**
   - Sell cosmetics directly. Cases are **earn-only** with odds shown, and no trading or cash-out at launch.
   - Proof of ownership in P2P: signed Steam Inventory snapshots on PC, and a tiny backend issuing signed entitlement tokens for cross-platform.
   - PEGI (June 2026 rules) gives PEGI 16 to paid random items, and Brazil's ECA Digital (in force 17 Mar 2026) bans loot boxes for minors.

---

## 1. P2P listen server and host migration

### 1.1 What the engine provides (and what it doesn't)

**Engine-verified facts:**

| Topic | Fact | Source |
|---|---|---|
| Game-state host migration | None. A grep for `HostMigration` in `$UE\Engine` matches only EOS lobby and session plumbing. | `OnlineSessionEOS.cpp`, `LobbiesEOSGS.cpp`, `eos_lobby_types.h`, `OnlineSessionNames.h` |
| Lobby owner migration (OSS EOS) | `SETTING_HOST_MIGRATION` defaults to true, which sets `CreateLobbyOptions.bDisableHostMigration = !bUseHostMigration`. On `EOS_LMS_PROMOTED`, OSS EOS sets `Session->OwningUserId`, sets `bHosting = true` and re-publishes the lobby. | `Plugins/Online/OnlineBase/Source/Public/Online/OnlineSessionNames.h:59-63`; `Plugins/Online/OnlineSubsystemEOS/.../OnlineSessionEOS.cpp:4115-4136, 1017-1047` |
| EOS SDK | "Is host migration allowed (will the lobby stay open if the original host leaves?)". `EOS_Lobby_PromoteMember` is allowed regardless of the setting. Members can set their **own** member attributes with `EOS_LobbyModification_AddMemberAttribute`. Lobbies can join or leave their RTC room manually with `EOS_Lobby_JoinRTCRoom` / `EOS_Lobby_LeaveRTCRoom`. | `Engine/Source/ThirdParty/EOSSDK/SDK/Include/eos_lobby_types.h:228-232`; `eos_lobby.h:129, 521, 536, 777` (EOS SDK **1.19.1**, `eos_version.h`) |
| EOS limits | 64 members per lobby, 16 lobbies per user at once, 64 lobby attributes, attribute names up to 64 characters. | `eos_lobby_types.h:48-63` |
| OSSv2 | `ILobbies::PromoteLobbyMember` exists. | `Plugins/Online/OnlineServices/Source/OnlineServicesInterface/Public/Online/Lobbies.h:290, 699` |
| EOS P2P address | "Expect URLs to look like `EOS:PUID:SocketName:Channel`". | `OnlineSessionEOS.cpp:1650` |
| EOS net driver | `UNetDriverEOS : public UIpNetDriver`, so it inherits the IpNetDriver config. `bIsUsingP2PSockets` has been deprecated since 5.6 and always behaves as if true. | `Plugins/Online/SocketSubsystemEOS/Source/SocketSubsystemEOS/Public/NetDriverEOS.h:14-40` |
| Relay control | `[SocketSubsystemEOS] RelayControl=NoRelays\|AllowRelays\|ForceRelays`. The default is AllowRelays, meaning direct first and relay as fallback. ForceRelays "will hide IP Addresses from peers" at a latency cost. | `SocketSubsystemEOS.cpp:18-34, 98-100`; `eos_p2p_types.h:431-445` |
| P2P limits | Packets ≤ 1170 B. At most 32 socket connections *per remote user*, which does not cap player count. NAT types are Open, Moderate and Strict. | `eos_p2p_types.h:14, 20, 26-33` |
| Same-server rejoin only | `AGameMode::InactivePlayerStateLifeSpan`, `MaxInactivePlayers`, `FindInactivePlayer`. These are no help for a *new* host. | `Engine/Source/Runtime/Engine/Classes/GameFramework/GameMode.h:144,148,187` |
| Timeout | `[/Script/OnlineSubsystemUtils.IpNetDriver] ConnectionTimeout=60.0`, `InitialConnectTimeout=60.0`. That is **far too slow** to detect a lost host. | `Engine/Config/BaseEngine.ini:1852-1856` |
| Replay snapshots | `DemoNetDriver` has `bCanUseIris=false`. Replays are not a sensible snapshot mechanism. | `BaseEngine.ini:346-347` |

**Web findings:**
- EOS **Sessions** have no host migration: the session is orphaned when its owner leaves (docs quoted via search snippet; UNVERIFIED wording). So the **Lobby** must be the object that survives a host loss.
- Seamless travel is server-driven. Connecting to a *different* host is a hard travel: full map load, and UserWidgets are destroyed. [WEB: UE 5.8 "Travelling in Multiplayer"]
- Community consensus since UE4 is "no built-in host migration" [WEB: forums 384489, 283823, 793445].
- `bAllowHostMigration` / `HostMigrationTimeout` in old configs are dead leftovers: "There is no trace of bAllowHostMigration in the engine source" (Tom Looman).

### 1.2 Prior art

| Source | Approach | Takeaway |
|---|---|---|
| DarxDev "Host Migration System" V1 / V2 (Fab) | A client is elected, the others reconnect, and a serialized game state is loaded (async save, compression, a Blueprint system for saving actor references). Claims Steam/EOS/Android/iOS support. | Rated about 3.1/5 (V1) and 3.3/5 (V2). 5.8 support and price are UNVERIFIED. Ask for a 5.8 demo before buying. Useful as a reference, risky as a foundation. |
| Betide EOS Integration Kit | Only the lobby owner migrates (`OnOwnerChanged`). No game state. | Confirms that the lobby layer is solved and the game layer is not. |
| Oculus *Unreal-SharedSpaces* `eos-5.x` | Lobby "master client" = listen host. On a master change the new master runs `open <level>?listen` and the others run `open <address>`. Only positions are restored. | **Closest free reference** for the travel mechanics. |
| Six Days in Fallujah (UE, Highwire) | Basic host migration built on the UE net driver. It needed a "full refactor" and every designer Blueprint had to be reworked. Budgeted 1 extra dev plus 1 tester. | **Design for it from day one** or pay later. |
| Halo: Reach (GDC 2011) | "Significant engineering undertaking". | — |
| Gears of War 3 (UE3) | Horde resumed at the same wave after migration: a coarse checkpoint. | **Coarse checkpoints are acceptable.** |
| Destiny 1 | Critical state lived on Bungie's servers; the peer "physics host" swapped silently. | Keep secret or critical state small and portable. |
| Unity Netcode for Entities + Lobby | The host uploads periodic snapshots to the lobby. The lobby elects a new host, who allocates a relay, writes connect data to the lobby and rebuilds. Clients reconnect when the lobby data changes. | The same pattern maps onto EOS lobby + P2P. |
| Steam `SetLobbyGameServer` → `LobbyGameCreated_t` | Built-in "connect info changed" broadcast to lobby members. | EOS equivalent: a lobby or member attribute update notification. |

### 1.3 Reconnect flow options

| Option | Works? | Cost | Verdict |
|---|---|---|---|
| **A. Seamless travel** | No. Only for a server-initiated map change on the *same* server. | — | N/A |
| **B. Successor `open Map?listen` + restore; clients `ClientTravel(EOS:<PUID>:GameNetDriver:<ch>, TRAVEL_Absolute)`** | Yes (the SharedSpaces pattern) | Map reload on every machine in parallel: 3–8 s on PC [EST], more on mobile | **Recommended** |
| C. In-place promotion: the successor keeps its world, re-listens with `UWorld::Listen`, and turns proxy actors into authority | Engine isn't designed for it. Proxies are `ROLE_SimulatedProxy`, GameMode never existed on the client, and NetGUIDs belong to the dead server. | Saves the reload | UNVERIFIED / research spike only. Don't plan on it. |
| D. Replay (DemoNetDriver) checkpoint as the snapshot | No one does this; it has GameMode side effects and is not Iris-capable. | — | Not recommended |

**Fastest practical timeline with B** [EST]:

| Step | Time |
|---|---|
| Detect host loss (heartbeat + lobby member status) | ~1.5–3 s |
| Successor starts listening after loading the map | 3–8 s |
| Clients connect via EOS P2P (NAT punch or relay) and load | 1–3 s after the successor is up; their load runs in parallel |
| Restore and "Resume in 3…" | ~1 s |
| **Total** | **~7–15 s on PC** |

Keep the map light and use a GameInstance-level loading screen (e.g. CommonLoadingScreen) that says "Host left — migrating…".

### 1.4 Pitfalls checklist

1. **NetGUIDs are per server.** Never store object pointers or NetGUIDs in the snapshot.
   - Give every spawned gameplay actor a `FGuid` held in a `UKGPersistentIdComponent`.
   - Match level-placed actors by stable path name. It's the same map, but World Partition streaming can hide actors, so keep gameplay actors **always loaded** (§5).
2. **PlayerState is rebuilt from scratch.** `PlayerId` changes, so key everything by `UniqueNetId` (EOS PUID).
   - Rebind in `PostLogin` / `OnPostLogin` / `InitNewPlayer`.
   - The engine's inactive-PlayerState rejoin only works on the same server.
3. **GameMode and server-only objects vanish.** Roles, RNG, AI, vote tallies and timers all live only on the host.
   - Snapshot them explicitly.
   - Store timers as **remaining seconds**, never as absolute server time. `GetServerWorldTimeSeconds` restarts on the new host.
4. **Physics can't be restored bit-exactly.** Save transform, linear and angular velocity, and sleep state.
   - Force-drop anything being held when migration starts.
   - Destroyed Geometry Collections are restored as "broken", swapped to a rubble mesh; don't try to rebuild the fragment layout.
5. **Reliable RPCs in flight are lost.** Model gameplay as replicated state, not fire-and-forget multicast events, and make events idempotent and keyed by sequence number.
6. **Split brain.** Two peers can each believe they are host.
   - Use a monotonically increasing `MigrationEpoch` plus a deterministic successor rank.
   - Clients always follow `(highest epoch, lowest rank)`.
7. **Detection must be fast and robust.**
   - The 60 s `ConnectionTimeout` default is useless. Add a 4 Hz unreliable heartbeat and treat 2 s of silence as a trigger.
   - Confirm with EOS lobby member status (`EOS_LMS_DISCONNECTED` / `LEFT`), and set a 10 s timeout as a backstop.
   - Override `UGameInstance::HandleNetworkError` / `GEngine->OnNetworkFailure()` so the engine doesn't dump the player to the main menu.
8. **Hard travel destroys widgets.** Keep HUD and match state in a `UGameInstanceSubsystem` and re-create widgets afterwards.
9. **Secrets.** In social deduction the host always knows every role, because the listen server is inherently trusted.
   - Pre-sending the secret snapshot to the successor lets a cheating successor read the roles *before* becoming host.
   - v1: accept this, since it is the same trust level as hosting.
   - v2 hardening: encrypt the snapshot and split the key with Shamir 3-of-N across the other peers, who send their shares to the new host.
10. **Mid-meeting or vote migration.** Pause the vote timer and re-open the vote with the saved ballots.
11. **Mobile successors.** Exclude them unless no PC peer is left (CPU, thermals, upload).
12. **The EOS lobby owner may not be the successor.**
    - EOS auto-promotes *some* member when the owner leaves.
    - Don't depend on lobby ownership. The successor announces itself through its **own member attribute** (`KG_HOST_EPOCH=<n>`, `KG_HOST_URL=EOS:<PUID>:GameNetDriver:<ch>`), which any member may set for itself.
    - Optionally, the auto-promoted owner then calls `PromoteMember(successor)`.
13. **Voice** carried by the game net driver dies during migration. Lobby RTC voice keeps working, which is another point for EOS RTC (§3).

### 1.5 Proposed architecture

**Components (C++ core, Blueprint-exposed):**

- `UKGSessionSubsystem` (GameInstanceSubsystem) owns the EOS lobby.
  - Uses OSS EOS with `bUseLobbiesIfAvailable = true`, `SETTING_HOST_MIGRATION = true` and `bUseLobbiesVoiceChatIfAvailable = true`.
  - **Lobby attributes:** `KG_BUILD`, `KG_MAP`, `KG_STATE` (Lobby / InMatch / Migrating), `KG_EPOCH`, `KG_HOST_PUID`, `KG_SUCCESSORS` (comma-separated ranked PUIDs, top 3).
  - **Member attributes:** `KG_HOST_EPOCH`, `KG_HOST_URL`, `KG_UPLINK_KBPS`, `KG_NAT`, `KG_PLATFORM`.
- `UKGMigrationSubsystem` (GameInstanceSubsystem) is a state machine: `Playing → Suspect → Electing → Hosting|Rejoining → Restoring → Playing`.
  - It survives hard travel because it lives on the GameInstance.
- `AKGGameState` replicates `SuccessorRanks[]` as a redundant copy of the lobby attribute, plus `MigrationEpoch` and `MatchClock` (remaining seconds).
- `UKGSnapshotComponent`, one per persistent actor, implements `IKGSnapshotable`:

  ```cpp
  void WriteSnapshot(FKGActorRecord&) const;
  void ReadSnapshot(const FKGActorRecord&);
  ```

  - `FKGActorRecord { FGuid Id; FSoftClassPath Class; FTransform Xf; FVector LinVel, AngVel; TArray<uint8> Blob; }`
  - `Blob` is written with `FMemoryWriter` + `FObjectAndNameAsStringProxyArchive`, `ArIsSaveGame = true`, so only `SaveGame`-flagged UPROPERTYs are written.
- `FKGMatchSnapshot { Epoch; Seq; MapName; RemainingPhaseTime; Phase; RNGSeed+Pos; TArray<FKGPlayerRecord> (keyed by PUID: role, alive, inventory, tasks, votes, cosmetics loadout id); TArray<FKGActorRecord>; }`
  - Compressed with Oodle or zlib (`FCompression::CompressMemory`). Target **≤ 32 KB**.
- **Snapshot transport:**
  - The host sends the full snapshot to the top 2 successors every **2 s**, and immediately after critical events (kill, meeting start or end, role change).
  - Uses a reliable Client RPC in chunks of ≤ 1 KB, or unreliable chunks with an ack bitmap.
  - Cost is ~16 KB/s peak to each of 2 peers, only during bursts [EST].
  - Only successors get secret fields. Everyone else can rebuild public state from replication.
- **Graceful leave:** when the host quits on purpose (menu, alt-F4 hook), it sends `ClientMigrationNotice(FinalSnapshot)` before closing. No data is lost and detection takes 0 s.

**Successor election** (host recomputes every 10 s and publishes to GameState and the lobby):

```
score = 40*isPC + 25*norm(uplinkKbps) + 15*NAT(Open=1,Moderate=.6,Strict=.1)
      + 10*norm(1/avgRTTtoPeers) + 10*frameTimeHeadroom  - 100*isMobile - 50*recentlyLagged
```

- Uplink is measured in the lobby pre-match (a short burst to 2 peers) and refreshed from `UNetConnection` stats in-match. Weights are [EST] and should be tuned.

**Recovery sequence:**

1. **Detect.** No packets from the host for 2 s *and* (the lobby reports the host `LEFT`/`DISCONNECTED` *or* 5 s pass) → `Suspect`.
2. **Elect.** Everyone reads `SuccessorRanks` and drops members absent from the lobby. The first remaining PUID is the successor.
3. **Successor:**
   - Sets member attributes `KG_HOST_EPOCH = Epoch+1` and `KG_HOST_URL = EOS:<self>:GameNetDriver:<ch>`.
   - Runs `UGameplayStatics::OpenLevel(this, Map, true, "listen?KGMigrate=1?Epoch=N")`.
   - The GameMode sees `KGMigrate` and restores from the cached snapshot, holding the match in `Migrating` (a pause flag in GameState).
   - Players are re-bound by PUID in `PostLogin`.
   - Play resumes when all expected PUIDs have joined or after 20 s. Missing players are marked "disconnected" but their bodies and roles stay, so the game logic can decide.
4. **Other clients:**
   - On the member-attribute notification, or 4 s after `Suspect` with the URL built from rank 1, they call `ClientTravel(URL, TRAVEL_Absolute)`.
   - Retry with backoff. If rank 1 hasn't published within 10 s, fall back to rank 2 with Epoch+1.
5. **Restore order on the new host:**
   1. World-placed actors
   2. Spawned actors (by `FGuid`)
   3. PlayerStates (as players arrive)
   4. Physics (set transform, set velocity, wake or sleep)
   5. Timers
   6. `bMatchPaused = false` after a 3 s countdown

**Rules for everyday code:**
- **Always:**
  - Every gameplay-relevant actor has a `UKGSnapshotComponent`.
  - Every server-only value is either a `SaveGame` property or written explicitly.
  - Timers go through `UKGMatchClock`.
  - Random numbers come from `FKGRng`, which is seeded and has a stream position.
- **Never:**
  - Use `GetWorld()->GetTimerManager()` for gameplay-critical durations.
  - Keep gameplay state only inside UI or Blueprint local variables.

### 1.6 Testing host migration

- **Unit:** snapshot round-trip in a UE Automation test. Write, destroy the world, reload, read, and compare.
- **Multi-process:** a Gauntlet test (§6.3) with Client ×N. One uses `MapOverride = "/Game/Maps/Town?listen"` and the others connect to it.
  - After T seconds the test kills the host process.
  - It asserts on log markers such as `KGMigration: Restored epoch=2 players=N-1` in each client log.
- **Transport abstraction:**
  - Locally, run over `IpNetDriver` with `127.0.0.1:<port>` and a fake lobby (a file or local beacon) so tests don't need EOS accounts.
  - Nightly, run on EOS using the EOS Dev Auth Tool with several dev credentials. Dev Auth Tool usage is from memory, UNVERIFIED for 5.8.

---

## 2. Player count 6–20: listen-server feasibility and bandwidth budget

### 2.1 Relevant engine defaults

| Setting | Value (5.8.3) | Where | Action |
|---|---|---|---|
| `MaxClientRate` / `MaxInternetClientRate` | 100000 B/s | `BaseEngine.ini:1860-1861` (IpNetDriver; `NetDriverEOS` inherits it) | This is a ceiling, not a target. For 20p set **40000** to protect host upload. |
| `NetServerMaxTickRate` / `MaxNetTickRate` | 30 / 120 | `BaseEngine.ini:1867-1868` | Keep 30 Hz for 13–20p; 30–45 Hz is fine for 6–12p. |
| `ConnectionTimeout` | 60 s | `BaseEngine.ini:1855` | Use a custom heartbeat (§1.4). Backstop 10 s in `[/Script/SocketSubsystemEOS.NetDriverEOS]`. |
| Legacy dynamic bandwidth `TotalNetBandwidth` / `MaxDynamicBandwidth` / `MinDynamicBandwidth` | 32000 / 7000 / 4000 | `BaseGame.ini:19-22` | `AGameNetworkManager::UpdateNetSpeeds` sets NetSpeed = clamp(32000/N, 4000, 7000) on listen servers. **Nothing in the 5.8.3 engine calls it** (`GameNetworkManager.cpp:91-145`). **Never call it**: at 20p it would force 4 KB/s per client. |
| `GameSession MaxPlayers` | 16 | `BaseGame.ini:68` | **Set `[/Script/Engine.GameSession] MaxPlayers=20`** |
| Adaptive update frequency | `net.UseAdaptiveNetUpdateFrequency` exists | `NetDriver.cpp:523` | Enable it, and set `MinNetUpdateFrequency` per class. |
| EOS lobby / P2P caps | 64 members; 32 socket connections per remote user | `eos_lobby_types.h:51`, `eos_p2p_types.h:20` | 20 players is fine. |

### 2.2 Bandwidth budget per client (host → each client) [EST]

**Assumptions:**
- About 35 B per movement update per pawn (quantized `FRepMovement` plus actor and bunch headers).
- UDP, IP and EOS overhead folded in (EOS packets ≤ 1170 B).

| Category | 6 players | 12 players | 20 players | Notes |
|---|---|---|---|---|
| Remote pawns (movement + anim state) | 5×20 Hz×35 B ≈ 3.5 KB/s | 11×(mix 30/10 Hz) ≈ 7 KB/s | 19 pawns: ~8 near at 30 Hz, ~11 far at 8 Hz ≈ 11 KB/s | Adaptive frequency and cull distances. Meetings (all visible) spike briefly. |
| Physics props (active only) | 1.5 | 2.5 | 3–6 KB/s | Resting props are dormant; ~5 moving at once. |
| GameState / PlayerStates / tasks / doors | 0.5 | 0.8 | 1.2 KB/s | Push model where possible. |
| Voice | 0 | 0 | 0 | With EOS RTC. UE VOIP relayed by the host would add ~2–3 KB/s per audible talker. |
| **Typical total** | **~6 KB/s** | **~11 KB/s** | **~16 KB/s** | |
| **Peak (meeting, chaos)** | 12 | 22 | **32 KB/s** | |

**What the host has to upload** (N−1 clients):

| Players | Typical | Peak |
|---|---|---|
| 6 | 5×6 KB/s = 30 KB/s ≈ **0.25 Mbps** | ≈ 0.5 Mbps |
| 12 | 11×11 KB/s ≈ **1 Mbps** | ≈ 2 Mbps |
| 20 | 19×16 KB/s ≈ **2.4 Mbps** | 19×32 ≈ **4.9 Mbps** |

- **Host download:** 19 × ~3 KB/s of ServerMove at 60 Hz plus input and RPCs ≈ 0.5 Mbps.
- **Rule:** hosts for 13–20p must have measured upload ≥ 5 Mbps; 6–12p need ≥ 2 Mbps. Show a "Best host" badge in the lobby and let the election override the lobby creator.
- **CPU on the listen host** (it renders and also serves 19 connections): budget **≤ 2.5 ms/frame** for replication plus physics authority on PC [EST]. Use Unreal Insights `NetTrace` and Network Insights to verify.
- **Scaling:** total traffic grows ~O(N²), so every per-client cost must stay ~O(visible pawns), not O(N). Keep per-client numbers fixed and let only the host total grow.

### 2.3 Relevancy, dormancy and culling

- **Zones instead of pure distance.** Split the town into `Surface`, `Underground` and `Interior_<Building>` zones (trigger volumes that set a replicated zone id on each pawn).
  - Override `AKGCharacter::IsNetRelevantFor` in the legacy system: relevant if same zone *or* within 25 m *or* in a meeting phase.
  - Add hysteresis of ~0.5 s. This matches the game design too: you can't see or hear underground players from the street.
- **Cull distances:** pawns `NetCullDistanceSquared` = (120 m)² on the surface. Small props (40 m)². Doors and task stations are dormant and always loaded.
- **Dormancy:**
  - Level-placed interactables: `DORM_Initial`, then `FlushNetDormancy()` on change.
  - Physics props: `DORM_DormantAll` when asleep, woken on grab or impact.
  - Corpses: dormant after settling.
- **Update frequency:**
  - Pawns: `NetUpdateFrequency` 30, `MinNetUpdateFrequency` 8.
  - Props: 20 / 2.
  - PlayerState: 2.
  - GameState: 10.
- **ReplicationGraph:** exists but is **Beta** (`Plugins/Runtime/ReplicationGraph/ReplicationGraph.uplugin`). Not needed at 20 players.
- **Iris** (if adopted later):
  - Default spatial filter `NetObjectGridWorldLocFilter` with scope hysteresis on (6 frames).
  - A `PlayerStateCountLimiter` prioritizer.
  - Delta compression for Pawn and PlayerState.
  - Source: `BaseEngine.ini:1479-1538`.
- **Push model:** compiled in only when `bWithPushModel` is true, and the default is **Editor targets only** (`Type == TargetType.Editor`). The flag carries `[RequiresUniqueBuildEnvironment]` (`Programs/UnrealBuildTool/Configuration/Rules/TargetRules.cs:1516-1527`).
  - Inference: a Game target on the **launcher (installed) engine can't turn it on**, because a unique build environment needs a source-built engine.
  - Still write `MARK_PROPERTY_DIRTY_FROM_NAME` calls. They cost nothing when compiled out and pay off on a source engine or with Iris.

---

## 3. Online backend (EOS)

### 3.1 What ships in 5.8.3

Allow lists and maturity flags come from each plugin's `.uplugin` under `$UE\Engine\Plugins\Online\` (and Experimental).

| Plugin | Platforms in `PlatformAllowList` | Flags |
|---|---|---|
| OnlineSubsystemEOS | Win64, Mac, Linux, LinuxArm64, Android (**no IOS**, though `Private/IOS/IOSEOSHelpers.*` source exists) | `IsBetaVersion:false`; depends on EOSShared, EOSVoiceChat, SocketSubsystemEOS, VoiceChat |
| SocketSubsystemEOS | Win64, Mac, Android | — |
| EOSVoiceChat | Android, Mac, Win64 | — |
| EOSShared (SDK init) | Android, **IOS**, Linux, LinuxArm64, Mac, Win64 | — |
| OnlineServicesEOS / OnlineServicesEOSGS | Win64, Mac, Linux, LinuxArm64, Android | No beta flag in the `.uplugin`, but the **docs say Beta** |
| OnlineSubsystemSteam | Win64, Mac, Linux, Android | Steamworks SDK **v1.64** (`ThirdParty/Steamworks/Steamv164`) |
| OnlineSubsystemEOSPlus | **Not present in 5.8.3** | `EOSSettings.h:190-193`: "new Login flow, which doesn't rely on EOSPlus" is deprecated because the legacy flow was removed |

- **EOS SDK:** 1.19.1 (`ThirdParty/EOSSDK/SDK/Include/eos_version.h`). Only `EOSSDK-Win64-Shipping.lib/.dll` are installed.
- **Steam login:** OSS EOS requests Steam tickets from OSS Steam when `[OnlineSubsystem] NativePlatformService=Steam` (`OnlineSubsystemModule.cpp:93`, `UserManagerEOS.cpp:113-193, 244, 582-604`).
  - `SteamTokenType` defaults to `"Session"`, which is marked deprecated. **Set `WebApi`** (`EOSSettings.h:195-206`).
- **OSS EOS external credential mapping** covers Steam, PSN, XBL, Nintendo, Apple ID token, and OpenID as the fallback.
  - **There is no DeviceID or Google mapping in OSS EOS** (`UserManagerEOS.cpp:113-193`).
  - OnlineServicesEOSGS *does* list `EOS_ECT_DEVICEID_ACCESS_TOKEN` and `EOS_ECT_GOOGLE_ID_TOKEN` (`AuthEOSGS.cpp:107-124`).
- **OSS EOS provides no voice interface:** `FOnlineSubsystemEOS::GetVoiceInterface()` returns `nullptr` (`OnlineSubsystemEOS.cpp:724-727`). This matters for §4.

### 3.2 OSSv1 (OnlineSubsystemEOS) vs OSSv2 (OnlineServicesEOS): use OSSv1

- **5.8 docs** [WEB] say Online Services is **Beta**, has "not been tested in shipping titles", and to "Use the Online Subsystem for any title shipping in the near future".
- **Lyra** uses OSSv1 by default; OSSv2 is optional via `-customconfig=EOS`.
- **Keep OSSv2 in view** for mobile DeviceID/Google login, or add a small custom `EOS_Connect_Login` path in OSSv1.
- **Third-party alternatives:**
  - Redpoint EOS Online Framework: free under $30k/yr revenue; iOS and Android supported [WEB].
  - Betide EIK: MIT on GitHub, "Alpha", no iOS listed [WEB].
  - Redpoint is the fallback if iOS support in Epic's plugins stays missing.

### 3.3 EOS vs Steam

| Need | EOS (Connect + Lobbies + P2P + RTC) | OnlineSubsystemSteam |
|---|---|---|
| Lobbies | Up to 64 members, attributes, owner migration, search by bucket | Steam lobbies with automatic owner reassignment, `SetLobbyGameServer` |
| NAT traversal / relay | EOS P2P: direct first, relay fallback; ForceRelays hides IPs | Steam Datagram Relay (SteamSockets plugin) |
| Crossplay with mobile | **Yes** via Connect (Apple, Google, DeviceID) | **No**, Steam only |
| Voice | EOS RTC (free), lobby-bound rooms, no 3D in UE's wrapper | Steam voice via UE VOIP (`FOnlineVoiceSteam`), 3D through UVOIPTalker |
| Cost | Free, "no royalty or hosting fees" [WEB] | Free for Steam titles |
| Account requirement | None with Connect. EAS (Epic account) needs Brand Review [WEB]. | Steam account |

- **Cost:** EOS Game Services are free, and voice plus EAC have been free since 2021-06-22 [WEB]. Per-service rate limits exist, but the numbers are UNVERIFIED.
- **A Steam release can use EOS** for lobbies, P2P and voice with Steam auth via Connect, with no Epic account.

**Starting config** (key names verified in source where cited; the rest are standard but UNVERIFIED for 5.8):

```ini
; DefaultEngine.ini
[OnlineSubsystem]
DefaultPlatformService=EOS
NativePlatformService=Steam

[OnlineSubsystemEOS]
bEnabled=true

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480        ; replace with real AppID

[/Script/OnlineSubsystemEOS.EOSSettings]
bUseEOSConnect=true      ; Game Services, no Epic account
bUseEAS=false
bUseEOSRTC=true
SteamTokenType=WebApi

[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOS",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[SocketSubsystemEOS]
RelayControl=AllowRelays ; consider ForceRelays for IP privacy (+latency)

; DefaultGame.ini
[/Script/Engine.GameSession]
MaxPlayers=20
```

### 3.4 Mobile blockers and work items

1. **Install Android and iOS target support** in the launcher (engine options). iOS builds need a Mac (local or remote build).
   - SDK requirements [ENGINE]:
     - Android: MinSDK 26, TargetSDK 36 (`BaseEngine.ini:3304-3305`); NDK r27c, build-tools 36.0.0 (`Config/Android/Android_SDK.json`).
     - iOS: minimum 15.0 (`Config/IOS/IOS_SDK.json`).
2. **iOS is missing from the 5.8.3 allow lists** of OSS EOS, SocketSubsystemEOS, EOSVoiceChat and OnlineServicesEOS.
   - Whether iOS support moved to a platform extension or was dropped is **UNVERIFIED**.
   - Options: copy the plugins into the project and add IOS (code exists; untested), or use Redpoint. Re-check when iOS work starts.
3. **Anonymous mobile login:** OSS EOS lacks a DeviceID mapping. Add a small custom path: `EOS_Connect_CreateDeviceId` → `EOS_Connect_Login(EOS_ECT_DEVICEID_ACCESS_TOKEN)`. Later link to Apple or Google with `EOS_Connect_TransferDeviceIdAccount` [WEB].
4. **Apple 4.8:** if a third-party login is primary, an equivalent privacy-focused login must also be offered [WEB]. Anonymous DeviceID avoids the problem at launch.

---

## 4. Voice chat (proximity, ghost channel, per-talker amplitude)

### 4.1 APIs found in 5.8.3

| API | What it gives | Source |
|---|---|---|
| `UVOIPTalker::GetVoiceLevel()` (BlueprintCallable) | "Current level of how loud this player is speaking … 0.0 if not talking." Fed by the voice audio component's envelope (`OnAudioComponentEnvelopeValue`). | `Engine/Source/Runtime/Engine/Public/Net/VoiceConfig.h:94-107`; bound in `OnlineSubsystemUtils/Private/VoiceEngineImpl.cpp:610-613` |
| `FVoiceSettings` on the talker | `ComponentToAttachTo` (spatialization), `AttenuationSettings`, `SourceEffectChain` | `VoiceConfig.h:45-71` |
| `IOnlineVoice::GetAmplitudeOfRemoteTalker(PlayerId)` | Sender-side mic amplitude, carried in **every voice packet** as Q15 | Written in `VoiceInterfaceImpl.cpp:733-740`, read at 815-817; interface in `OnlineSubsystem/Source/Public/Interfaces/VoiceInterface.h:541-544` |
| `IVoiceEngine::GetMicrophoneAmplitude(LocalUserNum)` | Local mic level, for your own mouth | `VoiceEngineImpl.h:341`, `VoiceEngineImpl.cpp:834-838` |
| `UVoipListenerSynthComponent` | Per-talker `USynthComponent` that plays decoded packets | `OnlineSubsystemUtils/Public/VoipListenerSynthComponent.h` |
| `Audio::FEnvelopeFollower` | Envelope (peak or RMS) on float buffers | `Engine/Source/Runtime/SignalProcessing/Public/DSP/EnvelopeFollower.h:257-318` |
| `IVoiceChatUser::OnVoiceChatPlayerTalkingUpdated()` | **Only a bool** (talking or not), per channel and player | `Plugins/Online/VoiceChat/VoiceChat/Source/Public/VoiceChat.h:116, 533` |
| `IVoiceChatUser::RegisterOnVoiceChatBeforeRecvUnmixedAudioRenderedDelegate` | **Per-participant PCM** (`TArrayView<int16>`, rate, channels, channel name, player name), meaning per-talker amplitude for EOS voice | `VoiceChat.h:123, 661`; EOS implementation `EOSVoiceChatUser.cpp:2354-2378`. Note: its `bIsSilence` is always false, with the TODO "EOS doesn't tell us if it's silence". |
| `RegisterOnVoiceChatBeforeCaptureAudioSentDelegate` | Local captured PCM plus `bIsSpeaking`, for your own mouth on EOS | `VoiceChat.h:122, 655` |
| `FEOSVoiceChatUser::Set3DPosition` | **`// Unimplemented`**, so there is no positional audio via UE's EOS wrapper | `EOSVoiceChatUser.cpp:652-655` |
| `SetPlayerVolume` → `EOS_RTCAudio_UpdateParticipantVolume` | Per-participant receive volume. EOS range 0–100 where 50 means unchanged; UE maps its volume ×50. | `EOSVoiceChatUser.cpp:1266-1276`; `eos_rtc_audio_types.h:496-511` |
| `EOS_Lobby_LocalRTCOptions::bUseManualAudioOutput` / `EOS_RTC_JoinRoomOptions::bManualAudioOutputEnabled` | "audio rendering is not started and the audio buffers must be received with `EOS_RTCAudio_AddNotifyAudioBeforeRender` and rendered manually". The route to true 3D. | `eos_lobby_types.h:164-179`; `eos_rtc_types.h:69-76`. **Not used anywhere in UE's plugins** (grep of `Plugins/Online` finds no match), so it needs custom SDK calls. |
| Non-lobby RTC rooms | Need `ClientBaseUrl` + `ParticipantToken` from a trusted backend. UE's `InsecureGetJoinToken` is a test stub. | `EOSVoiceChatUser.cpp:575-588, 893-902` |
| Server-side voice routing (UE VOIP) | `UNetDriver::ReplicateVoicePacket` calls `UNetConnection::ShouldReplicateVoicePacketFrom` **per packet, per connection**, which calls **virtual** `APlayerController::IsPlayerMuted(Sender)`. Override it to enforce ghost and proximity rules on the host. | `NetDriver.cpp:1684-1709`; `NetConnection.cpp:5525-5550`; `PlayerController.h:1008`, `PlayerController.cpp:3925` |
| Voice subsystem override | `[/Script/OnlineSubsystemUtils.OnlineEngineInterfaceImpl] VoiceSubsystemNameOverride`, used when the default OSS (EOS) has no voice interface | `OnlineSubsystemUtils/Private/OnlineEngineInterfaceImpl.h:55-57` |
| UE VOIP capture platforms | Voice module capture exists for **Windows, Mac, Linux, Android**. No iOS capture file in the installed source (UNVERIFIED whether it lives elsewhere). Opus codec; `EVoiceSampleRate` 16k/24k. | `Engine/Source/Runtime/Online/Voice/Private/*`; `Sound/AudioSettings.h:22-27` |

### 4.2 Options compared

| | **A. EOS RTC (lobby room)**, recommended | B. UE built-in VOIP over the game net driver | C. ODIN (4Players) | D. Vivox (Unity) |
|---|---|---|---|---|
| Transport | Epic voice servers | Through the **host** (adds to host upload) | 4Players servers | Unity servers |
| During host migration | **Keeps working** | Drops, since the connection dies | Keeps working | Keeps working |
| Proximity / 3D | Phase 1: volume by distance. Phase 2: manual output → UE spatial audio | Full UE 3D (attenuation, occlusion, effect chains) via UVOIPTalker | Built in (`OdinSynthComponent` on the pawn, server spatial culling) [WEB] | "3D positional" [WEB] |
| Per-talker amplitude | Unmixed PCM delegate + FEnvelopeFollower | `GetVoiceLevel()` / `GetAmplitudeOfRemoteTalker()` built in | Per-peer silence event; level API UNVERIFIED | UNVERIFIED |
| Ghost rules | Enforced on each client from replicated state (a modded client could ignore them) | **Enforced by the host per packet** (`IsPlayerMuted` override) | Rooms, server-side | Channels |
| Platforms (5.8.3) | Win64, Mac, Android (**no iOS in the allow list**) | Win/Mac/Linux/Android capture; iOS UNVERIFIED | UE 5.3+, Android and iOS binaries, 5.8-compatible v2.1.4 [WEB] | Official SDK lists only UE 5.3 [WEB] |
| Cost | Free | Free (host bandwidth) | 25 free PCU (peak concurrent users), then €0.29/PCU/month [WEB] | Free up to 5k PCU (UNVERIFIED) |
| Glue needed with OSS EOS | Low (OSS EOS enables lobby RTC) | **Custom:** OSS EOS returns no `IOnlineVoice`. Needs a tiny custom OSS whose `GetVoiceInterface()` returns `FOnlineVoiceImpl(EOS subsystem)` (the constructor takes any `IOnlineSubsystem*`: `VoiceInterfaceImpl.h:124`), selected via `VoiceSubsystemNameOverride`, so packet senders carry EOS IDs that match PlayerStates. ~1–2 days, UNVERIFIED end-to-end. | Plugin | Plugin |

The legacy Vivox plugin inside UE was deprecated in 4.26 and removed in 4.27. Photon Voice has no Unreal SDK [WEB].

### 4.3 Recommended voice design

**Phase 1 (prototype, days):** stock OSS EOS lobby RTC.
- Each client runs a `UKGVoiceSubsystem` that ticks at 10 Hz:
  - **Living listener:** mute all dead participants (`SetPlayerMuted` / `SetChannelPlayerMuted`) and set each living talker's volume = f(distance, same zone, occlusion trace). f is a smoothstep from 1.0 at 3 m to 0 at 25 m, halved through walls or across zones.
  - **Dead listener:** dead talkers at full volume (2D). Living talkers at distance-based volume if the design allows ghosts to hear the living, otherwise muted.
  - **Meeting phase:** everyone alive at full volume; the dead stay muted for the living.
- **Mouths:**
  - Register the unmixed delegate. On the audio thread, compute RMS per `PlayerName` (the PUID string), run `Audio::FEnvelopeFollower` (attack ~10 ms, release ~120 ms), and store the result in a lock-free `TMap<PUID, std::atomic<float>>` (or a `TArray` indexed by player slot).
  - The game thread reads it every frame into an AnimBP curve `JawOpen` (0–1), with a gate at ~-45 dBFS and a gamma of ~0.6 for readable movement.
  - Local mouth: use `RegisterOnVoiceChatBeforeCaptureAudioSentDelegate` the same way.
- **Push-to-talk and voice activation:** `TransmitToAllChannels` / `TransmitToNoChannels`.

**Phase 2 (before playtests):** true 3D with manual output.
- Join the lobby RTC room with `bUseManualAudioOutput = true` through direct EOS SDK calls. The UE wrapper doesn't expose this, so write a small `KGEOSVoice` module and call `EOS_Lobby_*` / `EOS_RTCAudio_*` directly.
- Push each participant's unmixed buffer into a ring buffer inside a `UKGVoiceSynthComponent : USynthComponent` attached to that player's head bone. Pattern: `UVoipListenerSynthComponent`.
- UE audio then handles attenuation, occlusion, reverb sends and a ghost "ethereal" submix. The same buffer drives the mouth.
- Ghost-to-ghost voice is 2D and non-spatialized.

**Phase 3 (optional hardening):**
- A separate **ghost lobby** (a user can be in up to 16 lobbies) with its own RTC room.
- Dead players call `EOS_Lobby_LeaveRTCRoom` on the main lobby's room (or disable sending), so living clients never even receive ghost audio.

**Fallback:** if EOS RTC becomes a problem (iOS, limits), switch to ODIN (fastest drop-in 3D) or option B (host-authoritative, but costs host upload and drops during migration).

**Open items:**
- **Maximum participants per EOS RTC room is UNVERIFIED.** Confirm it's ≥ 20 before committing. It isn't in the SDK headers, and Epic's voice docs didn't render.
- **PEGI June 2026:** voice or text chat without block/report tools gets **PEGI 18** [WEB]. Ship mute, block and report per player from day one.

---

## 5. Replication and physics

### 5.1 Iris status

- **In the 5.8.3 install [ENGINE]:**
  - `Engine/Plugins/Experimental/Iris/Iris.uplugin` has `"IsBetaVersion": true`.
  - Runtime switch `net.Iris.UseIrisReplication` defaults to **0**: `static int32 GUseIrisReplication = 0;` (`Engine/Source/Runtime/Net/Iris/Private/Iris/IrisConfig.cpp:15-16`).
  - It is always compiled in (`UE_WITH_IRIS=1` in `ModuleRules.SetupIrisSupport`, `UnrealBuildTool/Configuration/Rules/ModuleRules.cs:1747-1752`).
  - `GameNetDriver` has `bCanUseIris=true` (`BaseEngine.ini:346`).
- **Epic's statements [WEB]:**
  - 5.7: Beta, compiled in, still off by default (Epic staff, forum "5.7 Iris strategy"); no plan to deprecate legacy or RepGraph.
  - 5.8 release notes (5.8.0, 2026-06-17): "Iris is now production-ready for licensees in UE 5.8".
  - The 5.8 Iris docs page still shows Experimental and "opt-in".
  - No statement found that Iris will become the default.
- **Recommendation:** ship on **legacy replication** at 6–20 players, where it isn't the bottleneck, and stay Iris-ready:
  - `bReplicateUsingRegisteredSubObjectList = true` and `AddReplicatedSubObject`; no `ReplicateSubobjects` overrides.
  - Push-model macros.
  - `FFastArraySerializer` for lists.
  - No custom `NetSerialize` without planning an Iris `NetSerializer`.
  - Keep a branch with `net.Iris.UseIrisReplication=1` and run the 20-bot Gauntlet soak monthly. Switch only if it measurably lowers host CPU.

### 5.2 Physics props (R.E.P.O.-style grabbing)

**Modes** (`EPhysicsReplicationMode`, `Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h:3614-3634`) [ENGINE]:

| Mode | Engine description |
|---|---|
| `Default` | — |
| `PredictiveInterpolation` | "velocity interpolation … Not forward predicted … Use alongside a kinematic pawn (not physics simulated)". Designed for local predictive interaction with autonomous proxies. |
| `Resimulation` | Display name "Resimulation (WIP)". Needs Physics Prediction. |
| `None` | — |

- **Settings:** `UPhysicsSettings::PhysicsPrediction` has the display name **"Physics Prediction (Experimental)"**.
  - `bEnablePhysicsPrediction` "needs … Tick Physics Async enabled".
  - `MaxSupportedLatencyPrediction` defaults to 1000 ms.
  - Source: `Engine/Classes/PhysicsEngine/PhysicsSettings.h:191-267`.
- **Recommended setup:**
  - Characters use kinematic `CharacterMovementComponent` (not physics pawns).
  - Props: `SetPhysicsReplicationMode(PredictiveInterpolation)` with Physics Prediction and Async Physics (fixed 30–60 Hz) turned on. Resimulation stays off because it's WIP.
  - **Grab:** the client sends a grab-target point (camera ray + hold distance) at 30 Hz via an unreliable Server RPC. The server drives a `UPhysicsHandleComponent` or a PD spring on the authoritative body.
  - The holder's client runs a **local visual follow**: it spring-lerps the prop mesh toward its own hold point, blending to the replicated state over ~150 ms after release. This hides round-trip lag, which R.E.P.O.'s devs said was their hardest problem [WEB: Photon blog].
  - Multi-carry: the server sums spring forces from every grabber.
  - Budget: ≤ 150 simulating bodies and ≤ 20 awake at once on the host [EST]. Sleep aggressively; asleep means dormant.
  - Valuable damage (a R.E.P.O.-style mechanic): compute from server-side impact impulse only.

### 5.3 Chaos destruction (Geometry Collections)

- **Component properties [ENGINE]** (`GeometryCollectionComponent.h:1429-1655`):
  - `bEnableReplication`, `bEnableAbandonAfterLevel`, `ReplicationAbandonAfterLevel`.
  - `ReplicationMaxPositionAndVelocityCorrectionLevel`: "client will only receive the initial break velocity … only the destruction state … needs to be replicated".
  - Setter helpers are to be called before the component registers.
- **Cheapest multiplayer pattern:**
  - Gameplay-relevant breakables (doors, windows, crates): **don't replicate the GC**. Replicate a compact `FKGBreakEvent {BreakableId, ImpactPoint, Impulse, Seed}` as dormant replicated state.
  - Each client runs the fracture locally as a cosmetic effect with collision against pawns off, and debris despawns after 5–10 s.
  - Keep a replicated `bBroken` flag so late joiners and migrated clients show a pre-broken rubble mesh.
  - If fragment positions matter, use `bEnableReplication=true`, `bEnableAbandonAfterLevel=true`, `ReplicationAbandonAfterLevel=1`, `ReplicationMaxPositionAndVelocityCorrectionLevel=0`.
  - Forum note [WEB, community]: the owning actor needs Replicate Movement.
- **Mobile:** swap to a "pre-broken static mesh + Niagara burst" variant through a per-platform switch or scalability CVar.

---

## 6. Tooling

### 6.1 Visual Studio for UE 5.8

**Authoritative for UBT:** `$UE\Engine\Config\Windows\Windows_SDK.json` [ENGINE]

| Item | Value |
|---|---|
| **Minimum VS** | VS 2022 **17.8**; VS 2026 **18.0** |
| **Preferred MSVC** | `14.50.35717-14.50.99999` (VS 2026 18.0) and `14.44.35207-14.44.99999` (VS 2022 17.14) |
| **Banned MSVC** | `14.50.0-14.50.35722` (ICEs), `14.44.0-14.44.35210` (template error), `14.40.0-14.43.99999`, `14.39.x` |
| Minimum MSVC | 14.38.33130 |
| **So in practice** | **VS 2022 17.14 with MSVC ≥ 14.44.35211**, or **VS 2026 18.x with MSVC ≥ 14.50.35723** |
| Windows SDK | Main **10.0.22621.0**, minimum 10.0.19041.0 |
| Clang (optional) | Preferred 20.1.8+, minimum 18.1.8 |
| Suggested VS components | `Microsoft.Net.Component.4.6.2.TargetingPack`, `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`, `Microsoft.VisualStudio.Component.Windows11SDK.22621`, `Microsoft.VisualStudio.Component.VC.Llvm.Clang`, workloads `CoreEditor`, `ManagedDesktop`, `NativeDesktop`, `NativeGame`, plus `Component.Unreal.Ide` and `Component.Unreal.Debugger` |
| VS 2022 extra | `Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64` + `...ATL` |
| VS 2026 extra | `Microsoft.VisualStudio.Component.VC.14.50.18.0.x86.x64` + `...ATL` |
| .NET | UBT runs on bundled **.NET 10.0.7** runtime / SDK 10.0.203 (`Engine/Binaries/ThirdParty/DotNet/10.0/win-x64`); no separate install needed |

- **UE 5.8 docs [WEB]:**
  - VS 2022 17.14+ or **VS 2026 18.0+**; the docs say to use VS 2026 for general development.
  - Windows SDK recommended 10.0.26100+.
  - Workloads: Desktop development with C++, Game development with C++, .NET desktop development, .NET Multi-platform App UI development.
  - Components: C++ profiling tools, C++ AddressSanitizer, Windows 10/11 SDK, **Unreal Engine installer**.
  - Where the docs and the JSON differ, the JSON decides what UBT accepts.
- **Recommendation:** VS 2026 (18.x, latest), or VS 2022 17.14 if the team already has it. Pin the MSVC LTSC toolset component listed above.

### 6.2 Automation from an external agent

**Python Editor Script Plugin** (`Engine/Plugins/Experimental/PythonScriptPlugin`, `IsBetaVersion:true`, off by default):
- **Headless:** `UnrealEditor-Cmd.exe <Project>.uproject -run=PythonScript -Script="D:/path/build_level.py arg1"`.
  - The commandlet parses `-Script=` itself, force-enables Python, runs with `EPythonCommandFlags::Unattended` and returns non-zero on error (`PythonScriptCommandlet.cpp:15-78`).
- **With a UI or editor world:** `UnrealEditor.exe <Project> -ExecutePythonScript="script.py args"`.
  - Editor-only; it **errors under a commandlet** with "Use -run=PythonScript instead?" (`EditorPythonExecuter.cpp:144-207`).
- **Live control of a running editor:** `Content/Python/remote_execution.py` (UDP multicast discovery plus a TCP command channel). Turn on Project Settings → Python → Remote Execution.
- **Import:**
  - `unreal.AssetImportTask` / `AssetTools.import_asset_tasks` for FBX and glTF.
  - Interchange (`Engine/Plugins/Interchange/Runtime`, not experimental) handles glTF/FBX via Interchange pipelines, scriptable from Python.
  - `GLTFExporter` exists (Enterprise).
- **Level building:** `unreal.EditorLevelLibrary` / `EditorActorSubsystem.spawn_actor_from_class`, `LevelEditorSubsystem.save_current_level`, World Partition builders via commandlet (`-run=WorldPartitionBuilderCommandlet`, e.g. for HLODs; UNVERIFIED flags for 5.8).
- **Unreal MCP plugin:** new and Experimental, `Engine/Plugins/Experimental/ModelContextProtocol/ModelContextProtocol.uplugin`.
  - Descriptor: "Anthropic MCP (Model Context Protocol) server implementation for Unreal Engine", `NoRedist`, depends on `ToolsetRegistry`.
  - It's an in-editor HTTP server (HttpServer module; JSON-RPC with SSE streaming, origin-header validation) with a configurable port and path (tests use `/mcp`). Default port is UNVERIFIED.
  - Related plugins: `MCPClientToolset` (Beta and Experimental), `AIAssistant`, `PCGToolset`, `StateTreeToolset`.
  - **This is the most direct way for an external agent to drive the editor.** Start with Python commandlets for reproducible batch jobs and add MCP for interactive work.

### 6.3 Gauntlet for host-migration tests

- **Plugin:** `Engine/Plugins/Experimental/Gauntlet` (Beta). C# framework in `Engine/Source/Programs/AutomationTool/Gauntlet`.
- **Roles:** `UnrealTargetRole` = `Editor, EditorGame, EditorServer, Client, Server, Host, CookedEditor`. There is **no ListenServer role** (`Gauntlet.UnrealHelpers.cs:22-32`).
- **Pattern:**
  - `Config.RequireRoles(UnrealTargetRole.Client, UnrealTargetPlatform.Win64, N)`.
  - The host role gets `MapOverride="/Game/Maps/Town?listen"`; the others get `MapOverride="127.0.0.1:7777"` (or an EOS URL) (`Gauntlet.UnrealTestConfiguration.cs:449-544, 928-941, 1129`; [WEB] Redpoint gist).
  - Per-role `CommandLine` adds `-KGBot -KGTestSeed=…`.
  - The test's `TickTest()` kills the host role's process after T seconds, then parses client logs for markers.
  - Gauntlet also has an RPC framework (`Gauntlet/Framework/RpcFramework`) for driving running instances.
- **Caveat [WEB]:** a single *functional test* spanning several processes is not supported, so orchestrate from Gauntlet (C#), not from one in-engine test.
- **Fast iteration:** multi-client PIE ("Play as Listen Server", N clients) is fine for replication work. **It can't test host migration** because everything runs in one process with shared state. Use standalone processes.

---

## 7. First-person viewmodel (CS2-style)

### 7.1 Native First Person Rendering

**Exact names in 5.8.3 [ENGINE]:**

- **Primitive side** (`Engine/Classes/Components/PrimitiveComponent.h`):
  - `enum class EFirstPersonPrimitiveType : uint8 { None, FirstPerson, WorldSpaceRepresentation }` (lines 151-162).
  - `WorldSpaceRepresentation` "implicitly sets bCastHiddenShadow=false and bOwnerNoSee=true internally, which is required for first person shadow to work correctly with VSM".
  - Property `FirstPersonPrimitiveType` (line 728-730); setter `SetFirstPersonPrimitiveType()` (line 2120).
- **Camera side** (`Engine/Classes/Camera/CameraComponent.h`):
  - `FirstPersonFieldOfView` (horizontal degrees, lines 48-54), `FirstPersonScale` (0.001–1, "scale down primitives towards the camera such that they are small enough not to intersect with the scene", lines 56-62).
  - Enabled by `bEnableFirstPersonFieldOfView` and `bEnableFirstPersonScale` (lines 209-217).
  - The same fields exist on `USceneCaptureComponent2D`.
  - `FMinimalViewInfo` carries `FirstPersonScale` and `bUseFirstPersonParameters` (`CameraTypes.h:62, 107`).
  - `UGameplayStatics::TransformWorldToFirstPerson(ViewInfo, WorldPos, bIgnoreFirstPersonScale)` spawns muzzle FX and projectiles correctly (`GameplayStatics.h:1470-1479`).
- **Epic's own template** (`$UE\Templates\TP_FirstPerson\Source\TP_FirstPerson\TP_FirstPersonCharacter.cpp:22-38`):
  - Arms: `SetOnlyOwnerSee(true)` + `FirstPersonPrimitiveType = FirstPerson`.
  - Camera: `bEnableFirstPersonFieldOfView = true`, `bEnableFirstPersonScale = true`, `FirstPersonFieldOfView = 70`, `FirstPersonScale = 0.6`.
  - Body: `SetOwnerNoSee(true)` + `FirstPersonPrimitiveType = WorldSpaceRepresentation`.
  - The Shooter variant applies the same to weapon meshes (`ShooterWeapon.cpp:27-36`).
- **Renderer:**
  - The transform is applied in the vertex shader after WPO, in `BasePassVertexShader.usf:60` **and `MobileBasePassVertexShader.usf:67`**. So FOV and anti-clipping work on the mobile renderer too.
  - Self-shadow and the world-body shadow are deferred features (`Renderer/Private/Shadows/FirstPersonSelfShadow.cpp`, CVars `r.FirstPerson.SelfShadow*`, `r.FirstPerson.Shadow*`).
- **Docs and talk [WEB]:**
  - Advanced features (self-shadow, body shadow on the ground, reflections, the first-person scene-texture mask) need a GBuffer bit. They "do not work with the mobile renderer or with forward rendering", and need Allow Static Lighting off.
  - Ground shadows from the body need VSM or ray-traced shadows.
  - Grooms aren't supported.
  - Status: 5.5 Experimental → 5.6 Beta. The 5.8 page shows no status label; 5.7 "production-ready" is UNVERIFIED.
- **Without VSM** (our PC default and mobile): skip `WorldSpaceRepresentation` on the body. Use classic `bOwnerNoSee = true` + `bCastHiddenShadow = true` so the body still casts a CSM shadow. UNVERIFIED interplay; test it.
- **Custom camera managers** (Lyra-style) must fill the first-person fields in `GetCameraView` / `FMinimalViewInfo`. A 5.5 forum report said it doesn't work with Lyra out of the box [WEB].

### 7.2 Older approaches, for comparison

| Approach | Pros | Cons |
|---|---|---|
| Panini "FOV" material function via WPO (UT4) | Works everywhere | Per-material work; shadows and lighting are computed on distorted positions |
| `GetRenderMatrix` override (C++) | Per component | Attachments and particles need extra handling |
| SceneCapture2D viewmodel | Total isolation | Renders the scene twice, lighting doesn't match; worst on mobile |

**Verdict:** the native feature replaces all of these.

### 7.3 CS2-quality viewmodel animation

- **Rig:**
  - Arms skeleton plus a weapon on `ik_hand_gun`, with hand IK to weapon sockets.
  - One arms AnimBP with **Linked Anim Layers** per weapon: base `ABP_ItemAnimLayersBase` + per-weapon children, via `LinkAnimClassLayers` on equip (the Lyra pattern [WEB]).
- **Procedural layer** (C++ in `UAnimInstance::NativeThreadSafeUpdateAnimation`, output as an additive transform on the weapon root):
  - **Sway:** mouse delta → spring target rotation (a critically damped spring by half-life, [WEB] Holden "Spring-It-On").
  - `UKismetMathLibrary::VectorSpringInterp` / `QuaternionSpringInterp` with `FVectorSpringState` exist for Blueprint [WEB].
  - **Bob:** a Lissajous figure-8 driven by the step phase of the movement component, with amplitude from speed and damped when crouching or aiming.
  - **Recoil:** per-shot impulse into a position spring and a rotation spring (kick back, up, random yaw). A separate slower spring handles camera recoil. Recoil patterns come from a `UCurveVector` per weapon, so they are deterministic and learnable like CS.
  - **Landing and jump:** vertical spring impulse. **Wall proximity:** lower the weapon when a short trace from the camera hits, which FirstPersonScale makes nearly unnecessary.
- **Montages:** equip, fire, reload and **inspect** on an upper-body slot. Inspect is cancelled by fire, reload or ADS via `Montage_Stop` with a 0.1 s blend (UNVERIFIED design).
- **Frame-rate independence:** run springs with sub-stepping (fixed 240 Hz inner loop) so sway feels the same at 30 and 300 fps.
- **Third-person body:**
  - Game Animation Sample Project (motion matching, Chooser tables) looks great, but [WEB] has client jitter under latency and the trajectory doesn't replicate, and no cost figures exist for 16–20 characters.
  - **Recommendation:** start with a blendspace + state machine AnimBP (cheap, predictable, mobile-safe) and prototype motion matching later behind a scalability switch.
  - Use Animation Budget Allocator / URO for distant characters.
- **Spectating another player:** when the view target changes, swap visibility (`SetOnlyOwnerSee` checks follow the *view target* owner) and spawn a local "spectator arms" copy driven by replicated aim and fire state. Needs custom work; UNVERIFIED specifics.

### 7.4 Weapon skin system (CS-style)

- **Data:** `UKGPaintKit` (DataAsset) holds the base material instance, pattern textures, wear min/max, and pattern offset/rotation/scale ranges.
- **Item instance:** `FKGItemInstance { uint16 ItemDefId; uint16 PaintKitId; uint16 WearQ; uint16 Seed; uint8 StatTrakFlag; int32 StatTrakCount; TArray<FKGSticker,max 4> }`, replicated only on equip.
  - Wear ranges as in CS: FN 0–0.07, MW 0.07–0.15, FT 0.15–0.37 (0.38 per the community), WW 0.37–0.44 (0.45), BS 0.44–1.0 [WEB counter-strike.net].
  - Seed 0–1000 → hash → UV offset, rotation and scale within the kit's ranges (CS seed semantics are UNVERIFIED community knowledge).
- **Rendering:**
  - **Custom Primitive Data** (`UPrimitiveComponent::SetCustomPrimitiveDataFloat`, `PrimitiveComponent.h:868, 1185-1213`) avoids one dynamic material instance per weapon.
  - Slots: 0 = wear, 1–3 = pattern offset and rotation, 4–5 = StatTrak digits, and so on.
  - It's declared on `UPrimitiveComponent`, so it applies to skeletal meshes. Shader support on skinned meshes is UNVERIFIED; test it.
  - Wear = a grunge/scratch mask thresholded by wear, with edge-wear from a curvature map.
  - **Stickers:** UV-space projection inside the weapon material (per-slot UV rect and rotation as CPD, sampling a sticker atlas). It's cheaper than decal components and works in first person and on mobile.
- **Authoring:** a small editor utility widget previews kit × wear × seed; generate thumbnails with a Python script (§6.2).

---

## 8. Optimization and mobile ("do from day one")

**Engine facts [ENGINE]:**
- **Android:** Vulkan is the default RHI (`bSupportsVulkan=true`, `bDetectVulkanByDefault=true`, `BaseEngine.ini:3285-3288`). ES3.1 and Vulkan ES3.1 shader platforms (`BaseEngine.ini:3979-3980`).
- **High-end SM6 paths:**
  - Android SM6 preview with `bSupportsLumenGI=true`.
  - iOS `METAL_SM6_IOS` with `bSupportsNanite=true` and Lumen GI (`Config/Android|IOS/DataDrivenPlatformInfo.ini`).
  - These only suit top-tier devices, so **don't depend on them**.
- **World Partition server streaming** is **off** by default (`wp.Runtime.EnableServerStreaming=0`, `WorldPartition.cpp:128-134`). The server, which here is the **listen host**, keeps every cell loaded.

**Day-one list:**

1. **One content standard for all platforms:**
   - Every master material compiles for the ES3.1/Vulkan mobile feature level.
   - Test in Mobile Preview (Android Vulkan ES3.1) weekly from the first greybox.
   - At most ~8 master materials, with material instances everywhere. Stylized shading uses gradient-map lighting and a baked AO texture instead of GI.
2. **Renderer choice:**
   - **PC:** deferred renderer, Lumen **off** by default (optional "Ultra" setting later), VSM **off** (CSM with 3 cascades), TSR or TAA.
   - **Mobile:** mobile forward renderer, MSAA 4x, 1–2 CSM cascades, no shadowing local lights, ≤ 4 local lights per view.
   - No Nanite dependency: author real LODs (LOD0 ≤ 5k tris for buildings, auto-LOD 3 levels).
3. **Dynamic day-night that runs on mobile:**
   - One **movable** directional light (sun or moon swap), a sky atmosphere or a cheap gradient sky material, and a height fog color curve.
   - Skylight: a low-res cubemap blend between 3–4 pre-captured time-of-day cubemaps rather than realtime capture on mobile. Realtime capture on mobile is UNVERIFIED for 5.8.
   - A `UKGTimeOfDaySubsystem` drives everything from one `UCurveLinearColor` set per preset and replicates only `TimeOfDay` (a float).
   - Night gameplay: emissive windows and streetlamps as emissive plus a few non-shadowed point lights, culled by distance.
4. **World Partition** for the 500 m × 500 m town plus underground:
   - One File Per Actor for team merges. Runtime grid cell **64 m**, loading range **~160 m on PC / ~96–128 m on mobile** via per-platform config [EST].
   - **Underground:** a separate runtime grid or a Data Layer, activated by zone volumes. Streaming sources (player controllers) load what's near.
   - **HLOD layers:** instanced HLOD for props and foliage, merged + simplified mesh HLOD for building blocks. Build with the WP HLOD builder commandlet in CI.
   - **Gameplay actors** (task stations, doors, spawners, anything replicated or snapshotted) get `Is Spatially Loaded = false` so streaming never interferes with replication or host migration.
   - **Listen host pitfall:** with server streaming off, the host loads the whole map. That's fine at 500 m on PC, one more reason mobile never hosts. Check host render cost of "everything loaded" (UNVERIFIED whether it skips HLODs on the host).
5. **Instancing:** repeated props (fences, lamps, crates, windows) use ISM/HISM, or placement through PCG with instanced output. Keep town-block draw calls ≤ 300 on mobile [EST] and use texture atlases per district.
6. **Culling:** set `CullDistance` on small props (per-platform volume settings), use occlusion-friendly block layouts, and hide the underground from the surface with a dedicated streaming layer.
7. **Scalability:**
   - Ship `DefaultScalability.ini` overrides from day one with 4 PC tiers plus Android/iOS device profiles (`DefaultDeviceProfiles.ini`) keyed to GPU family.
   - Mobile targets **30 fps** at 60–70% screen percentage with dynamic resolution.
   - Budgets per tier: shadow distance, view distance, post-processing (bloom only on mobile), and skeletal mesh LOD bias for distant characters.
8. **Characters:**
   - ≤ 20 characters. Mobile skeletal LOD0 ≤ 15k tris, ≤ 60 bones in LOD1+.
   - Animation Budget Allocator on, URO for distant characters, no motion matching on mobile.
9. **Profiling from week 1:**
   - `stat unit`, `stat rhi`, Unreal Insights with `-trace=default,net`, Network Insights, on a mid-range Android device.
   - Use the Gauntlet 20-bot soak (§6.3) as a performance regression gate.
10. **Mobile networking:** a mobile client needs ~16 KB/s down and ~3 KB/s up at 20p. Mobile never hosts.

---

## 9. Game references and design lessons

### 9.1 LOCKDOWN Protocol (Mirage Creative Lab, UE 5.2 → 5.5)

- **Release:** early access July 2024, full release Nov 2025, up to 16 players [WEB].
- **Roles:** employees vs dissidents. Dissidents' only extra power is recognizing each other; they sabotage with the **same tools** everyone has. Each player has a 2-item pocket, and stamina also works as health [WEB].
- **Tasks are physical** (8 types):
  - Deliveries: carry non-pocketable packages one at a time to the room with the matching symbol.
  - Vents: unscrew a filter, carry it to the washer, put it back.
  - Power: fuses and charged batteries. Scanner: needs 2 players.
- **Faking and sabotage come from the tasks themselves:** press N to fail a computer verification, pull canisters, hide parts, insert the wrong canister and the machine explodes [WEB].
- **Progress is shown in the world** (red or blue lights, charge bars), not as a global bar.
- **Voice:** proximity voice with **sound propagation through level paths**, so voices have direction but no exact position, plus private dissident channels [WEB].
- Netcode and voice middleware are UNVERIFIED.

### 9.2 R.E.P.O. (semiwork)

- **Tech:** Unity + **Photon PUN + Photon Voice 2**. The devs built their own physics smoothing and use voice data "to animate the characters vividly" [WEB: Photon blog]. The mouth uses a loudness value (`clipLoudness`) [WEB: mod pages].
- **Physics grab:** a beam grab with up to 6 players on one object. Damage lowers an item's value [WEB].
- **Death:** a dead player becomes a head that teammates carry to extraction to revive. The "death head" can be possessed for short hops with proximity voice [WEB].

### 9.3 Garry's Mod "Murder" (MechanicalMind source)

- **Roles:** 1 murderer with a knife that can stab or be thrown. One random bystander gets the magnum.
- **Murderer selection is weighted:** `MurdererChance` rises each round a player isn't picked.
- **Weapon locks:** the murderer can't pick up the gun, and bystanders can't pick up the knife.
- **Shooting an innocent:**
  - The shooter drops the gun.
  - They move and jump at 50% for 20 s and can't pick the gun up during that time.
  - The screen goes almost black.
  - Optionally the shooter's name is announced.
- **Loot:** an item spawns about every 12 s. **5 loot = a gun**, then one every 15 more.
  - The murderer can spend 1 loot to **disguise as a corpse** (take its name and color).
- **Footprints:** only the **murderer** sees everyone's footprints, which last 30 s.
- **Murderer smoke:** after **240 s without a kill**, the murderer emits visible black smoke.
- **Codenames:** NATO names with random colors.
- **Voice:** during a round, the **living can't hear the dead**; the dead hear everyone.
- Sources: `sv_tker.lua`, `sv_loot.lua`, `sv_rounds.lua`, `cl_footsteps.lua` [WEB].

### 9.4 Design lessons

1. **Sabotage is using a task wrong** (wrong item, hidden part, failed verification), not a special button. That makes faking natural and gives us clues to read.
2. **Show task progress in the world** so players must go and look, which creates alibis and moments alone.
3. **Carry tasks with 1 item in hand and a tiny inventory** make intent visible. Place 2-person tasks and carry routes deliberately along kill paths.
4. **Anti random-kill penalty** (from Murder): drop the gun, 50% speed, blackout, 20 s pickup ban, announce the name. Friendly-fire hurts, but you're still in the round.
5. **Anti-stall:** a no-kill timer that reveals the killer (smoke or a glow).
6. **Codenames and colors hide identity**, and a corpse disguise costs a resource.
7. **Ghost chat:** the dead hear everything; the living never hear the dead. Enforce it in the voice layer (§4.3), and place spectator "ears" at the camera.
8. **Mouths driven by loudness** (§4.1/4.3): cheap and very readable. Use an envelope, a gate and gamma.
9. **Voice direction and propagation:** in phase 2 of voice, let occlusion and zone rules make voices *helpful but misleading*.
10. **Physics carrying is hard:** use server authority plus a local visual follow (§5.2), and budget time for it.
11. **Keep the dead busy** with ghost tasks, a possessable object, or revive-by-carry variants, without letting them talk to the living.
12. **Weighted role selection** so nobody goes 10 rounds without being the killer.

---

## 10. Cosmetics economy (cases, battle pass, ranks, ownership)

### 10.1 Case opening (spin reel)

- **The outcome is decided first on a trusted service; the reel is only presentation.**
  - Build a strip of ~60 slots from weighted fillers (following the rarity weights) and put the real result at the stop index ~50.
  - Ease out over 5–7 s with a cubic-out curve plus a small random overshoot within the slot, and a tick sound on each slot crossing.
  - Offer a skip button. **Show the odds table on the case screen.**
- **Steam:** Steam Inventory item schema `generator` items with weights (e.g. `501x90;502x9;503x1`), `ExchangeItems` for crate + key → item, `playtimegenerator` drops, `TriggerItemDrop` [WEB]. Steamworks SDK v1.64 with `isteaminventory.h` is bundled [ENGINE]. UE's OSS Steam doesn't wrap it, so call Steamworks directly.
- **X-Ray model** (CS2 in France since 2019, Germany since 16 Mar 2026): you see the item before paying, and you must claim it before scanning again [WEB]. Use this if we ever sell paid keys.

### 10.2 Legal constraints (as of Sept 2026) [WEB]

| Jurisdiction or platform | Rule |
|---|---|
| Belgium | Paid loot boxes = illegal gambling (2018). The Antwerp court (LS v Apple, Jan 2025) confirmed it. Enforcement is weak but the risk is real. |
| Netherlands | The Council of State overturned the €5M EA fine in March 2022. No national ban; the government pushes for an EU-level rule. |
| UK | Self-regulation (Ukie principles, 2023), with poor compliance. Legislation "under review". DCMS skins-gambling report, 25 Sept 2025. |
| **Brazil** | **Lei 15.211/2025 (ECA Digital)**, in force **17 Mar 2026**. Art. 20 bans loot boxes in games aimed at or likely accessed by minors; self-declared age is not enough. Fines up to 10% of Brazilian revenue (capped at R$50M). Whether *free* random rewards are covered is UNVERIFIED. |
| Australia | Since 22 Sept 2024, paid chance-based purchases → at least **M**; simulated gambling → **R18+**. |
| Germany | USK counts paid random items in age ratings (since 2023). CS2 X-Ray mandatory from Mar 2026. |
| EU | Consumer-network (CPC) virtual-currency principles (21 Mar 2025, soft law): show real-money prices, no forced bundles. Digital Fairness Act proposal expected Q4 2026 (odds disclosure, protection of minors). |
| **PEGI (games submitted from June 2026)** | **Paid random items → PEGI 16.** Time- or quantity-limited offers (a countdown battle pass) → PEGI 12, or 7 if spending is off until a parent enables it. **Chat without block/report → PEGI 18.** |
| ESRB | "In-Game Purchases (Includes Random Items)" label. |
| US | FTC vs HoYoverse, Jan 2025, $20M: no loot boxes for under-16s without parental consent, a direct-purchase option, odds and exchange rates shown. |
| China / South Korea | Odds disclosure mandatory (CN since 2017; KR since 22 Mar 2024, enforced). |
| Apple 3.1.1 / Google Play | **Odds must be disclosed before purchase.** Digital goods must go through store billing. Apple 3.1.3(b): items bought elsewhere can be used on iOS only if they're also sold as IAP there. |
| Steam | Market fee is 5% plus a developer-set game fee. Trade reversal window since July 2025. A written Valve policy on paid crates is UNVERIFIED. |

### 10.3 Battle pass, ranks and Gold

- **Battle pass:** free and premium tracks.
  - Tiers never expire once earned, and no punishment for missed days (PEGI and Digital Fairness Act direction).
  - Premium bought with real money at a displayed price, or with Gold earned only by playing.
  - Optional "purchases off until a parent enables them" setting for PEGI 7.
- **Gold:** earn-only soft currency, **never sold** (keeps us out of the EU virtual-currency scope). Show item prices in Gold without odd-sized bundles.
- **Rank:** hidden MMR using a **faction-weighted Elo**, which suits asymmetric roles: Town of Salem adjusts rating by the faction's win rate, team vs opponent average rating, and a K-factor [WEB]. Show a separate visible rank based on playtime and performance.
- **Anti-cheat in P2P:** match results are reported to the backend by **every** participant, as a signed result hash. The backend accepts a result only when a majority agree, and caps XP per hour.
  - Host-only reporting is forgeable. EOS Stats accepts client ingestion (`ingestForAnyUser` for hosts) and is **not** safe for currency [WEB].

### 10.4 Verifying cosmetic ownership in P2P (EOS backend)

| Option | Verdict |
|---|---|
| EOS Ecom | Epic Games Store only, with Epic accounts. **Not** usable for Steam or mobile purchases (partly UNVERIFIED). `bUseNewEcomFlow` exists in `EOSSettings.h:186-188` but is EGS-specific. |
| EOS Player Data Storage | Written by the client, **not safe** for items or currency [WEB]. OK for settings and loadout *preferences*. |
| EOS Stats / Leaderboards | Clients (or the host for everyone) can ingest, so not trustworthy for the economy. OK for casual leaderboards. |
| **Steam Inventory Service** | **Best for PC.** `SerializeResult` → peers check with `DeserializeResult` + `CheckResultSteamID`. The result is signed, can't be replayed, expires after 1 h, and is invalidated by trades, so peers can check each other **without our own server** [WEB]. Put wear and seed in **immutable tags** (`tag_generator`); dynamic properties are wiped on trade and client writes are insecure [WEB]. |
| PlayFab | The free Development Mode has been capped at 1,000 lifetime users since 11 Mar 2026 [WEB]. Not attractive. |
| Nakama (self-host) / Cloud Functions / small custom backend | Needed for **mobile IAP receipt validation** and cross-platform entitlements. |

**Recommended design:**
- **Source of truth:**
  - Steam purchases and drops live in Steam Inventory.
  - Mobile IAP is validated by a **tiny backend** (serverless, keyed by EOS PUID).
  - The backend also mirrors Steam items by webhook or on-login sync, UNVERIFIED mechanism.
- **Peer verification:**
  - On lobby join, each player sends a **backend-signed entitlement token**: Ed25519 over `{PUID, loadout items, expiry 2h}`.
  - Every peer checks it offline with the public key built into the game, so it works on mobile too with no Steam dependency.
  - On a pure-Steam build before the backend exists, send the Steam `SerializeResult` blob instead.
  - Unverified cosmetics show as the default skin, and no one is kicked.

---

## 11. Decisions we recommend

**Networking and host migration**
1. **Listen-server P2P over EOS** (`NetDriverEOS`, `RelayControl=AllowRelays`). **Lobby = the durable match object**, with lobby host migration on.
2. **Host migration v1 = successor re-listen + client `ClientTravel`** (§1.5), targeting ≤ 15 s on PC.
   - Build `UKGSnapshotComponent`, `UKGMatchClock`, `FKGRng` and PUID-keyed player records **before** any gameplay feature.
   - Graceful-leave final snapshot.
   - Heartbeat detection in 2 s.
   - Epoch + ranked successors announced via **member attributes**.
3. **Host election favours PC, open NAT and measured upload** (≥ 5 Mbps for 13–20p). **Mobile never hosts.**
4. **Bandwidth budget:** ~16 KB/s average, 32 KB/s peak per client at 20p. `MaxClientRate=40000`, `NetServerMaxTickRate=30`, `GameSession MaxPlayers=20`. Zone-based relevancy, aggressive dormancy, adaptive update frequency. **Never call `UpdateNetSpeeds`.**
5. **Replication:** legacy now, Iris-ready code, Iris trial branch. Props use `PredictiveInterpolation` with server-authoritative grabbing and client visual follow. Destruction = replicated break event + cosmetic fragments.

**Backend**
6. **OnlineSubsystemEOS (OSSv1)** with `NativePlatformService=Steam`, `bUseEOSConnect=true`, `bUseEAS=false`, `SteamTokenType=WebApi`. Revisit OSSv2 only for mobile login, or add a custom DeviceID Connect login.

**Voice**
7. **EOS RTC lobby room** for all voice.
   - Phase 1: volume-based proximity, ghost muting rules and envelope-driven mouths from the unmixed PCM delegate.
   - Phase 2: manual audio output → per-character spatial `USynthComponent`s.
   - Confirm the RTC room participant cap is ≥ 20 first.
   - Fallbacks: ODIN, then UE VOIP with the custom `IOnlineVoice` glue.
8. **Mute, block and report from day one** (PEGI 18 trigger otherwise).

**Rendering**
9. **Rendering baseline:**
   - PC deferred with Lumen and VSM off by default; mobile forward. No Nanite dependency.
   - Movable-sun day-night with preset ramps.
   - World Partition (64 m cells) + HLOD with gameplay actors always loaded.
   - Native First Person Rendering (FOV 70–74, Scale 0.6) with the world body casting shadow via `bCastHiddenShadow` when VSM is off.
10. **Viewmodel:** linked anim layers per weapon, a spring-based procedural sway/bob/recoil layer in C++, inspect montages. The third-person body uses blendspace + state machine first; motion matching later behind scalability.
11. **Skins:** paint kit DataAssets + Custom Primitive Data (wear, seed, StatTrak) + UV-space sticker atlas; compact replicated `FKGItemInstance`.

**Economy**
12. **Launch without paid random items.**
    - Direct-purchase shop with real-money prices.
    - Earn-only cases (reel animation, odds shown).
    - Earn-only Gold.
    - Free + premium battle pass with no expiry of earned tiers.
    - Trading and Market off at launch.
    - Brazil: turn off random rewards for BR accounts unless there is a reliable age gate.
13. **Ownership:** Steam Inventory on PC + a tiny backend (signed entitlement tokens, IAP validation, majority-reported match results for XP and MMR). Don't store items or currency in EOS PDS/Stats.

**Tooling**
14. **Toolchain:** VS 2026 18.x (MSVC ≥ 14.50.35723) or VS 2022 17.14 (MSVC ≥ 14.44.35211), Windows SDK 10.0.22621. Engine-bundled .NET 10.
15. **Automation:**
    - Python commandlets (`-run=PythonScript -Script=`) for batch import and level building, Interchange for glTF/FBX.
    - Evaluate the Experimental **Unreal MCP** plugin for live agent control.
    - A **Gauntlet** suite (listen host + N clients + host kill) as the nightly migration and performance gate.
16. **Engine build:** stay on the launcher 5.8.3 for now. Plan a **source build** when push-model networking (Game target), custom engine fixes, or iOS EOS plugin patches become necessary.

---

## 12. Blockers and open questions

| # | Item | Impact | Next step |
|---|---|---|---|
| B1 | Android/iOS target platforms not installed; EOS SDK has only Win64 libs locally | No mobile build yet | Install the Android (and later iOS + Mac) target components through the launcher |
| B2 | OSS EOS, SocketSubsystemEOS, EOSVoiceChat and OnlineServicesEOS have **no IOS** in their 5.8.3 allow lists | iOS P2P and voice via Epic plugins | Test a project-local plugin copy with IOS added, or evaluate Redpoint; ask on Epic dev forums |
| B3 | OSS EOS has no DeviceID or Google login mapping | Anonymous mobile login | Custom `EOS_Connect_*` path (≈1–2 days) or OSSv2 auth |
| B4 | EOS RTC room participant cap UNVERIFIED | Voice for 20p | Check the EOS voice docs or a 20-client test before committing |
| B5 | EOS P2P relay bandwidth / rate limits UNVERIFIED | 20p hosts behind strict NAT | Load-test ForceRelays with 20 clients |
| B6 | Push model needs a unique build environment (source engine) for Game targets (inference) | Minor CPU cost | Accept for now; revisit with a source build |
| B7 | UE VOIP with OSS EOS needs custom `IOnlineVoice` glue | Only if falling back to option B | Spike only if EOS RTC fails |
| B8 | First Person Rendering advanced shadows need a deferred renderer + VSM; not on mobile | Visual parity | Use `bCastHiddenShadow` fallback; test |
| B9 | DarxDev plugin 5.8 support and price UNVERIFIED | Buy vs build | Ask the seller for a 5.8 demo; plan to build our own |
| B10 | Brazil ECA Digital scope for *free* random rewards UNVERIFIED | Earn-only cases in BR | Region toggle; legal review before launch |

---

## 13. Sources

**Engine (all under `$UE = D:\Program Files\Epic Games\UE_5.8`)**

*Networking and host migration*
- `Engine/Build/Build.version`
- `Engine/Config/BaseEngine.ini` (lines 337, 346-347, 1479-1538, 1852-1868, 2510-2535, 3285-3305, 3979-3980)
- `Engine/Config/BaseGame.ini` (19-22, 68)
- `Engine/Source/Runtime/Engine/Private/GameNetworkManager.cpp`
- `Engine/Source/Runtime/Engine/Private/NetDriver.cpp` (523, 1684-1709)
- `Engine/Source/Runtime/Engine/Private/NetConnection.cpp` (5525-5550)
- `Engine/Source/Runtime/Engine/Classes/GameFramework/PlayerController.h` (923-1008)
- `Engine/Source/Runtime/Engine/Private/VoiceChannel.cpp`
- `Engine/Source/Runtime/Engine/Classes/GameFramework/GameMode.h`
- `Engine/Source/Runtime/Engine/Private/WorldPartition/WorldPartition.cpp` (128-141)
- `Engine/Source/Runtime/Net/Iris/Private/Iris/IrisConfig.cpp`
- `Engine/Plugins/Experimental/Iris/Iris.uplugin`

*Online plugins and EOS / Steam SDKs*
- `Engine/Plugins/Online/*/*.uplugin` (allow lists)
- `Engine/Plugins/Online/OnlineSubsystemEOS/Source/OnlineSubsystemEOS/Private/OnlineSessionEOS.cpp`, `UserManagerEOS.cpp`, `OnlineSubsystemEOS.cpp`, `Public/EOSSettings.h`
- `Engine/Plugins/Online/SocketSubsystemEOS/Source/SocketSubsystemEOS/Public/NetDriverEOS.h`, `Private/SocketSubsystemEOS.cpp`
- `Engine/Plugins/Online/OnlineServicesEOSGS/Source/Private/Online/AuthEOSGS.cpp`
- `Engine/Plugins/Online/OnlineServices/Source/OnlineServicesInterface/Public/Online/Lobbies.h`
- `Engine/Plugins/Online/OnlineBase/Source/Public/Online/OnlineSessionNames.h`
- `Engine/Plugins/Online/OnlineSubsystem/Source/Private/OnlineSubsystemModule.cpp`
- `Engine/Plugins/Online/OnlineSubsystemNull/Source/Private/OnlineSubsystemNull.cpp`
- `Engine/Plugins/Online/OnlineSubsystemSteam/Source/Private/OnlineSubsystemSteam.cpp`
- `Engine/Source/ThirdParty/EOSSDK/SDK/Include/{eos_version.h, eos_lobby.h, eos_lobby_types.h, eos_p2p_types.h, eos_rtc_types.h, eos_rtc_audio.h, eos_rtc_audio_types.h}`, `EOSSDK.Build.cs`
- `Engine/Source/ThirdParty/Steamworks/Steamv164`

*Voice*
- `Engine/Plugins/Online/VoiceChat/VoiceChat/Source/Public/VoiceChat.h`
- `Engine/Plugins/Online/VoiceChat/EOSVoiceChat/Source/EOSVoiceChat/Private/EOSVoiceChatUser.cpp`
- `Engine/Plugins/Online/OnlineSubsystemUtils/Source/OnlineSubsystemUtils/{Public/VoipListenerSynthComponent.h, Public/VoiceEngineImpl.h, Public/VoiceInterfaceImpl.h, Private/VoiceInterfaceImpl.cpp, Private/VoiceEngineImpl.cpp, Private/OnlineEngineInterfaceImpl.h/.cpp}`
- `Engine/Plugins/Online/OnlineSubsystem/Source/Public/Interfaces/VoiceInterface.h`
- `Engine/Source/Runtime/Engine/Public/Net/VoiceConfig.h`
- `Engine/Source/Runtime/Online/Voice/*`
- `Engine/Source/Runtime/SignalProcessing/Public/DSP/EnvelopeFollower.h`
- `Engine/Source/Runtime/Engine/Classes/Sound/AudioSettings.h`

*Physics*
- `Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h` (3614-3634)
- `Engine/Source/Runtime/Engine/Classes/PhysicsEngine/PhysicsSettings.h` (191-267)
- `Engine/Source/Runtime/Experimental/GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h`

*First-person rendering*
- `Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h`, `Camera/CameraComponent.h`, `Camera/CameraTypes.h`, `Kismet/GameplayStatics.h`
- `Engine/Shaders/Private/{BasePassVertexShader.usf, MobileBasePassVertexShader.usf}`
- `Engine/Source/Runtime/Renderer/Private/Shadows/FirstPersonSelfShadow.cpp`
- `Templates/TP_FirstPerson/Source/TP_FirstPerson/{TP_FirstPersonCharacter.cpp, Variant_Shooter/Weapons/ShooterWeapon.cpp}`

*Build, tooling and platform config*
- `Engine/Config/Windows/Windows_SDK.json`
- `Engine/Binaries/ThirdParty/DotNet/10.0`
- `Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/{TargetRules.cs, ModuleRules.cs}`
- `Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/{PythonScriptCommandlet.cpp, EditorUtilities/EditorPythonExecuter.cpp}`, `Content/Python/remote_execution.py`
- `Engine/Plugins/Experimental/ModelContextProtocol/*`
- `Engine/Source/Programs/AutomationTool/Gauntlet/Unreal/{Utils/Gauntlet.UnrealHelpers.cs, Base/Gauntlet.UnrealTestConfiguration.cs}`
- `Engine/Config/Android/Android_SDK.json`, `Engine/Config/IOS/IOS_SDK.json`, `Engine/Config/{Android,IOS}/DataDrivenPlatformInfo.ini`

**Web**

*Host migration*
- Forums: https://forums.unrealengine.com/t/host-migration-possible-with-client-server-listen-servers/384489 · https://forums.unrealengine.com/t/is-it-possible-to-set-up-host-migration/283823 · https://forums.unrealengine.com/t/host-migration-eu5-eos-2023-c/793445
- Plugins and samples: https://www.fab.com/listings/6e6f573d-cb01-461d-bcf4-58a88e68c873 · https://www.fab.com/listings/e5f036a3-c476-40bc-9eaa-531e9617c0c5 · https://github.com/betidestudio/EOSIntegrationKit · https://eik.betide.studio/multiplayer/matchmaking/lobbies · https://github.com/oculus-samples/Unreal-SharedSpaces/blob/eos-5.x/Documentation/SharedSpaces.md · https://docs.redpoint.games/docs/ossv1/lobbies/data/
- Talks and case studies: https://edgegap.com/blog/live-multiplayer-games-p2p-host-migration-a-technical-cost-analysis-of-backend-infrastructures-presentation-by-michal-buras-(lead-network-engineer-at-highwire-games) · https://www.gdcvault.com/play/1014345/I-Shot-You-First-Networking · https://gdcvault.com/play/1022247/Shared-World-Shooter-Destiny
- Other engines and platforms: https://docs.unity3d.com/Packages/com.unity.netcode@1.5/manual/host-migration/host-migration-intro.html · https://docs.unity.com/en-us/lobby/host-migration · https://partner.steamgames.com/doc/api/isteammatchmaking
- UE docs and community: https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine · https://dev.epicgames.com/documentation/en-us/unreal-engine/lobbies-interface-in-unreal-engine · https://wizardcell.com/unreal/persistent-data/ · https://ikrima.dev/ue4guide/networking/networkid-funiquenetid-fnetworkguid/

*EOS and online services*
- https://onlineservices.epicgames.com/ · https://www.engadget.com/epic-online-services-free-voice-easy-anti-cheat-130055322.html
- https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine · https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-online-services-in-unreal-engine · https://dev.epicgames.com/documentation/unreal-engine/using-lyra-with-epic-online-services-in-unreal-engine
- https://dev.epicgames.com/docs/api-ref/functions/eos-connect-transfer-device-id-account · https://dev.epicgames.com/docs/en-US/api-ref/functions/eos-rtc-audio-add-notify-audio-before-render
- https://docs.redpoint.games/docs/licensing/ · https://developer.apple.com/app-store/review/guidelines/

*Voice*
- https://odin.4players.io/pricing/ · https://github.com/4Players/odin-sdk-unreal/releases
- https://docs.unity.com/ugs/en-us/manual/vivox-unreal/manual/Unreal/unreal-release-notes · https://www.photonengine.com/sdks
- https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UVOIPTalker

*Replication and physics*
- https://forums.unrealengine.com/t/5-7-iris-strategy/2699685 · https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US
- https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview · https://vorixo.github.io/devtricks/phys-prediction-use/ · https://forums.unrealengine.com/t/5-3-chaos-destruction-replication/1326436

*Tooling*
- https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/gauntlet-automation-framework-overview-in-unreal-engine · https://gist.github.com/hach-que/8a736c37bc6a17e10a2a039f4b228467 · https://dev.epicgames.com/documentation/unreal-engine/testing-and-debugging-networked-games-in-unreal-engine?lang=en-US

*First-person rendering and animation*
- https://dev.epicgames.com/documentation/unreal-engine/first-person-rendering · https://www.youtube.com/watch?v=11sLIyw0pWQ · https://forums.unrealengine.com/t/how-to-use-first-person-rendering-5-5-new-experimental-feature/2122644
- https://dev.epicgames.com/documentation/unreal-engine/panini-projection-in-unreal-engine · https://sahildhanju.com/posts/render-first-person-fov/
- https://theorangeduck.com/page/spring-roll-call · https://www.devunallocated.com/projects/project-killhouse/procedural-weapon-animations-condensed · https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-in-lyra-sample-game-in-unreal-engine · https://gdcvault.com/play/1024319/Animation-Bootcamp-The-First-Person
- https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine · https://forums.unrealengine.com/t/updated-game-animation-sample-project-5-5-client-jitter-on-multiplayer-latency/2127597

*Weapon skins*
- https://www.counter-strike.net/workshop/workshopfinishes · https://dev.epicgames.com/documentation/en-us/unreal-engine/storing-custom-data-in-unreal-engine-materials-per-primitive

*Game references*
- LOCKDOWN Protocol: https://miragecreativelab.com/lockdown-protocol/ · https://miragecreativelab.com/the-state-of-lockdown-protocol-and-plans-for-the-future/ · https://steamcommunity.com/sharedfiles/filedetails/?id=3305518998
- R.E.P.O.: https://blog.photonengine.com/r-e-p-o-multiplayer-success-powered-by-photon/ · https://store.steampowered.com/app/3241660/REPO/
- GMod Murder: https://github.com/MechanicalMind/murder · https://wiki.facepunch.com/gmod/gamemodes/Murder

*Loot boxes, ratings and economy*
- Belgium, UK, Brazil: https://sites.google.com/view/leon-xiao/policy/belgium · https://royalsociety.org/blog/2025/06/video-game-industry-loot-boxes/ · https://www.machadomeyer.com.br/pt/inteligencia-juridica/publicacoes-ij/direito-digital/estatuto-digital-da-crianca-e-do-adolescente-lei-n-15-211-2025-entra-em-vigor-em-17-de-marco-de-2026
- Ratings and regulators: https://www.classification.gov.au/about-us/media-and-news/news/new-classifications-for-gambling-content-video-games · https://pegi.info/news/pegi-expands-age-rating-criteria-interactive-risk-categories · https://www.ftc.gov/news-events/news/press-releases/2025/01/genshin-impact-game-developer-will-be-banned-selling-lootboxes-teens-under-16-without-parental · https://commission.europa.eu/news-and-media/news/european-commission-hosts-stakeholders-talks-application-cpc-networks-key-principles-games-virtual-2025-06-03_en
- Stores and Steam: https://support.google.com/googleplay/android-developer/answer/9858738 · https://partner.steamgames.com/doc/features/inventory/schema · https://partner.steamgames.com/doc/api/ISteamInventory · https://partner.steamgames.com/doc/features/inventory/dynamicproperties
- CS2 X-Ray: https://games.gg/counter-strike-2/guides/cs2-x-ray-scanner-explained/ · https://www.pcgamer.com/games/fps/counter-strikes-x-ray-scanner-for-loot-containers-is-coming-to-germany-later-this-month/
- Backends and ranking: https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode · https://heroiclabs.com/docs/nakama/concepts/iap-validation/ · https://town-of-salem.fandom.com/wiki/Elo
