#pragma once

#include "CoreMinimal.h"
#include "Core/KGMatchClock.h"
#include "GameFramework/Info.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "KGLobbyState.generated.h"

class APlayerController;
class APlayerState;

/** One seat in the pre-game lobby. */
USTRUCT()
struct FKGLobbyEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> Player;

	UPROPERTY()
	bool bReady = false;

	UPROPERTY()
	bool bHost = false;

	/** Index into AKGLobbyState::GetPlayerColor (join order). */
	UPROPERTY()
	uint8 ColorIndex = 0;
};

USTRUCT()
struct FKGLobbyEntryArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FKGLobbyEntry> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FKGLobbyEntry, FKGLobbyEntryArray>(Items, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FKGLobbyEntryArray> : public TStructOpsTypeTraitsBase2<FKGLobbyEntryArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/** Host-controlled lobby options (replicated to everyone, editable only by the host). */
USTRUCT(BlueprintType)
struct FKGLobbySettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString LobbyName;

	/** Six-character join code advertised with the session. */
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString Code;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString MapPath;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MaxPlayers = 12;

	/** Role list preset (0 = Classic). Read by the role assignment when presets exist. */
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	uint8 RolePreset = 0;

	/** Top the match up with villager bots to the 6-player minimum when it starts. */
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bFillWithBots = true;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bPrivate = false;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bPassword = false;
};

/**
 * Pre-game lobby of a hosted match (spawned by AKGGameMode when the map was opened with ?KGLobby).
 * Server authoritative: seats follow the GameState player list, clients change their ready flag and the host
 * changes settings / kicks / starts through AKGPlayerController server RPCs. The start countdown is an
 * FKGMatchClock (remaining seconds), after which the game mode starts the match flow.
 */
UCLASS(NotPlaceable)
class KILLGODOT_API AKGLobbyState : public AInfo
{
	GENERATED_BODY()

public:
	AKGLobbyState();

	static AKGLobbyState* Get(const UObject* WorldContext);

	/** Minimum players for a match (roles need six). */
	static constexpr int32 MinPlayersToStart = 6;
	static constexpr float CountdownSeconds = 5.0f;
	static FLinearColor GetPlayerColor(int32 ColorIndex);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Queries (any machine) ---------------------------------------------------------------------------------
	const TArray<FKGLobbyEntry>& GetEntries() const { return Entries.Items; }
	const FKGLobbySettings& GetSettings() const { return Settings; }
	const FKGLobbyEntry* FindEntry(const APlayerState* Player) const;
	int32 CountHumans() const;
	int32 CountReady() const;
	bool IsCountingDown() const { return bCountingDown; }
	float GetCountdownRemaining() const { return Countdown.RemainingSeconds; }
	bool HasStarted() const { return bStarted; }
	/** Whether the host may press Start now (and why not). */
	bool CanStart(FText& OutReason) const;
	/** Bumped on every replicated change; UI rebuilds lists when it moves. */
	uint32 GetRevision() const { return Revision; }

	// --- Authority -------------------------------------------------------------------------------------------
	void AuthSetReady(APlayerState* Player, bool bReady);
	void AuthApplySettings(const FKGLobbySettings& NewSettings);
	void AuthSetCountdown(bool bStart);
	void AuthKick(APlayerState* Target);

protected:
	UFUNCTION()
	void OnRep_Lobby();

	void AuthSyncSeats();
	void AuthBeginMatch();
	void Touch();

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	FKGLobbyEntryArray Entries;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	FKGLobbySettings Settings;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	FKGMatchClock Countdown;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	bool bCountingDown = false;

	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	bool bStarted = false;

private:
	uint32 Revision = 1;
	uint8 NextColor = 0;
	float SyncAccumulator = 0.0f;
	int32 LastCountdownSecond = -1;
};
