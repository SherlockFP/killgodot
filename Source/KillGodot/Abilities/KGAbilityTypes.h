#pragma once

#include "CoreMinimal.h"
#include "Core/KGMatchClock.h"
#include "Core/KGTypes.h"
#include "KGAbilityTypes.generated.h"

/**
 * SPRINT-041 role-ability framework (MVP): the first role with a real ability in code (the Trapper) and the reusable
 * rules every later role ability goes through. Pure data + pure validation here (unit-tested in
 * KillGodot.Abilities.*); the server side lives in AKGAbilityHolder, the table of roles -> abilities comes from the role
 * catalog (FKGRoleInfo::AbilityIds).
 */

/** What an ability is aimed at. */
UENUM(BlueprintType)
enum class EKGAbilityTarget : uint8
{
	/** No target: fires at once. */
	Self,
	/** A chest / crate / barrel in front of the user (AKGStorageChest, AKGBreakable). */
	Container,
	/** A spot on the ground in front of the user. */
	Ground
};

/** Why the server refused an ability (None = allowed). */
UENUM(BlueprintType)
enum class EKGAbilityDeny : uint8
{
	None,
	Dead,
	WrongPhase,
	NoCharges,
	Cooldown,
	Seen,
	NoTarget,
	OutOfRange,
	Unknown
};

/** Phase bits of FKGAbilityDef::PhaseMask. */
namespace KGAbilityPhase
{
	constexpr uint8 Day = 1 << 0;
	constexpr uint8 Night = 1 << 1;
	constexpr uint8 Dawn = 1 << 2;
	constexpr uint8 Meeting = 1 << 3;
	constexpr uint8 DayAndNight = Day | Night;

	KILLGODOT_API uint8 BitOf(EKGPhase Phase);
}

/** One ability's rules (data; the catalog below holds the shipped ones, numbers in Docs/02_Roles.md "Tuzakçı"). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGAbilityDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") FName AbilityId;
	/** Charges given at every dawn (the "per night" cycle: dawn -> dawn). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 ChargesPerCycle = 1;
	/** Seconds between two uses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") float CooldownSecs = 10.0f;
	/** KGAbilityPhase bits when it may be used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") uint8 PhaseMask = KGAbilityPhase::DayAndNight;
	/** Only while no other living player has a line of sight within UnseenRadiusCm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") bool bRequiresUnseen = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") float UnseenRadiusCm = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") EKGAbilityTarget Target = EKGAbilityTarget::Self;
	/** Max distance from the user's eyes to the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") float RangeCm = 300.0f;
	/** Default key slot label on the bar ("Alt+1"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability") int32 Slot = 0;
};

/** One ability's live state (server authoritative, owner-only replicated through AKGAbilityHolder). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGAbilityState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Ability") FName AbilityId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Ability") int32 Charges = 0;
	/** Remaining cooldown (server FKGMatchClock; the replicated copy is refreshed in whole tenths). */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Ability") FKGMatchClock Cooldown;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Ability") int32 Uses = 0;
};

/** The inputs of one validation (everything the server measured). */
struct KILLGODOT_API FKGAbilityQuery
{
	EKGPhase Phase = EKGPhase::Day;
	bool bAlive = true;
	bool bSeen = false;
	bool bHasTarget = false;
	float TargetDistanceCm = 0.0f;
};

/** Pure rules (unit-tested). */
struct KILLGODOT_API FKGAbilityRules
{
	/** The first failing rule, in the order the player should hear about it. */
	static EKGAbilityDeny Validate(const FKGAbilityDef& Def, const FKGAbilityState& State, const FKGAbilityQuery& Q);
	/** Spends a charge and starts the cooldown (call only after Validate said None). */
	static void Commit(const FKGAbilityDef& Def, FKGAbilityState& State);
	/** Dawn: charges back to the per-cycle count, cooldown cleared. */
	static void Refill(const FKGAbilityDef& Def, FKGAbilityState& State);
	static const TCHAR* DenyName(EKGAbilityDeny Deny);
	/** Player-facing reason, EN / TR. */
	static FString DenyText(EKGAbilityDeny Deny, bool bTurkish);
};

/** The shipped abilities (data-driven: the role catalog lists ids, this table holds the numbers). */
struct KILLGODOT_API FKGAbilityCatalog
{
	static const TArray<FKGAbilityDef>& GetAll();
	static const FKGAbilityDef* Find(FName AbilityId);
	/** Defs of a role, in bar order (FKGRoleInfo::AbilityIds of the default role catalog). Empty = no abilities. */
	static TArray<const FKGAbilityDef*> ForRole(FName RoleId);
	/** Bar name + one-line hint, EN / TR. */
	static FString DisplayName(FName AbilityId, bool bTurkish);
	static FString TargetHint(FName AbilityId, bool bTurkish);
};

/** Trapper numbers (SPRINT-041, Docs/02_Roles.md "Tuzakçı"). Shared by the traps, the bot and the tests. */
namespace KGTrapperTuning
{
	constexpr float BiteDamage = 55.0f;
	/** A bite never takes the victim below this (heavy damage, not an instant kill). */
	constexpr float BiteMinHealthLeft = 10.0f;
	constexpr float BiteHoldSecs = 3.0f;
	constexpr float ScreamRadiusCm = 3000.0f;
	constexpr float BreathRadiusCm = 300.0f;
	constexpr float FlinchSpeedCmS = 250.0f;
	constexpr float FlinchRadiusCm = 140.0f;
	constexpr float SnareDamage = 25.0f;
	constexpr float SnareHoldSecs = 4.0f;
	constexpr float TripwireRearmSecs = 8.0f;
	/** Snares and tripwires show to everyone for this long after arming, then only to the Trapper. */
	constexpr float RevealSecs = 2.0f;
}
