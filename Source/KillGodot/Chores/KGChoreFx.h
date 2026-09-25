#pragma once

#include "CoreMinimal.h"
#include "Chores/KGChoreTypes.h"
#include "GameFramework/Actor.h"
#include "KGChoreFx.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class USoundAttenuation;
class UStaticMesh;
class UStaticMeshComponent;

/** One visual chore completion (public: everyone may see the effect, not who did it). */
USTRUCT()
struct FKGChoreFxEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FName ChoreId;

	/** EKGChoreFx. */
	UPROPERTY()
	uint8 Fx = 0;

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.0f;

	/** Server world time when it happened (clients age the effect from GetServerWorldTimeSeconds). */
	UPROPERTY()
	float ServerTime = 0.0f;

	UPROPERTY()
	int32 Serial = 0;
};

/**
 * Visual chores ("visual tasks"): the world effect of a real chore completion, replicated to everybody (always
 * relevant, spawned by the server on first use, never streamed out). Effects are built from engine shapes, a few
 * point lights and the procedural sounds, so they need no content:
 *  RingBell -> 3 bell strikes heard across the village; FuelLighthouse -> the lamp room blazes; BakeBread -> chimney
 *  smoke above the bakery; PostNotice -> a fresh sheet on the board; LightCandles -> candle flames; LightHarbourLamp ->
 *  the lamp lights; WindClock -> the clock chimes; ChopWood -> the log pile grows.
 * Late joiners see the persistent ones (age computed from server time) but no old sounds.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGChoreFx : public AActor
{
	GENERATED_BODY()

public:
	AKGChoreFx();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** The world's effect director; the server creates it on demand when bCreate. */
	static AKGChoreFx* Get(UWorld* World, bool bCreate);

	/** Authority: play Fx for everyone at Where. */
	void AuthTrigger(FName ChoreId, EKGChoreFx Fx, const FVector& Where, const FRotator& Facing);

	const TArray<FKGChoreFxEvent>& GetEvents() const { return Events; }
	/** How long an effect stays visible (seconds). */
	static float Duration(EKGChoreFx Fx);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Events, SaveGame)
	TArray<FKGChoreFxEvent> Events;

	UFUNCTION()
	void OnRep_Events();

private:
	struct FLive
	{
		int32 Serial = 0;
		EKGChoreFx Fx = EKGChoreFx::None;
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.0f;
		float ServerTime = 0.0f;
		/** Sound strikes already played. */
		int32 Strikes = 0;
		bool bFresh = true;
		TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
		TArray<TObjectPtr<UPointLightComponent>> Lights;
		FVector Base = FVector::ZeroVector;
	};

	void SyncLive();
	void Build(FLive& L);
	void Animate(FLive& L, float Age, float Dt);
	void DestroyLive(FLive& L);
	float ServerNow() const;
	UStaticMeshComponent* AddMesh(FLive& L, UStaticMesh* Mesh, UMaterialInterface* Material, const FLinearColor& Color, float Glow);
	UPointLightComponent* AddLight(FLive& L, const FLinearColor& Color, float Intensity, float Radius);
	void Strike(const FVector& Where, float Pitch, float Volume);

	TArray<FLive> Live;
	int32 NextSerial = 1;
	/** Keeps runtime components alive for the GC. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Owned;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> BellAttenuation;
};
