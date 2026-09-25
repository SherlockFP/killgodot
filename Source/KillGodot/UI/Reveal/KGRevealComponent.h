#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGRevealComponent.generated.h"

class APlayerController;
class APlayerState;

/** A fellow Impatient, sent only to the owner of the reveal component. */
USTRUCT()
struct FKGRevealMate
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> Player;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FName RoleId;
};

/**
 * Per-player half of the role reveal ceremony, a replicated dynamic component on every human APlayerController
 * (added by UKGRevealSubsystem on the server; player controllers only replicate to their owner, and every property
 * here is COND_OwnerOnly on top of that):
 *  - Mates: the other members of an Impatient team (Clockbreakers, vampires) - nobody else ever receives them,
 *  - the ready flag of this player plus the ready tally, so the ceremony can show "3 / 5 ready",
 *  - ServerSetReady: the per-player "skip" press. When every human is ready the server shortens the RoleReveal phase
 *    on the match clock (FKGMatchClock) to the fade-out.
 * The role itself stays where it always was: AKGPlayerState::PrivateRoleId (COND_OwnerOnly).
 */
UCLASS(ClassGroup = (KillGodot))
class KILLGODOT_API UKGRevealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGRevealComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UKGRevealComponent* Find(const APlayerController* PC);

	/** Authority, idempotent: humans only (bots never see a ceremony). */
	static UKGRevealComponent* Ensure(APlayerController* PC);

	/** Authority: roles were just dealt. Fills every human's mates list and clears the ready flags. */
	static void AuthDeal(UWorld* World);

	/** Authority: recount ready humans, publish the tally, cut the phase short when everyone is ready. */
	static void AuthUpdateReady(UWorld* World);

	/** Owning client: "I have seen my card" (Space). */
	UFUNCTION(Server, Reliable)
	void ServerSetReady();

	/** Owning client entry point (local host included). Returns false when it is too early / already ready. */
	bool RequestReady();

	const TArray<FKGRevealMate>& GetMates() const { return Mates; }
	bool IsReady() const { return bReady; }
	int32 GetReadyCount() const { return ReadyCount; }
	int32 GetHumanCount() const { return HumanCount; }
	int32 GetDealSerial() const { return DealSerial; }

private:
	UPROPERTY(Replicated)
	TArray<FKGRevealMate> Mates;

	/** Bumped by every deal; the ceremony only trusts Mates of the current deal. */
	UPROPERTY(Replicated)
	int32 DealSerial = 0;

	UPROPERTY(Replicated)
	bool bReady = false;

	UPROPERTY(Replicated)
	uint8 ReadyCount = 0;

	UPROPERTY(Replicated)
	uint8 HumanCount = 0;

	/** Local: when the last ready press was sent (re-sent after a second if the server refused it as too early). */
	double ReadyRequestedAt = -1.0;
};
