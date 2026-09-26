#include "Chat/KGChatComponent.h"
#include "Chat/KGEmoji.h"
#include "Emote/KGEmoteCatalog.h"
#include "Emote/KGEmoteComponent.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Roles/KGRoleListGenerator.h"

#define LOCTEXT_NAMESPACE "KGChat"

namespace KGChatComponentPrivate
{
	/** Chat view of FKGEmoteCatalog (the strings live in the catalog's static table). */
	const TArray<FKGChatEmote>& ChatEmotes()
	{
		static const TArray<FKGChatEmote> Rows = []()
		{
			static TArray<FString> Ids;
			const TArray<FKGEmoteDef>& All = FKGEmoteCatalog::GetAll();
			Ids.Reset();
			for (const FKGEmoteDef& Def : All)
			{
				Ids.Add(Def.Id.ToString());
			}
			TArray<FKGChatEmote> Out;
			for (int32 i = 0; i < All.Num(); ++i)
			{
				Out.Add({*Ids[i], *All[i].Emoji, *All[i].Verb});
			}
			return Out;
		}();
		return Rows;
	}

	const TCHAR* ChannelName(EKGChatChannel Channel)
	{
		switch (Channel)
		{
		case EKGChatChannel::All: return TEXT("All");
		case EKGChatChannel::Nearby: return TEXT("Nearby");
		case EKGChatChannel::Team: return TEXT("Team");
		case EKGChatChannel::Dead: return TEXT("Dead");
		default: return TEXT("System");
		}
	}

	void PhaseInfo(const UWorld* World, EKGPhase& OutPhase, float& OutRemaining)
	{
		const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
		OutPhase = GS ? GS->GetPhase() : EKGPhase::Lobby;
		OutRemaining = GS ? GS->GetPhaseRemaining() : 0.0f;
	}

	APlayerState* FindPlayerByName(const UWorld* World, const FString& Name, const APlayerState* Except)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS || Name.IsEmpty())
		{
			return nullptr;
		}
		APlayerState* Prefix = nullptr;
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS || PS == Except)
			{
				continue;
			}
			if (PS->GetPlayerName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return PS;
			}
			if (!Prefix && PS->GetPlayerName().StartsWith(Name, ESearchCase::IgnoreCase))
			{
				Prefix = PS;
			}
		}
		return Prefix;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// FKGChatEmote
// ---------------------------------------------------------------------------------------------------------------

const FKGChatEmote* FKGChatEmote::Find(FName Id)
{
	// Ids and aliases ("cheers", "o7") resolve to the catalog row.
	const int32 Index = FKGEmoteCatalog::IndexOf(Id);
	const TArray<FKGChatEmote>& Rows = KGChatComponentPrivate::ChatEmotes();
	return Rows.IsValidIndex(Index) ? &Rows[Index] : nullptr;
}

TArrayView<const FKGChatEmote> FKGChatEmote::GetAll()
{
	return MakeArrayView(KGChatComponentPrivate::ChatEmotes());
}

// ---------------------------------------------------------------------------------------------------------------
// UKGChatComponent
// ---------------------------------------------------------------------------------------------------------------

UKGChatComponent::UKGChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReactLimiter.Burst = 3.0f;
	ReactLimiter.RefillPerSecond = 1.0f;
	ReactLimiter.DuplicateWindow = 0.0f;
}

void UKGChatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UKGChatComponent, bSilenced, Params);
}

FKGOnChatReaction& UKGChatComponent::OnReaction()
{
	static FKGOnChatReaction Delegate;
	return Delegate;
}

FKGOnEmoteRequested& UKGChatComponent::OnEmoteRequested()
{
	static FKGOnEmoteRequested Delegate;
	return Delegate;
}

FKGEmoteGate& UKGChatComponent::EmoteGate()
{
	static FKGEmoteGate Delegate;
	return Delegate;
}

UKGChatComponent* UKGChatComponent::FindForPlayer(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->FindComponentByClass<UKGChatComponent>() : nullptr;
}

UKGChatComponent* UKGChatComponent::FindForController(const APlayerController* Controller)
{
	return Controller ? FindForPlayer(Controller->PlayerState) : nullptr;
}

APlayerController* UKGChatComponent::GetOwningController() const
{
	const APlayerState* PS = Cast<APlayerState>(GetOwner());
	return PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;
}

bool UKGChatComponent::IsLocallyOwned() const
{
	const APlayerController* PC = GetOwningController();
	return PC && PC->IsLocalController();
}

