#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/KGInventoryTypes.h"
#include "KGInventoryRPCComponent.generated.h"

class AKGCharacter;
class APlayerController;
class APlayerState;
class UKGInventoryComponent;
class UKGInventoryRPCComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FKGOnContainerViewChanged, UKGInventoryRPCComponent* /*Relay*/);

/** Something whose inventory a player can open in the two-panel window (storage chests, later corpses/stalls). */
UINTERFACE(MinimalAPI)
class UKGContainerOwner : public UInterface
{
	GENERATED_BODY()
};

class KILLGODOT_API IKGContainerOwner
{
	GENERATED_BODY()

public:
	virtual UKGInventoryComponent* GetContainerInventory() const = 0;
	virtual FText GetContainerTitle() const = 0;
	/** Server: may this player open / keep using it? (locks, ownership) */
	virtual bool CanPlayerUseContainer(const APlayerState* Player) const = 0;
	/** Server: may this player lock/unlock it from the window? */
	virtual bool CanPlayerToggleLock(const APlayerState* Player) const { return false; }
	virtual void SetContainerLocked(bool bLocked) {}
	virtual bool IsContainerLocked() const { return false; }
	/** Server: a viewer opened/closed it (lid animation, sounds). */
	virtual void OnViewerCountChanged(int32 NumViewers) {}
};

/**
 * Per-player server-RPC relay living on the player state (owned by the player's controller, so the client may call
 * Server RPCs on it). Handles the Minecraft-style container window:
 *  - the server mirrors the opened container into ViewedItems (COND_OwnerOnly), so container contents reach only
 *    the players who actually opened it,
 *  - clicks become ServerTransfer RPCs, validated for distance, life state and lock,
 *  - ServerStandUp lets a seated player leave an AKGSeat with E/Space without looking at it.
 * Created at runtime by UKGInventorySubsystem (or as a default subobject of AKGPlayerState, see the report).
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGInventoryRPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGInventoryRPCComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static UKGInventoryRPCComponent* FindForPlayer(const APlayerState* PlayerState);
	static UKGInventoryRPCComponent* FindForController(const APlayerController* Controller);

	// ---- Server API ----

	/** Authority: opens ContainerActor's window for this player (after the owner's own checks). */
	bool OpenContainer(AActor* ContainerActor);
	/** Authority: closes the window (walked away, died, pressed E/Esc, container destroyed). */
	void CloseContainer();
	UKGInventoryComponent* GetServerViewedInventory() const;

	// ---- Client requests (call on the owning client or the listen-server host; they route to the server) ----

	/** Click in the window: bFromContainer = the item is in the container panel. Num < 0 = whole stack. */
	void RequestTransfer(bool bFromContainer, int32 Slot, int32 Num);
	void RequestClose();
	void RequestSetLocked(bool bLocked);
	void RequestStandUp();
	/** Drop a stack from the pockets as a world pickup in front of the player. */
	void RequestDrop(int32 Slot, int32 Num);

	// ---- Replicated view state (owning client) ----

	AActor* GetViewedActor() const { return ViewedActor; }
	const FKGItemList& GetViewedItems() const { return ViewedItems; }
	int32 GetViewedCapacity() const { return ViewedCapacity; }
	bool IsViewedLocked() const { return bViewedLocked; }
	bool CanToggleViewedLock() const { return bViewerCanToggleLock; }
	FText GetViewedTitle() const;

	/** Fires on the owning client (and the host) when the window must open, close or refresh. */
	FKGOnContainerViewChanged OnViewChanged;

	/** Max distance (cm) between the player and the container to keep using it. */
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float MaxUseDistance = 400.0f;

protected:
	UFUNCTION(Server, Reliable) void ServerTransfer(bool bFromContainer, int32 Slot, int32 Num);
	UFUNCTION(Server, Reliable) void ServerCloseContainer();
	UFUNCTION(Server, Reliable) void ServerSetLocked(bool bLocked);
	UFUNCTION(Server, Reliable) void ServerStandUp();
	UFUNCTION(Server, Reliable) void ServerDrop(int32 Slot, int32 Num);

	void DoTransfer(bool bFromContainer, int32 Slot, int32 Num);
	void DoStandUp();
	void DoDrop(int32 Slot, int32 Num);
	bool IsViewStillValid() const;
	AKGCharacter* GetCharacter() const;
	void RefreshMirror();
	void HandleContainerChanged(UKGInventoryComponent* Inventory);
	void BroadcastView();

	UFUNCTION()
	void OnRep_View();

	UPROPERTY(ReplicatedUsing = OnRep_View)
	TObjectPtr<AActor> ViewedActor;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	FKGItemList ViewedItems;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	int32 ViewedCapacity = 0;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	bool bViewedLocked = false;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	bool bViewerCanToggleLock = false;

private:
	TWeakObjectPtr<UKGInventoryComponent> ServerViewed;
	FDelegateHandle ServerViewedHandle;
	void MarkViewDirty();
};
