#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "KGPlayerState.generated.h"

UENUM(BlueprintType)
enum class EKGLifeState : uint8
{
	Alive,
	Ghost,
	/** Won the Limbo duel and came back from the grave (Docs/01_GDD_Core.md §11.1). */
	Revenant
};

/**
 * Public identity of a player + the secret role, which only replicates to its owner.
 * Everything is keyed by the EOS PUID so host migration can rebind players to their records.
 */
UCLASS()
class KILLGODOT_API AKGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AKGPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Identity")
	FString Puid;

	/** Chosen "Face" (iconic head archetype), Docs/06_Art_Direction.md §2. */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Identity")
	FName FaceId;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Identity")
	FName ProfessionId;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Identity")
	int32 HouseIndex = INDEX_NONE;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|State")
	EKGLifeState LifeState = EKGLifeState::Alive;

	/** Filled in when the role becomes public (death without a Cleaner, Mayor reveal, epilogue). */
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly, Category = "KillGodot|Role")
	FName RevealedRoleId;

	/** Meeting: who this player accuses (public, like raising a hand). Cleared every meeting. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Vote")
	TObjectPtr<AKGPlayerState> AccuseTarget;

	/** Trial: 0 = no vote, 1 = guilty, 2 = innocent. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Vote")
	uint8 Verdict = 0;

	/** Chores dealt at role reveal (owner only: nobody may read your list). Parallel arrays. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Tasks")
	TArray<FName> TaskIds;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Tasks")
	TArray<bool> TaskDone;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Tasks")
	bool HasOpenTask(FName TaskId) const
	{
		const int32 i = TaskIds.IndexOfByKey(TaskId);
		return i != INDEX_NONE && TaskDone.IsValidIndex(i) && !TaskDone[i];
	}

	/** Authority: marks the chore done; returns false if it was not open. */
	bool CompleteTask(FName TaskId)
	{
		const int32 i = TaskIds.IndexOfByKey(TaskId);
		if (i == INDEX_NONE || !TaskDone.IsValidIndex(i) || TaskDone[i])
		{
			return false;
		}
		TaskDone[i] = true;
		ForceNetUpdate();
		return true;
	}

	UFUNCTION(BlueprintPure, Category = "KillGodot|Role")
	FName GetPrivateRoleId() const { return PrivateRoleId; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|State")
	bool IsAlive() const { return LifeState == EKGLifeState::Alive; }

	/** Authority only. */
	void SetPrivateRoleId(FName RoleId) { PrivateRoleId = RoleId; }

protected:
	/** Secret: replicated to the owning client only (COND_OwnerOnly). */
	UPROPERTY(Replicated, SaveGame)
	FName PrivateRoleId;
};
