#pragma once

#include "CoreMinimal.h"
#include "Abilities/KGAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "KGAbilityHolder.generated.h"

class AController;
class AKGCharacter;
class AKGTrap;
class UKGSnapshotComponent;

/** A private line for the owner only (tripwire alerts, denials). */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGAbilityNote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ability") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Ability") float ShownAt = 0.0f;
};

/**
 * SPRINT-041 role abilities of ONE player: an owner-only actor (like AKGForestPlayerInfo), spawned by
 * UKGAbilitySubsystem next to the player's controller when the dealt role has abilities (FKGRoleInfo::AbilityIds) and
 * destroyed when it has none. bOnlyRelevantToOwner + COND_OwnerOnly: nobody else ever receives it, so its existence,
 * the role id and the charges never leak (KillGodot.Abilities.NoLeak).
 * Server authoritative: charges refill at dawn, cooldowns are FKGMatchClock (remaining seconds), every use is validated
 * by FKGAbilityRules on the server with the server's own measurements (phase, alive, line of sight, target, range).
 * Owner UI: the ability bar + targeting prompt (KGAbilityHUD), keys Alt+1..3 (press = aim, press again / LMB = use,
 * RMB / Esc = cancel), console kg.Ability.Use <Id>.
 */
UCLASS(NotBlueprintable)
class KILLGODOT_API AKGAbilityHolder : public AActor
{
	GENERATED_BODY()

public:
	AKGAbilityHolder();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** The holder of Controller (server: any controller; client: only the local one exists). */
	static AKGAbilityHolder* FindFor(const AController* Controller);
	/** The local player's holder on this machine (UI). */
	static AKGAbilityHolder* FindLocal(const UWorld* World);

	/** Authority: binds to RoleId and gives the dawn charges. */
	void AuthSetup(FName InRoleId);
	/** Authority: dawn refill (charges back, cooldowns cleared, the trap list emptied). */
	void AuthRefill();
	/** Authority: validates and runs AbilityId with the given view (a bot passes its own eyes). Returns the verdict. */
	EKGAbilityDeny AuthUse(FName AbilityId, const FVector& ViewStart, const FVector& ViewDir, FString* OutDetail = nullptr);
	/** Authority: a private line for the owner (client RPC, or the log for bots). */
	void AuthNotify(const FString& Text);
	/** Authority: remembers one of the owner's traps (the owner's client shows it after the public reveal). */
	void AuthAddTrap(AKGTrap* Trap);

	/** Owner: asks the server to use AbilityId with the local camera view. */
	void RequestUse(FName AbilityId);

	FName GetRoleId() const { return RoleId; }
	const TArray<FKGAbilityState>& GetStates() const { return States; }
	const FKGAbilityState* GetState(FName AbilityId) const;
	const TArray<TObjectPtr<AKGTrap>>& GetMyTraps() const { return MyTraps; }
	const TArray<FKGAbilityNote>& GetNotes() const { return Notes; }
	AKGCharacter* GetBody() const;
	FString Describe() const;

	/** Local UI state: the ability being aimed (None = not aiming). */
	FName AimingAbility;
	/** Local: last denial / success line under the bar. */
	FString LastFeedback;
	float LastFeedbackAt = -100.0f;

	/** Server: is Who seen by another living player (line of sight within Radius)? Pure world query. */
	static bool IsSeenByOthers(const AKGCharacter* Who, float RadiusCm);

	/** Targeting (server validation and the owner's aiming preview use the same code). */
	static AActor* PickContainer(UWorld* World, const FVector& Eyes, const FVector& Dir, float RangeCm, float& OutDistCm);
	static bool PickGround(UWorld* World, const AActor* Ignore, const FVector& Eyes, const FVector& Dir, float RangeCm, FVector& OutPoint);

protected:
	UFUNCTION(Server, Reliable)
	void ServerUse(FName AbilityId, FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir);

	UFUNCTION(Client, Reliable)
	void ClientNotify(const FString& Text);

	UFUNCTION(Client, Reliable)
	void ClientFeedback(FName AbilityId, EKGAbilityDeny Verdict, const FString& Detail);

	UFUNCTION()
	void OnRep_MyTraps();

	/** Owner only: the role the abilities belong to. */
	UPROPERTY(Replicated, SaveGame)
	FName RoleId;

	/** Owner only: charges + cooldowns, in bar order. */
	UPROPERTY(Replicated, SaveGame)
	TArray<FKGAbilityState> States;

	/** Owner only: the traps this player armed tonight (shown to the owner after the public 2 s reveal). */
	UPROPERTY(ReplicatedUsing = OnRep_MyTraps)
	TArray<TObjectPtr<AKGTrap>> MyTraps;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	/** Local only (owner client / host): recent private notes. */
	TArray<FKGAbilityNote> Notes;

private:
	void TickInput(float DeltaSeconds);
	float ReplicateAccum = 0.0f;
};
