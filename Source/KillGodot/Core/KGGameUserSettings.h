#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "KGGameUserSettings.generated.h"

class USoundClass;
class USoundMix;

/** Volume sliders shown in Settings -> Audio. */
UENUM(BlueprintType)
enum class EKGVolumeChannel : uint8
{
	Master,
	Music,
	Effects,
	Voice
};

/**
 * Per-user settings for Kill Godot, persisted in GameUserSettings.ini next to the engine's video/scalability
 * settings. Registered through [/Script/Engine.Engine] GameUserSettingsClassName in DefaultEngine.ini.
 *
 * Graphics use the UGameUserSettings API as-is. Gameplay, audio and language values live here; call
 * ApplyNonResolutionSettings() (or ApplySettings) to push them live and listen to OnSettingsApplied to react.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class KILLGODOT_API UKGGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UKGGameUserSettings(const FObjectInitializer& ObjectInitializer);

	/**
	 * The engine's settings object. If DefaultEngine.ini does not (yet) point GameUserSettingsClassName at this
	 * class, a standalone instance is created once so the menus keep working (a warning is logged).
	 */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings", meta = (DisplayName = "Get KG Game User Settings"))
	static UKGGameUserSettings* Get();

	// UGameUserSettings
	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	/** Pushes gameplay + audio values live without touching graphics or saving (used while dragging sliders). */
	void ApplyLiveSettings();

	/** Resets only the gameplay/audio/language values to their defaults (graphics untouched). */
	void ResetKGSettingsToDefaults();

	// --- Gameplay -----------------------------------------------------------------------------------------------

	static constexpr float MinMouseSensitivity = 0.1f;
	static constexpr float MaxMouseSensitivity = 5.0f;
	static constexpr float MinFieldOfView = 80.0f;
	static constexpr float MaxFieldOfView = 110.0f;
	static constexpr float DefaultFieldOfView = 90.0f;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetMouseSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	bool GetInvertY() const { return bInvertY; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetInvertY(bool bValue) { bInvertY = bValue; }

	/** Horizontal field of view in degrees (80-110). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	float GetFieldOfView() const { return FieldOfView; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetFieldOfView(float Value);

	/** Viewmodel preset for FKGViewmodelSettings::FromPreset: 1 = Desktop, 2 = Couch, 3 = Classic. */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	int32 GetViewmodelPreset() const { return ViewmodelPreset; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetViewmodelPreset(int32 Preset);

	/** SPRINT-026: a small FOV widen at high ground speed (sprint/bhop), CS-style. On by default. */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	bool GetFOVKickOnSpeed() const { return bFOVKickOnSpeed; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetFOVKickOnSpeed(bool bValue) { bFOVKickOnSpeed = bValue; }

	// --- Streamer mode (Settings -> Gameplay, SPRINT-015) ---------------------------------------------------------

	/** Hides your role after the reveal (hold the peek key), shows other players under per-match pseudonyms and masks
	 *  join codes / addresses. Read through KGStreamer (UI/Reveal/KGStreamerMode.h). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	bool GetStreamerMode() const { return bStreamerMode; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetStreamerMode(bool bValue) { bStreamerMode = bValue; }

	/** Keys offered for "hold to peek at your role" (index 0 = the default, Tab). */
	static const TArray<FName>& GetStreamerPeekKeys();

	/** Key name held to peek at the hidden role (one of GetStreamerPeekKeys, default "Tab"). */
	FName GetStreamerPeekKey() const { return StreamerPeekKey; }
	void SetStreamerPeekKey(FName Key);

	// --- Audio --------------------------------------------------------------------------------------------------

	/** Slider value 0..1 of one channel. */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	float GetVolume(EKGVolumeChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetVolume(EKGVolumeChannel Channel, float Value);

	/** Channel volume multiplied by the master volume: what a sound of that channel should play at. */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	float GetEffectiveVolume(EKGVolumeChannel Channel) const;

	/**
	 * Master goes to every audio device's primary volume. Music / Effects / Voice drive sound class overrides
	 * when the classes exist (SoundClassPaths below); until then they are stored and exposed via GetEffectiveVolume.
	 */
	void ApplyAudioSettings();

	// --- Voice chat (SPRINT-023) ----------------------------------------------------------------------------------

	/** Open microphone: transmit whenever you speak. Off = hold V (push-to-talk). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	bool GetOpenMic() const { return bOpenMic; }

	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetOpenMic(bool bValue) { bOpenMic = bValue; }

	// --- Language -----------------------------------------------------------------------------------------------

	/** Culture codes offered in Settings -> Language (en, tr, ru). */
	static const TArray<FString>& GetSupportedLanguages();

	/** Chosen culture ("" = follow the OS). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Settings")
	FString GetLanguage() const { return Language; }

	/** Stores the culture and applies it immediately (the engine re-reads it from GameUserSettings.ini at boot). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Settings")
	void SetLanguage(const FString& CultureCode);

	/** Culture currently used for text ("en", "tr", "ru", ...). */
	static FString GetActiveLanguage();

	// --- Front-end memory ---------------------------------------------------------------------------------------

	const FString& GetLastJoinAddress() const { return LastJoinAddress; }
	void SetLastJoinAddress(const FString& Address) { LastJoinAddress = Address; }

	int32 GetLastHostMaxPlayers() const { return LastHostMaxPlayers; }
	void SetLastHostMaxPlayers(int32 Value) { LastHostMaxPlayers = FMath::Clamp(Value, 6, 20); }

	/** Writes only LastJoinAddress / LastHostMaxPlayers (pending, unapplied settings are left alone). */
	void PersistFrontEndMemory();

	/** Graphics state that has not been applied yet is reset to what the engine currently uses. */
	void RevertPendingScalability();

	/** Fired after gameplay/audio values were pushed live (ApplyNonResolutionSettings / ApplyLiveSettings). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FKGOnSettingsApplied, const UKGGameUserSettings& /*Settings*/);
	FKGOnSettingsApplied OnSettingsApplied;

protected:
	void ApplyLanguage();
	void SetKGDefaults();

	UPROPERTY(config)
	float MouseSensitivity = 1.0f;

	UPROPERTY(config)
	bool bInvertY = false;

	UPROPERTY(config)
	float FieldOfView = DefaultFieldOfView;

	UPROPERTY(config)
	int32 ViewmodelPreset = 1;

	UPROPERTY(config)
	bool bFOVKickOnSpeed = true;

	UPROPERTY(config)
	bool bStreamerMode = false;

	UPROPERTY(config)
	FName StreamerPeekKey = TEXT("Tab");

	UPROPERTY(config)
	bool bOpenMic = false;

	UPROPERTY(config)
	float MasterVolume = 1.0f;

	UPROPERTY(config)
	float MusicVolume = 0.7f;

	UPROPERTY(config)
	float EffectsVolume = 1.0f;

	UPROPERTY(config)
	float VoiceVolume = 1.0f;

	UPROPERTY(config)
	FString Language;

	UPROPERTY(config)
	FString LastJoinAddress = TEXT("127.0.0.1");

	UPROPERTY(config)
	int32 LastHostMaxPlayers = 12;

	/** Transient mix that carries the Music/Effects/Voice class overrides. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeMix;

	/** Loaded sound classes per channel (null entries = class asset not authored yet). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundClass>> ChannelClasses;

	bool bChannelClassesResolved = false;

	/** Audio devices that already have VolumeMix on their modifier stack (push once, update overrides after). */
	TSet<uint32> MixPushedDevices;
};
