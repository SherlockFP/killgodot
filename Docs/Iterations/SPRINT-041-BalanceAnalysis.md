# SPRINT-041 — Balance analysis: why every forced-Trapper bot match ends "Impatient win"

Static code read, 2026-09-26 (Linux container, no UE, nothing was built or run). The open item is SPRINT-041
acceptance 4: the Town win rate over 20 seeded bot matches with a forced Trapper must be 0.35–0.65. The only data
is 3 partial runs of `Tools/Gauntlet/kg_trapper_balance.ps1`, all "Impatient win".

**Verdict:** the code does not show a Trapper that is too strong. It shows a harness and a bot brain in which the
**Town cannot win at all**, whatever the killing role is. At the script's default `-Speed 20` no bot ever casts an
accusation, so no trial ever happens. Even at a sane speed, the bots vote with no evidence against 3 unlimited
instant-kill killers. The `-ForceRole Enforcer` control run should show the same ~0 % Town rate. Run it first
(plan below). If it does, retuning the Trapper numbers cannot reach the band.

Setup the analysis assumes (from the script): `kg.Bot.Fill 12` → 12 players including the idle listen-server host
(11 bots + host). Faction table at 12 (`Roles/KGRoleListGenerator.cpp:117`): Town 7, Impatient 3 (Clockmaster +
Enforcer-slot + 1 solo killer), Neutral 2. The win check is `ImpatientAlive == 0 → Town`, `ImpatientAlive >= OthersAlive
→ Impatient` (`Core/KGGameMode.cpp:505-521`), so the Impatient need 6 kills (9 others → 3).

## 1. Ranked causes (with evidence)

### C1 (harness, decisive): at `-Speed 20` meetings end before any bot votes, so there is never a trial
- `kg.Match.Speed` only scales the **phase clock**: `GS->Clock.Advance(DeltaSeconds * DevClockScale)`
  (`Core/KGGameMode.cpp:314`, `Dev/KGDevCommands.cpp:473-479`). Movement, bot brains, cooldowns and trap timers keep
  running in real time.
- Phase lengths at N = 12 (`Core/KGGameMode.cpp:65-91`), in match seconds and real seconds at x20:

  | Phase | Match s | Real s at x20 | Real s at x3 |
  |---|---|---|---|
  | Day | 90 + 4N = 138 | 6.9 | 46 |
  | Meeting | 30 + 2N = 54 | **2.7** | 18 |
  | Trial | 32 | 1.6 | 10.7 |
  | Night | 45 + 2N = 69 | 3.45 | 23 |
  | Dawn | 8 | 0.4 | 2.7 |

- Bot vote timing is in **real** seconds: `VoteDelay = 4.0f + 10.0f * Hash01(...)` and then `VoteDelay -= DeltaSeconds`
  (`AI/KGBotController.cpp:917-925`). The shortest delay (4 s) is longer than the whole meeting (2.7 s), so
  `AccusePlayer` (`:969`) is never called. `HandleAccuse` is the only path to `StartTrial` (`Core/KGGameMode.cpp:625-648`).
  Bots are the only accusers, since the idle host never presses V. The only other caller is the dev verb
  (`Core/KGGameModeDev.cpp:141`).
- With no trials, the Town's only "wins" are accidents that kill all 3 Impatient: friendly backstabs (C5), wolves or
  the Mist. The Impatient win by attrition every time, and that matches the data. The charter already records this
  anecdote: `Docs/Design/KillGo_Pillars.md` S6.5, "smoke'ta yıpratmayla Sabırsız kazanıyor".
- It also breaks the roadmap rule: `Docs/Design/Pillar_Roadmap.md` §"Kabul soak'u" requires **x1 or global time dilation
  (movement scales too)** for acceptance soaks, and every second threshold in match-clock seconds.

### C2 (bot brain): the vote latch swallows the trial verdict of every bot that accused
- `VoteDelay = 1000.0f; // one decision per phase` (`AI/KGBotController.cpp:926`) is reset only when the phase is neither
  Meeting nor Trial (`:912-916`). The trial starts the moment the accusation majority is reached
  (`Core/KGGameMode.cpp:645-648`). The bots that made up that majority (`CountAlive()/2+1`, 7 of 12) still hold
  `VoteDelay = 1000` in the Trial and never call `VoteGuilty/VoteInnocent` (`:972-981`). Only the minority that had not
  acted yet gives a verdict. `ResolveTrial` hangs only on `Guilty > Innocent` (`Core/KGGameMode.cpp:687-721`), so with
  2–4 voters many trials end "spared" (0 vs 0 → spared).

