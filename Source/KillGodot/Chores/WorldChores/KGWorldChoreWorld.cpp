#include "Chores/WorldChores/KGWorldChoreWorld.h"

#include "Engine/World.h"   // before KGAudio.h (it uses UWorld)
#include "Audio/KGAudio.h"
#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Chores/WorldChores/KGWorldChoreComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/KGPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Online/KGSnapshotComponent.h"
#include "World/KGChoreItem.h"

namespace KGWorldChoreLook
{
	template <typename T>
	T* Load(const FString& Path)
	{
		return LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UStaticMesh* Shape(const TCHAR* Name)
	{
		return Load<UStaticMesh>(FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	UMaterialInterface* Solid()
	{
		return Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	UMaterialInterface* Glow()
	{
		if (UMaterialInterface* M = Load<UMaterialInterface>(TEXT("/Game/KillGodot/Materials/M_KG_ChoreGlow.M_KG_ChoreGlow")))
		{
			return M;
		}
		return Solid();
	}

	const FLinearColor Wood(0.30f, 0.17f, 0.08f);
	const FLinearColor DarkWood(0.13f, 0.08f, 0.045f);
	const FLinearColor Iron(0.10f, 0.10f, 0.11f);
	const FLinearColor WaterCol(0.10f, 0.42f, 0.75f);
	const FLinearColor PoisonCol(0.36f, 0.64f, 0.07f);
	const FLinearColor FlameCol(1.0f, 0.62f, 0.22f);
	const FLinearColor Gold(1.0f, 0.78f, 0.30f);

	const TCHAR* Props = TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/");
	FString P(const TCHAR* Name) { return FString(Props) + Name + TEXT(".") + Name; }
	FString Interior(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/KillGodot/Env/Furniture/KG_InteriorProps/StaticMeshes/%s.%s"), Name, Name);
	}
	FString Harbour(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/KillGodot/Env/Dress/KG_DressHarbour_Clean/StaticMeshes/%s.%s"), Name, Name);
	}

	/**
	 * The furniture pack's vertex-colour props (M_KG_PropVC: firewood, bread) render plain white in game captures; chore
	 * spots and items give those a flat colour of their own so the logs read as wood and the loaves as bread.
	 */
	void FlattenVertexColour(UStaticMeshComponent* C)
	{
		UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
		UMaterialInterface* M0 = C ? C->GetMaterial(0) : nullptr;
		if (!Mesh || !M0 || !M0->GetPathName().Contains(TEXT("M_KG_PropVC")))
		{
			return;
		}
		const FString Name = Mesh->GetName();
		const FLinearColor Col = Name.Contains(TEXT("Bread"))    ? FLinearColor(0.80f, 0.48f, 0.17f)
		                       : Name.Contains(TEXT("Firewood")) ? FLinearColor(0.42f, 0.24f, 0.10f)
		                                                         : FLinearColor(0.5f, 0.4f, 0.3f);
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Solid(), C);
			MID->SetVectorParameterValue(TEXT("Color"), Col);
			C->SetMaterial(i, MID);
		}
	}

	/** Kinds that take E (the others are pure "bring here" spots and must not block the trace to doors). */
	bool HasHitbox(FName Kind)
	{
		static const TSet<FName> NoE = {TEXT("Door"), TEXT("Market"), TEXT("Posts"), TEXT("Woodbox"), TEXT("Letterbox"), TEXT("FlourBin"),
		                                TEXT("CampWoodpile"), TEXT("TentSite"), TEXT("HerbTable")};   // SPRINT-034a bring targets
		return !NoE.Contains(Kind);
	}

	FVector GroundAt(const UWorld* World, const FVector& Hint, const AActor* Ignore)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(KGWorldChoreGround), false, Ignore);
		const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		if (World && World->LineTraceSingleByObjectType(Hit, Hint + FVector(0.0f, 0.0f, 150.0f), Hint - FVector(0.0f, 0.0f, 450.0f), Objects, Params))
		{
			return Hit.ImpactPoint;
		}
		return Hint;
	}
}

// =====================================================================================================================
// AKGChoreSpot
// =====================================================================================================================

AKGChoreSpot::AKGChoreSpot()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(Root);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hitbox->SetCanEverAffectNavigation(false);
	Hitbox->SetHiddenInGame(true);
}

const FKGWorldAnchor* AKGChoreSpot::GetAnchor() const
{
	const FKGWorldChoreCatalog& C = FKGWorldChoreCatalog::Get();
	return C.Anchors.IsValidIndex(AnchorIndex) ? &C.Anchors[AnchorIndex] : nullptr;
}

UStaticMeshComponent* AKGChoreSpot::Part(UStaticMesh* Mesh, const FVector& Rel, const FRotator& Rot, const FVector& Scale, const FLinearColor& Color, bool bGlow)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(Root);
	C->SetRelativeLocation(Rel);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	if (Mesh->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(bGlow ? KGWorldChoreLook::Glow() : KGWorldChoreLook::Solid(), C);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Intensity"), bGlow ? 5.0f : 0.0f);
		C->SetMaterial(0, MID);
		C->SetCastShadow(!bGlow);
	}
	return C;
}

UStaticMeshComponent* AKGChoreSpot::Asset(const TCHAR* Path, const FVector& Rel, const FRotator& Rot, float Scale)
{
	UStaticMeshComponent* C = Part(KGWorldChoreLook::Load<UStaticMesh>(Path), Rel, Rot, FVector(Scale));
	KGWorldChoreLook::FlattenVertexColour(C);
	return C;
}

void AKGChoreSpot::Setup(int32 InAnchorIndex)
{
	AnchorIndex = InAnchorIndex;
	const FKGWorldAnchor* A = GetAnchor();
	if (!A)
	{
		return;
	}
	SetActorLocationAndRotation(KGWorldChoreLook::GroundAt(GetWorld(), A->Location, this), FRotator(0.0f, A->Yaw, 0.0f));
#if WITH_EDITOR
	SetActorLabel(FString::Printf(TEXT("WorldChore_%s"), *A->Id.ToString()));
#endif
	BuildLook();
}

