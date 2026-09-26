#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Traps/KGTrap.h"
#include "KGFieldTraps.generated.h"

class AKGCharacter;
class UStaticMesh;

/**
 * SPRINT-041 Trapper ground traps (AKGTrap machines, armed only through the Trapper's ability). Both show to everyone
 * for KGTrapperTuning::RevealSecs after arming (a careful watcher can catch the Trapper at it), then only to the
 * Trapper's own client (AKGAbilityHolder::MyTraps is owner-only) until they spring. Dawn removes them
 * (UKGAbilitySubsystem). ArmedByPuid stays the server's secret (AKGTrap).
 */
UCLASS(Abstract)
class KILLGODOT_API AKGFieldTrap : public AKGTrap
{
	GENERATED_BODY()

public:
	AKGFieldTrap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Authority: spawns T at Where (ground point) facing Yaw, armed by By. */
	template <typename T>
	static T* AuthPlace(UWorld* World, const FVector& Where, float Yaw, AKGCharacter* By)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Trap = World ? World->SpawnActor<T>(Where, FRotator(0.0f, Yaw, 0.0f), P) : nullptr;
		if (Trap)
		{
			static_cast<AKGFieldTrap*>(Trap)->AuthSetup(By);   // virtual, looked up in the base (protected there)
		}
		return Trap;
	}

	/** Local: the owner's client sees its own traps after the public reveal. */
	bool bShowToLocalOwner = false;

	/** Is it drawn on this machine right now? (reveal window, sprung, or the owner's own). */
	bool IsShownLocally() const;
	bool IsSprung() const { return bSprung; }
	float GetArmedAt() const { return ArmedAt; }

protected:
	virtual void AuthSetup(AKGCharacter* By);
	virtual UStaticMesh* PickMesh() const { return nullptr; }

	/** Server world seconds of arming (public: the reveal window is public anyway). */
	UPROPERTY(Replicated)
	float ArmedAt = 0.0f;

	/** Sprung (a snare that closed): visible to all from then on. */
	UPROPERTY(Replicated, SaveGame)
	bool bSprung = false;

	UPROPERTY(Replicated)
	TObjectPtr<AKGCharacter> Victim;

	TWeakObjectPtr<AKGCharacter> HeldBody;
	UStaticMesh* LastMesh = nullptr;

	void ApplyHold(bool bWant);
};

/** Bear trap: roots (KGTrapperTuning::SnareHoldSecs) and wounds (SnareWound; never below 5 HP). One use. */
UCLASS()
class KILLGODOT_API AKGSnareTrap : public AKGFieldTrap
{
	GENERATED_BODY()

public:
	AKGSnareTrap();

protected:
	virtual void OnFire() override;
	virtual void OnEnd() override;
	virtual void OnReady() override;
	virtual UStaticMesh* PickMesh() const override;
	virtual void Tick(float DeltaSeconds) override;
};

/** Silent alarm: tells only the Trapper who crossed it and where. Re-arms itself (TripwireRearmSecs). */
UCLASS()
class KILLGODOT_API AKGTripwireTrap : public AKGFieldTrap
{
	GENERATED_BODY()

public:
	AKGTripwireTrap();

	int32 GetTrips() const { return Trips; }

protected:
	virtual void AuthSetup(AKGCharacter* By) override;
	virtual void OnTelegraph() override {}
	virtual void OnFire() override;
	virtual void OnEnd() override {}
	virtual void OnReady() override {}
	virtual UStaticMesh* PickMesh() const override;

	int32 Trips = 0;
};