### C3 (bot brain): no evidence, and the vote maths favours hanging villagers
- Town bots pick the most-accused player, or a random one with a 25 % chance (`AI/KGBotController.cpp:952-966`). In the
  trial they vote guilty with a fixed 60 % chance (`:979`). Nothing they could know is read: not the lighthouse reveal
  (`GS->LighthouseRevealed`, `Core/KGGameMode.cpp:849-870`, today only a glow in `Character/KGCharacter.cpp:1796`),
  not witnessed kills, not the bite/snare wounds or the mimic scream (`UKGAbilitySubsystem::NoteMimicScream` feeds
  only chest avoidance, `Abilities/KGAbilitySubsystem.cpp:212-229`).
- The Impatient never accuse each other (`:942-945`) and vote guilty on any villager (`:979`). A trial of an Impatient
  therefore needs 7 of the 8 non-Impatient, non-host voters to join one bandwagon. A trial of a villager needs 7 of
  10. Random bandwagons hang villagers far more often.

### C4 (kill economy, the structural one): 3 killers, each with unlimited instant kills in every phase
- Every Impatient-aligned role may draw the blade (`Character/KGCharacter.cpp:1402-1415`). That includes the solo
  killer (`Roles/KGRoleDefinition.cpp:3-13`), which counts as an Impatient ally in the win check and in bot targeting.
- The bot backstab block is role-agnostic and runs in **any phase** (`AI/KGBotController.cpp:733-756`; only the Prey hunt
  is night-only, `:772`). A backstab is 1000 damage (`Character/KGCharacter.cpp:1261-1268`), with a 2 s decision
  cooldown and **no per-night limit**.
- The design has a limit that the code lacks: `Docs/02_Roles.md:61-62` "Kurban Hakkı: takımın gecede 1 öldürme hakkı var,
  N ≥ 14 ise 2. Hakkı sadece Saat Ustası, Tetikçi ve Casus kullanabilir. Gündüz öldürmek o gecenin hakkından düşer."
  The Serial Killer also gets "Her gece 1 fiziksel öldürme" (`:104`). By the GDD, **the Trapper should not backstab at
  all**, because its kills come from the Clockmaster's budget plus traps. In code it kills like everyone else.
- `StartMeeting` teleports every living player into one half-ring (`Core/KGGameMode.cpp:576-620`), and the night
  starts from that ring. Each Impatient then picks the nearest villager (`AI/KGBotController.cpp:772-797`), who is 1–2 m
  away. Up to 3 kills per night means about 2 nights to win. The Town gets 1–2 meetings to find 3 Impatient.

### C5 (bot brain, a correctness bug that currently *helps* the Town): friendly fire
- `FindBackstabTarget` does not filter allies (`Character/KGCharacter.cpp:1559-1580`), and the bot stabs whatever it
  returns (`AI/KGBotController.cpp:737-750`). Impatient bots can kill each other. This is one of the few ways the Town
  can "win" today. Fixing it alone would make the Town rate even lower, so apply it only together with P2/P3.

### C6 (harness): the idle listen-server host takes a seat
- `-game ...?listen` creates a local player that never moves, votes or kills. `kg.Bot.Fill 12` counts it
  (`Dev/KGDevCommands.cpp:517-523`). It is a free victim, it raises the accusation threshold, and it gets a role:
  1 in 12 matches it **is** the forced Trapper, and that Trapper never acts. The script's `trapper` check
  (`kg_trapper_balance.ps1:61`) still passes, because the holder exists.
- In the Enforcer control run, the `trapper` check is false in every match (the Enforcer has no abilities, so no
  `KG_ABILITY holder` line). The control run therefore always exits 1 with "a match without the forced Trapper", even
  though its rate is printed correctly.

### C7 (Trapper-specific, second order): the traps cannot tip a match
- **The traps never kill.** A bite is clamped to leave 10 HP (`Traps/KGMimicTrap.cpp:290`), and a snare to leave 5 HP
  (`Traps/KGFieldTraps.cpp:179`, now `KGTrapperTuning::SnareMinHealthLeft`, done by the parallel agent). Both go
  straight to `UKGHealthComponent::ApplyDamage`. Only a later backstab, wolf or Mist kill finishes a victim. Bite and
  snare in a row: 100 → 45 → 20, never 0.