void AKGChoreSpot::BuildLook()
{
	using namespace KGWorldChoreLook;
	const FKGWorldAnchor* A = GetAnchor();
	const FName K = A->Kind;
	UStaticMesh* Cube = Shape(TEXT("Cube"));
	UStaticMesh* Cyl = Shape(TEXT("Cylinder"));
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	UStaticMesh* Cone = Shape(TEXT("Cone"));
	FVector HalfBox(40.0f, 40.0f, 60.0f);
	float BoxZ = 60.0f;

	if (K == TEXT("Well"))
	{
		// Spare buckets by the kerb: they rattle while someone cranks.
		Jiggle.Add(Asset(*P(TEXT("Bucket_Wooden_1")), FVector(60.0f, 75.0f, 0.0f), FRotator(0.0f, 20.0f, 0.0f)));
		Jiggle.Add(Asset(*P(TEXT("Bucket_Wooden_1")), FVector(95.0f, 40.0f, 0.0f), FRotator(0.0f, -35.0f, 0.0f)));
		JiggleAmp = 5.0f;
		HalfBox = FVector(70.0f, 70.0f, 70.0f);
	}
	else if (K == TEXT("Trough"))
	{
		// A plank trough, 1.6 x 0.6 m: bottom, two long sides, two ends, water inside.
		Part(Cube, FVector(0.0f, 0.0f, 6.0f), FRotator::ZeroRotator, FVector(1.6f, 0.6f, 0.12f), Wood);
		Part(Cube, FVector(0.0f, 27.0f, 25.0f), FRotator::ZeroRotator, FVector(1.6f, 0.06f, 0.45f), Wood);
		Part(Cube, FVector(0.0f, -27.0f, 25.0f), FRotator::ZeroRotator, FVector(1.6f, 0.06f, 0.45f), Wood);
		Part(Cube, FVector(77.0f, 0.0f, 25.0f), FRotator::ZeroRotator, FVector(0.06f, 0.6f, 0.45f), DarkWood);
		Part(Cube, FVector(-77.0f, 0.0f, 25.0f), FRotator::ZeroRotator, FVector(0.06f, 0.6f, 0.45f), DarkWood);
		for (float X : {-60.0f, 60.0f})
		{
			Part(Cube, FVector(X, 0.0f, 25.0f), FRotator::ZeroRotator, FVector(0.08f, 0.64f, 0.5f), Iron);
		}
		Water.Add(Part(Cube, FVector(0.0f, 0.0f, 14.0f), FRotator::ZeroRotator, FVector(1.48f, 0.48f, 0.02f), WaterCol));
		for (int32 i = 0; i < 5; ++i)
		{
			Bubbles.Add(Part(Sphere, FVector(-50.0f + 25.0f * i, (i % 2 ? 8.0f : -9.0f), 30.0f), FRotator::ZeroRotator, FVector(0.07f), PoisonCol, true));
		}
		HalfBox = FVector(85.0f, 35.0f, 30.0f);
		BoxZ = 30.0f;
	}
	else if (K == TEXT("Butt"))
	{
		Asset(*P(TEXT("Barrel")), FVector::ZeroVector);
		Water.Add(Part(Cyl, FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, FVector(0.6f, 0.6f, 0.02f), WaterCol));
		for (int32 i = 0; i < 4; ++i)
		{
			Bubbles.Add(Part(Sphere, FVector(-15.0f + 10.0f * i, (i % 2 ? 7.0f : -8.0f), 88.0f), FRotator::ZeroRotator, FVector(0.06f), PoisonCol, true));
		}
		HalfBox = FVector(40.0f, 40.0f, 50.0f);
		BoxZ = 50.0f;
	}
	else if (K == TEXT("CrateStack"))
	{
		const FString Crate = P(TEXT("Crate_Wooden"));
		Asset(*Crate, FVector(0.0f, 0.0f, 3.0f), FRotator(0.0f, 5.0f, 0.0f), 0.62f);
		Asset(*Crate, FVector(0.0f, 62.0f, 3.0f), FRotator(0.0f, -8.0f, 0.0f), 0.62f);
		Asset(*Crate, FVector(0.0f, 30.0f, 60.0f), FRotator(0.0f, 12.0f, 0.0f), 0.62f);
		Asset(TEXT("/Game/KillGodot/Env/WaterProps/KG_WaterProps/StaticMeshes/SM_KG_Fish_Cod.SM_KG_Fish_Cod"), FVector(0.0f, 30.0f, 118.0f));
		HalfBox = FVector(45.0f, 70.0f, 60.0f);
	}
	else if (K == TEXT("Market"))
	{
		const FString Crate = P(TEXT("Crate_Wooden"));
		for (int32 i = 0; i < 6; ++i)
		{
			Stacked.Add(Asset(*Crate, FVector(-70.0f + 60.0f * (i % 3), (i / 3) * 62.0f - 30.0f, 3.0f + (i >= 3 ? 0.0f : 0.0f)),
			                  FRotator(0.0f, 7.0f * (i - 2), 0.0f), 0.55f));
		}
	}
	else if (K == TEXT("BreadRack"))
	{
		Part(Cube, FVector(0.0f, 0.0f, 35.0f), FRotator::ZeroRotator, FVector(0.8f, 0.5f, 0.7f), Wood);
		Asset(*P(TEXT("FarmCrate_Empty")), FVector(0.0f, 0.0f, 70.0f), FRotator::ZeroRotator, 0.9f);
		for (int32 i = 0; i < 4; ++i)
		{
			Asset(*Interior(TEXT("SM_KG_Bread")), FVector(-21.0f + 14.0f * i, 0.0f, 76.0f), FRotator(0.0f, 90.0f, 0.0f), 0.8f);
		}
		HalfBox = FVector(45.0f, 35.0f, 50.0f);
	}
	else if (K == TEXT("Door"))
	{
		Part(Cube, FVector(0.0f, 0.0f, 1.0f), FRotator::ZeroRotator, FVector(0.5f, 0.35f, 0.02f), DarkWood);   // a doormat
		for (int32 i = 0; i < 3; ++i)
		{
			Stacked.Add(Asset(*Interior(TEXT("SM_KG_Bread")), FVector(-12.0f + 12.0f * i, 8.0f * (i % 2), 3.0f), FRotator(0.0f, 20.0f * i, 0.0f), 0.9f));
		}
	}
	else if (K == TEXT("TaperBox"))
	{
		Part(Cube, FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(0.4f, 0.28f, 0.9f), DarkWood);
		for (int32 i = 0; i < 3; ++i)
		{
			Asset(*P(TEXT("Candle_2")), FVector(-10.0f + 10.0f * i, 0.0f, 90.0f), FRotator::ZeroRotator, 1.2f);
		}
		Flame = Part(Sphere, FVector(0.0f, 0.0f, 122.0f), FRotator::ZeroRotator, FVector(0.05f, 0.05f, 0.08f), FlameCol, true);
		HalfBox = FVector(30.0f, 25.0f, 60.0f);
	}
	else if (K == TEXT("Lamp"))
	{
		Part(Cyl, FVector(0.0f, 0.0f, 150.0f), FRotator::ZeroRotator, FVector(0.13f, 0.13f, 3.0f), DarkWood);
		Part(Cube, FVector(28.0f, 0.0f, 285.0f), FRotator::ZeroRotator, FVector(0.6f, 0.07f, 0.07f), DarkWood);
		Asset(*P(TEXT("Lantern_Wall")), FVector(0.0f, 0.0f, 175.0f), FRotator(0.0f, -90.0f, 0.0f), 1.0f);
		Flame = Part(Sphere, FVector(95.0f, 0.0f, 243.0f), FRotator::ZeroRotator, FVector(0.09f, 0.09f, 0.13f), FlameCol, true);
		Soot = Part(Sphere, FVector(95.0f, 0.0f, 265.0f), FRotator::ZeroRotator, FVector(0.25f), FLinearColor(0.05f, 0.05f, 0.05f));
		Light = NewObject<UPointLightComponent>(this);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetLightColor(FLinearColor(1.0f, 0.66f, 0.32f));
		Light->SetAttenuationRadius(900.0f);
		Light->SetCastShadows(false);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(FVector(95.0f, 0.0f, 235.0f));
		Light->RegisterComponent();
		HalfBox = FVector(30.0f, 30.0f, 120.0f);
		BoxZ = 120.0f;
	}
	else if (K == TEXT("Winder"))
	{
		Part(Cyl, FVector(0.0f, 0.0f, 100.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.9f, 0.9f, 0.06f), Iron);
		Part(Cyl, FVector(0.0f, 0.0f, 100.0f), FRotator(90.0f, 0.0f, 0.0f), FVector(0.14f, 0.14f, 0.3f), Gold);
		Jiggle.Add(Part(Cube, FVector(0.0f, 16.0f, 125.0f), FRotator::ZeroRotator, FVector(0.05f, 0.05f, 0.5f), Wood));   // the crank handle
		JiggleAmp = 10.0f;
		Part(Cube, FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(0.3f, 0.3f, 0.9f), DarkWood);
		HalfBox = FVector(45.0f, 45.0f, 80.0f);
	}
	else if (K == TEXT("Bell"))
	{
		// The rope and its knob bounce and swing every time it is pulled.
		Jiggle.Add(Part(Cyl, FVector(0.0f, 0.0f, 150.0f), FRotator::ZeroRotator, FVector(0.04f, 0.04f, 3.0f), Wood));
		Jiggle.Add(Part(Sphere, FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 0.2f), DarkWood));
		JiggleAmp = 26.0f;
		HalfBox = FVector(40.0f, 40.0f, 90.0f);
		BoxZ = 90.0f;
	}
	else if (K == TEXT("NetPile"))
	{
		Asset(*Harbour(TEXT("SM_KG_NetPile")), FVector::ZeroVector, FRotator::ZeroRotator, 0.8f);
		HalfBox = FVector(55.0f, 55.0f, 35.0f);
		BoxZ = 35.0f;
	}
	else if (K == TEXT("NetRack"))
	{
		HalfBox = FVector(60.0f, 60.0f, 90.0f);
		BoxZ = 90.0f;
	}
	else if (K == TEXT("Posts"))
	{
		for (float Y : {-80.0f, 80.0f})
		{
			Part(Cyl, FVector(0.0f, Y, 90.0f), FRotator::ZeroRotator, FVector(0.15f, 0.15f, 1.8f), DarkWood);
		}
		Part(Cyl, FVector(0.0f, 0.0f, 175.0f), FRotator(0.0f, 0.0f, 90.0f), FVector(0.06f, 0.06f, 1.7f), Wood);
		// The hung nets (one drape per delivery).
		for (int32 i = 0; i < 3; ++i)
		{
			UStaticMeshComponent* Net = Asset(*Harbour(TEXT("SM_KG_NetPile")), FVector(4.0f + 3.0f * i, 0.0f, 172.0f - 8.0f * i), FRotator(0.0f, 90.0f, 85.0f), 1.0f);
			if (Net)
			{
				Net->SetRelativeScale3D(FVector(1.3f, 0.35f, 1.1f));
			}
			Stacked.Add(Net);
		}
	}
	else if (K == TEXT("ChopBlock"))
	{
		Asset(*P(TEXT("Anvil_Log")), FVector::ZeroVector, FRotator::ZeroRotator, 0.75f);
		Jiggle.Add(Asset(*P(TEXT("Axe_Bronze")), FVector(0.0f, 5.0f, 100.0f), FRotator(0.0f, 0.0f, 25.0f), 1.0f));   // hops on every chop
		JiggleAmp = 14.0f;
		for (int32 i = 0; i < 2; ++i)
		{
			Stacked.Add(Asset(*Interior(TEXT("SM_KG_Firewood")), FVector(-70.0f, -40.0f + 60.0f * i, 0.0f), FRotator(0.0f, 30.0f * i, 0.0f), 0.55f));
		}
		HalfBox = FVector(45.0f, 45.0f, 55.0f);
	}
	else if (K == TEXT("Woodbox"))
	{
		Asset(*P(TEXT("FarmCrate_Empty")), FVector::ZeroVector, FRotator::ZeroRotator, 1.3f);
		for (int32 i = 0; i < 3; ++i)
		{
			Stacked.Add(Asset(*Interior(TEXT("SM_KG_Firewood")), FVector(-25.0f + 25.0f * i, 0.0f, 10.0f + 12.0f * (i % 2)), FRotator(0.0f, 40.0f * i, 0.0f), 0.8f));
		}
	}
	else if (K == TEXT("Satchel"))
	{
		Part(Cube, FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, FVector(0.5f, 0.35f, 0.8f), Wood);
		Asset(*P(TEXT("Bag")), FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, 0.35f);
		for (int32 i = 0; i < 3; ++i)
		{
			Asset(*P(TEXT("Scroll_1")), FVector(-8.0f + 8.0f * i, 12.0f, 82.0f + 2.0f * i), FRotator(0.0f, 80.0f + 10.0f * i, 0.0f), 1.0f);
		}
		HalfBox = FVector(35.0f, 30.0f, 55.0f);
	}
	else if (K == TEXT("Letterbox"))
	{
		Part(Cyl, FVector(0.0f, 0.0f, 55.0f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 1.1f), DarkWood);
		Part(Cube, FVector(0.0f, 0.0f, 122.0f), FRotator::ZeroRotator, FVector(0.3f, 0.24f, 0.26f), FLinearColor(0.55f, 0.08f, 0.05f));
		Part(Cube, FVector(15.5f, 0.0f, 128.0f), FRotator::ZeroRotator, FVector(0.01f, 0.16f, 0.03f), Iron);   // the slot
		Flag = Part(Cube, FVector(0.0f, 14.0f, 140.0f), FRotator::ZeroRotator, FVector(0.02f, 0.02f, 0.3f), FLinearColor(0.95f, 0.8f, 0.2f));
		Plate = NewObject<UTextRenderComponent>(this);
		Plate->SetupAttachment(Root);
		Plate->SetRelativeLocation(FVector(15.8f, 0.0f, 112.0f));
		Plate->SetText(FText::FromString(A->Name));
		Plate->SetHorizontalAlignment(EHTA_Center);
		Plate->SetVerticalAlignment(EVRTA_TextCenter);
		Plate->SetWorldSize(4.2f);
		Plate->SetTextRenderColor(FColor(250, 236, 200));
		Plate->RegisterComponent();
	}
	else if (K == TEXT("Sacks"))
	{
		Asset(*P(TEXT("Bag")), FVector(0.0f, -35.0f, 0.0f), FRotator(0.0f, 10.0f, 0.0f), 0.7f);
		Asset(*P(TEXT("Bag")), FVector(5.0f, 20.0f, 0.0f), FRotator(0.0f, -20.0f, 4.0f), 0.7f);
		Asset(*P(TEXT("Bag")), FVector(-35.0f, -5.0f, 0.0f), FRotator(0.0f, 60.0f, 0.0f), 0.7f);
		HalfBox = FVector(50.0f, 60.0f, 40.0f);
	}
	else if (K == TEXT("Hopper"))
	{
		Asset(*P(TEXT("Barrel")), FVector::ZeroVector, FRotator::ZeroRotator, 1.0f);
		Part(Cone, FVector(0.0f, 0.0f, 120.0f), FRotator(180.0f, 0.0f, 0.0f), FVector(0.95f, 0.95f, 0.6f), Wood);
		Water.Add(Part(Cyl, FVector(0.0f, 0.0f, 132.0f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.02f), FLinearColor(0.85f, 0.7f, 0.35f)));
		HalfBox = FVector(50.0f, 50.0f, 80.0f);
	}
	else if (K == TEXT("FlourBin"))
	{
		Asset(*P(TEXT("Barrel")), FVector::ZeroVector, FRotator::ZeroRotator, 0.9f);
		for (int32 i = 0; i < 3; ++i)
		{
			UStaticMeshComponent* Sack = Asset(*P(TEXT("Bag")), FVector(55.0f, -35.0f + 35.0f * i, 0.0f), FRotator(0.0f, 25.0f * i, 0.0f), 0.5f);
			if (Sack)
			{
				Sack->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0.95f, 0.93f, 0.86f));
			}
			Stacked.Add(Sack);
		}
	}
	else if (K == TEXT("OilShelf"))
	{
		Asset(*P(TEXT("Barrel")), FVector(0.0f, 0.0f, 0.0f), FRotator::ZeroRotator, 0.9f);
		for (int32 i = 0; i < 3; ++i)
		{
			UStaticMeshComponent* Can = Asset(*P(TEXT("Bottle_1")), FVector(-15.0f + 15.0f * i, 45.0f, 0.0f), FRotator::ZeroRotator, 1.0f);
			if (Can)
			{
				Can->SetRelativeScale3D(FVector(2.2f, 2.2f, 1.4f));
			}
		}
		HalfBox = FVector(45.0f, 50.0f, 50.0f);
	}
	else if (K == TEXT("LighthouseLamp"))
	{
		Part(Cyl, FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator, FVector(0.5f, 0.5f, 1.0f), Iron);
		Flame = Part(Sphere, FVector(0.0f, 0.0f, 125.0f), FRotator::ZeroRotator, FVector(0.45f), FlameCol, true);
		Soot = Part(Sphere, FVector(0.0f, 0.0f, 150.0f), FRotator::ZeroRotator, FVector(0.5f), FLinearColor(0.05f, 0.05f, 0.05f));
		Light = NewObject<UPointLightComponent>(this);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetLightColor(FLinearColor(1.0f, 0.8f, 0.45f));
		Light->SetAttenuationRadius(2500.0f);
		Light->SetCastShadows(false);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(FVector(0.0f, 0.0f, 140.0f));
		Light->RegisterComponent();
		HalfBox = FVector(45.0f, 45.0f, 80.0f);
	}
	// ---- SPRINT-034a forest chores (Tools/Level/gen_forest_bands.py) ------------------------------------------------
	else if (K == TEXT("Deadwood"))
	{
		const TCHAR* Log = TEXT("/Game/KillGodot/Env/Ext/KG_Ext_KNature/StaticMeshes/SM_KG_KNature_log.SM_KG_KNature_log");
		Jiggle.Add(Asset(Log, FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 20.0f, 0.0f), 0.55f));
		Jiggle.Add(Asset(Log, FVector(10.0f, 25.0f, 12.0f), FRotator(8.0f, -35.0f, 0.0f), 0.45f));
		Jiggle.Add(Asset(Log, FVector(-15.0f, -20.0f, 8.0f), FRotator(-6.0f, 75.0f, 0.0f), 0.4f));
		for (int32 i = 0; i < 5; ++i)
		{
			Part(Cyl, FVector(-30.0f + 15.0f * i, (i % 2) ? 18.0f : -14.0f, 12.0f + 4.0f * (i % 3)), FRotator(80.0f, 30.0f * i, 0.0f),
			     FVector(0.04f, 0.04f, 0.9f), DarkWood);   // snapped dry sticks
		}
		JiggleAmp = 6.0f;
		HalfBox = FVector(55.0f, 55.0f, 35.0f);
		BoxZ = 35.0f;
	}
	else if (K == TEXT("Herbs"))
	{
		Jiggle.Add(Asset(TEXT("/Game/KillGodot/Env/KG_Nature/StaticMeshes/Flower_3_Group.Flower_3_Group"), FVector::ZeroVector, FRotator::ZeroRotator, 1.1f));
		Jiggle.Add(Asset(TEXT("/Game/KillGodot/Env/KG_Nature/StaticMeshes/Plant_7.Plant_7"), FVector(25.0f, 10.0f, 0.0f), FRotator(0.0f, 40.0f, 0.0f), 0.8f));
		Jiggle.Add(Asset(TEXT("/Game/KillGodot/Env/KG_Nature/StaticMeshes/Flower_3_Group.Flower_3_Group"), FVector(-20.0f, -15.0f, 0.0f), FRotator(0.0f, 110.0f, 0.0f), 0.9f));
		JiggleAmp = 4.0f;
		HalfBox = FVector(45.0f, 45.0f, 30.0f);
		BoxZ = 30.0f;
	}
	else if (K == TEXT("CampWoodpile"))
	{
		Asset(TEXT("/Game/KillGodot/Env/Ext/KG_Ext_KNature/StaticMeshes/SM_KG_KNature_log.SM_KG_KNature_log"), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), 0.8f);
		for (int32 i = 0; i < 6; ++i)
		{
			Stacked.Add(Asset(*Interior(TEXT("SM_KG_Firewood")), FVector(-40.0f + 40.0f * (i % 3), 30.0f, 15.0f + 28.0f * (i / 3)), FRotator(0.0f, 10.0f * i, 0.0f), 0.75f));
		}
	}
	else if (K == TEXT("TentSite"))
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const float Ang = i * 60.0f;
			Part(Sphere, FVector(150.0f * FMath::Cos(FMath::DegreesToRadians(Ang)), 150.0f * FMath::Sin(FMath::DegreesToRadians(Ang)), 4.0f),
			     FRotator::ZeroRotator, FVector(0.25f, 0.25f, 0.12f), FLinearColor(0.45f, 0.44f, 0.42f));   // a ring of stones marks the pitch
		}
		Stacked.Add(Asset(TEXT("/Game/KillGodot/Env/Dress/KG_DressWilds_Clean/StaticMeshes/SM_KG_Tent_A.SM_KG_Tent_A"), FVector::ZeroVector, FRotator(0.0f, 180.0f, 0.0f), 1.0f));
	}
	else if (K == TEXT("TentPeg"))
	{
		Part(Cyl, FVector(0.0f, 0.0f, 6.0f), FRotator::ZeroRotator, FVector(0.05f, 0.05f, 0.12f), Wood);   // the marked spot
		Stacked.Add(Part(Cyl, FVector(0.0f, 0.0f, 20.0f), FRotator(12.0f, 0.0f, 0.0f), FVector(0.06f, 0.06f, 0.4f), DarkWood));
		Jiggle.Add(Part(Cube, FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(0.14f, 0.08f, 0.08f), Iron));   // mallet head
		JiggleAmp = 10.0f;
		HalfBox = FVector(35.0f, 35.0f, 40.0f);
		BoxZ = 40.0f;
	}
	else if (K == TEXT("CanvasPile"))
	{
		Asset(*P(TEXT("Crate_Wooden")), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 10.0f, 0.0f), 0.6f);
		UStaticMeshComponent* Roll = Asset(*P(TEXT("Bag")), FVector(0.0f, 0.0f, 62.0f), FRotator(0.0f, 0.0f, 90.0f), 1.0f);
		if (Roll)
		{
			Roll->SetRelativeScale3D(FVector(1.6f, 0.9f, 0.6f));
		}
		HalfBox = FVector(50.0f, 50.0f, 50.0f);
	}
	else if (K == TEXT("HerbTable"))
	{
		Part(Cube, FVector(0.0f, 0.0f, 78.0f), FRotator::ZeroRotator, FVector(1.2f, 0.6f, 0.06f), Wood);
		for (const FVector2D L : {FVector2D(-52.0f, -24.0f), FVector2D(52.0f, -24.0f), FVector2D(-52.0f, 24.0f), FVector2D(52.0f, 24.0f)})
		{
			Part(Cube, FVector(L.X, L.Y, 38.0f), FRotator::ZeroRotator, FVector(0.06f, 0.06f, 0.76f), DarkWood);
		}
		for (int32 i = 0; i < 3; ++i)
		{
			Stacked.Add(Asset(TEXT("/Game/KillGodot/Env/KG_Nature/StaticMeshes/Flower_3_Group.Flower_3_Group"), FVector(-40.0f + 40.0f * i, 0.0f, 82.0f), FRotator(0.0f, 50.0f * i, 0.0f), 0.45f));
		}
	}
	Jiggle.RemoveAll([](const TObjectPtr<UStaticMeshComponent>& C) { return C == nullptr; });
	for (const UStaticMeshComponent* C : Jiggle)
	{
		JiggleBase.Add(C->GetRelativeLocation());
	}
	// Burst sparkle pieces (hidden until a step completes here).
	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* S = Part(Sphere, FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, FVector(0.06f), Gold, true);
		if (S)
		{
			S->SetVisibility(false);
			S->SetCastShadow(false);
			Burst.Add(S);
		}
	}
	if (HasHitbox(K))
	{
		Hitbox->SetBoxExtent(HalfBox);
		Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, BoxZ));
	}
	else
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ApplyState(FKGSpotState(), 0.0f);
}

