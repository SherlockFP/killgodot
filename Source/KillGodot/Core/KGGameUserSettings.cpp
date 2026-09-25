#include "Core/KGGameUserSettings.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/Package.h"
#include "KillGodot.h"

namespace
{
	// Optional sound classes (not authored yet). When an asset appears at one of these paths, its slider starts
	// driving it through a transient sound mix; until then the value is stored and exposed via GetEffectiveVolume.
	const TCHAR* const ChannelClassPaths[] = {
		TEXT("/Game/KillGodot/Audio/Classes/SC_KG_Music.SC_KG_Music"),
		TEXT("/Game/KillGodot/Audio/Classes/SC_KG_Effects.SC_KG_Effects"),
		TEXT("/Game/KillGodot/Audio/Classes/SC_KG_Voice.SC_KG_Voice"),
	};

	/** Game audio devices only: in the editor that means PIE / -game worlds, so a muted PIE never mutes the editor. */
	void ForEachGameAudioDevice(TFunctionRef<void(FAudioDevice&)> Fn)
	{
		if (!GEngine)
		{
			return;
		}
		if (!GIsEditor)
		{
			if (FAudioDeviceManager* Manager = GEngine->GetAudioDeviceManager())
			{
				TArray<FAudioDevice*> Devices;
				Manager->IterateOverAllDevices([&Devices](Audio::FDeviceId, FAudioDevice* Device)
				{
					if (Device)
					{
						Devices.Add(Device);
					}
				});
				for (FAudioDevice* Device : Devices)
				{
					Fn(*Device);
				}
			}
			return;
		}
		TSet<Audio::FDeviceId> Seen;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game))
			{
				continue;
			}
			FAudioDeviceHandle Handle = World->GetAudioDevice();
			if (Handle.IsValid() && !Seen.Contains(Handle.GetDeviceID()))
			{
				Seen.Add(Handle.GetDeviceID());
				Fn(*Handle.GetAudioDevice());
			}
		}
	}
}

UKGGameUserSettings::UKGGameUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetKGDefaults();
}

UKGGameUserSettings* UKGGameUserSettings::Get()
{
	if (!GEngine)
	{
		return nullptr;
	}
	if (UKGGameUserSettings* Engine = Cast<UKGGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		return Engine;
	}
	static UKGGameUserSettings* Fallback = nullptr;
	if (!Fallback)
	{
		UE_LOG(LogKillGodot, Warning,
		       TEXT("KG_SETTINGS GameUserSettingsClassName is not /Script/KillGodot.KGGameUserSettings; using a standalone instance"));
		Fallback = NewObject<UKGGameUserSettings>(GetTransientPackage(), TEXT("KG_StandaloneGameUserSettings"));
		Fallback->AddToRoot();
		Fallback->LoadSettings(false);
	}
	return Fallback;
}

void UKGGameUserSettings::PersistFrontEndMemory()
{
	const FString Section = GetClass()->GetPathName();
	GConfig->SetString(*Section, TEXT("LastJoinAddress"), *LastJoinAddress, GGameUserSettingsIni);
	GConfig->SetInt(*Section, TEXT("LastHostMaxPlayers"), LastHostMaxPlayers, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UKGGameUserSettings::RevertPendingScalability()
{
	ScalabilityQuality = Scalability::GetQualityLevels();
}

void UKGGameUserSettings::SetKGDefaults()
{
	MouseSensitivity = 1.0f;
	bInvertY = false;
	FieldOfView = DefaultFieldOfView;
	ViewmodelPreset = 1;
	bFOVKickOnSpeed = true;
	bStreamerMode = false;
	StreamerPeekKey = GetStreamerPeekKeys()[0];
	MasterVolume = 1.0f;
	MusicVolume = 0.7f;
	EffectsVolume = 1.0f;
	VoiceVolume = 1.0f;
}

void UKGGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	SetKGDefaults();
}

void UKGGameUserSettings::ResetKGSettingsToDefaults()
{
	SetKGDefaults();
}

void UKGGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	ApplyLanguage();
	ApplyLiveSettings();
}

void UKGGameUserSettings::ApplyLiveSettings()
{
	ApplyAudioSettings();
	OnSettingsApplied.Broadcast(*this);
}

void UKGGameUserSettings::SetMouseSensitivity(float Value)
{
	MouseSensitivity = FMath::Clamp(Value, MinMouseSensitivity, MaxMouseSensitivity);
}

void UKGGameUserSettings::SetFieldOfView(float Value)
{
	FieldOfView = FMath::Clamp(Value, MinFieldOfView, MaxFieldOfView);
}

void UKGGameUserSettings::SetViewmodelPreset(int32 Preset)
{
	ViewmodelPreset = FMath::Clamp(Preset, 1, 3);
}

