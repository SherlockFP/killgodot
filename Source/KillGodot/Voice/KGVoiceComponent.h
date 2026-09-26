#pragma once

#include "CoreMinimal.h"
#include "Chat/KGChatRules.h"
#include "Components/ActorComponent.h"
#include "Voice/KGVoiceCommands.h"
#include "Voice/KGVoiceRules.h"
#include "KGVoiceComponent.generated.h"

class AKGCharacter;
class APlayerController;
class APlayerState;
class IVoiceCapture;
class IVoiceDecoder;
class IVoiceEncoder;
class UAudioComponent;
class USoundWaveProcedural;

/** ~60 ms of one talker's voice on the wire (Opus frames from the engine's Voice module, or raw PCM as fallback). */
USTRUCT()
struct KILLGODOT_API FKGVoicePacket
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Serial = 0;

	/** EKGVoicePacketFlags */
	UPROPERTY()
	uint8 Flags = 0;

	/** Sender-side RMS, Q15 (the mouth on every machine follows it, whatever the receive gain). */
	UPROPERTY()
	uint16 Amplitude = 0;

	UPROPERTY()
	TArray<uint8> Data;
};

namespace EKGVoicePacketFlags
{
	enum Type : uint8
	{
		None = 0,
		/** Data is 16-bit PCM (no encoder available). */
		RawPCM = 1 << 0,
		/** Generated (kg.Voice.Tone / the voice smoke), not a microphone. */
		Synthetic = 1 << 1
	};
}

/** What this machine heard from one speaker (HUD indicator, mute accounting, the smoke's numbers). */
struct FKGVoiceHeard
{
	double LastTime = -1000.0;
	float LastGain = 0.0f;
	float LastAmplitude = 0.0f;
	int32 Packets = 0;
	int32 DroppedMuted = 0;
	/** Reset by the smoke every second. */
	int32 PacketsWindow = 0;
	float MaxGainWindow = 0.0f;
	int32 PlayedWindow = 0;
	double LastLogTime = -1000.0;
};

/**
 * Proximity voice relay + voice-command barks, one per player state (attached at runtime by UKGVoiceSubsystem, like
 * the chat relay; owned by the player's controller so the owning client may call the Server RPCs).
 *
 * Transport today (Null / LAN subsystem, no credentials): the owning client captures the microphone through the
 * engine's Voice module (16 kHz mono, Opus), or is fed a synthetic stream (InjectPCM), and sends 60 ms packets
 * with ServerVoice. The listen server applies FKGVoiceRules per receiver and forwards each packet only to the
 * machines that may hear it, with the gain (ghost voice never reaches a living machine). Receivers decode into a
 * procedural sound attached to the speaker's head (spatialised, volume = server gain x the Voice slider), push the
 * amplitude into the speaker's UKGMouthComponent and remember who they heard for the HUD indicator. The local mute
 * list is the chat's (/mute): muted speakers are dropped on arrival.
 *
 * EOS later: EOS RTC replaces capture / wire / playback (lobby voice room, no host upload). The seam is
 * OnVoiceHeard(): the RTC unmixed-audio delegate feeds it per participant, FKGVoiceRules::HearGain becomes the
 * per-participant volume set at 10 Hz, mouths, indicators, barks and the smoke stay as they are
 * (Docs/Research/TechResearch.md §4).
 */
