#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGFoliageField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Dense forests without thousands of actors: one HISM component per mesh, created on demand by the level builder
 * (Tools/Unreal/kg_build_village.py -> AddInstances). Trees keep collision (trunks block players and bullets of
 * the future); distance culling keeps the far forest cheap.
 */
UCLASS()
class KILLGODOT_API AKGFoliageField : public AActor
{
	GENERATED_BODY()

public:
	AKGFoliageField();

	/** Adds instances of Mesh (creates its HISM on first use). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Foliage")
	void AddInstances(UStaticMesh* Mesh, const TArray<FTransform>& Transforms, bool bCollide, float CullEnd);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Foliage")
	void ClearAll();

	UFUNCTION(BlueprintPure, Category = "KillGodot|Foliage")
	int32 GetInstanceTotal() const;

protected:
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Foliage")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Fields;
};