const TArray<FName>& UKGGameUserSettings::GetStreamerPeekKeys()
{
	// EKeys names. Tab is free in gameplay (chat uses it only while typing); the others are common alternatives.
	static const TArray<FName> Keys = {TEXT("Tab"), TEXT("LeftAlt"), TEXT("CapsLock"), TEXT("Q")};
	return Keys;
}

void UKGGameUserSettings::SetStreamerPeekKey(FName Key)
{
	StreamerPeekKey = GetStreamerPeekKeys().Contains(Key) ? Key : GetStreamerPeekKeys()[0];
}

float UKGGameUserSettings::GetVolume(EKGVolumeChannel Channel) const
{
	switch (Channel)
	{
	case EKGVolumeChannel::Music:
		return MusicVolume;
	case EKGVolumeChannel::Effects:
		return EffectsVolume;
	case EKGVolumeChannel::Voice:
		return VoiceVolume;
	default:
		return MasterVolume;
	}
}

void UKGGameUserSettings::SetVolume(EKGVolumeChannel Channel, float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
	switch (Channel)
	{
	case EKGVolumeChannel::Music:
		MusicVolume = Clamped;
		break;
	case EKGVolumeChannel::Effects:
		EffectsVolume = Clamped;
		break;
	case EKGVolumeChannel::Voice:
		VoiceVolume = Clamped;
		break;
	default:
		MasterVolume = Clamped;
		break;
	}
}

float UKGGameUserSettings::GetEffectiveVolume(EKGVolumeChannel Channel) const
{
	const float Master = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	return Channel == EKGVolumeChannel::Master ? Master : Master * FMath::Clamp(GetVolume(Channel), 0.0f, 1.0f);
}

void UKGGameUserSettings::ApplyAudioSettings()
{
	if (!bChannelClassesResolved)
	{
		bChannelClassesResolved = true;
		ChannelClasses.Reset();
		for (const TCHAR* Path : ChannelClassPaths)
		{
			USoundClass* Class = nullptr;
			if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Path))))
			{
				Class = LoadObject<USoundClass>(nullptr, Path);
			}
			ChannelClasses.Add(Class);
		}
	}

	bool bAnyClass = false;
	for (const TObjectPtr<USoundClass>& Class : ChannelClasses)
	{
		bAnyClass |= Class != nullptr;
	}
	if (bAnyClass && !VolumeMix)
	{
		VolumeMix = NewObject<USoundMix>(this, TEXT("KG_VolumeMix"), RF_Transient);
	}

	const float Master = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	const EKGVolumeChannel Channels[] = {EKGVolumeChannel::Music, EKGVolumeChannel::Effects, EKGVolumeChannel::Voice};
	ForEachGameAudioDevice([this, Master, &Channels](FAudioDevice& Device)
	{
		Device.SetTransientPrimaryVolume(Master);
		if (!VolumeMix)
		{
			return;
		}
		for (int32 Index = 0; Index < ChannelClasses.Num(); ++Index)
		{
			if (USoundClass* Class = ChannelClasses[Index])
			{
				Device.SetSoundMixClassOverride(VolumeMix, Class, GetVolume(Channels[Index]), 1.0f, 0.1f, true);
			}
		}
		if (!MixPushedDevices.Contains(Device.DeviceID))
		{
			MixPushedDevices.Add(Device.DeviceID);
			Device.PushSoundMixModifier(VolumeMix);
		}
	});
}

const TArray<FString>& UKGGameUserSettings::GetSupportedLanguages()
{
	static const TArray<FString> Languages = {TEXT("en"), TEXT("tr"), TEXT("ru")};
	return Languages;
}

FString UKGGameUserSettings::GetActiveLanguage()
{
	if (const UKGGameUserSettings* Settings = Get(); Settings && !Settings->Language.IsEmpty())
	{
		return Settings->Language;
	}
	return FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName();
}

void UKGGameUserSettings::SetLanguage(const FString& CultureCode)
{
	Language = CultureCode;
	ApplyLanguage();

	// Persist just this key right away; other pending (unapplied) values must not be saved by a language switch.
	GConfig->SetString(*GetClass()->GetPathName(), TEXT("Language"), *Language, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UKGGameUserSettings::ApplyLanguage()
{
	if (Language.IsEmpty())
	{
		return;
	}
	if (GIsEditor)
	{
		// Switching the culture inside the editor would also switch the editor's own UI. The choice is stored and
		// takes effect in -game / packaged builds.
		UE_LOG(LogKillGodot, Log, TEXT("KG_SETTINGS language '%s' stored (culture switch skipped inside the editor)"),
		       *Language);
		return;
	}
	if (FInternationalization::Get().GetCurrentCulture()->GetName() != Language)
	{
		if (!FInternationalization::Get().SetCurrentCulture(Language))
		{
			UE_LOG(LogKillGodot, Warning, TEXT("KG_SETTINGS culture '%s' is not available in this build"), *Language);
			return;
		}
	}
	// The engine reads this key at boot (TextLocalizationManager), so the language survives a restart.
	GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *Language, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
