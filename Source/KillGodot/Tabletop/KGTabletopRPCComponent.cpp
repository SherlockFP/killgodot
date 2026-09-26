#include "Tabletop/KGTabletopRPCComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Tabletop/KGBoardTable.h"

UKGTabletopRPCComponent::UKGTabletopRPCComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UKGTabletopRPCComponent* UKGTabletopRPCComponent::FindFor(const APlayerController* PC)
{
	return PC ? FindForPlayer(PC->PlayerState) : nullptr;
}

UKGTabletopRPCComponent* UKGTabletopRPCComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UKGTabletopRPCComponent>() : nullptr;
}

APlayerState* UKGTabletopRPCComponent::OwnerState() const
{
	return Cast<APlayerState>(GetOwner());
}

void UKGTabletopRPCComponent::Notice(const FString& Text)
{
	LastNotice = Text;
	LastNoticeTime = FPlatformTime::Seconds();
}

void UKGTabletopRPCComponent::RequestMove(AKGBoardTable* Table, const FString& MoveText)
{
	if (!Table)
	{
		return;
	}
	ServerMove(Table, MoveText);
}

void UKGTabletopRPCComponent::RequestAction(AKGBoardTable* Table, EKGTableAction Action)
{
	if (!Table)
	{
		return;
	}
	ServerAction(Table, static_cast<uint8>(Action));
}

void UKGTabletopRPCComponent::ServerMove_Implementation(AKGBoardTable* Table, const FString& MoveText)
{
	APlayerState* Who = OwnerState();
	if (!Table || !Who || !Table->HasAuthority())
	{
		return;
	}
	// G7.2: a client may not flood the table (20 requests per second is far above human play).
	const double Now = FPlatformTime::Seconds();
	if (Now - LastRequestTime > 1.0)
	{
		LastRequestTime = Now;
		RequestsThisSecond = 0;
	}
	if (++RequestsThisSecond > 20)
	{
		return;
	}
	FString Error;
	if (!Table->ServerTryMove(Who, MoveText.Left(40), Error))
	{
		ClientNotice(Error);
	}
}

void UKGTabletopRPCComponent::ServerAction_Implementation(AKGBoardTable* Table, uint8 Action)
{
	APlayerState* Who = OwnerState();
	if (!Table || !Who || !Table->HasAuthority())
	{
		return;
	}
	bool bOk = false;
	switch (static_cast<EKGTableAction>(Action))
	{
	case EKGTableAction::Resign: bOk = Table->ServerResign(Who); break;
	case EKGTableAction::OfferDraw: bOk = Table->ServerOfferDraw(Who); break;
	case EKGTableAction::Rematch: bOk = Table->ServerRematch(Who); break;
	default: break;
	}
	if (!bOk)
	{
		ClientNotice(TEXT("not now"));
	}
}

void UKGTabletopRPCComponent::ClientNotice_Implementation(const FString& Text)
{
	Notice(Text);
	UE_LOG(LogKillGodot, Log, TEXT("KG_TABLE_NOTICE %s"), *Text);
}