FKGChatParticipant UKGChatComponent::MakeParticipant(const APlayerState* PlayerState)
{
	FKGChatParticipant P;
	const AKGPlayerState* KGPS = Cast<AKGPlayerState>(PlayerState);
	if (!KGPS)
	{
		return P;
	}
	P.Life = KGPS->LifeState;
	// The private role is known on the server and on the owning client only (COND_OwnerOnly).
	const FName RoleId = KGPS->GetPrivateRoleId();
	if (!RoleId.IsNone())
	{
		if (const FKGRoleInfo* Role =
			    FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), RoleId))
		{
			P.bHasRole = true;
			P.Faction = Role->Faction;
		}
	}
	if (const UKGChatComponent* Chat = FindForPlayer(KGPS))
	{
		P.bSilenced = Chat->bSilenced;
	}
	if (const AKGGameState* GS = KGPS->GetWorld() ? KGPS->GetWorld()->GetGameState<AKGGameState>() : nullptr)
	{
		P.bOnTrial = GS->OnTrial == KGPS;
	}
	if (P.Life != EKGLifeState::Ghost)
	{
		if (const APawn* Pawn = KGPS->GetPawn())
		{
			P.Location = Pawn->GetActorLocation();
			P.bHasLocation = true;
		}
	}
	return P;
}

FKGChatParticipant UKGChatComponent::MakeLocalParticipant() const
{
	return MakeParticipant(Cast<APlayerState>(GetOwner()));
}

FKGChatMessage UKGChatComponent::MakeMessage(EKGChatChannel Channel, const FString& Text, uint8 Flags) const
{
	FKGChatMessage Message;
	Message.Channel = Channel;
	Message.Text = Text;
	Message.Flags = Flags;
	if (const APlayerState* PS = Cast<APlayerState>(GetOwner()))
	{
		Message.SenderName = PS->GetPlayerName();
		Message.SenderId = PS->GetPlayerId();
		const AKGPlayerState* KGPS = Cast<AKGPlayerState>(PS);
		Message.ColorSeed = FKGChatRules::MakeColorSeed(KGPS ? KGPS->Puid : FString(), Message.SenderName);
	}
	return Message;
}

// ---- client requests ------------------------------------------------------------------------------------------

