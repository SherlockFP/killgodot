#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "KGInteractable.generated.h"

class AKGCharacter;

UINTERFACE(MinimalAPI, BlueprintType)
class UKGInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the player can press E on: doors, task stations, NPCs, levers. Physics props are grabbed instead. */
class KILLGODOT_API IKGInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "KillGodot|Interaction")
	void Interact(AKGCharacter* By);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "KillGodot|Interaction")
	FText GetInteractPrompt() const;
};