- Charges refill only on the phase change into Dawn (`Abilities/KGAbilitySubsystem.cpp:116-121, 202-208`; `Refill` sets
  the charges, it does not add, `KGAbilityTypes.cpp:64-69`). Field traps are destroyed at dawn and armed mimics reset
  (`KGAbilitySubsystem.cpp:175-201`, `KGMimicTrap.cpp:247-270`). There is no stacking across nights, and no over-refill.
- Forcing the role **replaces** the Enforcer; it does not add a killer. The forced role takes the first slot, in fill
  order, that accepts it (`Roles/KGRoleListGenerator.cpp:285-299`), and at N = 12 that is the single `Enforcer` slot
  (`:162-167, 209-210`). The Trapper and the Enforcer have the same Power (7), and both forced runs draw the same RNG
  sequence, so the two runs deal the same role list apart from that one slot. That makes it a clean A/B test.
- At x20 the Trapper is *weaker*, not stronger. Its cooldowns (`KGAbilityHolder.cpp:446-451`), its rummage cadence
  (35–75 s real, `KGTrapperBot.cpp:157`), its retry pause (`:255`) and its 18 s goal timeout all run in real time. The
  bot plans only by day (`:267-270`), and the day is 6.9 s real. So a Trapper run at x20 is about the same as an
  Enforcer run.
- A real but small bias: Snare and Tripwire both aim at the approach to the *nearest* station, 3 m out
  (`KGTrapperBot.cpp:127-141`). The bot puts both snares and both wires on the same spot. A second snare on the same
  spot can double the root, but it is still non-lethal.
- No `FMath::Rand` and no `TimerManager` appear in `Abilities/`, `Traps/` or `AI/`. Ability cooldowns use
  `FKGMatchClock`, as the CLAUDE.md rule asks, but they are advanced with the unscaled delta (see C1).

### C8 (fixed here): stale Trapper-bot goals ran on into the night
- An arm or rummage goal planned late in the day stayed `bActive` through the Meeting and ran at Night (villagers
  walking off to rummage in the dark). **Applied:** the goal is dropped once the phase is no longer Day
  (`Abilities/KGTrapperBot.cpp:218-220`). The balance effect is negligible; it is a correctness fix.

## 2. Proposed patches (not applied — outside this agent's writable scope)

Order: P1 is enough to get a meaningful measurement. P2 fixes bot voting. P3–P5 change balance or behaviour and need
the owner's approval (Backlog "Proposed").

### P1 — `Tools/Gauntlet/kg_trapper_balance.ps1` (harness)
```diff
-    [int]$Speed = 20,
+    # x3 at 12 players: a 54 s meeting lasts 18 real s, longer than the bots' 4-14 s real vote delay (C1).
+    [int]$Speed = 3,
     [int]$Parallel = 2,
-    [int]$TimeoutSeconds = 900,
+    [int]$TimeoutSeconds = 2400,
```
```diff
         $r.trapper = [bool](Select-String -Path $log -Pattern "KG_ABILITY holder .* role=Trapper" -Quiet)
+        # The idle listen-server host can draw the forced role (C6): flag it, such a match has no acting Trapper.
+        $r.trapper_host = $r.trapper -and -not [bool](Select-String -Path $log -Quiet -Pattern "KG_ABILITY holder (Fisher Riza|Baker Nuri|Widow Hatice|Old Kemal|Netmaker Sevgi|Smith Cemal|Priest Aurel|Innkeeper Mara|Shepherd Yusuf|Lamplighter Ivo|Herbalist Dunya|Harbourmaster Osman|Tanner Petra|Miller Salih|Candlemaker Lale|Ferryman Boris|Weaver Ayse|Gravedigger Emin|Cooper Vlad) role=Trapper")
+        $r.trials = @(Select-String -Path $log -Pattern "Phase -> EKGPhase::Trial").Count
```
```diff
-if (@($results | Where-Object { -not $_.trapper }).Count) { $fail += "a match without the forced Trapper" }
+if ($ForceRole -eq "Trapper" -and @($results | Where-Object { -not $_.trapper }).Count) { $fail += "a match without the forced Trapper" }
```
(In `Read-Result`, the `[ordered]@{ ... }` initialiser also needs `trapper_host = $false; trials = 0;`.)

