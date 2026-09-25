#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGDevComponent.generated.h"

class AKGCharacter;
class APlayerController;

/**
 * Dev panel link on every human player controller (added at runtime by UKGDevSubsystem on the server, replicated to
 * its owner only because player controllers are). Carries the client -> host RPC for dev verbs and the per-player
 * cheat state, which it enforces every tick on the server and the owning client alike (so prediction agrees):
 *  - god mode (CanBeDamaged off, server),
 *  - fly / noclip (MOVE_Flying + no collision; Space rises, Ctrl/C sinks),
 *  - movement speed multiplier (UKGCharacterMovement::DevSpeedScale).
 * Development builds only: the subsystem never creates it in Shipping and the RPC refuses there.
 */
UCLASS(ClassGroup = (KillGodot), NotBlueprintable)
class KILLGODOT_API UKGDevComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGDevComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static UKGDevComponent* FindFor(const APlayerController* PC);

	/** Client -> host: run one dev verb line for this player (refused unless FKGDev::MayRun). */
	UFUNCTION(Server, Reliable)
	void ServerRun(const FString& Line);

	/** Host -> client: the result of ServerRun, shown in the panel footer. */
	UFUNCTION(Client, Reliable)
	void ClientReport(bool bOk, const FString& Message);

	/** Host -> client: refill the locally predicted stamina too (Me.Stamina). */
	UFUNCTION(Client, Reliable)
	void ClientRefillStamina();

	/** Refills AKGCharacter's (protected) stamina through reflection. Any machine. */
	static bool RefillStamina(AKGCharacter* Character);

	/** Host decided this player may run server verbs (the host itself, or kg.Dev.AllowClients 1). */
	UPROPERTY(Replicated)
	bool bMayRun = false;

	UPROPERTY(Replicated)
	bool bGod = false;

	UPROPERTY(Replicated)
	bool bFly = false;

	UPROPERTY(Replicated)
	float SpeedScale = 1.0f;

private:
	APlayerController* GetPC() const;

	TWeakObjectPtr<AKGCharacter> AppliedPawn;
	bool bAppliedGod = false;
	bool bAppliedFly = false;
	float SavedMaxFlySpeed = 600.0f;
	float SavedFlyBraking = 0.0f;
};
