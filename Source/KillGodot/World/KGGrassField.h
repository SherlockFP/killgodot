#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KGGrassField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Wind-swept meadow (the "Ghost of Tsushima" grass the user asked for): four HISM layers of procedural grass
 * clumps (Tools/Blender/kg_make_grass.py, M_KG_Grass wind WPO). Instances are baked into the level by
 * Tools/Unreal/kg_build_village.py through AddClumps; nothing is generated at runtime.
 * No collision, no navigation, no shadows (cost), distance culled per layer.
 */
UCLASS()
class KILLGODOT_API AKGGrassField : public AActor
{
	GENERATED_BODY()

public:
	AKGGrassField();

	/** Layer: 0 short, 1 mid, 2 tall, 3 flowers. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Grass")
	void AddClumps(int32 Layer, const TArray<FTransform>& Transforms);

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Grass")
	void ClearClumps();

	UFUNCTION(BlueprintPure, Category = "KillGodot|Grass")
	int32 GetClumpCount() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Grass")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassLayers;
};
