#pragma once

#include "CoreMinimal.h"
#include "Dig/KGDigTypes.h"
#include "GameFramework/Actor.h"
#include "KGDigManager.generated.h"

class AKGCharacter;
class AKGPickup;
class UInstancedStaticMeshComponent;
class UKGSnapshotComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The match's dig spots (Docs/01_GDD_Core.md section 16). One per world, spawned by UKGDigSubsystem on the server,
 * replicated to everyone (always relevant): a FastArray of FKGDigSpot, so every hole deepens for all players.
 *
 * Server: generates the spots when the match seed changes (FKGRng: graves from the level's gravestones, mounds / X
 * marks / glints / hidden buried chests scattered in the Morrowmere v2 zones), validates and applies finished stages,
 * rolls the loot (FKGDigRules::StageRng: same seed = same loot), places the per-match shovels.
 * Every machine: the spot meshes (mound, X, glint, hole stages, dirt pile, open grave, buried chest), dirt bursts and
 * sounds when a stage finishes (a grave is loud: heard 40 m away), polled from the replicated list.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGDigManager : public AActor
{
	GENERATED_BODY()

public:
	AKGDigManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The world's manager (any machine), or null. Authority: bCreate spawns it. */
	static AKGDigManager* Get(const UWorld* World, bool bCreate = false);

	const TArray<FKGDigSpot>& GetSpots() const { return Spots.Items; }
	const FKGDigSpot* FindSpot(uint16 Id) const;
	int32 IndexOfSpot(uint16 Id) const;
	/** Closest public spot to P within its aim radius (graves are wider), or INDEX_NONE. */
	int32 FindSpotNear(const FVector& P, float Slack = 0.0f) const;
	/** Closest public spot to P within MaxDist (2D), any state. */
	int32 FindNearestSpot(const FVector& P, float MaxDist, TOptional<EKGDigKind> Kind = {}) const;

	/** Matches seed the spots were generated from (0 = the pre-match free-roam set). */
	uint64 GetGeneratedSeed() const { return GeneratedSeed; }

	// ---- Server ----

	/** Authority: throws the old spots away and scatters a fresh set from Seed (graves from the level). */
	void Regenerate(uint64 Seed);
	/** Authority: the hidden buried chests (treasure-map targets) not dug up yet. */
	const TArray<FKGDigSpot>& GetBuried() const { return Buried; }
	/** Authority: a buried chest within Radius of P becomes a public spot (stage 0). Returns its index or INDEX_NONE. */
	int32 RevealBuriedNear(const FVector& P, float Radius);
	/** Authority: one more stage on the spot at Index. Rolls its loot into OutItems. Returns false if already dug out. */
	bool ApplyStage(int32 Index, TArray<FKGItemStack>& OutItems);
	/** Authority (dev / smoke / captures): force a spot's stage without loot. */
	void DebugSetStage(int32 Index, uint8 Stage);
	/** Authority: a public spot (dev Spawn / tests). Returns its index. */
	int32 AddSpot(EKGDigKind Kind, const FVector& Ground, float YawDeg, EKGDigZone Zone = EKGDigZone::None);
	/** Authority: buried chest (tests / dev). */
	void AddBuried(const FVector& Ground);

	/** Ground point under XY for spot generation: the terrain only (no roofs, props, trees, water), gentle slopes. */
	static bool ProbeGround(UWorld* World, const FVector2D& XY, FVector& OutGround);
	/** Standard stone -> grave spot: the grave lies in front of the stone (its local -Y). */
	static FVector GraveFromStone(const FVector& StoneLocation, float StoneYaw, float& OutYaw);

	/** Sound + dust for a finished stage (every machine, from the replicated stage change). */
	void PlayStageFx(const FKGDigSpot& Spot, uint8 NewStage);
	/** Local: a small puff at a spot (strokes in progress, seen by everyone). */
	void Puff(const FVector& At, float Size);

	/** Where the last loud dig (grave) happened and when (local real time), for the HUD "you hear digging" cue. */
	bool GetLastNoise(FVector& OutWhere, double& OutAt) const
	{
		OutWhere = LastNoiseAt;
		OutAt = LastNoiseTime;
		return LastNoiseTime > 0.0;
	}

protected:
	UPROPERTY(Replicated, SaveGame)
	FKGDigSpotList Spots;

	/** Server only: hidden buried chests. */
	UPROPERTY(SaveGame)
	TArray<FKGDigSpot> Buried;

	UPROPERTY(Replicated, SaveGame)
	uint64 GeneratedSeed = 0;

	UPROPERTY(SaveGame)
	uint16 NextId = 1;

	UPROPERTY(SaveGame)
	bool bGenerated = false;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	/** Per-match shovels lying around (server). */
	TArray<TWeakObjectPtr<AKGPickup>> Shovels;

private:
	void SpawnShovels(FKGRng& Rng);
	void MarkChanged(int32 Index);

	// Visuals (every machine)
	struct FSpotVisual
	{
		TWeakObjectPtr<UStaticMeshComponent> Main;
		TWeakObjectPtr<UStaticMeshComponent> Pile;
		TWeakObjectPtr<UStaticMeshComponent> Extra;
		uint8 ShownStage = 255;
		EKGDigKind Kind = EKGDigKind::Mound;
	};
	void SyncVisuals();
	void ApplyVisual(const FKGDigSpot& Spot, FSpotVisual& V);
	UStaticMeshComponent* MakeMeshComp(const FName& Name);
	void TickFx(float DeltaSeconds);
	TMap<uint16, FSpotVisual> Visuals;
	bool bVisualsPrimed = false;
	int32 LastSpotCount = -1;
	uint64 LastSeedSeen = 0;

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Clods;
	struct FClod
	{
		FVector P;
		FVector V;
		float Age;
		float Life;
		float Size;
	};
	TArray<FClod> ClodList;
	FKGRng CosmeticRng;
	FVector LastNoiseAt = FVector::ZeroVector;
	double LastNoiseTime = -1.0;
	float GlintTime = 0.0f;
};
