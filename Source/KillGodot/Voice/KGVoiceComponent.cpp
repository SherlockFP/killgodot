#include "Voice/KGVoiceComponent.h"
#include "Audio/KGAudio.h"
#include "Character/KGAppearanceComponent.h"
#include "Character/KGCharacter.h"
#include "Character/KGVillagerLook.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGChatUI.h"
#include "Chat/KGEmoji.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/KGGameState.h"
#include "Core/KGGameUserSettings.h"
#include "Core/KGPlayerState.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "KillGodot.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/Package.h"
#include "Voice/KGMouthComponent.h"
#include "Voice/KGVoiceRules.h"
#include "VoiceModule.h"
#include "World/KGMapInfo.h"

namespace KGVoicePrivate
{
	const TCHAR* Machine(const UWorld* World)
	{
		return World && World->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Host");
	}

	/** Spatialised (panned) but not distance-attenuated: the server's gain already is the attenuation. */
	USoundAttenuation* VoiceAttenuation()
	{
		static TWeakObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("KG_VoiceAttenuation"));
			A->AddToRoot();
			A->Attenuation.bAttenuate = false;
			A->Attenuation.bSpatialize = true;
			A->Attenuation.bAttenuateWithLPF = false;
			Att = A;
		}
		return Att.Get();
	}

	float VoiceVolume()
	{
		const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
		return Settings ? Settings->GetEffectiveVolume(EKGVolumeChannel::Voice) : 1.0f;
	}

	void PhaseInfo(const UWorld* World, EKGPhase& OutPhase, float& OutRemaining)
	{
		const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
		OutPhase = GS ? GS->GetPhase() : EKGPhase::Lobby;
		OutRemaining = GS ? GS->GetPhaseRemaining() : 0.0f;
	}
}

UKGVoiceComponent::UKGVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	BarkLimiter = FKGVoiceCommandCatalog::MakeLimiter();
}

void UKGVoiceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKGVoiceComponent, SmokeTag);
}

UKGVoiceComponent* UKGVoiceComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UKGVoiceComponent>() : nullptr;
}

UKGVoiceComponent* UKGVoiceComponent::FindForController(const APlayerController* Controller)
{
	return Controller ? FindForPlayer(Controller->PlayerState) : nullptr;
}

APlayerController* UKGVoiceComponent::GetOwningController() const
{
	const AActor* PS = GetOwner();
	return PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
}

bool UKGVoiceComponent::IsLocallyOwned() const
{
	const APlayerController* PC = GetOwningController();
	return PC && PC->IsLocalController();
}

AKGCharacter* UKGVoiceComponent::CharacterOf(const APlayerState* PS) const
{
	return PS ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
}

void UKGVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownCapture();
	for (TPair<int32, FTalker>& Pair : Talkers)
	{
		if (Pair.Value.Audio.IsValid())
		{
			Pair.Value.Audio->Stop();
			Pair.Value.Audio->DestroyComponent();
		}
	}
	Talkers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UKGVoiceComponent::SetSmokeTag(FName Tag)
{
	if (GetOwner() && GetOwner()->HasAuthority() && SmokeTag != Tag)
	{
		SmokeTag = Tag;
		GetOwner()->ForceNetUpdate();
	}
}

EKGVoiceChannel UKGVoiceComponent::GetLocalChannel() const
{
	EKGPhase Phase;
	float Remaining;
	KGVoicePrivate::PhaseInfo(GetWorld(), Phase, Remaining);
	return FKGVoiceRules::ChannelOf(UKGChatComponent::MakeParticipant(Cast<APlayerState>(GetOwner())), Phase);
}

bool UKGVoiceComponent::WasHeardRecently(const APlayerState* Speaker, double Window) const
{
	const FKGVoiceHeard* H = Speaker ? Heard.Find(Speaker->GetPlayerId()) : nullptr;
	return H && FPlatformTime::Seconds() - H->LastTime <= Window;
}

// ---- transmit -------------------------------------------------------------------------------------------------

