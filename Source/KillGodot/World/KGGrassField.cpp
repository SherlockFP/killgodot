#include "World/KGGrassField.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AKGGrassField::AKGGrassField()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	struct FLayerDef
	{
		const TCHAR* Name;
		const TCHAR* Mesh;
		int32 CullStart;
		int32 CullEnd;
	};
	// Short grass reaches furthest (it is cheap and fills the horizon); tall grass is the near-field drama.
	const FLayerDef Defs[] = {
		{TEXT("Short"), TEXT("/Game/KillGodot/Env/Grass/KG_Grass/StaticMeshes/SM_KG_GrassShort.SM_KG_GrassShort"), 6000, 8000},
		{TEXT("Mid"), TEXT("/Game/KillGodot/Env/Grass/KG_Grass/StaticMeshes/SM_KG_GrassMid.SM_KG_GrassMid"), 5000, 7000},
		{TEXT("Tall"), TEXT("/Game/KillGodot/Env/Grass/KG_Grass/StaticMeshes/SM_KG_GrassTall.SM_KG_GrassTall"), 4500, 6500},
		{TEXT("Flowers"), TEXT("/Game/KillGodot/Env/Grass/KG_Grass/StaticMeshes/SM_KG_GrassFlowers.SM_KG_GrassFlowers"), 3500, 5000},
	};
	for (const FLayerDef& Def : Defs)
	{
		UHierarchicalInstancedStaticMeshComponent* H =
			CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Def.Name);
		H->SetupAttachment(GetRootComponent());
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Def.Mesh);   // non-static: Live Coding safe
		if (Mesh.Succeeded())
		{
			H->SetStaticMesh(Mesh.Object);
		}
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCanEverAffectNavigation(false);
		H->SetCastShadow(false);
		H->bAffectDistanceFieldLighting = false;
		H->SetCullDistances(Def.CullStart, Def.CullEnd);
		H->bDisableCollision = true;
		GrassLayers.Add(H);
	}
}

void AKGGrassField::AddClumps(int32 Layer, const TArray<FTransform>& Transforms)
{
	if (GrassLayers.IsValidIndex(Layer) && GrassLayers[Layer])
	{
		GrassLayers[Layer]->AddInstances(Transforms, false, true);
	}
}

void AKGGrassField::ClearClumps()
{
	for (UHierarchicalInstancedStaticMeshComponent* H : GrassLayers)
	{
		if (H)
		{
			H->ClearInstances();
		}
	}
}

int32 AKGGrassField::GetClumpCount() const
{
	int32 N = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* H : GrassLayers)
	{
		N += H ? H->GetInstanceCount() : 0;
	}
	return N;
}
