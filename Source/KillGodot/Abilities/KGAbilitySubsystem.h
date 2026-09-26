#pragma once

#include "CoreMinimal.h"
#include "Core/KGTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "KGAbilitySubsystem.generated.h"

class AActor;
class AController;
class AKGAbilityHolder;
class AKGCharacter;

/**
 * SPRINT-041 ability glue (world, tickable):
 * - Server: keeps one AKGAbilityHolder per player whose dealt role has abilities (role catalog FKGRoleInfo::AbilityIds),
 *   refills charges and expires every Trapper trap at dawn (mimics that never bit go back to sleep, snares and
 *   tripwires are removed).
 * - Server: bot memory for the deduction hook "bots avoid chests they saw someone get bitten at" (a scream within 30 m
 *   teaches every bot that heard it), read by KGTrapperBot.
 * - Every machine with a player: registers the owner UI (KGAbilityHUD) on AHUD::OnHUDPostRender.
 * - -KGTrapperSmoke: the scripted two-process network smoke (Tools/Unreal/kg_trapper_smoke.ps1).
 */
UCLASS()
class KILLGODOT_API UKGAbilitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UKGAbilitySubsystem* Get(const UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterHolder(AKGAbilityHolder* Holder);
	void UnregisterHolder(AKGAbilityHolder* Holder);
	const TArray<TWeakObjectPtr<AKGAbilityHolder>>& GetHolders() const { return Holders; }

	/** Server: syncs holders with the dealt roles now (also runs every 0.5 s). */
	void SyncHolders();
	/** Server: the dawn sweep (charges back, Trapper traps expire). */
	void OnDawn();

	/** Server: a mimic on Host bit someone at At: every bot within earshot remembers Host. */
	void NoteMimicScream(AActor* Host, const FVector& At, AKGCharacter* Victim);
	/** Server: has this bot heard Host scream? */
	bool IsKnownMimic(const AController* Bot, const AActor* Host) const;
	int32 CountKnownMimics(const AController* Bot) const;

	/** Server: a private line to the player with Puid (the Trapper), if they have a holder. */
	void NotifyArmer(const FString& Puid, const FString& Text);

	/** A readable place name near Location (dev teleport names in dev builds, else empty). */
	static FName PlaceName(const UWorld* World, const FVector& Location);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void TickSmoke(float DeltaTime);

	TArray<TWeakObjectPtr<AKGAbilityHolder>> Holders;
	TMap<TWeakObjectPtr<const AController>, TArray<TWeakObjectPtr<const AActor>>> KnownMimics;
	float SyncAccum = 0.0f;
	EKGPhase LastPhase = EKGPhase::Lobby;

	// -KGTrapperSmoke scratch.
	float SmokeClock = -1.0f;
	int32 SmokeStep = 0;
	float SmokeStepAt = 0.0f;
	float SmokeLogAccum = 0.0f;
	bool bSmokeDone = false;
	TWeakObjectPtr<AActor> SmokeChest;
	TWeakObjectPtr<AActor> SmokeThrown;
	FVector SmokeSpot = FVector::ZeroVector;
	bool bSawHeld = false;
	bool bSawBite = false;
	bool bSawSnareHeld = false;
	bool bInteracted = false;
};