void AKGChoreSpot::ApplyState(const FKGSpotState& S, float DeltaSeconds)
{
	const FKGWorldAnchor* A = GetAnchor();
	if (!A)
	{
		return;
	}
	const FName K = A->Kind;
	for (UStaticMeshComponent* W : Water)
	{
		if (!W)
		{
			continue;
		}
		const float Level = FMath::Clamp(S.Level, 0.0f, 1.0f);
		W->SetVisibility(K == TEXT("Hopper") ? S.Count > 0 : Level > 0.01f);
		if (K == TEXT("Trough"))
		{
			W->SetRelativeLocation(FVector(0.0f, 0.0f, 14.0f + 32.0f * Level + 0.6f * FMath::Sin(Clock * 2.0f)));
		}
		else if (K == TEXT("Butt"))
		{
			W->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f + 46.0f * Level));
		}
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(W->GetMaterial(0)))
		{
			MID->SetVectorParameterValue(TEXT("Color"), S.bSpoiled ? KGWorldChoreLook::PoisonCol : KGWorldChoreLook::WaterCol);
		}
	}
	for (int32 i = 0; i < Bubbles.Num(); ++i)
	{
		if (UStaticMeshComponent* B = Bubbles[i])
		{
			const bool bShow = S.bSpoiled && S.Level > 0.01f;
			B->SetVisibility(bShow);
			if (bShow)
			{
				const float Phase = FMath::Frac(Clock * 0.7f + i * 0.23f);
				const FVector Rel = B->GetRelativeLocation();
				const float Base = K == TEXT("Trough") ? 14.0f + 32.0f * S.Level : 40.0f + 46.0f * S.Level;
				B->SetRelativeLocation(FVector(Rel.X, Rel.Y, Base + 6.0f * Phase));
				B->SetRelativeScale3D(FVector(0.04f + 0.05f * Phase));
			}
		}
	}
	// Delivered things: Count of them (split logs on the block cycle 0..2 as the bundle pops out every third chop).
	const int32 NumShown = K == TEXT("ChopBlock") ? S.Count % 3 : S.Count;
	for (int32 i = 0; i < Stacked.Num(); ++i)
	{
		if (Stacked[i])
		{
			Stacked[i]->SetVisibility(i < NumShown);
		}
	}
	if (Flag)
	{
		Flag->SetRelativeRotation(FRotator(S.Count > 0 ? 0.0f : 90.0f, 0.0f, 0.0f));   // up = mail in the box
	}
	const bool bLampKind = K == TEXT("Lamp") || K == TEXT("LighthouseLamp");
	if (Flame)
	{
		const bool bOn = bLampKind ? S.bLit : true;
		Flame->SetVisibility(bOn);
		if (bOn)
		{
			const float F = 1.0f + 0.15f * FMath::Sin(Clock * 19.0f) + 0.08f * FMath::Sin(Clock * 31.0f);
			const FVector Base = K == TEXT("LighthouseLamp") ? FVector(0.45f) : K == TEXT("Lamp") ? FVector(0.09f, 0.09f, 0.13f) : FVector(0.05f, 0.05f, 0.08f);
			Flame->SetRelativeScale3D(Base * F);
		}
	}
	if (Soot)
	{
		Soot->SetVisibility(S.bSpoiled && !S.bLit);
	}
	if (Light)
	{
		const float Base = K == TEXT("LighthouseLamp") ? 900.0f : 60.0f;
		Light->SetIntensity(S.bLit ? Base * (1.0f + 0.1f * FMath::Sin(Clock * 17.0f)) : 0.0f);
	}
	// Cue bounce: the bell rope jumps and swings, the axe hops, the crank handle kicks, the spare buckets rattle.
	JiggleAge += DeltaSeconds;
	if (JiggleAge < 1.4f || DeltaSeconds == 0.0f)
	{
		const float Decay = FMath::Exp(-3.5f * JiggleAge);
		const float Hop = JiggleAge < 1.4f ? JiggleAmp * FMath::Sin(JiggleAge * 26.0f) * Decay : 0.0f;
		const float Swing = JiggleAge < 1.4f ? 12.0f * FMath::Sin(JiggleAge * 11.0f) * Decay : 0.0f;
		for (int32 i = 0; i < Jiggle.Num() && i < JiggleBase.Num(); ++i)
		{
			if (UStaticMeshComponent* J = Jiggle[i])
			{
				J->SetRelativeLocation(JiggleBase[i] + FVector(K == TEXT("Bell") ? Swing * 0.6f : 0.0f, 0.0f, FMath::Abs(Hop)));
			}
		}
	}
	// Step-done sparkle: eight gold sparks fly up and out, then fade.
	if (BurstAge < 1.2f)
	{
		for (int32 i = 0; i < Burst.Num(); ++i)
		{
			if (UStaticMeshComponent* B = Burst[i])
			{
				const float Ang = i * UE_TWO_PI / Burst.Num();
				const float T = BurstAge / 1.2f;
				B->SetVisibility(true);
				B->SetRelativeLocation(FVector(FMath::Cos(Ang) * 90.0f * T, FMath::Sin(Ang) * 90.0f * T, 80.0f + 140.0f * T - 120.0f * T * T));
				B->SetRelativeScale3D(FVector(0.07f * (1.0f - T) + 0.01f));
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(B->GetMaterial(0)))
				{
					MID->SetVectorParameterValue(TEXT("Color"), BurstColor);
				}
			}
		}
	}
	else
	{
		for (UStaticMeshComponent* B : Burst)
		{
			if (B && B->IsVisible())
			{
				B->SetVisibility(false);
			}
		}
	}
}

