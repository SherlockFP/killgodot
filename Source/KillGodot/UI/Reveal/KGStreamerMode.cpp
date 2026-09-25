#include "UI/Reveal/KGStreamerMode.h"

#include "Core/KGGameState.h"
#include "Core/KGGameUserSettings.h"
#include "Core/KGPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "KillGodot.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "UI/Reveal/KGRevealSubsystem.h"

#define LOCTEXT_NAMESPACE "KGStreamer"

namespace KGStreamerPrivate
{
	const UWorld* ResolveWorld(const UWorld* World)
	{
		if (World)
		{
			return World;
		}
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	/** The first local player's state in World (its own name is never masked). */
	const APlayerState* LocalPlayerState(const UWorld* World)
	{
		const APlayerController* PC = World && GEngine ? GEngine->GetFirstLocalPlayerController(const_cast<UWorld*>(World)) : nullptr;
		return PC ? PC->PlayerState.Get() : nullptr;
	}

	bool IsWordChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_');
	}

	FString Bullets(int32 Count)
	{
		FString Out;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Out.AppendChar(TEXT('•'));
		}
		return Out;
	}
}

// --- KGStreamer -------------------------------------------------------------------------------------------------------

bool KGStreamer::IsEnabled()
{
	static const bool bForced = FParse::Param(FCommandLine::Get(), TEXT("KGStreamer"));
	const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	return bForced || (Settings && Settings->GetStreamerMode());
}

FKey KGStreamer::GetPeekKey()
{
	const UKGGameUserSettings* Settings = UKGGameUserSettings::Get();
	const FKey Key(Settings ? Settings->GetStreamerPeekKey() : FName(TEXT("Tab")));
	return Key.IsValid() ? Key : EKeys::Tab;
}

FText KGStreamer::GetPeekKeyLabel()
{
	return GetPeekKey().GetDisplayName(false);
}

bool KGStreamer::IsPeeking(const APlayerController* PC)
{
	return PC && PC->IsLocalController() && PC->IsInputKeyDown(GetPeekKey());
}

bool KGStreamer::IsRoleHidden(const APlayerController* PC)
{
	if (!IsEnabled() || !PC)
	{
		return false;
	}
	const UWorld* World = PC->GetWorld();
	const AKGGameState* GS = World ? World->GetGameState<AKGGameState>() : nullptr;
	if (GS && GS->GetPhase() == EKGPhase::RoleReveal)
	{
		return false;
	}
	if (UKGRevealSubsystem::IsOverlayShown(World))
	{
		return false;
	}
	return !IsPeeking(PC);
}

FString KGStreamer::PlayerKey(const APlayerState* Player)
{
	if (!Player)
	{
		return FString();
	}
	if (const AKGPlayerState* KGPS = Cast<AKGPlayerState>(Player); KGPS && !KGPS->Puid.IsEmpty())
	{
		return TEXT("puid:") + KGPS->Puid;
	}
	if (const FUniqueNetIdPtr NetId = Player->GetUniqueId().GetUniqueNetId(); NetId.IsValid() && !Player->IsABot())
	{
		return TEXT("net:") + NetId->ToString();   // virtual: no CoreOnline link dependency
	}
	return TEXT("name:") + Player->GetPlayerName();
}

FString KGStreamer::DisplayName(const APlayerState* Player)
{
	if (!Player)
	{
		return FString();
	}
	const FString Real = Player->GetPlayerName();
	if (!IsEnabled())
	{
		return Real;
	}
	const UWorld* World = Player->GetWorld();
	if (Player == KGStreamerPrivate::LocalPlayerState(World))
	{
		return Real;
	}
	UKGStreamerSubsystem* Streamer = UKGStreamerSubsystem::Find(World);
	return Streamer ? Streamer->PseudonymOf(Player) : KGStreamerPrivate::Bullets(6);
}

FString KGStreamer::DisplayNameOf(const UWorld* World, const FString& RealName)
{
	if (!IsEnabled() || RealName.IsEmpty())
	{
		return RealName;
	}
	World = KGStreamerPrivate::ResolveWorld(World);
	const APlayerState* Local = KGStreamerPrivate::LocalPlayerState(World);
	if (Local && Local->GetPlayerName() == RealName)
	{
		return RealName;
	}
	UKGStreamerSubsystem* Streamer = UKGStreamerSubsystem::Find(World);
	const FString Pseudonym = Streamer ? Streamer->PseudonymOfName(RealName) : FString();
	// Unknown sender (left the match): still never show the real name.
	return Pseudonym.IsEmpty() ? LOCTEXT("SomeVillager", "A villager").ToString() : Pseudonym;
}

FString KGStreamer::MaskText(const UWorld* World, const FString& Text)
{
	if (!IsEnabled() || Text.IsEmpty())
	{
		return Text;
	}
	UKGStreamerSubsystem* Streamer = UKGStreamerSubsystem::Find(KGStreamerPrivate::ResolveWorld(World));
	return Streamer ? Streamer->MaskText(Text) : Text;
}

