#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Forest/KGForestTypes.h"
#include "World/KGInteractable.h"
#include "KGForestActors.generated.h"

class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UAnimSequence;
class AKGCharacter;

/** kg.Forest.AutoWalk: 0 off, 1 always, 2 only while a Mist tongue follows (the local player walks along its arrow). */
extern KILLGODOT_API int32 GKGForestAutoWalk;

/** Damage types: the corpse tells what killed it (the case file reads the class name). */
UCLASS()
class KILLGODOT_API UKGDamageType_WolfBite : public UDamageType
{
	GENERATED_BODY()
};

UCLASS()
class KILLGODOT_API UKGDamageType_Mist : public UDamageType
{
	GENERATED_BODY()
};

/**
 * Owner-only forest state of one player (spawned by the server per player controller, bOnlyRelevantToOwner): the HUD
 * reads the frost, the wolf stage and the compass arrow to the nearest safe cell. Nobody else ever receives it.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGForestPlayerInfo : public AActor
{
	GENERATED_BODY()

public:
	AKGForestPlayerInfo();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** The local player's own info (clients: the one replicated to them; listen host: its own). */
	static AKGForestPlayerInfo* FindLocal(const UWorld* World);

	UPROPERTY(Replicated)
	uint8 Band = 0;

	UPROPERTY(Replicated)
	uint8 WolfStage = 0;

	UPROPERTY(Replicated)
	uint8 MistStage = 0;

	/** Frost amount 0..1 (the Mist's notice meter past the frost mark, then the core). */
	UPROPERTY(Replicated)
	float Frost = 0.0f;

	/** Direction (2D, world) to the nearest safe cell; zero when on one. */
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal SafeDir = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float SafeDist = 0.0f;

	/** Short subtitle ("A wolf howls to the north", "Your breath fogs...") and until when (server time). */
	UPROPERTY(Replicated)
	FString Line;

	UPROPERTY(Replicated)
	float LineUntil = 0.0f;

	/** Last bite (server time) for the red flash. */
	UPROPERTY(Replicated)
	float BittenAt = -100.0f;

	/** Signed tonight's Camp Vigil (1 = signed, 2 = in the ring). */
	UPROPERTY(Replicated)
	uint8 Vigil = 0;

	virtual void Tick(float DeltaSeconds) override;

	TWeakObjectPtr<AKGCharacter> Body;
	FString LastLoggedStage;
	bool bAutoWalkLogged = false;
};

/** The replicated forest director (always relevant): trail-head lanterns, the Mist Wall ring, the camp fire state. */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGForestDirector : public AActor
{
	GENERATED_BODY()

public:
	AKGForestDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AKGForestDirector* Get(const UWorld* World);

	/** Howl heard by everyone within KG_HEAR_HOWL (the sector name only within KG_HOWL_SECTOR_R, done by the HUD). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHowl(FVector_NetQuantize At, uint8 Sector);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCue(FVector_NetQuantize At, uint8 Cue);

	/** Last howl, read by the HUD of every machine. */
	FVector LastHowlAt = FVector::ZeroVector;
	uint8 LastHowlSector = 0;
	float LastHowlTime = -100.0f;

	/** The ring of fog at the forest boundary (cosmetic, local). */
	void BuildWall();
	void BuildLanterns();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WallParts;
	float Clock = 0.0f;
};

/** A wolf: the Quaternius wolf with its own Idle / Walking clips, glowing eyes at night. Movement is server-driven. */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGWolf : public ACharacter
{
	GENERATED_BODY()

public:
	AKGWolf();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0 idle / sleeping, 1 sneak, 2 trot, 3 lunge, 4 flee. Drives the clip rate on every machine. */
	UPROPERTY(Replicated)
	uint8 Gait = 0;

	/** Eyes glow (night or the eyes stage). */
	UPROPERTY(Replicated)
	bool bEyesLit = false;

	/** Hidden at the den while no one is hunted. */
	UPROPERTY(ReplicatedUsing = OnRep_Awake)
	bool bAwake = false;

	UFUNCTION()
	void OnRep_Awake();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSnarl();

	void SetAwake(bool bIn);

	// ---- server brain (driven by UKGForestSubsystem) ----
	int32 Pack = 0;
	int32 Slot = 0;
	FVector Den = FVector::ZeroVector;
	float LungeLeft = 0.0f;
	float BackOff = 0.0f;
	float RepelLeft = 0.0f;
	float OrbitPhase = 0.0f;
	float Repath = 0.0f;
	float DebugLog = 0.0f;
	/** Last move request: 0 navmesh path, 1 no path (went straight), 2 path crossed a village / lit cell (straight). */
	int32 MoveKind = 0;
	bool bSeenLogged = false;

private:
	void ApplyLook();
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> EyeL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> EyeR;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> EyeLight;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ClipIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ClipWalk;
	uint8 ShownGait = 255;
};

/** A rolling Mist tongue following one player. Replicated; the look is translucent smoke spheres (headless-safe). */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGMistTongue : public AActor
{
	GENERATED_BODY()

public:
	AKGMistTongue();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** 0..1 visibility (fades in, fades out when the target escapes). */
	UPROPERTY(Replicated)
	float Density = 0.0f;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> TargetPS;

	// ---- server ----
	float Age = 0.0f;
	bool bFading = false;
	bool bSeenLogged = false;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
	TArray<FVector> PuffBase;
	float Clock = 0.0f;
};

/** The camp fire (SPRINT-034b): lit / fed / burned down, glows over the village at night. Replicated. */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGCampfire : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGCampfire();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	static AKGCampfire* Get(const UWorld* World);

	UPROPERTY(Replicated)
	float Fuel = 0.0f;

	/** 0 fresh logs, 1 cold ash, 2 wet ash (doused), 3 scattered embers (kicked) - kept until dawn. */
	UPROPERTY(Replicated)
	uint8 Ash = 0;

	/** Authority: light (needs fuel) or feed one log from the camp woodpile. Returns what happened. */
	FString AuthUse(AKGCharacter* By);
	void AuthAddLog();
	/** Dawn: fresh logs, no fuel. */
	void AuthReset();
	bool IsLit() const { return Fuel > 0.0f; }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Logs;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> Hitbox;
	float Clock = 0.0f;
};

/** The vigil book at the notice board: sign in the day (from day 2) to keep the camp fire that night. */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGVigilBook : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGVigilBook();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	static AKGVigilBook* Get(const UWorld* World);

	/** Signatures count (public: the book shows how many, not who). */
	UPROPERTY(Replicated)
	uint8 NumSigned = 0;

	UPROPERTY(Replicated)
	uint8 MaxSigners = 4;

	UPROPERTY(Replicated)
	bool bOpen = false;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> Hitbox;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
};

/**
 * Bite wounds on a bitten / killed player (distinct evidence: a wolf's torn marks, never a clean knife cut). Attached to
 * the body, replicated; E on a corpse's wounds reads them out. TODO(020b): write the evidence into the Event Ledger.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGBiteEvidence : public AActor, public IKGInteractable
{
	GENERATED_BODY()

public:
	AKGBiteEvidence();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Interact_Implementation(AKGCharacter* By) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** "bite" (wolf) or "frost" (the Mist). */
	UPROPERTY(ReplicatedUsing = OnRep_Look)
	FName Kind;

	UPROPERTY(ReplicatedUsing = OnRep_Look)
	uint8 Wounds = 0;

	UPROPERTY(Replicated)
	FString Sector;

	UFUNCTION()
	void OnRep_Look();

	void AuthAddWound();

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UBoxComponent> Hitbox;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Marks;
};