### P2 — `Source/KillGodot/AI/KGBotController.cpp` `UpdateVotes` (C1 + C2; .cpp only, Live Coding safe)
```diff
 	if (Phase != EKGPhase::Meeting && Phase != EKGPhase::Trial)
 	{
 		VoteDelay = -1.0f;
 		return;
 	}
+	// The meeting's latch must not swallow the trial verdict: a bot that accused still owes one.
+	if (Phase == EKGPhase::Trial && VoteDelay >= 1000.0f && MyPS->Verdict == 0 && MyPS != GS->OnTrial)
+	{
+		VoteDelay = -1.0f;
+	}
 	if (VoteDelay < 0.0f)
 	{
-		VoteDelay = 4.0f + 10.0f * Hash01(BotSeed * 53u + GS->GetDayIndex() * 7u + static_cast<uint32>(Phase));
+		// When to decide, as a fraction of the phase clock (10-55 %): kg.Match.Speed only speeds up the phase clock, so a
+		// real-seconds delay (was 4-14 s) never fired in a 2.7 s meeting at x20 and no bot match ever held a trial.
+		VoteDelay = 0.10f + 0.45f * Hash01(BotSeed * 53u + GS->GetDayIndex() * 7u + static_cast<uint32>(Phase));
 	}
-	VoteDelay -= DeltaSeconds;
-	if (VoteDelay > 0.0f)
+	if (GS->Clock.GetAlpha() < VoteDelay)
 	{
 		return;
 	}
 	VoteDelay = 1000.0f;   // one decision per phase
```
(`DeltaSeconds` becomes unused. UE disables C4100, so the signature can stay.)

### P3 — same file, the lighthouse as bot evidence (C3; needs approval: new bot behaviour)
```diff
 		AKGPlayerState* Pick = nullptr;
 		int32 Top = 0;
+		// Evidence the village really has: the lighthouse showed one of the Impatient (AKGGameMode::LighthouseIllumination).
+		AKGPlayerState* Glowed = GS->LighthouseRevealed.Get();
+		const bool bKnowsOne = !bImpatient && Glowed && Glowed->IsAlive() && Candidates.Contains(Glowed);
 		for (const TPair<AKGPlayerState*, int32>& Pair : Counts)
```
```diff
-		if ((!Pick || Hash01(BotSeed * 3u + GS->GetDayIndex()) < 0.25f) && Candidates.Num() > 0)
+		if (bKnowsOne)
+		{
+			Pick = Glowed;
+		}
+		else if ((!Pick || Hash01(BotSeed * 3u + GS->GetDayIndex()) < 0.25f) && Candidates.Num() > 0)
```
```diff
-		const bool bGuilty = bImpatient ? !bAccusedImpatient : Hash01(BotSeed * 17u + GS->GetDayIndex()) < 0.6f;
+		const bool bGuilty = bImpatient ? !bAccusedImpatient
+		                                : (Accused && Accused == GS->LighthouseRevealed.Get()) || Hash01(BotSeed * 17u + GS->GetDayIndex()) < 0.6f;
```
Later, the same pattern can feed the Trapper evidence: a bot that sees a snared or bitten villager, or a teeth-marked
chest, suspects whoever it saw arming nearby.

### P4 — same file, Tick backstab block: never an ally's back (C5; apply only with P2/P3, it lowers the Town rate)
```diff
-		if (AKGCharacter* Victim = Me->FindBackstabTarget())
+		AKGCharacter* Victim = Me->FindBackstabTarget();
+		const AKGPlayerState* VictimPS = Victim ? Victim->GetPlayerState<AKGPlayerState>() : nullptr;
+		const FKGRoleInfo* VictimRole = VictimPS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(),
+		                                                                          VictimPS->GetPrivateRoleId()) : nullptr;
+		if (VictimRole && VictimRole->GetAlignment() == EKGAlignment::Impatient)
+		{
+			Victim = nullptr;   // an ally's back is not a target
+		}
+		if (Victim)
 		{
```

