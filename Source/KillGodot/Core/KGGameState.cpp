#include "Core/KGGameState.h"
#include "Cosmetics/KGProfileSave.h"
#include "GameFramework/PlayerController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Core/KGPlayerState.h"
#include "Audio/KGAudio.h"


namespace
{
	struct FKGPhaseLook
	{
		float SunPitch;
		float SunYaw;
		float SunLux;
		FLinearColor SunColor;
		float SkyIntensity;
	};

	// Docs/06_Art_Direction.md: warm golden days, blue readable nights (never pitch black).
	FKGPhaseLook GetLook(EKGPhase Phase)
	{
		switch (Phase)
		{
		case EKGPhase::Night:
			return {-22.0f, 200.0f, 0.9f, FLinearColor(0.45f, 0.58f, 1.0f), 0.35f};   // moonlight
		case EKGPhase::Dawn:
		case EKGPhase::RoleReveal:
			return {-9.0f, 95.0f, 5.0f, FLinearColor(1.0f, 0.62f, 0.45f), 0.8f};
		case EKGPhase::Meeting:
		case EKGPhase::Trial:
		case EKGPhase::Epilogue:
			return {-16.0f, 62.0f, 7.5f, FLinearColor(1.0f, 0.78f, 0.58f), 1.1f};  // dusk
		default:
			return {-42.0f, 35.0f, 9.0f, FLinearColor(1.0f, 0.93f, 0.82f), 1.3f};  // day
		}
	}
}

AKGGameState::AKGGameState()
{
	SetNetUpdateFrequency(10.0f);
	PrimaryActorTick.bCanEverTick = true;
}

void AKGGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (LightBlend < 1.0f)
	{
		UpdatePhaseLighting(DeltaSeconds, false);
	}
}

void AKGGameState::UpdatePhaseLighting(float DeltaSeconds, bool bSnap)
{
	if (GetNetMode() == NM_DedicatedServer || Phase == EKGPhase::Lobby)
	{
		return;
	}
	// Six-second eased blend; each step moves the lights a fraction of the way to the target look.
	LightBlend = bSnap ? 1.0f : FMath::Min(1.0f, LightBlend + DeltaSeconds / 6.0f);
	const float Alpha = bSnap ? 1.0f : FMath::Clamp(DeltaSeconds * 1.2f, 0.0f, 1.0f);
	const FKGPhaseLook Look = GetLook(Phase);
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (!Sun || Sun->Mobility != EComponentMobility::Movable)
		{
			continue;
		}
		const FRotator Current = Sun->GetComponentRotation();
		const FRotator Target(Look.SunPitch, Look.SunYaw, 0.0f);
		Sun->SetWorldRotation(FMath::RInterpTo(Current, Target, 1.0f, Alpha));
		Sun->SetIntensity(FMath::Lerp(Sun->Intensity, Look.SunLux, Alpha));
		Sun->SetLightColor(FMath::Lerp(Sun->GetLightColor(), Look.SunColor, Alpha));
	}
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		if (USkyLightComponent* Sky = It->GetLightComponent())
		{
			Sky->SetIntensity(FMath::Lerp(Sky->Intensity, Look.SkyIntensity, Alpha));
		}
	}
}

void AKGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGGameState, Phase);
	DOREPLIFETIME(AKGGameState, Clock);
	DOREPLIFETIME(AKGGameState, DayIndex);
	DOREPLIFETIME(AKGGameState, MigrationEpoch);
	DOREPLIFETIME(AKGGameState, SuccessorRanks);
	DOREPLIFETIME(AKGGameState, Preparation);
	DOREPLIFETIME(AKGGameState, bHasWinner);
	DOREPLIFETIME(AKGGameState, Winner);
	DOREPLIFETIME(AKGGameState, OnTrial);
	DOREPLIFETIME(AKGGameState, Announcement);
	DOREPLIFETIME(AKGGameState, AnnouncementUntil);
	DOREPLIFETIME(AKGGameState, LighthouseRevealed);
	DOREPLIFETIME(AKGGameState, RevealUntil);
}

void AKGGameState::SetPhase(EKGPhase NewPhase, float Duration)
{
	Phase = NewPhase;
	Clock.Start(Duration);
	OnRep_Phase();
}

void AKGGameState::OnRep_Phase()
{
	LightBlend = 0.0f;
	// The church bell marks the village's day: dawn and the evening meeting (with the gong).
	if (Phase == EKGPhase::Meeting)
	{
		KGAudio::UI(this, TEXT("S_MeetingGong"), 0.9f);
		KGAudio::UI(this, TEXT("S_ChurchBell"), 0.7f);
	}
	else if (Phase == EKGPhase::Dawn || Phase == EKGPhase::RoleReveal)
	{
		KGAudio::UI(this, TEXT("S_ChurchBell"), 0.6f);
	}
	else if (Phase == EKGPhase::Epilogue)
	{
		// Match over: every local player banks the coins in their pockets into profile gold (cosmetic shop
		// currency, local save) and gets the participation reward. Runs once per machine per match.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController())
			{
				UKGProfileSave::BankMatchCoins(PC);
				if (UKGProfileSave* Profile = UKGProfileSave::GetProfile())
				{
					Profile->AwardGold(50, TEXT("MatchPlayed"));
				}
			}
		}
	}
	OnPhaseChanged.Broadcast(Phase);
}

void AKGGameState::Announce(const FString& Text, float Seconds)
{
	Announcement = Text;
	AnnouncementUntil = GetServerWorldTimeSeconds() + Seconds;
	ForceNetUpdate();
}
