#include "World/KGLadder.h"
#include "Components/BoxComponent.h"

AKGLadder::AKGLadder()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ClimbVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ClimbVolume"));
	ClimbVolume->SetupAttachment(GetRootComponent());
	ClimbVolume->SetCollisionProfileName(TEXT("Trigger"));
	ClimbVolume->SetGenerateOverlapEvents(true);
	ClimbVolume->SetCanEverAffectNavigation(false);
	ClimbVolume->ComponentTags.Add(TEXT("KG_Ladder"));
	SetHeight(Height);
}

void AKGLadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetHeight(Height);
}

void AKGLadder::SetHeight(float NewHeight)
{
	Height = NewHeight;
	// Volume in front of the ladder (-X side), a bit taller so you can step off at the top.
	ClimbVolume->SetBoxExtent(FVector(45.0f, 55.0f, Height * 0.5f + 60.0f));
	ClimbVolume->SetRelativeLocation(FVector(-35.0f, 0.0f, Height * 0.5f + 30.0f));
}