FString KGStreamer::DisplayHostName(const UWorld* World, const FString& HostName)
{
	if (!IsEnabled() || HostName.IsEmpty())
	{
		return HostName;
	}
	UKGStreamerSubsystem* Streamer = UKGStreamerSubsystem::Find(KGStreamerPrivate::ResolveWorld(World));
	return Streamer ? Streamer->HostPseudonym(HostName) : KGStreamerPrivate::Bullets(6);
}

FString KGStreamer::MaskCode(const FString& Code)
{
	return IsEnabled() && !Code.IsEmpty() ? KGStreamerPrivate::Bullets(Code.Len()) : Code;
}

FString KGStreamer::MaskAddress(const FString& Address)
{
	if (!IsEnabled() || Address.IsEmpty())
	{
		return Address;
	}
	// Keep the separators so it still reads as "an address", hide every digit / letter.
	FString Out = Address;
	for (TCHAR& C : Out)
	{
		C = FChar::IsAlnum(C) ? TEXT('•') : C;
	}
	return Out;
}

// --- UKGStreamerSubsystem ---------------------------------------------------------------------------------------------

bool UKGStreamerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UKGStreamerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Cosmetic salt, local to this machine and this world (a new match = a new world = new names).
	const FGuid Guid = FGuid::NewGuid();
	Table.Reset((static_cast<uint64>(Guid.A) << 32 | Guid.B) ^ (static_cast<uint64>(Guid.C) << 32 | Guid.D));
}

UKGStreamerSubsystem* UKGStreamerSubsystem::Find(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGStreamerSubsystem>() : nullptr;
}

void UKGStreamerSubsystem::ReserveCurrentNames()
{
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			if (PS)
			{
				Table.ReserveRealName(PS->GetPlayerName());
			}
		}
	}
}

FString UKGStreamerSubsystem::PseudonymOf(const APlayerState* Player)
{
	if (!Player)
	{
		return FString();
	}
	ReserveCurrentNames();
	return Table.Get(KGStreamer::PlayerKey(Player), Player->GetPlayerName());
}

FString UKGStreamerSubsystem::PseudonymOfName(const FString& RealName)
{
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			if (PS && PS->GetPlayerName() == RealName)
			{
				return PseudonymOf(PS);
			}
		}
	}
	return FString();
}

FString UKGStreamerSubsystem::HostPseudonym(const FString& HostName)
{
	Table.ReserveRealName(HostName);
	return Table.Get(TEXT("host:") + HostName, HostName);
}

FString UKGStreamerSubsystem::MaskText(const FString& Text)
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return Text;
	}
	const APlayerState* Local = KGStreamerPrivate::LocalPlayerState(GetWorld());
	TArray<TPair<FString, FString>> Swaps;   // real -> pseudonym, longest real name first
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const FString Real = PS ? PS->GetPlayerName() : FString();
		if (PS && PS != Local && Real.Len() >= 2)
		{
			Swaps.Emplace(Real, PseudonymOf(PS));
		}
	}
	if (Swaps.Num() == 0)
	{
		return Text;
	}
	Swaps.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key.Len() > B.Key.Len(); });
	// One left-to-right pass on whole words, so an inserted pseudonym is never matched again.
	FString Out;
	Out.Reserve(Text.Len() + 16);
	int32 Index = 0;
	while (Index < Text.Len())
	{
		bool bSwapped = false;
		if (Index == 0 || !KGStreamerPrivate::IsWordChar(Text[Index - 1]))
		{
			for (const TPair<FString, FString>& Swap : Swaps)
			{
				const int32 End = Index + Swap.Key.Len();
				if (End <= Text.Len() && FCString::Strncmp(*Text + Index, *Swap.Key, Swap.Key.Len()) == 0 &&
				    (End == Text.Len() || !KGStreamerPrivate::IsWordChar(Text[End])))
				{
					Out += Swap.Value;
					Index = End;
					bSwapped = true;
					break;
				}
			}
		}
		if (!bSwapped)
		{
			Out.AppendChar(Text[Index++]);
		}
	}
	return Out;
}

// --- Console (dev) ----------------------------------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
namespace KGStreamerPrivate
{
	/** kg.Streamer [0|1|dump]: toggle streamer mode for this session, or log how every player name is shown here. */
	FAutoConsoleCommandWithWorldAndArgs GKGStreamerCommand(
		TEXT("kg.Streamer"),
		TEXT("Streamer mode: kg.Streamer 1|0 (this session) | dump (real name -> shown name for every player)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Verb = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("dump"));
			if (Verb == TEXT("1") || Verb == TEXT("0") || Verb == TEXT("on") || Verb == TEXT("off"))
			{
				if (UKGGameUserSettings* Settings = UKGGameUserSettings::Get())
				{
					Settings->SetStreamerMode(Verb == TEXT("1") || Verb == TEXT("on"));
				}
			}
			FString Lines;
			if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
			{
				for (const APlayerState* PS : GS->PlayerArray)
				{
					if (PS)
					{
						Lines += FString::Printf(TEXT(" [%s -> %s]"), *PS->GetPlayerName(), *KGStreamer::DisplayName(PS));
					}
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_STREAMER on=%d peek=%s names:%s"), KGStreamer::IsEnabled() ? 1 : 0,
			       *KGStreamer::GetPeekKey().ToString(), *Lines);
		}));
}
#endif

#undef LOCTEXT_NAMESPACE