void UKGVoiceComponent::EnsureCapture()
{
	if (bCaptureTried)
	{
		return;
	}
	bCaptureTried = true;
	if (!FVoiceModule::IsAvailable())
	{
		UE_LOG(LogKillGodot, Warning, TEXT("KG_VOICE_CODEC %s codec=raw mic=0 (Voice module unavailable)"), KGVoicePrivate::Machine(GetWorld()));
		return;
	}
	FVoiceModule& VM = FVoiceModule::Get();
	Encoder = VM.CreateVoiceEncoder(SampleRate, NumChannels, EAudioEncodeHint::VoiceEncode_Voice);
	bOpus = Encoder.IsValid();
	if (bOpus)
	{
		Encoder->SetBitrate(24000);
	}
	if (VM.IsVoiceEnabled() && VM.DoesPlatformSupportVoiceCapture())
	{
		Capture = VM.CreateVoiceCapture(FString(), SampleRate, NumChannels);
		bHasMicrophone = Capture.IsValid();
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_CODEC %s codec=%s mic=%d enabled=%d"), KGVoicePrivate::Machine(GetWorld()),
	       GetCodecName(), bHasMicrophone ? 1 : 0, VM.IsVoiceEnabled() ? 1 : 0);
}

void UKGVoiceComponent::ShutdownCapture()
{
	if (Capture.IsValid())
	{
		Capture->Stop();
		Capture->Shutdown();
		Capture.Reset();
	}
	if (Encoder.IsValid())
	{
		Encoder->Destroy();
		Encoder.Reset();
	}
	bHasMicrophone = false;
}

void UKGVoiceComponent::SetTransmitting(bool bOn)
{
	if (bOn == bTransmitting)
	{
		return;
	}
	bTransmitting = bOn;
	if (bOn)
	{
		EnsureCapture();
	}
	else
	{
		PendingPCM.Reset();
		LocalAmplitude = 0.0f;
	}
	if (Capture.IsValid())
	{
		if (bOn && !Capture->IsCapturing())
		{
			Capture->Start();
		}
		else if (!bOn && Capture->IsCapturing())
		{
			Capture->Stop();
		}
	}
}

void UKGVoiceComponent::InjectPCM(const int16* Samples, int32 Num)
{
	if (Samples && Num > 0)
	{
		PendingPCM.Append(reinterpret_cast<const uint8*>(Samples), Num * sizeof(int16));
	}
}

void UKGVoiceComponent::TickCapture(float DeltaTime)
{
	if (!bTransmitting)
	{
		return;
	}
	if (Capture.IsValid() && Capture->IsCapturing())
	{
		uint32 Available = 0;
		if (Capture->GetCaptureState(Available) == EVoiceCaptureState::Ok && Available > 0)
		{
			const int32 Old = PendingPCM.Num();
			PendingPCM.AddUninitialized(Available);
			uint32 Got = 0;
			const EVoiceCaptureState::Type State = Capture->GetVoiceData(PendingPCM.GetData() + Old, Available, Got);
			PendingPCM.SetNum(Old + (State == EVoiceCaptureState::Ok ? static_cast<int32>(Got) : 0), EAllowShrinking::No);
		}
	}
	if (bTone)
	{
		const int32 Num = FMath::Clamp(FMath::RoundToInt(DeltaTime * SampleRate), 0, SampleRate / 4);
		TArray<int16> Tone;
		Tone.SetNumUninitialized(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			TonePhase += 220.0f / SampleRate;
			TonePhase -= FMath::Floor(TonePhase);
			Tone[i] = static_cast<int16>(FMath::Sin(TonePhase * 2.0f * PI) * 0.5f * 32767.0f);
		}
		InjectPCM(Tone.GetData(), Num);
	}
	constexpr int32 PacketBytes = PacketSamples * sizeof(int16);
	const bool bSynthetic = bTone || !bHasMicrophone;
	int32 Sent = 0;
	while (PendingPCM.Num() >= PacketBytes && Sent < 8)
	{
		SendChunk(reinterpret_cast<const int16*>(PendingPCM.GetData()), bSynthetic);
		PendingPCM.RemoveAt(0, PacketBytes, EAllowShrinking::No);
		++Sent;
	}
	if (PendingPCM.Num() > SampleRate * static_cast<int32>(sizeof(int16)))
	{
		PendingPCM.Reset();   // a second of backlog: the frame rate collapsed, drop rather than lag
	}
}

