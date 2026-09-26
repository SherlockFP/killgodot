#include "Manor/KGSecretPassage.h"
#include "Audio/KGAudio.h"
#include "Character/KGCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "KillGodot.h"
#include "Manor/KGManorSubsystem.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "KGSecretPassage"

AKGSecretPassage::AKGSecretPassage()
{
	SoundName = TEXT("S_Passage_Door");
}

void AKGSecretPassage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGSecretPassage, bDiscovered);
}

void AKGSecretPassage::Interact_Implementation(AKGCharacter* By)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bDiscovered)
	{
		Super::Interact_Implementation(By);
		return;
	}
	if (!By || By->IsDead())
	{
		return;
	}
	DiscoverAll(GetWorld(), SecretId, By);
}

FText AKGSecretPassage::GetInteractPrompt_Implementation() const
{
	if (!bDiscovered)
	{
		return DiscoverPrompt.IsEmpty() ? LOCTEXT("Search", "Search") : DiscoverPrompt;
	}
	return Super::GetInteractPrompt_Implementation();
}

void AKGSecretPassage::AuthSetDiscovered(bool bNow, bool bAnnounce)
{
	if (!HasAuthority() || bDiscovered == bNow)
	{
		return;
	}
	FlushNetDormancy();
	bDiscovered = bNow;
	ForceNetUpdate();
	if (bNow && bAnnounce)
	{
		MulticastRevealed();
	}
}

int32 AKGSecretPassage::DiscoverAll(UWorld* World, FName SecretId, AKGCharacter* By)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return 0;
	}
	int32 N = 0;
	bool bWasNew = false;
	for (TActorIterator<AKGSecretPassage> It(World); It; ++It)
	{
		if (It->SecretId != SecretId)
		{
			continue;
		}
		++N;
		if (!It->bDiscovered)
		{
			bWasNew = true;
			It->AuthSetDiscovered(true, true);
		}
	}
	if (bWasNew)
	{
		if (UKGManorSubsystem* M = UKGManorSubsystem::Get(World))
		{
			M->OnSecretDiscovered(SecretId, By);
		}
	}
	return N;
}

void AKGSecretPassage::OnRep_Discovered()
{
	// The prompt and the minimap read bDiscovered directly; the sound comes through MulticastRevealed.
}

void AKGSecretPassage::MulticastRevealed_Implementation()
{
	KGAudio::At(this, *SoundName.ToString(), GetActorLocation(), 1.0f);
}

#undef LOCTEXT_NAMESPACE
