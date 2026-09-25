#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/Reveal/KGPseudonyms.h"
#include "KGStreamerMode.generated.h"

class APlayerController;
class APlayerState;
class UWorld;

/**
 * Streamer mode (Settings -> Gameplay -> Streamer mode, UKGGameUserSettings; SPRINT-015 acceptance 3):
 *  (a) after the role reveal the own role is hidden in the HUD (role chip, role card) until the peek key is held,
 *  (b) other players' names are replaced by stable per-match pseudonyms wherever this client shows names,
 *  (c) join codes and host addresses are masked.
 * Purely local presentation: nothing is sent to the server, other players see nothing different.
 * `-KGStreamer` on the command line forces it on (headless checks).
 */
namespace KGStreamer
{
	KILLGODOT_API bool IsEnabled();

	/** The hold-to-peek key from the settings (default Tab). */
	KILLGODOT_API FKey GetPeekKey();
	KILLGODOT_API FText GetPeekKeyLabel();

	/** True while the peek key is held by PC's player. */
	KILLGODOT_API bool IsPeeking(const APlayerController* PC);

	/**
	 * True when PC's HUD must not show its own role right now: streamer mode is on, the reveal ceremony is over and the
	 * peek key is not held. (The reveal ceremony itself always shows the role: that is its job.)
	 */
	KILLGODOT_API bool IsRoleHidden(const APlayerController* PC);

	/** Name to show for Player on this machine: the real name for the local player or with streamer mode off. */
	KILLGODOT_API FString DisplayName(const APlayerState* Player);

	/** Same when only the real name is known (chat lines): resolved against the players of World (null = the game world). */
	KILLGODOT_API FString DisplayNameOf(const UWorld* World, const FString& RealName);

	/** Replaces every other player's real name inside Text by its pseudonym (announcements, crier lines, chat). */
	KILLGODOT_API FString MaskText(const UWorld* World, const FString& Text);

	/** Host names in the server browser (no player state): pseudonym keyed by the host name, stable per menu visit. */
	KILLGODOT_API FString DisplayHostName(const UWorld* World, const FString& HostName);

	/** "ABC123" -> "••••••" (same length) when streamer mode is on. */
	KILLGODOT_API FString MaskCode(const FString& Code);

	/** "192.168.1.20 : 7777" -> "•••.•••.•.•• : ••••" style mask when streamer mode is on. */
	KILLGODOT_API FString MaskAddress(const FString& Address);

	/** Stable pseudonym key of a player: EOS PUID, else the net id, else (bots) the name. */
	KILLGODOT_API FString PlayerKey(const APlayerState* Player);
}

/**
 * Per-world pseudonym table: a new world (next match, next map, back to the title) gets a new random salt, so names
 * are stable for a match and differ between matches. Cosmetic, client-local randomness (not FKGRng gameplay state).
 */
UCLASS()
class KILLGODOT_API UKGStreamerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UKGStreamerSubsystem* Find(const UWorld* World);

	/** Pseudonym of Player (never its real name). */
	FString PseudonymOf(const APlayerState* Player);

	/** Pseudonym of a player known only by RealName; empty when no player of this world has that name. */
	FString PseudonymOfName(const FString& RealName);

	FString MaskText(const FString& Text);

	FString HostPseudonym(const FString& HostName);

	const FKGPseudonyms& GetTable() const { return Table; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Reserves every current player's real name (so no pseudonym ever equals one). */
	void ReserveCurrentNames();

	FKGPseudonyms Table;
};