void AKGChoreSpot::PlayBurst(const FLinearColor& Color)
{
	BurstAge = 0.0f;
	BurstColor = Color;
}

void AKGChoreSpot::PlayCue(EKGWorldCue Cue)
{
	if (Cue != EKGWorldCue::None && Jiggle.Num() > 0)
	{
		JiggleAge = 0.0f;
	}
}

void AKGChoreSpot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Clock += DeltaSeconds;
	BurstAge += DeltaSeconds;
	const AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(GetWorld());
	const FKGSpotState* S = Dir ? Dir->GetSpot(AnchorIndex) : nullptr;
	const FKGSpotState State = S ? *S : FKGSpotState();
	if (!bShownValid && S && !HasAuthority())
	{
		UE_LOG(LogKillGodot, Verbose, TEXT("KG_WORLDCHORE_SPOT %d first state"), AnchorIndex);
	}
	if (S && GetNetMode() == NM_Client && (!bShownValid || State.Level != Shown.Level || State.Count != Shown.Count || State.bLit != Shown.bLit ||
	                                       State.bSpoiled != Shown.bSpoiled))
	{
		if (bShownValid)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SPOT %s level=%.2f count=%d lit=%d spoiled=%d authority=0"), *GetAnchor()->Id.ToString(),
			       State.Level, State.Count, State.bLit ? 1 : 0, State.bSpoiled ? 1 : 0);
		}
		Shown = State;
		bShownValid = true;
	}
	ApplyState(State, DeltaSeconds);
}