void UKGVoiceComponent::SendChunk(const int16* Samples, bool bSynthetic)
{
	LocalAmplitude = FKGVoiceRules::Amplitude(Samples, PacketSamples);
	FKGVoicePacket Packet;
	Packet.Serial = ++TxSerial;
	Packet.Amplitude = static_cast<uint16>(FMath::Clamp(LocalAmplitude, 0.0f, 1.0f) * 32767.0f);
	Packet.Flags = bSynthetic ? EKGVoicePacketFlags::Synthetic : EKGVoicePacketFlags::None;
	bool bEncoded = false;
	if (Encoder.IsValid())
	{
		ScratchEncoded.SetNumUninitialized(4096, EAllowShrinking::No);
		uint32 OutSize = ScratchEncoded.Num();
		Encoder->Encode(reinterpret_cast<const uint8*>(Samples), PacketSamples * sizeof(int16), ScratchEncoded.GetData(), OutSize);
		if (OutSize > 0 && OutSize <= static_cast<uint32>(ScratchEncoded.Num()))
		{
			Packet.Data.Append(ScratchEncoded.GetData(), OutSize);
			bEncoded = true;
		}
	}
	if (!bEncoded)
	{
		Packet.Flags |= EKGVoicePacketFlags::RawPCM;
		Packet.Data.Append(reinterpret_cast<const uint8*>(Samples), PacketSamples * sizeof(int16));
	}
	if (AKGCharacter* Me = CharacterOf(Cast<APlayerState>(GetOwner())))
	{
		if (UKGMouthComponent* Mouth = Me->GetMouth())
		{
			Mouth->PushAmplitude(LocalAmplitude);
		}
	}
	++TxPackets;
	if (TxPackets == 1 || TxPackets % 50 == 0)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_TX %s who=%s packets=%d bytes=%d amp=%.2f codec=%s"), KGVoicePrivate::Machine(GetWorld()),
		       *GetNameSafe(GetOwner()), TxPackets, Packet.Data.Num(), LocalAmplitude, bEncoded ? TEXT("opus") : TEXT("raw"));
	}
	ServerVoice(Packet);
}

void UKGVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsLocallyOwned())
	{
		TickCapture(DeltaTime);
	}
}

// ---- server routing ---------------------------------------------------------------------------------------------

void UKGVoiceComponent::RouteLog(const APlayerState* To, float Gain, float Dist)
{
	const double Now = FPlatformTime::Seconds();
	double& Last = RouteLogTimes.FindOrAdd(To->GetPlayerId(), -1000.0);
	if (Now - Last < 1.0)
	{
		return;
	}
	Last = Now;
	UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_ROUTE from=%s to=%s gain=%.2f dist=%.0f tag=%s"), *GetNameSafe(GetOwner()),
	       *To->GetPlayerName(), Gain, Dist, *SmokeTag.ToString());
}

void UKGVoiceComponent::ServerVoice_Implementation(const FKGVoicePacket& Packet)
{
	UWorld* World = GetWorld();
	APlayerState* Me = Cast<APlayerState>(GetOwner());
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || !Me || !Me->HasAuthority())
	{
		return;
	}
	EKGPhase Phase;
	float Remaining;
	KGVoicePrivate::PhaseInfo(World, Phase, Remaining);
	const FKGChatParticipant Speaker = UKGChatComponent::MakeParticipant(Me);
	if (!FKGVoiceRules::CanSpeak(Speaker, Phase))
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastRejectLog > 1.0)
		{
			LastRejectLog = Now;
			UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_REJECT who=%s phase=%d life=%d"), *Me->GetPlayerName(), int32(Phase), int32(Speaker.Life));
		}
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		UKGVoiceComponent* Target = FindForPlayer(PS);
		if (!Target || !Target->GetOwningController())
		{
			continue;   // bots: nobody listening
		}
		const FKGChatParticipant Listener = UKGChatComponent::MakeParticipant(PS);
		const float Gain = FKGVoiceRules::HearGain(Speaker, Listener, Phase, PS == Me);
		const float Dist = Speaker.bHasLocation && Listener.bHasLocation ? static_cast<float>(FVector::Dist(Speaker.Location, Listener.Location)) : -1.0f;
		if (PS != Me)
		{
			RouteLog(PS, Gain, Dist);
		}
		if (Gain <= 0.0f)
		{
			continue;
		}
		Target->ClientVoice(Me, Packet, FKGVoiceRules::QuantizeGain(Gain));
		++RoutedPackets;
	}
}

