#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

class APlayerController;
class UWorld;

/** Outcome of one dev verb (shown in the dev panel footer and logged). */
struct KILLGODOT_API FKGDevResult
{
	bool bOk = true;
	FString Message;

	static FKGDevResult Ok(const FString& InMessage) { return {true, InMessage}; }
	static FKGDevResult Fail(const FString& InMessage) { return {false, InMessage}; }
};

/** Server verbs change the match and need the host; Local verbs only change what this machine sees. */
enum class EKGDevScope : uint8
{
	Server,
	Local
};

/** Who runs a verb, and where. */
struct KILLGODOT_API FKGDevContext
{
	/** Server verbs: the authority world. Local verbs: the caller's world. */
	UWorld* World = nullptr;
	/** Server-side controller of whoever asked (the "Me" of Me.* verbs). Null for headless automation. */
	APlayerController* Requester = nullptr;
};

/** A teleport target (map regions, level tags, layout JSON, landmarks). */
struct KILLGODOT_API FKGDevLocation
{
	FString Name;
	FString Group;
	FVector Location = FVector::ZeroVector;
};

/**
 * Dev panel / kg.* verb table (development builds only). Every verb is one line of text, e.g. "Bot.Add 5",
 * "Match.Phase Night", "Me.Role Sheriff". The same line runs from:
 *  - the dev panel buttons (F1 / kg.Dev),
 *  - the console: each verb is also a console command "kg.<Verb>" (kg.Bot.Add 5), or "kg.Dev <Verb> ...",
 *  - automation and agents (FKGDev::Execute on the server world).
 * Server verbs run on the host only: a client's request goes through UKGDevComponent::ServerRun and is refused
 * unless kg.Dev.AllowClients is 1. See Docs/05_Tech_Architecture.md section 12 for the full list.
 */
class KILLGODOT_API FKGDev
{
public:
	/** Any machine: runs Line in World. Local verbs run here; server verbs run here on the host (listen server,
	 *  standalone) or are sent to the host from a client. The result is logged and pushed to the panel. */
	static FKGDevResult Submit(UWorld* World, const FString& Line);

	/** Authority (or local verbs anywhere): runs Line for Context.Requester. No permission check. */
	static FKGDevResult Execute(const FKGDevContext& Context, const FString& Line);

	/** Whether Line names a server-scope verb (false for local verbs and unknown lines). */
	static bool IsServerVerb(const FString& Line);

	/** Host-only rule: the listen server's own controller, or anyone while kg.Dev.AllowClients is 1. */
	static bool MayRun(const APlayerController* Requester);

	/** The local player controller of World (first one), or null. */
	static APlayerController* LocalController(UWorld* World);

	/** The authoritative world for reading state: World itself, or its PIE server twin in the same process. */
	static UWorld* AuthorityWorld(UWorld* World);

	/** Teleport targets for World (cached per world). */
	static const TArray<FKGDevLocation>& GatherLocations(UWorld* World);

	/** Standing spot at XY: the lowest walkable floor with head room (so building interiors, not roofs). */
	static FVector GroundAt(UWorld* World, const FVector2D& XY, float HintZ);

	/** Multi-line snapshot of the match (phase, clock, seed, players, roles, chores, bots). */
	static FString DumpState(UWorld* World);

	/** "kg.Bot.Add [Count=1] : ..." lines for every verb. */
	static void GetHelpLines(TArray<FString>& OutLines);

	/** Logs Result and shows it in World's dev panel footer. */
	static void Report(UWorld* World, const FKGDevResult& Result);

	/** Names of the float tuning properties of UKGViewmodelComponent the panel exposes (reflection). */
	static TArray<FName> GetViewmodelTuningProperties();
};

#endif