void AKGChoreSpot::Interact_Implementation(AKGCharacter* By)
{
	if (!By || !HasAuthority())
	{
		return;
	}
	if (UKGWorldChoreComponent* WC = UKGWorldChoreComponent::FindFor(By))
	{
		WC->AuthInteract(AnchorIndex);
	}
}

FText AKGChoreSpot::GetInteractPrompt_Implementation() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UKGWorldChoreComponent* WC = PC ? UKGWorldChoreComponent::FindFor(PC->GetPawn()) : nullptr;
	return WC ? WC->PromptFor(AnchorIndex) : FText::GetEmpty();
}

// =====================================================================================================================
// AKGWorldChoreDirector
// =====================================================================================================================

AKGWorldChoreDirector::AKGWorldChoreDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(8.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CreateDefaultSubobject<UKGSnapshotComponent>(TEXT("Snapshot"));
}

void AKGWorldChoreDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKGWorldChoreDirector, Spots);
}

AKGWorldChoreDirector* AKGWorldChoreDirector::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AKGWorldChoreDirector> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

AKGWorldChoreDirector* AKGWorldChoreDirector::AuthCreate(UWorld* World)
{
	if (AKGWorldChoreDirector* Existing = Get(World))
	{
		return Existing;
	}
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Name = TEXT("KGWorldChoreDirector");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	AKGWorldChoreDirector* Dir = World->SpawnActor<AKGWorldChoreDirector>(AKGWorldChoreDirector::StaticClass(), FTransform::Identity, Params);
	if (Dir)
	{
		Dir->Spots.SetNum(FKGWorldChoreCatalog::Get().Anchors.Num());
	}
	return Dir;
}