UCLASS(ClassGroup = (KillGodot), meta = (BlueprintSpawnableComponent))
class KILLGODOT_API UKGVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKGVoiceComponent();

	static constexpr int32 SampleRate = 16000;
	static constexpr int32 NumChannels = 1;
	/** 20 ms Opus frames, three per packet. */
	static constexpr int32 FrameSamples = 320;
	static constexpr int32 PacketSamples = FrameSamples * 3;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static UKGVoiceComponent* FindForPlayer(const APlayerState* PlayerState);
	static UKGVoiceComponent* FindForController(const APlayerController* Controller);

	// ---- owning client: transmit ----

	/** Push-to-talk held / open mic on. Starts the microphone on first use. */
	void SetTransmitting(bool bOn);
	bool IsTransmitting() const { return bTransmitting; }
	/** A capture device was opened (false headless / no microphone: only injected audio goes out). */
	bool HasMicrophone() const { return bHasMicrophone; }
	/** Own outgoing level 0..1 (HUD mic meter). */
	float GetLocalAmplitude() const { return LocalAmplitude; }
	/** Synthetic 16 kHz mono PCM into the same encode -> send path as the microphone (smoke, kg.Voice.Tone). */
	void InjectPCM(const int16* Samples, int32 Num);
	/** Generate a 220 Hz tone while on (dev). */
	void SetSyntheticTone(bool bOn) { bTone = bOn; }
	bool IsSyntheticToneOn() const { return bTone; }
	int32 GetTxPackets() const { return TxPackets; }
	const TCHAR* GetCodecName() const { return bOpus ? TEXT("opus") : TEXT("raw"); }

	// ---- barks (voice commands) ----

	/** Owning client: say a voice command (Z/X/C radials, kg.Bark). */
	void RequestBark(FName CommandId);
	/** Authority: bark on behalf of this player (bots, dev verbs). bIgnoreRateLimit for kg.Bark.Bots. */
	bool ServerSayBark(FName CommandId, bool bIgnoreRateLimit = false);
	uint8 GetBarkSerial() const { return BarkSerial; }
	/** Last bark this machine presented for its player (tests / smokes). */
	FName GetLastBarkHeard() const { return LastBarkHeard; }
	int32 GetBarksHeard() const { return BarksHeard; }

	// ---- this machine: what it hears ----

	const FKGVoiceHeard* GetHeard(int32 SpeakerId) const { return Heard.Find(SpeakerId); }
	/** Heard a packet from Speaker within Window seconds (speaking indicator). */
	bool WasHeardRecently(const APlayerState* Speaker, double Window = 0.35) const;
	TMap<int32, FKGVoiceHeard>& GetHeardMap() { return Heard; }
	/** EOS seam / injected receive: one decoded (or opaque) chunk from Speaker at Gain. */
	void OnVoiceHeard(APlayerState* Speaker, const FKGVoicePacket& Packet, float Gain);

	// ---- server ----

	int32 GetRoutedPackets() const { return RoutedPackets; }
	/** Non-shipping: the smoke's current step, replicated so the client can label what it hears. */
	void SetSmokeTag(FName Tag);
	FName GetSmokeTag() const { return SmokeTag; }

	/** Local voice channel label for the HUD (Nearby / Square / Ghost / Closed). */
	EKGVoiceChannel GetLocalChannel() const;

protected:
	UFUNCTION(Server, Unreliable)
	void ServerVoice(const FKGVoicePacket& Packet);

	UFUNCTION(Client, Unreliable)
	void ClientVoice(APlayerState* Speaker, const FKGVoicePacket& Packet, uint8 Gain);

	UFUNCTION(Server, Reliable)
	void ServerBark(FName CommandId);

	UFUNCTION(Client, Reliable)
	void ClientBark(APlayerState* Speaker, uint8 Command, uint8 Voice, uint8 Serial);

	UPROPERTY(Replicated)
	FName SmokeTag;

	/** Procedural waves per talker (strong refs; FTalker keeps weak ones). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWaveProcedural>> Waves;

private:
	struct FTalker
	{
		TSharedPtr<IVoiceDecoder> Decoder;
		TWeakObjectPtr<USoundWaveProcedural> Wave;
		TWeakObjectPtr<UAudioComponent> Audio;
		TWeakObjectPtr<AActor> AttachedTo;
	};

	APlayerController* GetOwningController() const;
	bool IsLocallyOwned() const;
	AKGCharacter* CharacterOf(const APlayerState* PS) const;
	void EnsureCapture();
	void ShutdownCapture();
	void TickCapture(float DeltaTime);
	void SendChunk(const int16* Samples, bool bSynthetic);
	void PlayVoice(APlayerState* Speaker, const FKGVoicePacket& Packet, float Gain);
	FTalker& TalkerFor(APlayerState* Speaker);
	void RouteLog(const APlayerState* To, float Gain, float Dist);

	// transmit
	TSharedPtr<IVoiceCapture> Capture;
	TSharedPtr<IVoiceEncoder> Encoder;
	TArray<uint8> PendingPCM;
	TArray<uint8> ScratchPCM;
	TArray<uint8> ScratchEncoded;
	bool bTransmitting = false;
	bool bHasMicrophone = false;
	bool bCaptureTried = false;
	bool bOpus = false;
	bool bTone = false;
	float TonePhase = 0.0f;
	float LocalAmplitude = 0.0f;
	uint8 TxSerial = 0;
	int32 TxPackets = 0;

	// receive
	TMap<int32, FKGVoiceHeard> Heard;
	TMap<int32, FTalker> Talkers;
	FName LastBarkHeard;
	int32 BarksHeard = 0;

	// server
	FKGChatRateLimiter BarkLimiter;
	uint8 BarkSerial = 0;
	int32 RoutedPackets = 0;
	TMap<int32, double> RouteLogTimes;
	double LastRejectLog = -1000.0;
};
