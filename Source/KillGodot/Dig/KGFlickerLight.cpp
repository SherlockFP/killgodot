#include "Dig/KGFlickerLight.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/Crc.h"

AKGFlickerLight::AKGFlickerLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	SetRootComponent(Light);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(BaseIntensity);
	Light->SetAttenuationRadius(520.0f);
	Light->SetLightColor(FLinearColor(1.0f, 0.62f, 0.32f));
}

void AKGFlickerLight::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || Flicker <= 0.0f)
	{
		SetActorTickEnabled(false);
	}
	Phase = (FCrc::StrCrc32(*GetName()) % 1000) * 0.0137f;
	Light->SetIntensity(BaseIntensity);
}

void AKGFlickerLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Only near a local camera (the underground is small, but the village above does not need these ticking).
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager &&
	    FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) > FMath::Square(3000.0f))
	{
		return;
	}
	Time += DeltaSeconds;
	const float N = 0.55f * FMath::Sin(Time * 7.3f + Phase) + 0.3f * FMath::Sin(Time * 13.1f + Phase * 2.0f) +
	                0.15f * FMath::Sin(Time * 23.7f + Phase * 3.0f);
	Light->SetIntensity(BaseIntensity * (1.0f + Flicker * N));
}