FKGSpotState* AKGWorldChoreDirector::AuthMutableSpot(int32 Index)
{
	if (!HasAuthority())
	{
		return nullptr;
	}
	if (Spots.Num() < FKGWorldChoreCatalog::Get().Anchors.Num())
	{
		Spots.SetNum(FKGWorldChoreCatalog::Get().Anchors.Num());
	}
	return Spots.IsValidIndex(Index) ? &Spots[Index] : nullptr;
}

void AKGWorldChoreDirector::OnRep_Spots()
{
	++Serial;
}

void AKGWorldChoreDirector::AuthCue(EKGWorldCue Cue, int32 SpotIndex, const FVector& Where, float Volume)
{
	if (!HasAuthority() || Cue == EKGWorldCue::None)
	{
		return;
	}
	MulticastCue(static_cast<uint8>(Cue), static_cast<int16>(SpotIndex), Where, Volume);
}

void AKGWorldChoreDirector::MulticastCue_Implementation(uint8 Cue, int16 SpotIndex, FVector_NetQuantize Where, float Volume)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const EKGWorldCue C = static_cast<EKGWorldCue>(Cue);
	if (const TCHAR* Sound = KGWorldChores::CueSound(C))
	{
		// StepDone: a bright chime for everyone near (the public HUD tick of a world chore).
		KGAudio::At(this, Sound, Where + FVector(0.0f, 0.0f, 80.0f), C == EKGWorldCue::StepDone ? 0.55f : Volume);
	}
	if (const UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld()))
	{
		if (AKGChoreSpot* Spot = Sub->GetSpot(SpotIndex))
		{
			Spot->PlayCue(C);
		}
	}
	if (C == EKGWorldCue::StepDone || C == EKGWorldCue::Poison || C == EKGWorldCue::Snuff)
	{
		if (const UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(GetWorld()))
		{
			if (AKGChoreSpot* Spot = Sub->GetSpot(SpotIndex))
			{
				Spot->PlayBurst(C == EKGWorldCue::Poison ? KGWorldChoreLook::PoisonCol
				                : C == EKGWorldCue::Snuff ? FLinearColor(0.2f, 0.2f, 0.2f) : KGWorldChoreLook::Gold);
			}
		}
	}
}

// =====================================================================================================================
// AKGWorldChoreStation
// =====================================================================================================================

AKGWorldChoreStation::AKGWorldChoreStation()
{
	Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // the spots take E
	WorkSeconds = 30.0f;   // last-resort hold-E fallback only (bots walk the real steps, Chores/WorldChores)
	WorkRadius = 400.0f;
}

void AKGWorldChoreStation::Interact_Implementation(AKGCharacter* By)
{
	// No E here: the chore's spots take it.
}

void AKGWorldChoreStation::Tick(float DeltaSeconds)
{
	AActor::Tick(DeltaSeconds);   // not AKGTaskStation::Tick: the marker follows the current step instead
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const bool bWanted = IsWantedByLocalPlayer();
	bool bShow = false;
	if (bWanted)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const UKGWorldChoreComponent* WC = PC ? UKGWorldChoreComponent::FindFor(PC->GetPawn()) : nullptr;
		TArray<FKGWorldWaypoint> Points;
		if (WC)
		{
			WC->GetWaypoints(Points);
		}
		for (const FKGWorldWaypoint& W : Points)
		{
			if (W.Chore == TaskId)
			{
				MarkerTime += DeltaSeconds;
				Marker->SetWorldLocation(W.Location + FVector(0.0f, 0.0f, (W.bItem ? 120.0f : 250.0f) + 18.0f * FMath::Sin(MarkerTime * 2.4f)));
				Marker->SetRelativeRotation(FRotator(180.0f, MarkerTime * 60.0f, 0.0f));
				bShow = true;
				break;
			}
		}
	}
	Marker->SetVisibility(bShow);
}

// =====================================================================================================================
// UKGWorldChoreSubsystem
// =====================================================================================================================

bool UKGWorldChoreSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UKGWorldChoreSubsystem* UKGWorldChoreSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGWorldChoreSubsystem>() : nullptr;
}

void UKGWorldChoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UKGWorldChoreSubsystem::HandleActorSpawned));
	FString Smoke;
	if (FParse::Value(FCommandLine::Get(), TEXT("-KGWorldChoreSmoke="), Smoke))
	{
		SmokeChore = FName(*Smoke);
	}
}

void UKGWorldChoreSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Pending.Reset();
	Spots.Reset();
	Super::Deinitialize();
}

TStatId UKGWorldChoreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGWorldChoreSubsystem, STATGROUP_Tickables);
}

void UKGWorldChoreSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SetupWorld(false);
}

