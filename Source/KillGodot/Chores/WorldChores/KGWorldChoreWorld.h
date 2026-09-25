#pragma once

#include "CoreMinimal.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/KGInteractable.h"
#include "World/KGTaskStation.h"
#include "KGWorldChoreWorld.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace KGWorldChoreLook
{
	/** Vertex-colour furniture props (firewood, bread) get a flat colour of their own (they render white in game). */
	void FlattenVertexColour(UStaticMeshComponent* C);
}

/** Public state of one world spot (replicated by AKGWorldChoreDirector, indexed like the catalog's anchors). */
USTRUCT()
struct FKGSpotState
{
	GENERATED_BODY()

	/** Water level (troughs, butts), 0..1. */
	UPROPERTY(SaveGame)
	float Level = 0.0f;

	/** Delivered things shown at the spot (crates, loaves, letters, nets, bundles, split logs). */
	UPROPERTY(SaveGame)
	uint8 Count = 0;

	UPROPERTY(SaveGame)
	bool bLit = false;

	/** Sabotaged by the Impatient: poisoned water / a snuffed, sooty lamp. */
	UPROPERTY(SaveGame)
	bool bSpoiled = false;
};

/**
 * One world chore spot (the well, a trough, a lamp, a letterbox...). Spawned locally on every machine by
 * UKGWorldChoreSubsystem from the catalog (same index everywhere), never replicated itself: its look follows the
 * director's replicated FKGSpotState. E on it goes to the presser's UKGWorldChoreComponent on the server.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGChoreSpot : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGChoreSpot();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Builds the look for Anchor (index into the catalog) and snaps to the ground under it. */
	void Setup(int32 InAnchorIndex);
	int32 GetAnchorIndex() const { return AnchorIndex; }
	const FKGWorldAnchor* GetAnchor() const;
	/** A short sparkle + pop when a step completes here (every machine, from the director's cue). */
	void PlayBurst(const FLinearColor& Color);
	/** Any cue played here (crank, chop, knock, rope...): the spot's moving bits bounce (the bell rope, the axe, the crank). */
	void PlayCue(EKGWorldCue Cue);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** E-trace target (Visibility only). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Hitbox;

private:
	UStaticMeshComponent* Part(UStaticMesh* Mesh, const FVector& Rel, const FRotator& Rot, const FVector& Scale,
	                           const FLinearColor& Color = FLinearColor::White, bool bGlow = false);
	UStaticMeshComponent* Asset(const TCHAR* Path, const FVector& Rel, const FRotator& Rot = FRotator::ZeroRotator, float Scale = 1.0f);
	void BuildLook();
	void ApplyState(const FKGSpotState& S, float DeltaSeconds);

	int32 AnchorIndex = INDEX_NONE;
	float Clock = 0.0f;
	float BurstAge = 10.0f;
	FLinearColor BurstColor = FLinearColor::White;
	FKGSpotState Shown;
	bool bShownValid = false;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Water;         // water planes / discs
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Stacked;       // shown for Count
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Bubbles;       // poison
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Burst;         // step-done sparkle
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flame;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Soot;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flag;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> Plate;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Jiggle;       // bounce on a cue
	TArray<FVector> JiggleBase;
	float JiggleAge = 10.0f;
	float JiggleAmp = 0.0f;
};

/**
 * Replicated world chore state for everyone (always relevant, one per world, spawned by the server): the spots'
 * public state (water levels, lit lamps, delivered crates, poisoned troughs) and cue sounds / sparkles.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGWorldChoreDirector : public AActor
{
	GENERATED_BODY()

public:
	AKGWorldChoreDirector();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AKGWorldChoreDirector* Get(const UWorld* World);
	static AKGWorldChoreDirector* AuthCreate(UWorld* World);

	const FKGSpotState* GetSpot(int32 Index) const { return Spots.IsValidIndex(Index) ? &Spots[Index] : nullptr; }
	FKGSpotState* AuthMutableSpot(int32 Index);
	void AuthDirty() { ForceNetUpdate(); }

	/** Authority: a sound (and, for StepDone, a sparkle) at a spot / place for everyone near. */
	void AuthCue(EKGWorldCue Cue, int32 SpotIndex, const FVector& Where, float Volume = 1.0f);

	int32 GetSerial() const { return Serial; }

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Spots, SaveGame)
	TArray<FKGSpotState> Spots;

	UFUNCTION()
	void OnRep_Spots();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCue(uint8 Cue, int16 SpotIndex, FVector_NetQuantize Where, float Volume);

	int32 Serial = 0;
};

/**
 * The chore-list entry of a world chore (AKGTaskStation subclass, so the deal pool, the HUD list names, the dev
 * verbs and the bots find it like any station). Spawned locally on every machine at the chore's first spot; it has no
 * E hitbox (the spots take E). Its floating marker hovers over the local player's CURRENT step target.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGWorldChoreStation : public AKGTaskStation
{
	GENERATED_BODY()

public:
	AKGWorldChoreStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override { return FText::GetEmpty(); }
};

/**
 * World chore glue (zero-integration like UKGFishingSubsystem):
 *  - every machine, at world begin play on the catalog's map: spots + chore-list stations (local actors);
 *  - server: the director, and a UKGWorldChoreComponent on every AKGCharacter (runtime replicated subobject);
 *  - dev (-KGWorldChoreSmoke=<Chore> on a client): the scripted end-to-end run of Tools/Unreal/kg_chore_smoke.ps1.
 */
UCLASS()
class KILLGODOT_API UKGWorldChoreSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UKGWorldChoreSubsystem* Get(const UWorld* World);

	/** Spawns spots + stations (+ the director on the server). bForce: any map (tests, dev). Idempotent. */
	void SetupWorld(bool bForce = false);
	bool IsSetUp() const { return bSetUp; }

	AKGChoreSpot* GetSpot(int32 AnchorIndex) const;
	/** Where a spot really is (ground-snapped); falls back to the catalog. */
	FVector SpotLocation(int32 AnchorIndex) const;
	/** Where a walker stands for it (navmesh side; tower tops: the tower door). */
	FVector StandLocation(int32 AnchorIndex) const;

	/** Authority: gives Character its world chore component (idempotent). */
	static void EnsureComponent(class AKGCharacter* Character);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void TickSmoke(float DeltaTime);

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<class AKGCharacter>> Pending;
	TArray<TWeakObjectPtr<AKGChoreSpot>> Spots;
	bool bSetUp = false;
	float SweepSeconds = 0.0f;

	// -KGWorldChoreSmoke
	FName SmokeChore;
	int32 SmokeState = 0;
	float SmokeClock = 0.0f;
	float SmokeStateClock = 0.0f;
	int32 SmokeRegrabs = 0;
	int32 SmokePathStep = -1;
	float SmokeLegClock = 0.0f;
	float SmokeIdleClock = 0.0f;
	int32 SmokeRepaths = 0;
};