// ---- receive ----------------------------------------------------------------------------------------------------

void UKGVoiceComponent::ClientVoice_Implementation(APlayerState* Speaker, const FKGVoicePacket& Packet, uint8 Gain)
{
	OnVoiceHeard(Speaker, Packet, FKGVoiceRules::DequantizeGain(Gain));
}

void UKGVoiceComponent::OnVoiceHeard(APlayerState* Speaker, const FKGVoicePacket& Packet, float Gain)
{
	if (!Speaker)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	FKGVoiceHeard& H = Heard.FindOrAdd(Speaker->GetPlayerId());
	H.LastTime = Now;
	H.LastGain = Gain;
	H.LastAmplitude = static_cast<float>(Packet.Amplitude) / 32767.0f;
	++H.Packets;
	++H.PacketsWindow;
	H.MaxGainWindow = FMath::Max(H.MaxGainWindow, Gain);
	const UKGChatComponent* Chat = UKGChatComponent::FindForPlayer(Cast<APlayerState>(GetOwner()));
	const bool bMuted = Chat && Chat->IsMuted(Speaker->GetPlayerId());
	if (Now - H.LastLogTime >= 1.0)
	{
		H.LastLogTime = Now;
		const UKGVoiceComponent* SpeakerVoice = FindForPlayer(Speaker);
		UE_LOG(LogKillGodot, Log, TEXT("KG_VOICE_RX %s me=%s from=%s gain=%.2f amp=%.2f bytes=%d packets=%d muted=%d tag=%s"),
		       KGVoicePrivate::Machine(GetWorld()), *GetNameSafe(GetOwner()), *Speaker->GetPlayerName(), Gain, H.LastAmplitude,
		       Packet.Data.Num(), H.Packets, bMuted ? 1 : 0, SpeakerVoice ? *SpeakerVoice->GetSmokeTag().ToString() : TEXT(""));
	}
	if (bMuted)
	{
		++H.DroppedMuted;
		return;
	}
	if (AKGCharacter* Body = CharacterOf(Speaker))
	{
		if (UKGMouthComponent* Mouth = Body->GetMouth())
		{
			Mouth->PushAmplitude(H.LastAmplitude);
		}
	}
	PlayVoice(Speaker, Packet, Gain);
	++H.PlayedWindow;
}

UKGVoiceComponent::FTalker& UKGVoiceComponent::TalkerFor(APlayerState* Speaker)
{
	return Talkers.FindOrAdd(Speaker->GetPlayerId());
}