void UKGWorldChoreSubsystem::SetupWorld(bool bForce)
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		FKGWorldChoreCatalog::SelectForWorld(World);   // one catalog per map (Morrowmere v2, Storm Manor)
	}
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	if (bSetUp || !World || (!bForce && !Cat.ForMap(World)) || Cat.Anchors.Num() == 0)
	{
		return;
	}
	bSetUp = true;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Spots.SetNum(Cat.Anchors.Num());
	for (int32 i = 0; i < Cat.Anchors.Num(); ++i)
	{
		AKGChoreSpot* Spot = World->SpawnActor<AKGChoreSpot>(AKGChoreSpot::StaticClass(), FTransform(Cat.Anchors[i].Location), Params);
		if (Spot)
		{
			Spot->Setup(i);
			Spots[i] = Spot;
		}
	}
	for (const FKGWorldChoreDef& Def : Cat.Chores)
	{
		const TArray<FName> First = Def.Targets(0, 0);
		const int32 Index = First.Num() > 0 ? Cat.AnchorIndex(First[0]) : INDEX_NONE;
		const FVector At = Index != INDEX_NONE ? StandLocation(Index) : Cat.Hub;
		if (AKGWorldChoreStation* Station = World->SpawnActor<AKGWorldChoreStation>(AKGWorldChoreStation::StaticClass(), FTransform(At), Params))
		{
			Station->TaskId = Def.Id;
			Station->TaskName = Def.Title;
		}
	}
	if (World->GetNetMode() != NM_Client)
	{
		AKGWorldChoreDirector::AuthCreate(World);
	}
	// Panel stations often stand right where a world chore spot is worked from (DrawWater on the well kerb, MendNets at the
	// mending rack, ChopWood by the block): their invisible E cylinder would swallow the E meant for the spot. Slide that
	// hitbox 1.6 m further away from the spot; the panel chore stays usable for whoever is dealt it.
	int32 Yielded = 0;
	for (TActorIterator<AKGTaskStation> It(World); It; ++It)
	{
		if (It->IsA<AKGWorldChoreStation>())
		{
			continue;
		}
		UPrimitiveComponent* Box = nullptr;
		for (UActorComponent* C : It->GetComponents())
		{
			if (C && C->GetFName() == TEXT("Hitbox"))
			{
				Box = Cast<UPrimitiveComponent>(C);
			}
		}
		if (!Box)
		{
			continue;
		}
		for (int32 i = 0; i < Cat.Anchors.Num(); ++i)
		{
			if (!KGWorldChoreLook::HasHitbox(Cat.Anchors[i].Kind))
			{
				continue;
			}
			const FVector SpotAt = SpotLocation(i);
			const FVector StandAt = StandLocation(i);
			const FVector From = Box->GetComponentLocation();
			const double Near = FMath::Min(FVector::Dist2D(From, SpotAt), FVector::Dist2D(From, StandAt));
			if (Near > 250.0 || FMath::Abs(From.Z - SpotAt.Z) > 400.0)
			{
				continue;
			}
			FVector Away = (From - SpotAt).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = It->GetActorForwardVector().GetSafeNormal2D();
			}
			Box->SetMobility(EComponentMobility::Movable);
			Box->SetWorldLocation(From + Away * 160.0f);
			++Yielded;
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_YIELD panel station %s moved its E box off %s"), *It->TaskId.ToString(),
			       *Cat.Anchors[i].Id.ToString());
			break;
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SETUP %s: %d spots, %d chores, authority=%d, %d panel stations yielded"), *World->GetMapName(),
	       Cat.Anchors.Num(), Cat.Chores.Num(), World->GetNetMode() != NM_Client ? 1 : 0, Yielded);
}

AKGChoreSpot* UKGWorldChoreSubsystem::GetSpot(int32 AnchorIndex) const
{
	return Spots.IsValidIndex(AnchorIndex) ? Spots[AnchorIndex].Get() : nullptr;
}

FVector UKGWorldChoreSubsystem::SpotLocation(int32 AnchorIndex) const
{
	if (const AKGChoreSpot* Spot = GetSpot(AnchorIndex))
	{
		return Spot->GetActorLocation();
	}
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	return Cat.Anchors.IsValidIndex(AnchorIndex) ? Cat.Anchors[AnchorIndex].Location : FVector::ZeroVector;
}

FVector UKGWorldChoreSubsystem::StandLocation(int32 AnchorIndex) const
{
	const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
	if (!Cat.Anchors.IsValidIndex(AnchorIndex))
	{
		return FVector::ZeroVector;
	}
	const FKGWorldAnchor& A = Cat.Anchors[AnchorIndex];
	if (A.Stand.Equals(A.Location, 1.0))
	{
		return SpotLocation(AnchorIndex);
	}
	return KGWorldChoreLook::GroundAt(GetWorld(), A.Stand, nullptr);
}

void UKGWorldChoreSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AKGCharacter* Character = Cast<AKGCharacter>(Actor))
	{
		Pending.Add(Character);
	}
}

void UKGWorldChoreSubsystem::EnsureComponent(AKGCharacter* Character)
{
	if (!IsValid(Character) || !Character->HasAuthority() || Character->IsActorBeingDestroyed() ||
	    Character->FindComponentByClass<UKGWorldChoreComponent>())
	{
		return;
	}
	UKGWorldChoreComponent* WC = NewObject<UKGWorldChoreComponent>(Character, TEXT("KGWorldChores"));
	WC->SetIsReplicated(true);
	Character->AddInstanceComponent(WC);
	WC->RegisterComponent();
}

void UKGWorldChoreSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !bSetUp)
	{
		return;
	}
	if (World->GetNetMode() != NM_Client)
	{
		for (const TWeakObjectPtr<AKGCharacter>& C : Pending)
		{
			EnsureComponent(C.Get());
		}
		Pending.Reset();
		SweepSeconds += DeltaTime;
		if (SweepSeconds >= 1.0f)
		{
			SweepSeconds = 0.0f;
			for (TActorIterator<AKGCharacter> It(World); It; ++It)
			{
				EnsureComponent(*It);
			}
		}
	}
	if (!SmokeChore.IsNone())
	{
		TickSmoke(DeltaTime);
	}
}

// ---- -KGWorldChoreSmoke=<Chore>: a client plays one world chore end to end like a person would ----------------------

