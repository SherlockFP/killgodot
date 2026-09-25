#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGSnapshotComponent.generated.h"

/** One actor inside a host-migration snapshot. Never stores pointers or NetGUIDs. */
USTRUCT()
struct KILLGODOT_API FKGActorRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Id;

	UPROPERTY()
	FSoftClassPath Class;

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FVector LinearVelocity = FVector::ZeroVector;

	UPROPERTY()
	FVector AngularVelocityDeg = FVector::ZeroVector;

	UPROPERTY()
	bool bSleeping = true;

	/** Every SaveGame-flagged property of the owning actor. */
	UPROPERTY()
	TArray<uint8> Blob;
};

/**
 * Makes an actor survive host migration (Docs/05_Tech_Architecture.md §4). Required on every gameplay-relevant
 * actor. Server-only values must be UPROPERTY(SaveGame) on the owner so they end up in the blob.
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGSnapshotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGSnapshotComponent();

	virtual void OnRegister() override;

	const FGuid& GetPersistentId() const { return PersistentId; }

	void WriteRecord(FKGActorRecord& OutRecord) const;
	void ReadRecord(const FKGActorRecord& Record);

protected:
	/** Stable across hosts. Level-placed actors get it at edit time; spawned actors get one at spawn. */
	UPROPERTY(EditAnywhere, SaveGame, Category = "Snapshot")
	FGuid PersistentId;
};