### P5 — the GDD kill budget ("Kurban Hakkı") for bots (C4; needs approval; the real fix is a sprint that also applies it to players)
In the anonymous namespace of `AI/KGBotController.cpp`:
```cpp
	/** Docs/02_Roles.md "Kurban Hakkı" for bots: the Clockbreakers share one kill per day+night (two from 14 players), only
	 *  the Clockmaster, Enforcer and Spy spend it; every other Impatient-aligned killer (solo) has one of its own. */
	struct FKillBudget
	{
		TWeakObjectPtr<const UWorld> World;
		int32 DayIndex = -1;
		int32 TeamUsed = 0;
		TArray<TWeakObjectPtr<const AController>> SoloUsed;
	};

	FKillBudget& KillBudget(const UWorld* World, int32 DayIndex)
	{
		static FKillBudget B;
		if (B.World.Get() != World || B.DayIndex != DayIndex)
		{
			B = FKillBudget();
			B.World = World;
			B.DayIndex = DayIndex;
		}
		return B;
	}

	/** bSpend = false only asks. The day and the following night share DayIndex (it grows on entering Day). */
	bool UseKill(const AController* Bot, const AKGGameState* GS, bool bSpend)
	{
		const AKGPlayerState* PS = Bot ? Bot->GetPlayerState<AKGPlayerState>() : nullptr;
		const FKGRoleInfo* R = PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId()) : nullptr;
		if (!R || !GS)
		{
			return false;
		}
		FKillBudget& B = KillBudget(Bot->GetWorld(), GS->GetDayIndex());
		if (R->Faction == EKGFaction::Clockbreakers)
		{
			const bool bKiller = R->RoleId == TEXT("Clockmaster") || R->RoleId == TEXT("Enforcer") || R->RoleId == TEXT("Spy");
			if (!bKiller || B.TeamUsed >= (GS->PlayerArray.Num() >= 14 ? 2 : 1))
			{
				return false;
			}
			B.TeamUsed += bSpend ? 1 : 0;
			return true;
		}
		if (B.SoloUsed.Contains(Bot))
		{
			return false;
		}
		if (bSpend)
		{
			B.SoloUsed.Add(Bot);
		}
		return true;
	}
```
In `Tick` (on top of P4): change `if (Victim)` to `if (Victim && UseKill(this, GS, false))`, and put `UseKill(this, GS, true);`
right before `Me->Attack();`. Also gate the Prey pick at `:772` with `&& UseKill(this, GS, false)`, so a killer without
a kill left stops stalking. Under this rule the Trapper bot never backstabs, which is GDD-correct. The Trapper team
then kills 1 per night (the Clockmaster) plus traps, the same as the Enforcer team, which kills 1 per night (Clockmaster
or Enforcer). **That is the moment the Trapper's numbers start to matter.**

### P6 — `Abilities/KGTrapperBot.cpp` (in scope, deliberately not applied: a behaviour change, not a bug)
Spread the field traps: in `PlanTrapper`, skip a station that already has one of `H`'s traps within 6 m (read
`AKGAbilityHolder::MyTraps` through a getter; `KGAbilityHolder.h` belongs to the other agent). Worth doing once P2–P5
make the traps count.

## 3. Verification plan (Windows, headless, in this order)

Pass `-Seeds` with `-Command`, not `-File`. In `-File` mode PowerShell passes `101,102,103,104` as one string, and the
`[int[]]` binding most likely fails. Wrap every UE use in the lock (HANDOFF). Chunks of at most 4 matches.