void UKGWorldChoreSubsystem::TickSmoke(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AKGCharacter* Char = PC ? Cast<AKGCharacter>(PC->GetPawn()) : nullptr;
	UKGWorldChoreComponent* WC = UKGWorldChoreComponent::FindFor(Char);
	const AKGPlayerState* PS = PC ? PC->GetPlayerState<AKGPlayerState>() : nullptr;
	SmokeClock += DeltaTime;
	SmokeStateClock += DeltaTime;
	if (!Char || !WC || !PS || SmokeState >= 100)
	{
		return;
	}
	auto Go = [this](int32 Next, const TCHAR* What)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE state %d -> %d: %s (t=%.1fs)"), SmokeState, Next, What, SmokeClock);
		SmokeState = Next;
		SmokeStateClock = 0.0f;
	};
	auto Fail = [this, &Go](const TCHAR* Why)
	{
		UE_LOG(LogKillGodot, Warning, TEXT("KG_WC_SMOKE_FAIL %s"), Why);
		Go(100, Why);
	};
	auto AimAt = [Char, PC](const FVector& Target)
	{
		const FVector Eye = Char->GetFirstPersonCamera()->GetComponentLocation();
		PC->SetControlRotation((Target - Eye).Rotation());
	};
	const FKGWorldProgress* P = WC->FindProgress(SmokeChore);
	const FKGWorldChoreDef* Def = FKGWorldChoreCatalog::Get().FindChore(SmokeChore);
	const int32 Step = P ? P->Step : -1;
	auto NextSpot = [this, WC, P, Def]() -> FVector
	{
		if (!P || !Def)
		{
			return FVector::ZeroVector;
		}
		const TArray<FName> T = Def->Targets(P->Step, P->Variant);
		return T.Num() > 0 ? SpotLocation(FKGWorldChoreCatalog::Get().AnchorIndex(T[0])) : FVector::ZeroVector;
	};
	switch (SmokeState)
	{
	case 0:
		if (SmokeStateClock > 6.0f)
		{
			WC->ServerDev(FString::Printf(TEXT("Give %s 0"), *SmokeChore.ToString()));
			Go(1, TEXT("asked the host for the chore"));
		}
		break;
	case 1:
		if (P && PS->HasOpenTask(SmokeChore))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE given %s variant=%d (replicated to the client)"), *SmokeChore.ToString(), P->Variant);
			WC->ServerDev(TEXT("Path next"));
			Go(2, TEXT("walking to the first spot"));
		}
		else if (SmokeStateClock > 20.0f)
		{
			Fail(TEXT("chore never arrived"));
		}
		break;
	case 2:   // walk, then E on the spot (take)
		if (SmokeStateClock > 3.0f && !WC->IsAutopilotActive())
		{
			AimAt(NextSpot() + FVector(0.0f, 0.0f, 40.0f));
			Char->Interact();
			Go(3, TEXT("pressed E at the spot"));
		}
		else if (SmokeStateClock > 90.0f)
		{
			Fail(TEXT("walk to the first spot timed out"));
		}
		break;
	case 3:   // the item popped out: pick it up (hold E)
		if (Step >= 1 && P && IsValid(P->Item))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE item %s replicated to the client at %s"), *P->Item->GetKind().ToString(),
			       *P->Item->GetActorLocation().ToCompactString());
			if (SmokeStateClock > 1.5f)
			{
				AimAt(P->Item->GetActorLocation());
				Char->Interact();
				Go(4, TEXT("grabbing the item"));
			}
		}
		else if (SmokeStateClock > 6.0f && SmokeRegrabs < 3)
		{
			++SmokeRegrabs;
			AimAt(NextSpot() + FVector(0.0f, 0.0f, 40.0f));
			Char->Interact();
			SmokeStateClock = 0.0f;
		}
		else if (SmokeStateClock > 20.0f)
		{
			Fail(TEXT("the take step never completed"));
		}
		break;
	case 4:
		if (Char->bHoldingObject)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE carrying (hold E) step=%d"), Step + 1);
			SmokePathStep = -1;
			Go(5, TEXT("carrying to the next spot"));
		}
		else if (SmokeStateClock > 3.0f && P && IsValid(P->Item) && SmokeRegrabs < 8)
		{
			++SmokeRegrabs;
			AimAt(P->Item->GetActorLocation());
			{
				// Diagnostics: what does the E trace see from here?
				FHitResult Hit;
				const FVector Eye = Char->GetPawnViewLocation();
				const bool bHit = Char->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + (P->Item->GetActorLocation() - Eye).GetSafeNormal() * 260.0f,
				                                                              ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(KGSmokeGrab), false, Char));
				UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE regrab %d: me=%s item=%s dist=%.0f hit=%s/%s"), SmokeRegrabs,
				       *Char->GetActorLocation().ToCompactString(), *P->Item->GetActorLocation().ToCompactString(),
				       FVector::Dist(Eye, P->Item->GetActorLocation()), bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("-"),
				       bHit && Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("-"));
			}
			Char->Interact();
			SmokeStateClock = 0.0f;
		}
		else if (SmokeStateClock > 25.0f)
		{
			Fail(TEXT("could not pick the item up"));
		}
		break;
	case 5:   // carrying: the bring steps complete on their own when the item is inside the spot
		if (!PS->HasOpenTask(SmokeChore))
		{
			Go(6, TEXT("ticked off the client list; waiting for the spot to show it"));
			break;
		}
		if (!Char->bHoldingObject && P && IsValid(P->Item) && SmokeStateClock > 1.0f)
		{
			WC->StopAutopilot();
			Go(4, TEXT("dropped it - picking it up again"));
			break;
		}
		if (P && Step != SmokePathStep)
		{
			SmokePathStep = Step;
			SmokeLegClock = 0.0f;
			SmokeIdleClock = 0.0f;
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE step %d, item fill=%.2f (client view)"), Step + 1, IsValid(P->Item) ? P->Item->GetFill() : -1.0f);
			WC->ServerDev(TEXT("Path next"));
		}
		if (SmokeLegClock < 1.5f && SmokeLegClock + DeltaTime >= 1.5f)
		{
			// Again a moment later: the step and the bucket's fill replicate on separate actors.
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE step %d, item fill=%.2f (client view, 1.5 s in)"), Step + 1, P && IsValid(P->Item) ? P->Item->GetFill() : -1.0f);
		}
		if (FMath::FloorToInt((SmokeLegClock + DeltaTime) / 3.0f) != FMath::FloorToInt(SmokeLegClock / 3.0f))
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE leg %d t=%.0fs me=%s vel=%.0f holding=%d item=%s fill=%.2f autopilot=%d"), Step + 1,
			       SmokeLegClock, *Char->GetActorLocation().ToCompactString(), Char->GetVelocity().Size2D(), Char->bHoldingObject ? 1 : 0,
			       P && IsValid(P->Item) ? *P->Item->GetActorLocation().ToCompactString() : TEXT("-"), P && IsValid(P->Item) ? P->Item->GetFill() : -1.0f,
			       WC->IsAutopilotActive() ? 1 : 0);
		}
		// Arrived (path walked) but the step did not validate: walk the path again (the item may have snagged).
		SmokeIdleClock = WC->IsAutopilotActive() ? 0.0f : SmokeIdleClock + DeltaTime;
		if (SmokeIdleClock > 12.0f && SmokeRepaths < 4)
		{
			++SmokeRepaths;
			SmokeIdleClock = 0.0f;
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE step %d not validated after the walk - walking it again"), Step + 1);
			WC->ServerDev(TEXT("Path next"));
		}
		SmokeLegClock += DeltaTime;
		if (SmokeLegClock > 100.0f)
		{
			Fail(TEXT("carry leg timed out"));
		}
		break;
	case 6:   // the chore list and the spot state replicate on different actors: give the spot a moment
	{
		const AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(World);
		const FKGWorldChoreDef* D = FKGWorldChoreCatalog::Get().FindChore(SmokeChore);
		const TArray<FName> Last = D ? D->Targets(D->NumSteps() - 1, 0) : TArray<FName>();
		const int32 Index = Last.Num() > 0 ? FKGWorldChoreCatalog::Get().AnchorIndex(Last[0]) : INDEX_NONE;
		const FKGSpotState* S = Dir ? Dir->GetSpot(Index) : nullptr;
		if ((S && (S->Level > 0.3f || S->Count > 0 || S->bLit)) || SmokeStateClock > 6.0f)
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WC_SMOKE_DONE %s done on the client list; spot %s level=%.2f count=%d (replicated) t=%.1fs"),
			       *SmokeChore.ToString(), Last.Num() > 0 ? *Last[0].ToString() : TEXT("?"), S ? S->Level : -1.0f, S ? S->Count : -1, SmokeClock);
			WC->StopAutopilot();
			Go(100, TEXT("done"));
		}
		break;
	}
	default:
		break;
	}
}
