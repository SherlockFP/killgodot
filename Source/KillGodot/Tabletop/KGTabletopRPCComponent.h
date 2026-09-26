#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KGTabletopRPCComponent.generated.h"

class AKGBoardTable;
class APlayerController;
class APlayerState;

UENUM()
enum class EKGTableAction : uint8
{
	Resign,
	OfferDraw,
	Rematch
};

/**
 * Client -> server relay for the board tables, added to every AKGPlayerState by UKGTabletopSubsystem (the
 * UKGPlayerExtrasSubsystem pattern: a replicated dynamic subobject owned by the player's connection, so its Server
 * RPCs route from the owning client; on the listen host they run at once). The table validates everything
 * (seat, turn, phase, legality); refusals come back as ClientNotice for the panel's toast.
 */
UCLASS()
class KILLGODOT_API UKGTabletopRPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGTabletopRPCComponent();

	static UKGTabletopRPCComponent* FindFor(const APlayerController* PC);
	static UKGTabletopRPCComponent* FindForPlayer(const APlayerState* PlayerState);

	/** Any machine: plays MoveText ("e2e4", "e7e8q", "c3xe5xg7") on Table as this player. */
	void RequestMove(AKGBoardTable* Table, const FString& MoveText);
	void RequestAction(AKGBoardTable* Table, EKGTableAction Action);

	const FString& GetLastNotice() const { return LastNotice; }
	double GetLastNoticeTime() const { return LastNoticeTime; }

	UFUNCTION(Server, Reliable)
	void ServerMove(AKGBoardTable* Table, const FString& MoveText);

	UFUNCTION(Server, Reliable)
	void ServerAction(AKGBoardTable* Table, uint8 Action);

	UFUNCTION(Client, Unreliable)
	void ClientNotice(const FString& Text);

private:
	APlayerState* OwnerState() const;
	void Notice(const FString& Text);

	FString LastNotice;
	double LastNoticeTime = -100.0;
	double LastRequestTime = -100.0;
	int32 RequestsThisSecond = 0;
};
