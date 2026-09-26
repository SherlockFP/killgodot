#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGWoundComponent.generated.h"

class AKGCharacter;

/** One mark on a body (public evidence: anyone who looks closely can read it). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGWound
{
	GENERATED_BODY()

	/** BiteMarks (a mimic), SnareWound (a bear trap). */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Wound") FName Kind;
	/** Server world seconds when it happened. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Wound") float At = 0.0f;
};

/**
 * SPRINT-041 deduction hook: distinct trap wounds on victims and bodies. Added by the server to the victim's pawn the
 * first time a trap marks it (so no change to AKGCharacter), replicated to everyone (a wound is something you can
 * see, not role data), read by the look-at line of KGAbilityHUD ("Bite marks: rows of small teeth").
 */
UCLASS(ClassGroup = (KillGodot))
class KILLGODOT_API UKGWoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGWoundComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority: adds Kind to Body (creates the component on first use). */
	static void AuthAdd(AKGCharacter* Body, FName Kind);
	static const UKGWoundComponent* FindOn(const AActor* Body);
	/** "BiteMarks,SnareWound" or "" (tests, smokes, KG_ logs). */
	static FString ListOn(const AActor* Body);
	/** Player-facing line for one kind, EN / TR. */
	static FString Describe(FName Kind, bool bTurkish);

	bool Has(FName Kind) const { return Wounds.ContainsByPredicate([Kind](const FKGWound& W) { return W.Kind == Kind; }); }
	const TArray<FKGWound>& GetWounds() const { return Wounds; }

	static const FName BiteMarks;
	static const FName SnareWound;

protected:
	UPROPERTY(Replicated, SaveGame)
	TArray<FKGWound> Wounds;
};

/** Hold-in-place for trap victims: MOVE_None on the server and the owning client (no prediction fight, like AKGSeat). */
namespace KGTrapHold
{
	/** Call on every machine; it only acts where it is allowed to (authority, or the locally controlled body). */
	KILLGODOT_API void Apply(AKGCharacter* Body, bool bHold);
	KILLGODOT_API bool IsHeld(const AKGCharacter* Body);
}