void UKGVoiceComponent::PlayVoice(APlayerState* Speaker, const FKGVoicePacket& Packet, float Gain)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || Packet.Data.Num() == 0)
	{
		return;
	}
	FTalker& T = TalkerFor(Speaker);
	const uint8* PCM = nullptr;
	int32 Bytes = 0;
	if (Packet.Flags & EKGVoicePacketFlags::RawPCM)
	{
		PCM = Packet.Data.GetData();
		Bytes = Packet.Data.Num();
	}
	else
	{
		if (!T.Decoder.IsValid() && FVoiceModule::IsAvailable())
		{
			T.Decoder = FVoiceModule::Get().CreateVoiceDecoder(SampleRate, NumChannels);
		}
		if (!T.Decoder.IsValid())
		{
			return;
		}
		// The engine's Opus decoder refuses any output buffer smaller than its own maximum (48 KB).
		ScratchPCM.SetNumUninitialized(48 * 1024, EAllowShrinking::No);
		uint32 OutSize = ScratchPCM.Num();
		T.Decoder->Decode(Packet.Data.GetData(), Packet.Data.Num(), ScratchPCM.GetData(), OutSize);
		PCM = ScratchPCM.GetData();
		Bytes = static_cast<int32>(OutSize);
	}
	if (!PCM || Bytes <= 0)
	{
		return;
	}
	if (!T.Wave.IsValid())
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
		Wave->SetSampleRate(SampleRate);
		Wave->NumChannels = NumChannels;
		Wave->Duration = INDEFINITELY_LOOPING_DURATION;
		Wave->SoundGroup = SOUNDGROUP_Voice;
		Wave->bLooping = false;
		Wave->bProcedural = true;
		Waves.Add(Wave);
		T.Wave = Wave;
	}
	AKGCharacter* Body = CharacterOf(Speaker);
	USceneComponent* Attach = Body ? Body->GetMesh() : nullptr;
	if (!T.Audio.IsValid() || T.AttachedTo.Get() != Body)
	{
		if (T.Audio.IsValid())
		{
			T.Audio->Stop();
			T.Audio->DestroyComponent();
		}
		if (Attach)
		{
			T.Audio = UGameplayStatics::SpawnSoundAttached(T.Wave.Get(), Attach, TEXT("Head"), FVector::ZeroVector,
			                                               EAttachLocation::SnapToTarget, true, 1.0f, 1.0f, 0.0f,
			                                               KGVoicePrivate::VoiceAttenuation(), nullptr, false);
		}
		else
		{
			T.Audio = UGameplayStatics::SpawnSound2D(this, T.Wave.Get(), 1.0f, 1.0f, 0.0f, nullptr, false, false);
		}
		T.AttachedTo = Body;
	}
	if (T.Audio.IsValid())
	{
		T.Audio->SetVolumeMultiplier(Gain * KGVoicePrivate::VoiceVolume());
		if (!T.Audio->IsPlaying())
		{
			T.Audio->Play();
		}
	}
	T.Wave->QueueAudio(PCM, Bytes);
}

// ---- barks ------------------------------------------------------------------------------------------------------

void UKGVoiceComponent::RequestBark(FName CommandId)
{
	if (IsLocallyOwned())
	{
		ServerBark(CommandId);
	}
}

void UKGVoiceComponent::ServerBark_Implementation(FName CommandId)
{
	ServerSayBark(CommandId, false);
}

bool UKGVoiceComponent::ServerSayBark(FName CommandId, bool bIgnoreRateLimit)
{
	UWorld* World = GetWorld();
	APlayerState* Me = Cast<APlayerState>(GetOwner());
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || !Me || !Me->HasAuthority())
	{
		return false;
	}
	const int32 Index = FKGVoiceCommandCatalog::IndexOf(CommandId);
	const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Get(Index);
	if (!Def)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_BARK_REJECT who=%s id=%s reason=Unknown"), *Me->GetPlayerName(), *CommandId.ToString());
		return false;
	}
	EKGPhase Phase;
	float Remaining;
	KGVoicePrivate::PhaseInfo(World, Phase, Remaining);
	const FKGChatParticipant Speaker = UKGChatComponent::MakeParticipant(Me);
	if (!FKGChatRules::CanReact(Speaker, Phase, Remaining))
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_BARK_REJECT who=%s id=%s reason=Phase"), *Me->GetPlayerName(), *Def->Id.ToString());
		return false;
	}
	if (!bIgnoreRateLimit && BarkLimiter.TryConsume(World->GetRealTimeSeconds(), FString()) != EKGChatReject::None)
	{
		UE_LOG(LogKillGodot, Log, TEXT("KG_BARK_REJECT who=%s id=%s reason=RateLimited"), *Me->GetPlayerName(), *Def->Id.ToString());
		return false;
	}
	FText Place;
	if (Def->UsesPlace() && Speaker.bHasLocation)
	{
		if (const AKGMapInfo* Map = AKGMapInfo::Find(World))
		{
			const int32 Region = Map->FindRegionAt(FVector2D(Speaker.Location.X, Speaker.Location.Y));
			if (Map->Regions.IsValidIndex(Region))
			{
				Place = Map->Regions[Region].Name;
			}
		}
	}
	const FString Text = FKGVoiceCommandCatalog::FormatLine(*Def, Place);
	EKGBarkVoice Voice = FKGVoiceCommandCatalog::VoiceFor(false, false, Me->GetPlayerId());
	AKGCharacter* Body = CharacterOf(Me);
	if (Body && Body->GetAppearance() && Body->GetAppearance()->GetAppliedLook().IsAssigned())
	{
		const FKGVillagerArchetype& A = FKGVillagerLookGen::ArchetypeOf(Body->GetAppearance()->GetAppliedLook());
		Voice = FKGVoiceCommandCatalog::VoiceFor(A.bFemale, A.bOld, Me->GetPlayerId());
	}
	++BarkSerial;
	const bool bGhost = Speaker.Life == EKGLifeState::Ghost;
	if (UKGChatComponent* Chat = UKGChatComponent::FindForPlayer(Me))
	{
		Chat->ServerSay(bGhost ? EKGChatChannel::Dead : EKGChatChannel::Nearby, Text, EKGChatFlags::None);
	}
	if (!bGhost && !Def->Gesture.IsNone())
	{
		if (UKGEmoteComponent* Emote = UKGEmoteComponent::FindForPlayer(Me))
		{
			Emote->ServerTryStart(Def->Gesture, true);
		}
	}
	int32 Delivered = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		UKGVoiceComponent* Target = FindForPlayer(PS);
		if (Target && Target->GetOwningController() &&
		    FKGChatRules::CanSeeReaction(Speaker, UKGChatComponent::MakeParticipant(PS), PS == Me))
		{
			Target->ClientBark(Me, static_cast<uint8>(Index), static_cast<uint8>(Voice), BarkSerial);
			++Delivered;
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_BARK %s who=%s id=%s voice=%s place=\"%s\" text=\"%s\" serial=%d delivered=%d"),
	       KGVoicePrivate::Machine(World), *Me->GetPlayerName(), *Def->Id.ToString(), FKGVoiceCommandCatalog::VoiceName(Voice),
	       *Place.ToString(), *Text, BarkSerial, Delivered);
	return true;
}

