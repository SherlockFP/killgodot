#include "World/KGFoliageField.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AKGFoliageField::AKGFoliageField()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AKGFoliageField::AddInstances(UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCollide, float CullEnd)
{
	if (!Mesh || Transforms.Num() == 0)
	{
		return;
	}
	UHierarchicalInstancedStaticMeshComponent* Target = nullptr;
	for (UHierarchicalInstancedStaticMeshComponent* H : Fields)
	{
		if (H && H->GetStaticMesh() == Mesh)
		{
			Target = H;
			break;
		}
	}
	if (!Target)
	{
		// Editor-time component: created with RF_Transactional and registered as an instance component so the
		// level saves it with the actor.
		Target = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
		Target->SetupAttachment(GetRootComponent());
		Target->SetStaticMesh(Mesh);
		Target->SetMobility(EComponentMobility::Static);
		Target->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Target->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Target->SetCanEverAffectNavigation(bCollide);
		Target->SetCullDistances(static_cast<int32>(CullEnd * 0.8f), static_cast<int32>(CullEnd));
		Target->bAffectDistanceFieldLighting = false;
		AddInstanceComponent(Target);
		Target->RegisterComponent();
		Fields.Add(Target);
	}
	// World-space transforms from the builder -> relative to this actor.
	TArray<FTransform> Local;
	Local.Reserve(Transforms.Num());
	for (const FTransform& T : Transforms)
	{
		Local.Add(T.GetRelativeTransform(GetActorTransform()));
	}
	Target->AddInstances(Local, false, false);
}

void AKGFoliageField::ClearAll()
{
	for (UHierarchicalInstancedStaticMeshComponent* H : Fields)
	{
		if (H)
		{
			H->ClearInstances();
		}
	}
}

int32 AKGFoliageField::GetInstanceTotal() const
{
	int32 N = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* H : Fields)
	{
		N += H ? H->GetInstanceCount() : 0;
	}
	return N;
}