void UKGChatComponent::SubmitInput(EKGChatChannel Channel, const FString& Typed)
{
	using namespace KGChatComponentPrivate;
	const FString Text = Typed.TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}
	if (!Text.StartsWith(TEXT("/")))
	{
		RequestSend(Channel, Text);
		return;
	}
	FString Word = Text;
	FString Rest;
	int32 Space = INDEX_NONE;
	if (Text.FindChar(TEXT(' '), Space))
	{
		Word = Text.Left(Space);
		Rest = Text.Mid(Space + 1);
	}
	Word = Word.ToLower();
	Rest = Rest.TrimStartAndEnd();

	EKGChatChannel Switch;
	if (FKGChatRules::ParseChannelCommand(Word, Switch))
	{
		RequestSend(Switch, Rest);
		return;
	}
	if (Word == TEXT("/r") || Word == TEXT("/react") || Word == TEXT("/e"))
	{
		FString Name = Rest.Replace(TEXT(":"), TEXT(""));
		int32 Index = FKGEmoji::Find(Name);
		if (Index == INDEX_NONE)
		{
			const FString Converted = FKGEmoji::ConvertAll(Rest);
			Index = Converted.IsEmpty() ? INDEX_NONE : FKGEmoji::FromChar(Converted[0]);
		}
		if (Index == INDEX_NONE)
		{
			AddHint(LOCTEXT("UnknownEmoji", "Unknown emoji. Try /emojis."));
			return;
		}
		RequestReaction(Index);
		return;
	}
	if (Word == TEXT("/mute") || Word == TEXT("/block") || Word == TEXT("/unmute") || Word == TEXT("/unblock"))
	{
		const bool bMute = !Word.StartsWith(TEXT("/un"));
		const APlayerState* Target = FindPlayerByName(GetWorld(), Rest, Cast<APlayerState>(GetOwner()));
		if (!Target)
		{
			AddHint(LOCTEXT("NoSuchPlayer", "No player with that name."));
			return;
		}
		SetMuted(Target->GetPlayerId(), bMute);
		AddHint(FText::Format(bMute ? LOCTEXT("Muted", "Muted {0}. Their messages are hidden (/unmute {0}).")
		                            : LOCTEXT("Unmuted", "Unmuted {0}."),
		                      FText::FromString(Target->GetPlayerName())));
		return;
	}
	if (Word == TEXT("/report"))
	{
		const APlayerState* Target = FindPlayerByName(GetWorld(), Rest, Cast<APlayerState>(GetOwner()));
		if (!Target)
		{
			AddHint(LOCTEXT("NoSuchPlayerReport", "Usage: /report <name>"));
			return;
		}
		// Stub until the EOS reports interface lands (M3): the log keeps the evidence.
		UE_LOG(LogKillGodot, Warning, TEXT("[Chat] REPORT by %s against %s (id %d)"),
		       *GetNameSafe(GetOwner()), *Target->GetPlayerName(), Target->GetPlayerId());
		for (const FKGChatLine& Line : History)
		{
			if (Line.Message.SenderId == Target->GetPlayerId())
			{
				UE_LOG(LogKillGodot, Warning, TEXT("[Chat]   evidence: %s"), *FKGEmoji::ToPlainText(Line.Message.Text));
			}
		}
		AddHint(FText::Format(LOCTEXT("Reported", "Report about {0} noted. You can also /mute them."),
		                      FText::FromString(Target->GetPlayerName())));
		return;
	}
	if (Word == TEXT("/help") || Word == TEXT("/?"))
	{
		AddHint(LOCTEXT("Help1", "Enter/T: talk   Tab: switch channel   G (hold): emoji + emote wheel   Shift+1-4: favourite emotes   Esc: cancel"));
		AddHint(LOCTEXT("Help2", "/all /near /team /dead <text>   /r <emoji>   /wave /dance /sit ... (/emotes)   /stop   /mute <name>   /report <name>"));
		AddHint(LOCTEXT("Help3", "Emojis: :skull: :sus: :knife: ...  or  :) :D :( ;) <3  - see /emojis"));
		return;
	}
	if (Word == TEXT("/emojis") || Word == TEXT("/emoji"))
	{
		FString Line;
		for (int32 i = 0; i < FKGEmoji::Num(); ++i)
		{
			Line += FString::Printf(TEXT("%c:%s: "), FKGEmoji::ToChar(i), FKGEmoji::GetId(i));
		}
		FKGChatMessage Message;
		Message.Channel = EKGChatChannel::System;
		Message.Text = Line;
		Message.Flags = EKGChatFlags::Local | EKGChatFlags::Hint;
		AddLocalLine(Message);
		return;
	}
	if (Word == TEXT("/stop"))
	{
		if (UKGEmoteComponent* Emote = UKGEmoteComponent::FindForPlayer(Cast<APlayerState>(GetOwner())))
		{
			Emote->RequestStop();
		}
		return;
	}
	if (Word == TEXT("/emotes"))
	{
		FString Line;
		for (const FKGChatEmote& Emote : FKGChatEmote::GetAll())
		{
			Line += FString::Printf(TEXT("/%s  "), Emote.Id);
		}
		AddHint(FText::FromString(Line));
		return;
	}
	if (const FKGChatEmote* Emote = FKGChatEmote::Find(FName(*Word.Mid(1))))
	{
		RequestEmote(FName(Emote->Id));
		return;
	}
	AddHint(LOCTEXT("UnknownCommand", "Unknown command. Type /help."));
}

