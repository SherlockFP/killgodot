#pragma once

#include "CoreMinimal.h"

/**
 * Streamer mode pseudonyms (SPRINT-015, Docs/08_UI_UX.md "Streamer mode"). Pure logic, no UObjects, so it is unit
 * tested directly (KillGodot.Reveal.Pseudonyms).
 *
 * One instance lives for one match (its salt). The first time a player key is seen it gets a village-themed name
 * ("Foggy Herring"); that key keeps the same name for the life of the instance, no two keys share a name, and a
 * pseudonym is never (case-insensitively) the player's own real name or any other real name the table was told about.
 * A new salt (the next match) gives everyone different names.
 */
class KILLGODOT_API FKGPseudonyms
{
public:
	explicit FKGPseudonyms(uint64 InSalt = 0) : Salt(InSalt) {}

	/** Forgets every assignment and starts a new match with NewSalt. */
	void Reset(uint64 NewSalt);
	uint64 GetSalt() const { return Salt; }

	/** A real name that must never be handed out as somebody's pseudonym. */
	void ReserveRealName(const FString& RealName);

	/**
	 * Pseudonym of the player identified by Key (a stable id: EOS PUID, net id, or the bot name). RealName is reserved
	 * too. The returned reference stays valid until the next Get/Reset.
	 */
	const FString& Get(const FString& Key, const FString& RealName);

	/** Pseudonym already assigned to Key, or null. */
	const FString* Find(const FString& Key) const { return ByKey.Find(Key); }
	int32 Num() const { return ByKey.Num(); }

	/** Number of distinct names the generator can produce before it falls back to "Villager N". */
	static int32 PoolSize();
	static FString PoolName(int32 Index);

private:
	bool IsTaken(const FString& Candidate) const;

	uint64 Salt = 0;
	TMap<FString, FString> ByKey;
	/** Lower-case: names already handed out, real names that may never be used. */
	TSet<FString> Used;
	TSet<FString> Reserved;
};
