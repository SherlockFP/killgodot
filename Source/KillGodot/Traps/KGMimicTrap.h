#pragma once

#include "CoreMinimal.h"
#include "Traps/KGTrap.h"
#include "KGMimicTrap.generated.h"

class AKGCharacter;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * SPRINT-041 Trapper "Mimic": a chest / crate / barrel (AKGStorageChest, AKGBreakable) that bites. An AKGTrap
 * (SPRINT-040 machine + event log) that rides on its Host container instead of owning a mesh: overlay components
 * (teeth ring, tongue, drool, teeth marks: Tools/Blender/kg_make_mimic.py) are fitted to the host mesh's bounds on
 * every machine, so any container shape works.
 *
 * Life: the Trapper arms it (Idle -> Armed, ArmedByPuid secret). Telegraph while armed, for sharp eyes: a faint
 * breath within 3 m, the lid seam ajar with teeth tips showing, a drool drip; a thrown object that lands within
 * ~1.4 m makes it flinch. The next non-Trapper who opens it (the chest's Interact hook), hits it (the breakable's
 * TakeDamage hook) or picks it up (the host moves) is bitten: Telegraph (the snap) -> Active 3 s (heavy damage that
 * never kills, held in place, a scream heard 30 m away, BiteMarks wound) -> Cooldown -> Idle as a normal container
 * with teeth marks. Dawn disarms a mimic that never bit (UKGAbilitySubsystem).
 */
UCLASS()
class KILLGODOT_API AKGMimicTrap : public AKGTrap
{
	GENERATED_BODY()

public:
	AKGMimicTrap();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** A chest / crate / barrel a mimic may sit in (and not already broken). */
	static bool IsContainer(const AActor* Actor);
	/** The mimic riding on Host, if any (any machine). */
	static AKGMimicTrap* FindOn(const AActor* Host);

	/** Authority: turns Host into an armed mimic for By (reuses a spent one). Null when Host is armed already. */
	static AKGMimicTrap* AuthArmOn(AActor* Host, AKGCharacter* By);

	/**
	 * Authority container hook: By tries to open / hit / lift Host. True = the mimic bit (the caller must NOT open the
	 * container). The Trapper's own mimic never bites them (they open it normally).
	 */
	static bool TryBite(AActor* Host, AKGCharacter* By);

	/** Authority: dawn. A mimic that never bit goes back to sleep (destroyed); a spent one keeps its teeth marks. */
	void AuthExpire();

	/** Authority (dev / shots): bites Who now, even the Trapper. */
	bool AuthBite(AKGCharacter* Who);
	/** Authority (dev / shots / tests): a flinch as if something was thrown at it. */
	void AuthFlinch();

	AActor* GetHost() const { return Host; }
	AKGCharacter* GetVictim() const { return Victim; }
	bool HasTeethMarks() const { return bTeethMarks; }
	bool IsMimicArmed() const { return State == EKGTrapState::Armed; }
	bool IsBiting() const { return State == EKGTrapState::Telegraph || State == EKGTrapState::Active; }
	uint8 GetFlinchSerial() const { return FlinchSerial; }
	/** Cosmetic: "rest" / "bite" / "marks" / "flinch" / "hidden" (smokes, shots). */
	FString GetLookName() const;

protected:
	virtual bool WantsTrigger(const TArray<AKGCharacter*>& InZone) const override { return false; }
	virtual void OnTelegraph() override;
	virtual void OnFire() override;
	virtual void OnEnd() override;
	virtual void OnReady() override;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMimicCue(uint8 Cue, FVector_NetQuantize At);

	void ServerWatch(float DeltaSeconds);
	void TickLook(float DeltaSeconds);
	void FitOverlays();
	void ApplyHold();
	UStaticMeshComponent* HostMesh() const;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> Host;

	/** The body in the jaws (public: everyone can see who is being bitten). */
	UPROPERTY(Replicated)
	TObjectPtr<AKGCharacter> Victim;

	UPROPERTY(Replicated, SaveGame)
	bool bTeethMarks = false;

	/** Bumped by every flinch (clients jolt the lid on change). */
	UPROPERTY(Replicated)
	uint8 FlinchSerial = 0;

	/** Horizontal direction the mouth faces (towards where the Trapper stood). */
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal FrontDir = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Teeth;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Tongue;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Drool;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Marks;

private:
	// Server scratch.
	FVector HostRestLocation = FVector::ZeroVector;
	float FlinchCooldown = 0.0f;
	// Cosmetic scratch (every machine but a dedicated server).
	float LookTime = 0.0f;
	float BreathTimer = 0.0f;
	float DripTimer = 0.0f;
	float FlinchLook = 0.0f;
	uint8 SeenFlinchSerial = 0;
	FVector HostMeshRestScale = FVector::OneVector;
	TWeakObjectPtr<UStaticMeshComponent> TouchedHostMesh;
	TWeakObjectPtr<AKGCharacter> HeldBody;
};