void UKGChatComponent::RequestSend(EKGChatChannel Channel, const FString& Text)
{
	if (Text.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	ServerSendChat(Channel, Text.Left(FKGChatRules::MaxChars * 4));
}

void UKGChatComponent::RequestReaction(int32 EmojiIndex)
{
	if (EmojiIndex >= 0 && EmojiIndex < FKGEmoji::Num())
	{
		ServerReact(static_cast<uint8>(EmojiIndex));
	}
}

void UKGChatComponent::RequestEmote(FName EmoteId)
{
	ServerEmote(EmoteId);
}

// ---- server ---------------------------------------------------------------------------------------------------

void UKGChatComponent::ServerSendChat_Implementation(EKGChatChannel Channel, const FString& Text)
{
	HandleSend(Channel, Text);
}

void UKGChatComponent::HandleSend(EKGChatChannel Channel, const FString& Text)
{
	using namespace KGChatComponentPrivate;
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FString Clean = FKGChatRules::Sanitize(Text);
	if (Clean.IsEmpty())
	{
		return;
	}
	EKGPhase Phase;
	float Remaining;
	PhaseInfo(World, Phase, Remaining);
	const FKGChatParticipant Sender = MakeParticipant(Cast<APlayerState>(GetOwner()));
	EKGChatReject Reject = FKGChatRules::CanSend(Sender, Channel, Phase, Remaining);
	if (Reject == EKGChatReject::None)
	{
		Reject = ChatLimiter.TryConsume(World->GetRealTimeSeconds(), Clean);
	}
	if (Reject != EKGChatReject::None)
	{
		UE_LOG(LogKillGodot, Verbose, TEXT("[Chat] rejected %s on %s: %d"), *GetNameSafe(GetOwner()),
		       ChannelName(Channel), static_cast<int32>(Reject));
		ClientChatRejected(Reject);
		return;
	}
	const FKGChatMessage Message = MakeMessage(Channel, Clean, EKGChatFlags::None);
	UE_LOG(LogKillGodot, Log, TEXT("[Chat][%s] %s: %s"), ChannelName(Channel), *Message.SenderName,
	       *FKGEmoji::ToPlainText(Clean));
	Deliver(Message, Sender, false, INDEX_NONE);
}

void UKGChatComponent::ServerReact_Implementation(uint8 EmojiIndex)
{
	if (EmojiIndex < FKGEmoji::Num())
	{
		HandleReact(EmojiIndex, nullptr);
	}
}

void UKGChatComponent::ServerEmote_Implementation(FName EmoteId)
{
	if (const FKGChatEmote* Emote = FKGChatEmote::Find(EmoteId))
	{
		HandleReact(FKGEmoji::Find(Emote->Emoji), Emote);
	}
}

void UKGChatComponent::HandleReact(int32 EmojiIndex, const FKGChatEmote* Emote)
{
	using namespace KGChatComponentPrivate;
	UWorld* World = GetWorld();
	APlayerState* Me = Cast<APlayerState>(GetOwner());
	if (!World || !Me || !Me->HasAuthority() || EmojiIndex == INDEX_NONE)
	{
		return;
	}
	EKGPhase Phase;
	float Remaining;
	PhaseInfo(World, Phase, Remaining);
	const FKGChatParticipant Sender = MakeParticipant(Me);
	if (!FKGChatRules::CanReact(Sender, Phase, Remaining))
	{
		ClientChatRejected(Sender.Life == EKGLifeState::Revenant ? EKGChatReject::Revenant : EKGChatReject::Closed);
		return;
	}
	if (ReactLimiter.TryConsume(World->GetRealTimeSeconds(), FString()) != EKGChatReject::None)
	{
		return;   // quietly: a held key spamming reactions is not worth a hint
	}
	if (Emote && Sender.Life != EKGLifeState::Ghost && EmoteGate().IsBound())
	{
		// The body goes first: a refused emote (moving, busy, rate limit) pops no bubble and no narration.
		const EKGChatReject Gate = EmoteGate().Execute(Me, FName(Emote->Id));
		if (Gate != EKGChatReject::None)
		{
			ClientChatRejected(Gate);
			return;
		}
	}
	UE_LOG(LogKillGodot, Log, TEXT("[Chat][React] %s: %s%s"), *Me->GetPlayerName(), FKGEmoji::GetId(EmojiIndex),
	       Emote ? *FString::Printf(TEXT(" (emote %s)"), Emote->Id) : TEXT(""));

	const AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return;
	}
	if (Sender.Life == EKGLifeState::Ghost)
	{
		// Ghosts have no body to float a bubble over: the reaction becomes a Dead-chat line.
		const FString Text = Emote ? FString::Printf(TEXT("%s %c"), Emote->Verb, FKGEmoji::ToChar(EmojiIndex))
		                           : FString::Chr(FKGEmoji::ToChar(EmojiIndex));
		Deliver(MakeMessage(EKGChatChannel::Dead, Text,
		                    Emote ? EKGChatFlags::Action : EKGChatFlags::Reaction), Sender, false, INDEX_NONE);
	}
	else
	{
		Deliver(FKGChatMessage(), Sender, true, EmojiIndex);
		if (Emote)
		{
			const FKGChatMessage Action = MakeMessage(EKGChatChannel::Nearby, Emote->Verb, EKGChatFlags::Action);
			// Seen rather than heard: same audience as the bubble.
			for (APlayerState* PS : GS->PlayerArray)
			{
				UKGChatComponent* Target = FindForPlayer(PS);
				if (Target && Target->GetOwningController() &&
				    FKGChatRules::CanSeeReaction(Sender, MakeParticipant(PS), PS == Me))
				{
					Target->ClientReceiveChat(Action);
				}
			}
		}
	}
	if (Emote)
	{
		OnEmoteRequested().Broadcast(Me, FName(Emote->Id));
	}
}