void UKGVoiceComponent::ClientBark_Implementation(APlayerState* Speaker, uint8 Command, uint8 Voice, uint8 Serial)
{
	const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Get(Command);
	UWorld* World = GetWorld();
	if (!Def || !Speaker || !World)
	{
		return;
	}
	const UKGChatComponent* Chat = UKGChatComponent::FindForPlayer(Cast<APlayerState>(GetOwner()));
	if (Chat && Speaker != GetOwner() && Chat->IsMuted(Speaker->GetPlayerId()))
	{
		return;
	}
	LastBarkHeard = Def->Id;
	++BarksHeard;
	AKGCharacter* Body = CharacterOf(Speaker);
	if (Body)
	{
		if (UKGMouthComponent* Mouth = Body->GetMouth())
		{
			Mouth->StartBark(Def->Syllables, Serial);
		}
	}
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		const EKGBarkVoice V = static_cast<EKGBarkVoice>(FMath::Min<uint8>(Voice, static_cast<uint8>(EKGBarkVoice::Count) - 1));
		const float Volume = KGVoicePrivate::VoiceVolume();
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FKGVoiceCommandCatalog::WavePath(V, Def->Id)))
		{
			if (Body && Body->GetMesh())
			{
				UGameplayStatics::SpawnSoundAttached(Sound, Body->GetMesh(), TEXT("Head"), FVector::ZeroVector, EAttachLocation::SnapToTarget,
				                                     false, Volume, 1.0f, 0.0f, KGAudio::Near());
			}
			else
			{
				UGameplayStatics::PlaySound2D(this, Sound, Volume);
			}
		}
		APlayerController* PC = GetOwningController();
		if (PC && PC->IsLocalController() && !Def->Emoji.IsEmpty())
		{
			const int32 Emoji = FKGEmoji::Find(Def->Emoji);
			if (Emoji != INDEX_NONE)
			{
				FKGChatUI::ShowBubble(PC, Speaker, Emoji);
			}
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("KG_BARK_RX %s me=%s from=%s id=%s serial=%d mouth=%d"), KGVoicePrivate::Machine(World),
	       *GetNameSafe(GetOwner()), *Speaker->GetPlayerName(), *Def->Id.ToString(), Serial, Body && Body->GetMouth() ? 1 : 0);
}