```powershell
# 0. Build with this change (KGTrapperBot.cpp only; plus whatever the parallel agent changed)
python Tools/Gauntlet/kg_ue_lock.py "balance: build" -- powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/run_gates.ps1

# 1. Confirm C1 at the current default (x20), both roles, 4 seeds each (~2 min per chunk)
python Tools/Gauntlet/kg_ue_lock.py "balance: trapper x20" -- powershell -ExecutionPolicy Bypass -Command "& .\Tools\Gauntlet\kg_trapper_balance.ps1 -Seeds 101,102,103,104 -Speed 20"
python Tools/Gauntlet/kg_ue_lock.py "balance: enforcer x20" -- powershell -ExecutionPolicy Bypass -Command "& .\Tools\Gauntlet\kg_trapper_balance.ps1 -Seeds 101,102,103,104 -Speed 20 -ForceRole Enforcer -OutName EnforcerControl_x20 -PortBase 8000"
Select-String -Path Saved\Logs\TrapperBalance\Match_*.log, Saved\Logs\EnforcerControl_x20\Match_*.log -Pattern "Phase -> EKGPhase::Trial" | Measure-Object   # expect Count 0
Select-String -Path Saved\Logs\EnforcerControl_x20\Match_*.log -Pattern "Match decided"                                                         # expect Impatient every time

# 2. Same A/B at x3, 20 seeds per role (5 chunks each; about 5-8 min per match, 2 in parallel)
foreach ($c in "101,102,103,104","105,106,107,108","109,110,111,112","113,114,115,116","117,118,119,120") {
  python Tools/Gauntlet/kg_ue_lock.py "balance: trapper x3 $c" -- powershell -ExecutionPolicy Bypass -Command "& .\Tools\Gauntlet\kg_trapper_balance.ps1 -Seeds $c -Speed 3 -TimeoutSeconds 2400 -OutName Trapper_x3"
  python Tools/Gauntlet/kg_ue_lock.py "balance: enforcer x3 $c" -- powershell -ExecutionPolicy Bypass -Command "& .\Tools\Gauntlet\kg_trapper_balance.ps1 -Seeds $c -Speed 3 -TimeoutSeconds 2400 -ForceRole Enforcer -OutName Enforcer_x3 -PortBase 8000"
}
powershell -ExecutionPolicy Bypass -Command "& .\Tools\Gauntlet\kg_trapper_balance.ps1 -Aggregate -OutName Trapper_x3"
powershell -ExecutionPolicy Bypass -Command "& .\Tools\Gauntlet\kg_trapper_balance.ps1 -Aggregate -OutName Enforcer_x3"
# Per match, check: trials > 0 (Phase -> EKGPhase::Trial), "Bot fill: 11 bots, 12 players total", which player holds
# role=Trapper (a host name = C6), arms/bites/snares > 0 (the Trapper actually acted).

# 3. After P2 (and P3-P5 if approved): repeat step 2 (same seeds, same OutNames with a _p2 suffix).
```
How to read it:
- Trapper ≈ Enforcer, both far below 0.35 → C1–C4 confirmed. The Trapper's numbers are not the problem. Fix the
  harness and the bot brain (P1–P5) before any tuning.
- Trapper clearly below Enforcer (by more than about 0.2) *after* P2–P5 → only then tune the Trapper: bite 55 → 40,
  snare hold 4 → 3 s, mimic charges 2 → 1 (`Abilities/KGAbilityTypes.h` `KGTrapperTuning` / `FKGAbilityCatalog`,
  owned by the other agent).
- Statistics: with n = 20, the 95 % interval around 0.5 is about ±0.22, so a 0.35–0.65 gate cannot tell "balanced"
  from noise. `Docs/Design/Pillar_Roadmap.md` already makes the balance bands report-only until after SPRINT-028 (bot
  parameters frozen, n_min, 95 % CI). The owner should confirm that SPRINT-041 acceptance 4 is a **report**, not a gate.

## 4. What could not be determined statically
- The actual win rates, at any speed, before or after the patches. The claim that the Town cannot reach the band even
  at x3 without P2–P5 is an estimate from the kill economy (C4), not a measurement.
- Whether the 3 partial runs had `trapper=True`: whether the `-ini:Engine:[ConsoleVariables]:kg.Roles.Force=...` override
  reaches a cvar registered by the game module. The summary's `trapper` field answers it.
- How often Town "wins" come from accidents (friendly backstab, wolves, the Mist), and whether the lighthouse
  (`Preparation` ≥ 1) is ever reached in a bot match (P3 depends on it).
- Whether `L_Morrowmere_v2` has containers within 15 m of task stations (the Trapper's `Mimic` plan needs them). The
  `arms` and `bites` counts show it.
- Reproducibility: the seed fixes only the role list and gameplay RNG. `BotSeed = GetUniqueID()`
  (`AI/KGBotController.cpp:273`) and `KGTrapperBot`'s real-time hash (`KGTrapperBot.cpp:157`) change between processes,
  so the "same seed" Trapper/Enforcer pair is not a fully paired A/B test.
- The exact PowerShell `-File` binding error for `-Seeds` (from known PowerShell behaviour; not run here).
- Compilation: the one-line `KGTrapperBot.cpp` change was not compiled. It only uses names already used in the same
  function (`GS->GetPhase()`, `EKGPhase::Day`).