void UKGChatComponent::Deliver(const FKGChatMessage& Message, const FKGChatParticipant& Sender, bool bReaction,
                               int32 EmojiIndex)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	APlayerState* Me = Cast<APlayerState>(GetOwner());
	if (!GS)
	{
		return;
	}
	int32 Delivered = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		UKGChatComponent* Target = FindForPlayer(PS);
		if (!Target || !Target->GetOwningController())
		{
			continue;   // bots: nobody to show it to
		}
		const bool bSelf = PS == Me;
		const FKGChatParticipant Receiver = MakeParticipant(PS);
		if (bReaction)
		{
			if (FKGChatRules::CanSeeReaction(Sender, Receiver, bSelf))
			{
				Target->ClientReaction(Me, static_cast<uint8>(EmojiIndex));
				++Delivered;
			}
		}
		else if (FKGChatRules::CanReceive(Sender, Receiver, Message.Channel, bSelf))
		{
			Target->ClientReceiveChat(Message);
			++Delivered;
		}
	}
	UE_LOG(LogKillGodot, Verbose, TEXT("[Chat] delivered to %d"), Delivered);
}

void UKGChatComponent::ServerSay(EKGChatChannel Channel, const FString& Text, uint8 Flags, int32 EmojiIndex)
{
	APlayerState* Me = Cast<APlayerState>(GetOwner());
	if (!Me || !Me->HasAuthority())
	{
		return;
	}
	const FKGChatParticipant Sender = MakeParticipant(Me);
	const FString Clean = FKGChatRules::Sanitize(Text);
	if (!Clean.IsEmpty())
	{
		Deliver(MakeMessage(Channel, Clean, Flags), Sender, false, INDEX_NONE);
	}
	if (EmojiIndex != INDEX_NONE && EmojiIndex < FKGEmoji::Num())
	{
		Deliver(FKGChatMessage(), Sender, true, EmojiIndex);
	}
}

void UKGChatComponent::BroadcastSystem(UWorld* World, const FString& Text)
{
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || !GS->HasAuthority())
	{
		return;
	}
	FKGChatMessage Message;
	Message.Channel = EKGChatChannel::System;
	Message.Text = FKGChatRules::Sanitize(Text);
	for (APlayerState* PS : GS->PlayerArray)
	{
		UKGChatComponent* Target = FindForPlayer(PS);
		if (Target && Target->GetOwningController())
		{
			Target->ClientReceiveChat(Message);
		}
	}
}

void UKGChatComponent::SetSilenced(bool bInSilenced)
{
	if (GetOwner() && GetOwner()->HasAuthority() && bSilenced != bInSilenced)
	{
		bSilenced = bInSilenced;
		MARK_PROPERTY_DIRTY_FROM_NAME(UKGChatComponent, bSilenced, this);
	}
}

// ---- client ---------------------------------------------------------------------------------------------------

void UKGChatComponent::ClientReceiveChat_Implementation(const FKGChatMessage& Message)
{
	const APlayerState* Me = Cast<APlayerState>(GetOwner());
	const bool bFromMe = Me && Message.SenderId == Me->GetPlayerId();
	if (!bFromMe && Message.Channel != EKGChatChannel::System && IsMuted(Message.SenderId))
	{
		return;
	}
	AddLocalLine(Message);
}

void UKGChatComponent::ClientReaction_Implementation(APlayerState* Sender, uint8 EmojiIndex)
{
	if (Sender && Sender != GetOwner() && IsMuted(Sender->GetPlayerId()))
	{
		return;
	}
	OnReaction().Broadcast(GetOwningController(), Sender, EmojiIndex);
}

void UKGChatComponent::ClientChatRejected_Implementation(EKGChatReject Reason)
{
	const FText Text = FKGChatRules::RejectReason(Reason);
	if (!Text.IsEmpty())
	{
		AddHint(Text);
	}
}

void UKGChatComponent::AddLocalLine(const FKGChatMessage& Message)
{
	FKGChatLine& Line = History.AddDefaulted_GetRef();
	Line.Message = Message;
	Line.ReceivedAt = FPlatformTime::Seconds();
	Line.Serial = NextSerial++;
	const FKGChatLine Copy = Line;
	if (History.Num() > HistorySize)
	{
		History.RemoveAt(0, History.Num() - HistorySize);
	}
	OnLine.Broadcast(Copy);
}

void UKGChatComponent::AddHint(const FText& Text)
{
	FKGChatMessage Message;
	Message.Channel = EKGChatChannel::System;
	Message.Text = Text.ToString();
	Message.Flags = EKGChatFlags::Local | EKGChatFlags::Hint;
	AddLocalLine(Message);
}

void UKGChatComponent::ClearHistory()
{
	History.Reset();
}

void UKGChatComponent::SetMuted(int32 PlayerId, bool bMuted)
{
	if (bMuted)
	{
		MutedIds.Add(PlayerId);
	}
	else
	{
		MutedIds.Remove(PlayerId);
	}
}

#undef LOCTEXT_NAMESPACE
