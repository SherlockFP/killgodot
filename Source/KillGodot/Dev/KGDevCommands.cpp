#include "Dev/KGDevCommands.h"

#if !UE_BUILD_SHIPPING

#include "AI/KGBotController.h"
#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Character/KGViewmodelComponent.h"
#include "Combat/KGHealthComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Cosmetics/KGProfileSave.h"
#include "Dev/KGDevComponent.h"
#include "Dev/KGDevSubsystem.h"
#include "Dom/JsonObject.h"
#include "Chat/KGChatComponent.h"
#include "Chores/KGChoreComponent.h"
#include "Chores/KGChoreTypes.h"
#include "Emote/KGEmoteCatalog.h"
#include "Emote/KGEmoteComponent.h"
#include "Emote/KGEmoteSubsystem.h"
#include "Voice/KGVoiceCommands.h"
#include "Voice/KGVoiceComponent.h"
#include "Voice/KGVoiceSubsystem.h"
#include "Voice/KGVoiceUI.h"
#include "Fishing/KGFishingComponent.h"
#include "Fishing/KGFishingTypes.h"
#include "Fishing/KGFishMarket.h"
#include "Inventory/KGInventoryComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/Light.h"
#include "Engine/NetDriver.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Inventory/KGItemCatalog.h"
#include "Inventory/KGPlayerExtrasSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "KillGodot.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Roles/KGRoleListGenerator.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "World/KGBreakable.h"
#include "World/KGFoliageField.h"
#include "World/KGGrassField.h"
#include "World/KGMapInfo.h"
#include "World/KGTaskStation.h"
#include "Dig/KGDigDev.h"            // KG_DIG hook: Dig.* verbs
#include "Traps/KGTrapDev.h"        // SPRINT-040 hook: Trap.* verbs
#include "Manor/KGManorDev.h"       // SPRINT-040 hook: Manor.* verbs
#include "Dig/KGUndergroundInfo.h"   // KG_DIG hook: GroundAt stays on the surface

namespace KGDevPrivate
{
	TAutoConsoleVariable<int32> CVarAllowClients(
		TEXT("kg.Dev.AllowClients"), 0,
		TEXT("Dev panel: 1 lets clients run server verbs (match, bots, cheats) through the host. 0 = host only."));

	using FVerbFn = TFunction<FKGDevResult(const FKGDevContext&, const TArray<FString>&)>;

	struct FVerb
	{
		FString Name;
		FString Usage;
		FString Help;
		EKGDevScope Scope = EKGDevScope::Server;
		FVerbFn Run;
	};

	// ---- argument helpers ---------------------------------------------------------------------------------------

	/** Splits on whitespace; "double quotes" keep spaces (bot names). */
	TArray<FString> Tokenize(const FString& Line)
	{
		TArray<FString> Out;
		FString Current;
		bool bQuoted = false;
		for (const TCHAR Ch : Line)
		{
			if (Ch == TEXT('"'))
			{
				bQuoted = !bQuoted;
			}
			else if (FChar::IsWhitespace(Ch) && !bQuoted)
			{
				if (!Current.IsEmpty())
				{
					Out.Add(MoveTemp(Current));
					Current.Reset();
				}
			}
			else
			{
				Current.AppendChar(Ch);
			}
		}
		if (!Current.IsEmpty())
		{
			Out.Add(MoveTemp(Current));
		}
		return Out;
	}

	int32 IntArg(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) && Args[Index].IsNumeric() ? FCString::Atoi(*Args[Index]) : Default;
	}

	float FloatArg(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) && Args[Index].IsNumeric() ? FCString::Atof(*Args[Index]) : Default;
	}

	/** "1/on/true" -> true, "0/off/false" -> false, missing -> !Current (toggle). */
	bool BoolArg(const TArray<FString>& Args, int32 Index, bool Current)
	{
		if (!Args.IsValidIndex(Index))
		{
			return !Current;
		}
		const FString& A = Args[Index];
		return A == TEXT("1") || A.Equals(TEXT("on"), ESearchCase::IgnoreCase) || A.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	FString Rest(const TArray<FString>& Args, int32 From)
	{
		FString Out;
		for (int32 i = From; i < Args.Num(); ++i)
		{
			Out += (i > From ? TEXT(" ") : TEXT("")) + Args[i];
		}
		return Out;
	}

	FString PhaseName(EKGPhase Phase)
	{
		return StaticEnum<EKGPhase>()->GetNameStringByValue(static_cast<int64>(Phase));
	}

	bool ParsePhase(const FString& Text, EKGPhase& Out)
	{
		const UEnum* Enum = StaticEnum<EKGPhase>();
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
		{
			if (Enum->GetNameStringByIndex(i).Equals(Text, ESearchCase::IgnoreCase))
			{
				Out = static_cast<EKGPhase>(Enum->GetValueByIndex(i));
				return true;
			}
		}
		return false;
	}

	bool ParseAlignment(const FString& Text, EKGAlignment& Out)
	{
		const UEnum* Enum = StaticEnum<EKGAlignment>();
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
		{
			if (Enum->GetNameStringByIndex(i).Equals(Text, ESearchCase::IgnoreCase))
			{
				Out = static_cast<EKGAlignment>(Enum->GetValueByIndex(i));
				return true;
			}
		}
		return false;
	}

	FString SeedText(int64 Seed)
	{
		return Seed == 0 ? FString(TEXT("-")) : FString::Printf(TEXT("%016llX"), static_cast<uint64>(Seed));
	}

	/** "fish_market" -> "Fish Market". */
	FString Pretty(const FString& Id)
	{
		FString Out = Id.Replace(TEXT("_"), TEXT(" "));
		bool bStart = true;
		for (TCHAR& Ch : Out)
		{
			if (bStart)
			{
				Ch = FChar::ToUpper(Ch);
			}
			bStart = Ch == TEXT(' ');
		}
		return Out;
	}

	const FKGRoleInfo* RoleOf(const AKGPlayerState* PS)
	{
		return PS ? FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), PS->GetPrivateRoleId())
		          : nullptr;
	}

	// ---- context helpers ----------------------------------------------------------------------------------------

	AKGGameMode* GameMode(const FKGDevContext& C)
	{
		return C.World ? C.World->GetAuthGameMode<AKGGameMode>() : nullptr;
	}

	AKGGameState* GameState(const FKGDevContext& C)
	{
		return C.World ? C.World->GetGameState<AKGGameState>() : nullptr;
	}

	AKGPlayerState* MyPS(const FKGDevContext& C)
	{
		return C.Requester ? C.Requester->GetPlayerState<AKGPlayerState>() : nullptr;
	}

	AKGCharacter* CharacterOf(const APlayerState* PS)
	{
		return PS ? Cast<AKGCharacter>(PS->GetPawn()) : nullptr;
	}

	AController* ControllerOf(const APlayerState* PS)
	{
		return PS ? Cast<AController>(PS->GetOwner()) : nullptr;
	}

	/** "#3" = PlayerArray index, else exact name, else first name containing the text (case-insensitive). */
	AKGPlayerState* FindPlayer(UWorld* World, const FString& Query)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		if (!GS || Query.IsEmpty())
		{
			return nullptr;
		}
		if (Query.StartsWith(TEXT("#")))
		{
			const int32 Index = FCString::Atoi(*Query.Mid(1));
			return GS->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS->PlayerArray[Index]) : nullptr;
		}
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (PS && PS->GetPlayerName().Equals(Query, ESearchCase::IgnoreCase))
			{
				return Cast<AKGPlayerState>(PS);
			}
		}
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (PS && PS->GetPlayerName().Contains(Query, ESearchCase::IgnoreCase))
			{
				return Cast<AKGPlayerState>(PS);
			}
		}
		return nullptr;
	}

	/** Moves the requester (pawn, or spectator when a ghost) to Where, facing Yaw. */
	FKGDevResult TeleportRequester(const FKGDevContext& C, const FVector& Where, float Yaw, const FString& What)
	{
		APawn* Pawn = C.Requester ? C.Requester->GetPawnOrSpectator() : nullptr;
		if (!Pawn)
		{
			return FKGDevResult::Fail(TEXT("No pawn to teleport"));
		}
		Pawn->TeleportTo(Where, FRotator(0.0f, Yaw, 0.0f), false, true);
		C.Requester->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		C.Requester->ClientSetRotation(FRotator(0.0f, Yaw, 0.0f));
		return FKGDevResult::Ok(FString::Printf(TEXT("Teleported to %s"), *What));
	}

	UKGDevComponent* MyDev(const FKGDevContext& C)
	{
		return UKGDevComponent::FindFor(C.Requester);
	}

#define KG_DEV_REQUIRE(Condition, Message) \
	if (!(Condition))                        \
	{                                        \
		return FKGDevResult::Fail(Message);  \
	}

	// ---- local view helpers -------------------------------------------------------------------------------------

	/** Actors of a perf-test group: Grass, Foliage, Dress (set dressing), Village (level builder props). */
	bool IsInGroup(AActor* Actor, FName Group)
	{
		if (Group == TEXT("Grass"))
		{
			return Actor->IsA<AKGGrassField>();
		}
		if (Group == TEXT("Foliage"))
		{
			return Actor->IsA<AKGFoliageField>() || Actor->GetClass()->GetName() == TEXT("InstancedFoliageActor");
		}
		const TCHAR* Folder = Group == TEXT("Dress") ? TEXT("Dress") : Group == TEXT("Village") ? TEXT("Village") : nullptr;
		if (!Folder)
		{
			return false;
		}
#if WITH_EDITOR
		// Outliner folders exist in editor builds (PIE, -game on the editor binary).
		if (Actor->GetFolderPath().ToString().StartsWith(Folder))
		{
			return true;
		}
#endif
		// Cooked builds: judge static mesh actors by their mesh package (dressing packs live under /Env/Dress/).
		const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);
		const UStaticMesh* Mesh = SMA && SMA->GetStaticMeshComponent() ? SMA->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			return false;
		}
		const FString Path = Mesh->GetPathName();
		return Group == TEXT("Dress") ? Path.Contains(TEXT("/Env/Dress/")) : Path.Contains(TEXT("/Env/KG_Village/"));
	}

	int32 SetGroupHidden(UWorld* World, FName Group, bool bHidden)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsInGroup(*It, Group))
			{
				It->SetActorHiddenInGame(bHidden);
				++Count;
			}
		}
		return Count;
	}

	/** Preview of the phase looks (mirrors AKGGameState::GetLook; the next phase change blends back). */
	bool SunLook(const FString& Name, float& Pitch, float& Yaw, float& Lux, FLinearColor& Color)
	{
		struct FLook
		{
			const TCHAR* Name;
			float Pitch, Yaw, Lux;
			FLinearColor Color;
		};
		static const FLook Looks[] = {
			{TEXT("Day"), -42.0f, 35.0f, 9.0f, FLinearColor(1.0f, 0.93f, 0.82f)},
			{TEXT("Dusk"), -16.0f, 62.0f, 7.5f, FLinearColor(1.0f, 0.78f, 0.58f)},
			{TEXT("Night"), -22.0f, 200.0f, 0.9f, FLinearColor(0.45f, 0.58f, 1.0f)},
			{TEXT("Dawn"), -9.0f, 95.0f, 5.0f, FLinearColor(1.0f, 0.62f, 0.45f)},
		};
		for (const FLook& Look : Looks)
		{
			if (Name.Equals(Look.Name, ESearchCase::IgnoreCase))
			{
				Pitch = Look.Pitch;
				Yaw = Look.Yaw;
				Lux = Look.Lux;
				Color = Look.Color;
				return true;
			}
		}
		return false;
	}

	int32 SetSun(UWorld* World, float Pitch, float Yaw, float Lux, const FLinearColor* Color)
	{
		int32 Count = 0;
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (!Sun || Sun->Mobility != EComponentMobility::Movable)
			{
				continue;
			}
			Sun->SetWorldRotation(FRotator(Pitch, Yaw, 0.0f));
			if (Lux >= 0.0f)
			{
				Sun->SetIntensity(Lux);
			}
			if (Color)
			{
				Sun->SetLightColor(*Color);
			}
			++Count;
		}
		return Count;
	}

	UKGViewmodelComponent* LocalViewmodel(const FKGDevContext& C)
	{
		const AKGCharacter* Character = C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
		return Character ? Character->GetViewmodel() : nullptr;
	}

	// ---- verb table ---------------------------------------------------------------------------------------------

	TArray<FVerb> BuildVerbs()
	{
		TArray<FVerb> V;
		auto Add = [&V](const TCHAR* Name, const TCHAR* Usage, const TCHAR* Help, EKGDevScope Scope, FVerbFn Run)
		{
			V.Add({Name, Usage, Help, Scope, MoveTemp(Run)});
		};
		constexpr EKGDevScope Server = EKGDevScope::Server;
		constexpr EKGDevScope Local = EKGDevScope::Local;

		// ---- Match ----
		Add(TEXT("Match.Start"), TEXT("[Seed]"), TEXT("Start the match flow now (a lobby starts its countdown)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    GM->DevStartMatch(A.IsValidIndex(0) ? FCString::Atoi64(*A[0]) : 0);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Match started (seed %s)"), *SeedText(GM->GetMatchSeed())));
		    });
		Add(TEXT("Match.Restart"), TEXT(""), TEXT("Reload the map with everyone (a fresh round)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(GameMode(C), TEXT("No KG game mode here"));
			    C.World->ServerTravel(TEXT("?Restart"), false);
			    return FKGDevResult::Ok(TEXT("Restarting the round..."));
		    });
		Add(TEXT("Match.Phase"), TEXT("<Warmup|RoleReveal|Dawn|Day|Meeting|Trial|Night|Epilogue|Lobby>"),
		    TEXT("Jump straight into a phase (deals roles if needed; Trial puts a bot on the gallows)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    EKGPhase Phase;
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    KG_DEV_REQUIRE(A.IsValidIndex(0) && ParsePhase(A[0], Phase), TEXT("Usage: Match.Phase <Phase>"));
			    GM->DevJumpToPhase(Phase);
			    const AKGGameState* GS = GameState(C);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Phase -> %s (day %d)"), *PhaseName(GS ? GS->GetPhase() : Phase),
			                                            GS ? GS->GetDayIndex() : 0));
		    });
		Add(TEXT("Match.Freeze"), TEXT("[0|1]"), TEXT("Freeze / unfreeze the phase timer (toggle without argument)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameState* GS = GameState(C);
			    KG_DEV_REQUIRE(GS && GameMode(C), TEXT("No KG match here"));
			    GS->Clock.bPaused = BoolArg(A, 0, GS->Clock.bPaused);
			    GS->ForceNetUpdate();
			    return FKGDevResult::Ok(GS->Clock.bPaused ? TEXT("Timer frozen") : TEXT("Timer running"));
		    });
		Add(TEXT("Match.Skip"), TEXT(""), TEXT("End the current phase now (same as kg.SkipPhase 1)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameState* GS = GameState(C);
			    KG_DEV_REQUIRE(GS && GameMode(C), TEXT("No KG match here"));
			    KG_DEV_REQUIRE(GS->GetPhase() != EKGPhase::Lobby, TEXT("The match has not started (Match.Start)"));
			    GS->Clock.bPaused = false;
			    GS->Clock.RemainingSeconds = FMath::Min(GS->Clock.RemainingSeconds, 0.01f);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Skipping %s"), *PhaseName(GS->GetPhase())));
		    });
		Add(TEXT("Match.Time"), TEXT("<Seconds>"), TEXT("Set the time left in the current phase."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameState* GS = GameState(C);
			    KG_DEV_REQUIRE(GS && GameMode(C), TEXT("No KG match here"));
			    const float Seconds = FloatArg(A, 0, -1.0f);
			    KG_DEV_REQUIRE(Seconds > 0.0f, TEXT("Usage: Match.Time <Seconds>"));
			    GS->Clock.RemainingSeconds = Seconds;
			    GS->Clock.PhaseDuration = FMath::Max(GS->Clock.PhaseDuration, Seconds);
			    GS->ForceNetUpdate();
			    return FKGDevResult::Ok(FString::Printf(TEXT("%.0f s left in %s"), Seconds, *PhaseName(GS->GetPhase())));
		    });
		Add(TEXT("Match.Speed"), TEXT("<Scale>"), TEXT("Phase clock speed: 1 = real time, 5 = five times faster."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    GM->DevClockScale = FMath::Clamp(FloatArg(A, 0, 1.0f), 0.1f, 50.0f);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Phase clock x%g"), GM->DevClockScale));
		    });
		Add(TEXT("Match.Win"), TEXT("<Town|Impatient|Neutral>"), TEXT("Declare a winner and go to the epilogue."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    EKGAlignment Winner;
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    KG_DEV_REQUIRE(A.IsValidIndex(0) && ParseAlignment(A[0], Winner), TEXT("Usage: Match.Win <Town|Impatient|Neutral>"));
			    GM->DevForceWin(Winner);
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s win"), *StaticEnum<EKGAlignment>()->GetNameStringByValue(int64(Winner))));
		    });
		Add(TEXT("Match.Reveal"), TEXT(""), TEXT("Reveal every role publicly (as in the epilogue)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    GM->DevRevealAllRoles();
			    return FKGDevResult::Ok(TEXT("All roles revealed"));
		    });
		Add(TEXT("Match.Seed"), TEXT(""), TEXT("Show the match seed (replay a match with Match.Start <Seed>)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("Seed %s (%lld)"), *SeedText(GM->GetMatchSeed()), GM->GetMatchSeed()));
		    });

		// ---- Bots ----
		Add(TEXT("Bot.Add"), TEXT("[Count=1]"), TEXT("Add villager bots now (any phase)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM && C.World->GetGameState(), TEXT("No KG game mode here"));
			    const int32 Room = FMath::Max(0, 32 - C.World->GetGameState()->PlayerArray.Num());
			    const int32 Added = GM->DevAddBots(FMath::Clamp(IntArg(A, 0, 1), 1, Room));
			    return FKGDevResult::Ok(FString::Printf(TEXT("+%d bots (%d players)"), Added, C.World->GetGameState()->PlayerArray.Num()));
		    });
		Add(TEXT("Bot.Fill"), TEXT("<Players>"), TEXT("Top the match up to N players with bots."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM && C.World->GetGameState(), TEXT("No KG game mode here"));
			    const int32 Added = GM->DevFillBots(FMath::Clamp(IntArg(A, 0, FKGRoleListGenerator::MinPlayers), 1, 32));
			    return FKGDevResult::Ok(FString::Printf(TEXT("+%d bots (%d players)"), Added, C.World->GetGameState()->PlayerArray.Num()));
		    });
		Add(TEXT("Bot.Remove"), TEXT("[Count=1 | Name]"), TEXT("Remove the newest bots, or one bot by name."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    if (A.IsValidIndex(0) && !A[0].IsNumeric())
			    {
				    const AKGPlayerState* PS = FindPlayer(C.World, Rest(A, 0));
				    KG_DEV_REQUIRE(PS && PS->IsABot(), FString::Printf(TEXT("No bot called '%s'"), *Rest(A, 0)));
				    const FString Name = PS->GetPlayerName();
				    KG_DEV_REQUIRE(GM->DevRemoveBots(0, ControllerOf(PS)) == 1, TEXT("Could not remove it"));
				    return FKGDevResult::Ok(FString::Printf(TEXT("Removed %s"), *Name));
			    }
			    const int32 Removed = GM->DevRemoveBots(FMath::Max(1, IntArg(A, 0, 1)));
			    return FKGDevResult::Ok(FString::Printf(TEXT("-%d bots"), Removed));
		    });
		Add(TEXT("Bot.RemoveAll"), TEXT(""), TEXT("Remove every bot."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("-%d bots"), GM->DevRemoveBots(1000)));
		    });
		Add(TEXT("Bot.AI"), TEXT("[0|1]"), TEXT("Freeze / resume every bot brain (kg.BotAI)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.BotAI"));
			    KG_DEV_REQUIRE(CVar, TEXT("kg.BotAI missing"));
			    const bool bOn = BoolArg(A, 0, CVar->GetInt() != 0);
			    CVar->Set(bOn ? 1 : 0, ECVF_SetByConsole);
			    if (!bOn)
			    {
				    for (TActorIterator<AKGBotController> It(C.World); It; ++It)
				    {
					    It->StopMovement();
				    }
			    }
			    return FKGDevResult::Ok(bOn ? TEXT("Bot AI on") : TEXT("Bot AI frozen"));
		    });
		Add(TEXT("Bot.List"), TEXT(""), TEXT("Log every player with role, life state and bot status."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AGameStateBase* GS = C.World ? C.World->GetGameState() : nullptr;
			    KG_DEV_REQUIRE(GS, TEXT("No game state"));
			    int32 Bots = 0;
			    for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
			    {
				    const AKGPlayerState* PS = Cast<AKGPlayerState>(GS->PlayerArray[i]);
				    if (!PS)
				    {
					    continue;
				    }
				    const AKGBotController* Bot = Cast<AKGBotController>(ControllerOf(PS));
				    Bots += PS->IsABot() ? 1 : 0;
				    UE_LOG(LogKillGodot, Log, TEXT("  #%-2d %-22s %-5s %-18s %-8s %s"), i, *PS->GetPlayerName(),
				           PS->IsABot() ? TEXT("bot") : TEXT("human"), *PS->GetPrivateRoleId().ToString(),
				           *StaticEnum<EKGLifeState>()->GetNameStringByValue(int64(PS->LifeState)),
				           Bot ? *Bot->GetDevStatus() : TEXT(""));
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d players, %d bots (see log)"), GS->PlayerArray.Num(), Bots));
		    });
		Add(TEXT("Bot.Goto"), TEXT("<Name | #Index>"), TEXT("Teleport behind a bot (or any player)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AKGPlayerState* PS = FindPlayer(C.World, Rest(A, 0));
			    KG_DEV_REQUIRE(PS, FString::Printf(TEXT("Nobody called '%s'"), *Rest(A, 0)));
			    const APawn* Target = PS->GetPawn();
			    KG_DEV_REQUIRE(Target, FString::Printf(TEXT("%s has no body"), *PS->GetPlayerName()));
			    const FVector Fwd = Target->GetActorForwardVector();
			    return TeleportRequester(C, Target->GetActorLocation() - Fwd * 160.0 + FVector(0, 0, 20.0),
			                             Fwd.Rotation().Yaw, PS->GetPlayerName());
		    });

		// ---- Me ----
		Add(TEXT("Me.Role"), TEXT("<RoleId> [Player]"), TEXT("Set your secret role (or another player's)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(A.IsValidIndex(0), TEXT("Usage: Me.Role <RoleId> [Player]"));
			    const FKGRoleInfo* Role = FKGRoleListGenerator::FindRole(FKGRoleListGenerator::GetDefaultCatalog(), FName(*A[0]));
			    KG_DEV_REQUIRE(Role, FString::Printf(TEXT("Unknown role '%s'"), *A[0]));
			    AKGPlayerState* PS = A.Num() > 1 ? FindPlayer(C.World, Rest(A, 1)) : MyPS(C);
			    KG_DEV_REQUIRE(PS, TEXT("No such player"));
			    PS->SetPrivateRoleId(Role->RoleId);
			    PS->ForceNetUpdate();
			    if (AKGCharacter* Character = CharacterOf(PS); Character && !Character->CanDrawBlade())
			    {
				    Character->bHoldingAssassinBlade = false;
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s is now %s (%s)"), *PS->GetPlayerName(), *Role->RoleId.ToString(),
			                                            *StaticEnum<EKGAlignment>()->GetNameStringByValue(int64(Role->GetAlignment()))));
		    });
		Add(TEXT("Me.God"), TEXT("[0|1]"), TEXT("God mode: no damage (toggle)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGDevComponent* Dev = MyDev(C);
			    KG_DEV_REQUIRE(Dev, TEXT("No dev link on this player"));
			    Dev->bGod = BoolArg(A, 0, Dev->bGod);
			    return FKGDevResult::Ok(Dev->bGod ? TEXT("God mode on") : TEXT("God mode off"));
		    });
		Add(TEXT("Me.Fly"), TEXT("[0|1]"), TEXT("Fly + noclip (Space up, Ctrl/C down) (toggle)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGDevComponent* Dev = MyDev(C);
			    KG_DEV_REQUIRE(Dev, TEXT("No dev link on this player"));
			    Dev->bFly = BoolArg(A, 0, Dev->bFly);
			    return FKGDevResult::Ok(Dev->bFly ? TEXT("Fly / noclip on") : TEXT("Fly / noclip off"));
		    });
		Add(TEXT("Me.Speed"), TEXT("<Scale>"), TEXT("Movement speed multiplier (1 = normal)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGDevComponent* Dev = MyDev(C);
			    KG_DEV_REQUIRE(Dev, TEXT("No dev link on this player"));
			    Dev->SpeedScale = FMath::Clamp(FloatArg(A, 0, 1.0f), 0.1f, 10.0f);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Speed x%g"), Dev->SpeedScale));
		    });
		Add(TEXT("Me.Heal"), TEXT(""), TEXT("Full health."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGCharacter* Character = CharacterOf(MyPS(C));
			    KG_DEV_REQUIRE(Character && Character->GetHealth(), TEXT("No body"));
			    KG_DEV_REQUIRE(!Character->IsDead(), TEXT("Dead: use Me.Revive"));
			    UKGHealthComponent* Health = Character->GetHealth();
			    Health->Heal(Health->MaxHealth);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Health %.0f"), Health->GetHealth()));
		    });
		Add(TEXT("Me.Stamina"), TEXT(""), TEXT("Full stamina."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGCharacter* Character = CharacterOf(MyPS(C));
			    KG_DEV_REQUIRE(Character, TEXT("No body"));
			    KG_DEV_REQUIRE(UKGDevComponent::RefillStamina(Character), TEXT("Stamina not found on the character"));
			    if (UKGDevComponent* Dev = MyDev(C))
			    {
				    Dev->ClientRefillStamina();   // the owning client predicts stamina too
			    }
			    return FKGDevResult::Ok(TEXT("Stamina full"));
		    });
		Add(TEXT("Me.Kill"), TEXT("[Player]"), TEXT("Kill yourself (become a ghost) or another player."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGPlayerState* PS = A.Num() > 0 ? FindPlayer(C.World, Rest(A, 0)) : MyPS(C);
			    AKGCharacter* Character = CharacterOf(PS);
			    KG_DEV_REQUIRE(Character, TEXT("No body"));
			    KG_DEV_REQUIRE(!Character->IsDead(), TEXT("Already dead"));
			    const bool bCould = Character->CanBeDamaged();
			    Character->SetCanBeDamaged(true);   // god mode must not block the deliberate kill
			    UGameplayStatics::ApplyDamage(Character, 10000.0f, nullptr, Character, UDamageType::StaticClass());
			    if (!Character->IsDead())
			    {
				    Character->SetCanBeDamaged(bCould);
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s died"), *PS->GetPlayerName()));
		    });
		Add(TEXT("Me.Revive"), TEXT("[Player]"), TEXT("Bring yourself (or another player) back to life where you are."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    AKGPlayerState* PS = A.Num() > 0 ? FindPlayer(C.World, Rest(A, 0)) : MyPS(C);
			    AController* Controller = ControllerOf(PS);
			    KG_DEV_REQUIRE(GM && PS && Controller, TEXT("No such player"));
			    APawn* Old = Controller->GetPawn();
			    const AKGCharacter* OldCharacter = Cast<AKGCharacter>(Old);
			    KG_DEV_REQUIRE(!PS->IsAlive() || !Old || (OldCharacter && OldCharacter->IsDead()), TEXT("Already alive"));
			    FVector Where = FVector::ZeroVector;
			    float Yaw = Controller->GetControlRotation().Yaw;
			    if (Old)
			    {
				    Where = Old->GetActorLocation();
				    Controller->UnPossess();
				    Old->Destroy();
			    }
			    else if (const APlayerController* PC = Cast<APlayerController>(Controller))
			    {
				    // Ghost: the spectator reports its position to the server (ServerSetSpectatorLocation).
				    Where = PC->GetSpectatorPawn() ? PC->GetSpectatorPawn()->GetActorLocation() : PC->GetSpawnLocation();
			    }
			    Where = FKGDev::GroundAt(C.World, FVector2D(Where), Where.Z);
			    PS->LifeState = EKGLifeState::Alive;
			    PS->RevealedRoleId = NAME_None;
			    PS->ForceNetUpdate();
			    GM->RestartPlayerAtTransform(Controller, FTransform(FRotator(0.0f, Yaw, 0.0f), Where));
			    KG_DEV_REQUIRE(Controller->GetPawn(), TEXT("Respawn failed (blocked spot?)"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s revived"), *PS->GetPlayerName()));
		    });
		Add(TEXT("Me.Blade"), TEXT(""), TEXT("Draw / sheathe the Impatient blade (any role)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGCharacter* Character = CharacterOf(MyPS(C));
			    KG_DEV_REQUIRE(Character && !Character->IsDead(), TEXT("No living body"));
			    Character->bHoldingAssassinBlade = !Character->bHoldingAssassinBlade;
			    Character->ForceNetUpdate();
			    return FKGDevResult::Ok(Character->bHoldingAssassinBlade ? TEXT("Blade drawn") : TEXT("Blade sheathed"));
		    });
		Add(TEXT("Me.Give"), TEXT("<Item> [Count=1]"), TEXT("Put items in your pockets (kg.ListItems)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const FName Item = A.IsValidIndex(0) ? UKGItemCatalog::ResolveLoose(A[0]) : NAME_None;
			    KG_DEV_REQUIRE(!Item.IsNone(), TEXT("Usage: Me.Give <Item> [Count] (unknown item)"));
			    KG_DEV_REQUIRE(MyPS(C), TEXT("No player"));
			    const int32 Added = KGExtrasDev::GiveItem(MyPS(C), Item, FMath::Max(1, IntArg(A, 1, 1)));
			    return FKGDevResult{Added > 0, FString::Printf(TEXT("+%d %s"), Added, *UKGItemCatalog::GetDisplayName(Item).ToString())};
		    });
		Add(TEXT("Me.Coins"), TEXT("[Count=100]"), TEXT("In-match gold: Coin items in your pockets."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(MyPS(C), TEXT("No player"));
			    const int32 Added = KGExtrasDev::GiveItem(MyPS(C), FKGItemIds::Coin, FMath::Max(1, IntArg(A, 0, 100)));
			    return FKGDevResult::Ok(FString::Printf(TEXT("+%d coins"), Added));
		    });
		Add(TEXT("Me.Gold"), TEXT("[Amount=500]"), TEXT("Profile gold (cosmetic shop currency, this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGProfileSave* Profile = UKGProfileSave::GetProfile();
			    KG_DEV_REQUIRE(Profile, TEXT("No profile"));
			    Profile->AwardGold(FMath::Max(1, IntArg(A, 0, 500)), TEXT("Dev"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("Profile gold %d"), Profile->GetGold()));
		    });

		// ---- Spawn ----
		Add(TEXT("Spawn.Crate"), TEXT("[Count=1]"), TEXT("Breakable crates in front of you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const APawn* Pawn = C.Requester ? C.Requester->GetPawn() : nullptr;
			    KG_DEV_REQUIRE(Pawn, TEXT("No body"));
			    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KillGodot/Env/KG_Props/StaticMeshes/Crate_Wooden.Crate_Wooden"));
			    if (!Mesh)
			    {
				    Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			    }
			    const int32 Count = FMath::Clamp(IntArg(A, 0, 1), 1, 20);
			    const FRotator Facing(0.0f, Pawn->GetActorRotation().Yaw, 0.0f);
			    int32 Spawned = 0;
			    for (int32 i = 0; i < Count; ++i)
			    {
				    const FVector Offset = Facing.RotateVector(FVector(200.0, (i % 5 - 2) * 90.0, 0.0));
				    const FVector Where = Pawn->GetActorLocation() + Offset + FVector(0, 0, 60.0 + (i / 5) * 90.0);
				    FActorSpawnParameters Params;
				    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				    if (AKGBreakable* Crate = C.World->SpawnActor<AKGBreakable>(AKGBreakable::StaticClass(), Where, Facing, Params))
				    {
					    Crate->SetMesh(Mesh);
					    Crate->LootSeed = static_cast<int32>(C.World->GetTimeSeconds() * 1000.0) + i;
					    ++Spawned;
				    }
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d crates"), Spawned));
		    });
		Add(TEXT("Spawn.Chest"), TEXT("[Locked=0] [Mine=0]"), TEXT("Storage chest in front of you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AActor* Chest = KGExtrasDev::SpawnChest(MyPS(C), IntArg(A, 0, 0) != 0, IntArg(A, 1, 0) != 0, TEXT("Chest"));
			    return Chest ? FKGDevResult::Ok(TEXT("Chest spawned")) : FKGDevResult::Fail(TEXT("No body to spawn in front of"));
		    });
		Add(TEXT("Spawn.Seat"), TEXT("[Stool|Chair|Bench]"), TEXT("A seat in front of you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AActor* Seat = KGExtrasDev::SpawnSeat(MyPS(C), A.IsValidIndex(0) ? A[0] : TEXT("Stool"));
			    return Seat ? FKGDevResult::Ok(TEXT("Seat spawned")) : FKGDevResult::Fail(TEXT("No body to spawn in front of"));
		    });
		Add(TEXT("Spawn.Pickup"), TEXT("<Item> [Count=1]"), TEXT("An item pickup on the ground in front of you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const FName Item = A.IsValidIndex(0) ? UKGItemCatalog::ResolveLoose(A[0]) : NAME_None;
			    KG_DEV_REQUIRE(!Item.IsNone(), TEXT("Usage: Spawn.Pickup <Item> [Count] (unknown item)"));
			    const AActor* Pickup = KGExtrasDev::SpawnPickup(MyPS(C), Item, FMath::Max(1, IntArg(A, 1, 1)));
			    return Pickup ? FKGDevResult::Ok(TEXT("Pickup spawned")) : FKGDevResult::Fail(TEXT("No body to spawn in front of"));
		    });
		Add(TEXT("Spawn.Loot"), TEXT("[Crate|Barrel|Pot|Chest|Grave|Fishing] [Seed=1]"), TEXT("Roll a loot table in front of you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const int32 Count = KGExtrasDev::SpawnLoot(MyPS(C), FName(A.IsValidIndex(0) ? *A[0] : TEXT("Crate")), IntArg(A, 1, 1));
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d pickups"), Count));
		    });

		// ---- World ----
		Add(TEXT("World.Goto"), TEXT("<Place | X Y Z>"), TEXT("Teleport to a named place (World.Places) or coordinates (cm)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(A.Num() > 0, TEXT("Usage: World.Goto <Place | X Y Z>"));
			    if (A.Num() >= 3 && A[0].IsNumeric() && A[1].IsNumeric() && A[2].IsNumeric())
			    {
				    const FVector Where(FCString::Atod(*A[0]), FCString::Atod(*A[1]), FCString::Atod(*A[2]));
				    return TeleportRequester(C, Where, C.Requester ? C.Requester->GetControlRotation().Yaw : 0.0f, Where.ToCompactString());
			    }
			    const FString Query = Rest(A, 0);
			    const TArray<FKGDevLocation>& Places = FKGDev::GatherLocations(C.World);
			    const FKGDevLocation* Found = Places.FindByPredicate([&Query](const FKGDevLocation& L) { return L.Name.Equals(Query, ESearchCase::IgnoreCase); });
			    if (!Found)
			    {
				    Found = Places.FindByPredicate([&Query](const FKGDevLocation& L) { return L.Name.Contains(Query, ESearchCase::IgnoreCase); });
			    }
			    KG_DEV_REQUIRE(Found, FString::Printf(TEXT("No place called '%s' (World.Places)"), *Query));
			    return TeleportRequester(C, Found->Location, C.Requester ? C.Requester->GetControlRotation().Yaw : 0.0f, Found->Name);
		    });
		Add(TEXT("World.Places"), TEXT(""), TEXT("Log the teleport targets."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const TArray<FKGDevLocation>& Places = FKGDev::GatherLocations(C.World);
			    for (const FKGDevLocation& L : Places)
			    {
				    UE_LOG(LogKillGodot, Log, TEXT("  %-10s %-28s %s"), *L.Group, *L.Name, *L.Location.ToCompactString());
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d places (see log)"), Places.Num()));
		    });
		Add(TEXT("World.Look"), TEXT("<Day|Dusk|Night|Dawn>"), TEXT("Preview a phase's sun (this machine; next phase blends back)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    float Pitch, Yaw, Lux;
			    FLinearColor Color;
			    KG_DEV_REQUIRE(A.IsValidIndex(0) && SunLook(A[0], Pitch, Yaw, Lux, Color), TEXT("Usage: World.Look <Day|Dusk|Night|Dawn>"));
			    const int32 Suns = SetSun(C.World, Pitch, Yaw, Lux, &Color);
			    return FKGDevResult{Suns > 0, FString::Printf(TEXT("%s look (%d movable suns)"), *A[0], Suns)};
		    });
		Add(TEXT("World.Sun"), TEXT("<Pitch> [Yaw] [Lux]"), TEXT("Set the sun angle / intensity (this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const int32 Suns = SetSun(C.World, FloatArg(A, 0, -42.0f), FloatArg(A, 1, 35.0f), FloatArg(A, 2, -1.0f), nullptr);
			    return FKGDevResult{Suns > 0, FString::Printf(TEXT("Sun pitch %.0f (%d movable suns)"), FloatArg(A, 0, -42.0f), Suns)};
		    });
		Add(TEXT("World.Hide"), TEXT("<Grass|Foliage|Dress|Village> [0|1]"), TEXT("Hide a group of actors for perf tests (this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(A.IsValidIndex(0), TEXT("Usage: World.Hide <Grass|Foliage|Dress|Village> [0|1]"));
			    const FName Group = *Pretty(A[0].ToLower());
			    KG_DEV_REQUIRE(Group == TEXT("Grass") || Group == TEXT("Foliage") || Group == TEXT("Dress") || Group == TEXT("Village"),
			                   TEXT("Groups: Grass, Foliage, Dress, Village"));
			    UKGDevSubsystem* Dev = UKGDevSubsystem::Get(C.World);
			    KG_DEV_REQUIRE(Dev, TEXT("No dev subsystem"));
			    const bool bHide = BoolArg(A, 1, Dev->HiddenGroups.Contains(Group));
			    bHide ? (void)Dev->HiddenGroups.Add(Group) : (void)Dev->HiddenGroups.Remove(Group);
			    const int32 Count = SetGroupHidden(C.World, Group, bHide);
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s %s (%d actors)"), *Group.ToString(), bHide ? TEXT("hidden") : TEXT("shown"), Count));
		    });
		Add(TEXT("World.Stat"), TEXT("<fps|unit|unitgraph|net|game|none>"), TEXT("Toggle an engine stat overlay (this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(C.Requester, TEXT("No local player"));
			    const FString Stat = A.IsValidIndex(0) ? A[0].ToLower() : TEXT("fps");
			    C.Requester->ConsoleCommand(Stat == TEXT("none") ? TEXT("stat none") : TEXT("stat ") + Stat);
			    return FKGDevResult::Ok(TEXT("stat ") + Stat);
		    });
		Add(TEXT("World.NavMesh"), TEXT(""), TEXT("Toggle the navmesh overlay (this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(C.Requester, TEXT("No local player"));
			    C.Requester->ConsoleCommand(TEXT("show navigation"));
			    return FKGDevResult::Ok(TEXT("show navigation"));
		    });
		Add(TEXT("World.ChoreMarkers"), TEXT("[0|1]"), TEXT("Draw every chore station with its radius and id (this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGDevSubsystem* Dev = UKGDevSubsystem::Get(C.World);
			    KG_DEV_REQUIRE(Dev, TEXT("No dev subsystem"));
			    Dev->bShowChoreMarkers = BoolArg(A, 0, Dev->bShowChoreMarkers);
			    return FKGDevResult::Ok(Dev->bShowChoreMarkers ? TEXT("Chore markers on") : TEXT("Chore markers off"));
		    });

		// ---- Chores ----
		Add(TEXT("Chore.List"), TEXT(""), TEXT("Log your chores and every chore station."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AKGPlayerState* PS = MyPS(C);
			    int32 Stations = 0;
			    for (TActorIterator<AKGTaskStation> It(C.World); It; ++It, ++Stations)
			    {
				    const int32 Mine = PS ? PS->TaskIds.IndexOfByKey(It->TaskId) : INDEX_NONE;
				    UE_LOG(LogKillGodot, Log, TEXT("  %-16s %-40s %s %s"), *It->TaskId.ToString(), *It->TaskName,
				           Mine == INDEX_NONE ? TEXT("     ") : (PS->TaskDone.IsValidIndex(Mine) && PS->TaskDone[Mine] ? TEXT("DONE ") : TEXT("OPEN ")),
				           *It->GetActorLocation().ToCompactString());
			    }
			    const AKGGameState* GS = GameState(C);
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d stations, %d yours, preparation %.0f%% (see log)"), Stations,
			                                            PS ? PS->TaskIds.Num() : 0, GS ? GS->Preparation * 100.0f : 0.0f));
		    });
		Add(TEXT("Chore.Done"), TEXT("[TaskId|all]"), TEXT("Complete your chores (all, or one)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGPlayerState* PS = MyPS(C);
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(PS && GM, TEXT("No player"));
			    KG_DEV_REQUIRE(PS->TaskIds.Num() > 0, TEXT("No chores dealt yet (they come with the role reveal; Chore.Reset all deals now)"));
			    const bool bAll = !A.IsValidIndex(0) || A[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);
			    int32 Done = 0;
			    for (const FName TaskId : TArray<FName>(PS->TaskIds))
			    {
				    if ((bAll || TaskId.ToString().Equals(A[0], ESearchCase::IgnoreCase)) && PS->CompleteTask(TaskId))
				    {
					    GM->OnTaskCompleted(PS, TaskId);
					    ++Done;
				    }
			    }
			    const AKGGameState* GS = GameState(C);
			    return FKGDevResult{Done > 0, FString::Printf(TEXT("%d chores done, preparation %.0f%%"), Done, GS ? GS->Preparation * 100.0f : 0.0f)};
		    });
		Add(TEXT("Chore.Reset"), TEXT("[all]"), TEXT("Re-open your chores; 'all' deals everyone fresh chores."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGGameMode* GM = GameMode(C);
			    KG_DEV_REQUIRE(GM, TEXT("No KG game mode here"));
			    const bool bAll = A.IsValidIndex(0) && A[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);
			    KG_DEV_REQUIRE(bAll || MyPS(C), TEXT("No player (use Chore.Reset all)"));
			    GM->DevResetTasks(bAll ? nullptr : MyPS(C));
			    return FKGDevResult::Ok(bAll ? TEXT("Fresh chores dealt to everyone") : TEXT("Your chores are open again"));
		    });
		Add(TEXT("Chore.Goto"), TEXT("<TaskId>"), TEXT("Teleport next to a chore station."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(A.IsValidIndex(0), TEXT("Usage: Chore.Goto <TaskId>"));
			    for (TActorIterator<AKGTaskStation> It(C.World); It; ++It)
			    {
				    if (It->TaskId.ToString().Equals(A[0], ESearchCase::IgnoreCase))
				    {
					    const FVector At = It->GetActorLocation();
					    const FVector Back = -It->GetActorForwardVector() * 150.0;
					    const FVector Where = FKGDev::GroundAt(C.World, FVector2D(At + Back), At.Z);
					    return TeleportRequester(C, Where, (At - Where).Rotation().Yaw, It->TaskId.ToString());
				    }
			    }
			    return FKGDevResult::Fail(FString::Printf(TEXT("No chore station '%s'"), *A[0]));
		    });
		Add(TEXT("Chore.Play"), TEXT("<ChoreId>"),
		    TEXT("Open a chore minigame right here (server-validated; counts if it is on your list, else practice)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    FString Ids;
			    for (const FKGChoreDef& Def : FKGChoreCatalog::GetAll())
			    {
				    Ids += (Ids.IsEmpty() ? TEXT("") : TEXT(" ")) + Def.Id.ToString();
			    }
			    KG_DEV_REQUIRE(A.IsValidIndex(0), TEXT("Usage: Chore.Play <Id>: ") + Ids);
			    const FKGChoreDef* Found = nullptr;
			    for (const FKGChoreDef& Def : FKGChoreCatalog::GetAll())
			    {
				    Found = Def.Id.ToString().Equals(A[0], ESearchCase::IgnoreCase) ? &Def : Found;
			    }
			    KG_DEV_REQUIRE(Found, FString::Printf(TEXT("No chore '%s'. Chores: %s"), *A[0], *Ids));
			    AKGCharacter* Me = C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
			    UKGChoreComponent* Chores = Me ? Me->GetChores() : nullptr;
			    KG_DEV_REQUIRE(Chores, TEXT("No living villager body to play with"));
			    KG_DEV_REQUIRE(Chores->AuthOpen(Found->Id, nullptr, true), TEXT("Could not open it (dead?)"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s opened (%s)"), *Found->Id.ToString(),
			                                            Chores->IsSessionPractice() ? TEXT("practice") : TEXT("counts")));
		    });
		Add(TEXT("Chore.AutoWin"), TEXT("[0|1]"),
		    TEXT("Chore minigames play themselves (each stage solves just after the server's timing floor). This machine."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGChoreComponent::SetAutoWin(BoolArg(A, 0, UKGChoreComponent::IsAutoWin()));
			    return FKGDevResult::Ok(UKGChoreComponent::IsAutoWin() ? TEXT("Chore auto-win on") : TEXT("Chore auto-win off"));
		    });

		// ---- Voice / barks / partner emotes (SPRINT-023) ----
		Add(TEXT("Voice.Tone"), TEXT("[0|1]"), TEXT("Transmit a synthetic 220 Hz tone from this machine (routing / mouth test without a microphone)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGVoiceComponent* Voice = UKGVoiceComponent::FindForController(C.Requester);
			    KG_DEV_REQUIRE(Voice, TEXT("No voice relay for this player yet"));
			    const bool bOn = BoolArg(A, 0, !Voice->IsSyntheticToneOn());
			    Voice->SetSyntheticTone(bOn);
			    Voice->SetTransmitting(bOn);
			    return FKGDevResult::Ok(bOn ? TEXT("Voice tone ON (kg.Voice.Tone 0 stops)") : TEXT("Voice tone off"));
		    });
		Add(TEXT("Voice.Wheel"), TEXT("<0|1|2|close> [Slot]"), TEXT("Open a voice-command radial (0 Z Calls, 1 X Deduction, 2 C Social) as if the key were held, optionally aiming at a slot 0-7; close = release without sending (screenshots)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    APlayerController* PC = C.Requester;
			    KG_DEV_REQUIRE(PC, TEXT("No local player controller"));
			    IConsoleVariable* Pin = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.Voice.WheelPin"));
			    if (A.Num() > 0 && A[0].Equals(TEXT("close"), ESearchCase::IgnoreCase))
			    {
				    if (Pin)
				    {
					    Pin->Set(false);
				    }
				    FKGVoiceUI::CloseWheel(PC, false);
				    return FKGDevResult::Ok(TEXT("Voice wheel closed"));
			    }
			    const int32 Menu = FMath::Clamp(A.Num() > 0 ? FCString::Atoi(*A[0]) : 0, 0, 2);
			    if (Pin)
			    {
				    Pin->Set(true);   // the voice subsystem would otherwise close + send it next tick (key not held)
			    }
			    FKGVoiceUI::OpenWheel(PC, Menu);
			    if (A.Num() > 1)
			    {
				    const float Angle = FCString::Atoi(*A[1]) * (2.0f * PI / 8.0f);   // slot 0 at the top, clockwise
				    FKGVoiceUI::UpdateWheel(PC, FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * 160.0);
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("Voice wheel %d open (kg.Voice.Wheel close)"), Menu));
		    });
		Add(TEXT("Voice.Mute"), TEXT("<Player> [0|1]"), TEXT("Mute / unmute a player's voice and text on this machine (same list as /mute)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGChatComponent* Chat = UKGChatComponent::FindForController(C.Requester);
			    KG_DEV_REQUIRE(Chat && A.IsValidIndex(0), TEXT("Usage: Voice.Mute <Player> [0|1]"));
			    const AKGPlayerState* PS = FindPlayer(C.World, A[0]);
			    KG_DEV_REQUIRE(PS, FString::Printf(TEXT("No player '%s'"), *A[0]));
			    const bool bMute = BoolArg(A, 1, !Chat->IsMuted(PS->GetPlayerId()));
			    Chat->SetMuted(PS->GetPlayerId(), bMute);
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s %s"), *PS->GetPlayerName(), bMute ? TEXT("muted") : TEXT("unmuted")));
		    });
		Add(TEXT("Bark"), TEXT("<Id|list>"), TEXT("Say a voice command (Z/X/C radial line) through the relay: bark audio, mouth, gesture, NEAR line."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const FString Arg = A.IsValidIndex(0) ? A[0] : TEXT("list");
			    if (Arg.Equals(TEXT("list"), ESearchCase::IgnoreCase))
			    {
				    FString Line;
				    for (const FKGVoiceMenuDef& Menu : FKGVoiceCommandCatalog::GetMenus())
				    {
					    Line += FString::Printf(TEXT("[%s %s] "), *Menu.Key.ToString(), *Menu.Title.ToString());
					    for (const int32 Index : Menu.Commands)
					    {
						    if (const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Get(Index))
						    {
							    Line += Def->Id.ToString() + TEXT(" ");
						    }
					    }
				    }
				    UE_LOG(LogKillGodot, Log, TEXT("Voice commands: %s"), *Line);
				    return FKGDevResult::Ok(FString::Printf(TEXT("%d voice commands (see log)"), FKGVoiceCommandCatalog::GetAll().Num()));
			    }
			    const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Find(FName(*Arg));
			    KG_DEV_REQUIRE(Def, FString::Printf(TEXT("Unknown voice command '%s' (kg.Bark list)"), *Arg));
			    UKGVoiceSubsystem::RequestBark(C.Requester, Def->Id);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Bark %s requested"), *Def->Id.ToString()));
		    });
		Add(TEXT("Bark.Bots"), TEXT("<Id|all>"), TEXT("Every bot says a voice command (all = a different line each); ignores the rate limit."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AGameStateBase* GS = C.World ? C.World->GetGameState() : nullptr;
			    KG_DEV_REQUIRE(GS, TEXT("No game state"));
			    const FString Arg = A.IsValidIndex(0) ? A[0] : TEXT("all");
			    const bool bAll = Arg.Equals(TEXT("all"), ESearchCase::IgnoreCase);
			    const FKGVoiceCommandDef* Def = FKGVoiceCommandCatalog::Find(FName(*Arg));
			    KG_DEV_REQUIRE(bAll || Def, FString::Printf(TEXT("Unknown voice command '%s' (kg.Bark list)"), *Arg));
			    const TArray<FKGVoiceCommandDef>& All = FKGVoiceCommandCatalog::GetAll();
			    int32 Done = 0;
			    int32 Next = 0;
			    for (APlayerState* PS : GS->PlayerArray)
			    {
				    UKGVoiceComponent* Voice = PS && PS->IsABot() ? UKGVoiceComponent::FindForPlayer(PS) : nullptr;
				    if (Voice && Voice->ServerSayBark(bAll ? All[Next++ % All.Num()].Id : Def->Id, true))
				    {
					    ++Done;
				    }
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d bots barked"), Done));
		    });
		Add(TEXT("Mouth.Pin"), TEXT("<-1|0..1>"), TEXT("Pin every mouth to an opening for screenshots (-1 = voice / bark driven); sets kg.Mouth.Force."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.Mouth.Force"));
			    KG_DEV_REQUIRE(CVar, TEXT("kg.Mouth.Force missing"));
			    const float Value = A.IsValidIndex(0) ? FCString::Atof(*A[0]) : -1.0f;
			    CVar->Set(Value, ECVF_SetByConsole);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Mouths %s"), Value < 0.0f ? TEXT("voice driven") : *FString::Printf(TEXT("pinned at %.2f"), Value)));
		    });
		Add(TEXT("Partner"), TEXT("<highfive|handshake|rps|danceoff|accept|cancel>"),
		    TEXT("Offer a partner emote (a nearby player accepts with E), accept the nearest offer, or cancel."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGEmoteComponent* Emote = C.Requester ? UKGEmoteComponent::FindForPlayer(C.Requester->PlayerState) : nullptr;
			    KG_DEV_REQUIRE(Emote, TEXT("No body"));
			    const FString Arg = A.IsValidIndex(0) ? A[0] : TEXT("highfive");
			    if (Arg.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
			    {
				    return Emote->TryAcceptNearbyOffer() ? FKGDevResult::Ok(TEXT("Accepting the nearest offer"))
				                                       : FKGDevResult::Fail(TEXT("Nobody within 3 m is offering a partner emote"));
			    }
			    if (Arg.Equals(TEXT("cancel"), ESearchCase::IgnoreCase))
			    {
				    Emote->RequestPartnerCancel();
				    return FKGDevResult::Ok(TEXT("Partner emote cancelled"));
			    }
			    const FKGPartnerEmoteDef* Def = FKGPartnerCatalog::Find(FName(*Arg));
			    KG_DEV_REQUIRE(Def, FString::Printf(TEXT("Unknown partner emote '%s' (highfive|handshake|rps|danceoff)"), *Arg));
			    Emote->RequestPartnerOffer(Def->Id);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Offering %s - a partner presses E within %.0f s"), *Def->DisplayName.ToString(), FKGPartnerRules::OfferSeconds));
		    });
		Add(TEXT("Partner.Bots"), TEXT("<highfive|handshake|rps|danceoff>"), TEXT("Two bots perform a partner emote next to each other (renders / smoke)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AGameStateBase* GS = C.World ? C.World->GetGameState() : nullptr;
			    KG_DEV_REQUIRE(GS, TEXT("No game state"));
			    const FKGPartnerEmoteDef* Def = FKGPartnerCatalog::Find(FName(*(A.IsValidIndex(0) ? A[0] : TEXT("highfive"))));
			    KG_DEV_REQUIRE(Def, TEXT("Unknown partner emote (highfive|handshake|rps|danceoff)"));
			    TArray<AKGCharacter*> Bots;
			    for (APlayerState* PS : GS->PlayerArray)
			    {
				    if (AKGCharacter* Bot = PS && PS->IsABot() ? CharacterOf(PS) : nullptr; Bot && !Bot->IsDead())
				    {
					    Bots.Add(Bot);
				    }
			    }
			    KG_DEV_REQUIRE(Bots.Num() >= 2, TEXT("Need two living bots (kg.Bot.Add 2, kg.Bot.AI 0)"));
			    AKGCharacter* A0 = Bots[0];
			    AKGCharacter* B0 = Bots[1];
			    B0->TeleportTo(A0->GetActorLocation() + A0->GetActorForwardVector() * 150.0f, A0->GetActorRotation(), false, true);
			    const EKGEmoteReject Offer = A0->GetEmote()->ServerPartnerOffer(Def->Id, true);
			    KG_DEV_REQUIRE(Offer == EKGEmoteReject::None, FString::Printf(TEXT("Offer refused: %s"), *StaticEnum<EKGEmoteReject>()->GetNameStringByValue(int64(Offer))));
			    const EKGEmoteReject Accept = B0->GetEmote()->ServerPartnerAccept(A0->GetEmote());
			    KG_DEV_REQUIRE(Accept == EKGEmoteReject::None, FString::Printf(TEXT("Accept refused: %s"), *StaticEnum<EKGEmoteReject>()->GetNameStringByValue(int64(Accept))));
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s and %s: %s"), *A0->GetName(), *B0->GetName(), *Def->DisplayName.ToString()));
		    });

		// ---- Emotes ----
		Add(TEXT("Emote"), TEXT("<Id|stop|list>"),
		    TEXT("Play an emote the normal way (chat relay + server rules): wave, point, accuse, dance, reel, sit..."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    APlayerController* PC = C.Requester ? C.Requester : FKGDev::LocalController(C.World);
			    KG_DEV_REQUIRE(PC, TEXT("No local player"));
			    const FString Arg = A.IsValidIndex(0) ? A[0] : TEXT("list");
			    if (Arg.Equals(TEXT("stop"), ESearchCase::IgnoreCase))
			    {
				    UKGEmoteSubsystem::RequestStop(PC);
				    return FKGDevResult::Ok(TEXT("Emote stopped"));
			    }
			    if (Arg.Equals(TEXT("list"), ESearchCase::IgnoreCase))
			    {
				    FString Line;
				    for (const FKGEmoteDef& Def : FKGEmoteCatalog::GetAll())
				    {
					    Line += FString::Printf(TEXT("%s%s(%s%s) "), *Def.Id.ToString(), Def.bLoop ? TEXT("*") : TEXT(""),
					                            Def.IsFullBody() ? TEXT("full") : TEXT("upper"),
					                            Def.UsesThirdPersonCamera() ? TEXT("") : TEXT(", fp"));
				    }
				    UE_LOG(LogKillGodot, Log, TEXT("Emotes: %s"), *Line);
				    return FKGDevResult::Ok(FString::Printf(TEXT("%d emotes (see log; * = loop)"), FKGEmoteCatalog::GetAll().Num()));
			    }
			    const FKGEmoteDef* Def = FKGEmoteCatalog::Find(Arg);
			    KG_DEV_REQUIRE(Def, FString::Printf(TEXT("Unknown emote '%s' (kg.Emote list)"), *Arg));
			    KG_DEV_REQUIRE(UKGChatComponent::FindForController(PC), TEXT("No chat relay yet (still joining?)"));
			    UKGEmoteSubsystem::RequestEmote(PC, Def->Id);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Emote %s requested"), *Def->Id.ToString()));
		    });
		Add(TEXT("Emote.Bots"), TEXT("<Id|all|stop>"),
		    TEXT("Every bot plays an emote (all = a different one each) so you can watch them; ignores the rate limit."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AGameStateBase* GS = C.World ? C.World->GetGameState() : nullptr;
			    KG_DEV_REQUIRE(GS, TEXT("No game state"));
			    const FString Arg = A.IsValidIndex(0) ? A[0] : TEXT("dance");
			    const bool bStop = Arg.Equals(TEXT("stop"), ESearchCase::IgnoreCase);
			    const bool bAll = Arg.Equals(TEXT("all"), ESearchCase::IgnoreCase);
			    const FKGEmoteDef* Def = FKGEmoteCatalog::Find(Arg);
			    KG_DEV_REQUIRE(bStop || bAll || Def, FString::Printf(TEXT("Unknown emote '%s' (kg.Emote list)"), *Arg));
			    const TArray<FKGEmoteDef>& All = FKGEmoteCatalog::GetAll();
			    int32 Done = 0;
			    int32 Refused = 0;
			    int32 Next = 0;
			    for (APlayerState* PS : GS->PlayerArray)
			    {
				    AKGCharacter* Character = PS && PS->IsABot() ? CharacterOf(PS) : nullptr;
				    UKGEmoteComponent* Emote = Character ? Character->GetEmote() : nullptr;
				    if (!Emote)
				    {
					    continue;
				    }
				    if (bStop)
				    {
					    Emote->ServerStop(EKGEmoteStop::Requested);
					    ++Done;
					    continue;
				    }
				    const FName Id = bAll ? All[Next++ % All.Num()].Id : Def->Id;
				    (Emote->ServerTryStart(Id, true) == EKGEmoteReject::None ? Done : Refused) += 1;
			    }
			    if (bStop)
			    {
				    return FKGDevResult::Ok(FString::Printf(TEXT("%d bots stopped"), Done));
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d bots emote %s (%d refused: phase / moving / busy)"), Done,
			                                            bAll ? TEXT("(all)") : *Def->Id.ToString(), Refused));
		    });
		Add(TEXT("Emote.Cam"), TEXT("[0|1]"), TEXT("Hold the third-person emote camera to look at your own body (this machine)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.Emote.DebugCam"));
			    KG_DEV_REQUIRE(CVar, TEXT("kg.Emote.DebugCam missing"));
			    CVar->Set(BoolArg(A, 0, CVar->GetInt() != 0) ? 1 : 0, ECVF_SetByConsole);
			    return FKGDevResult::Ok(CVar->GetInt() ? TEXT("Third-person emote camera on") : TEXT("First person"));
		    });

		// ---- Fishing (Source/KillGodot/Fishing, Docs/01_GDD_Core.md section 15) ----
		Add(TEXT("Fish.Give"), TEXT("[Rod | <Species> [Kg] | <Item>]"),
		    TEXT("A fishing rod (default), a weighed fish (Fish.Give Salmon 5.2) or fishing junk into your pockets."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGPlayerState* PS = MyPS(C);
			    KG_DEV_REQUIRE(PS, TEXT("No player"));
			    const FString Arg = A.IsValidIndex(0) ? A[0] : TEXT("Rod");
			    if (Arg.Equals(TEXT("Rod"), ESearchCase::IgnoreCase))
			    {
				    const int32 Added = KGExtrasDev::GiveItem(PS, FKGItemIds::FishingRod, 1);
				    return FKGDevResult{Added > 0, Added > 0 ? FString(TEXT("+1 Fishing Rod (H takes it out)")) : FString(TEXT("Pockets full"))};
			    }
			    const FName Item = UKGItemCatalog::ResolveLoose(Arg);
			    const FKGFishSpecies* S = FKGFishingRules::Find(FName(*Arg));
			    S = S ? S : FKGFishingRules::FindByItem(Item);
			    if (S && !S->ItemId.IsNone())
			    {
				    const float Kg = FMath::Clamp(FloatArg(A, 1, 0.5f * (S->MinKg + S->MaxKg)), 0.05f, 50.0f);
				    UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(PS);
				    KG_DEV_REQUIRE(Pockets, TEXT("No pockets yet"));
				    const int32 Added = Pockets->AddItem(S->ItemId, 1, FMath::RoundToInt(Kg * 1000.0f));
				    return FKGDevResult{Added > 0, FString::Printf(TEXT("+%s %.2f kg"), *UKGItemCatalog::GetDisplayName(S->ItemId).ToString(), Kg)};
			    }
			    KG_DEV_REQUIRE(!Item.IsNone(), FString::Printf(TEXT("Unknown rod/species/item '%s'"), *Arg));
			    const int32 Added = KGExtrasDev::GiveItem(PS, Item, 1);
			    return FKGDevResult{Added > 0, FString::Printf(TEXT("+%d %s"), Added, *UKGItemCatalog::GetDisplayName(Item).ToString())};
		    });
		Add(TEXT("Fish.Bite"), TEXT("[Species|Item]"),
		    TEXT("A bite right now (gives a rod, takes it out and casts ahead if needed). Species: Mackerel Cod Salmon GoldenCarp, or a junk item."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGPlayerState* PS = MyPS(C);
			    UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(CharacterOf(PS));
			    KG_DEV_REQUIRE(Fishing, TEXT("No villager body with fishing (still joining, or a ghost?)"));
			    UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(PS);
			    if (Pockets && !Pockets->Has(FKGItemIds::FishingRod))
			    {
				    Pockets->AddItem(FKGItemIds::FishingRod, 1);
			    }
			    if (Fishing->GetServerPhase() == EKGFishPhase::Stowed)
			    {
				    KG_DEV_REQUIRE(Fishing->ServerTrySetRod(true), TEXT("Cannot fish now (meeting/trial, blade out, hands full)"));
			    }
			    const FName Want = A.IsValidIndex(0) ? FName(*A[0]) : NAME_None;
			    Fishing->ServerForceBite(Want);
			    return FKGDevResult::Ok(FString::Printf(TEXT("Bite forced%s%s - strike with LMB when the bobber goes under"),
			                                            Want.IsNone() ? TEXT("") : TEXT(": "), Want.IsNone() ? TEXT("") : *Want.ToString()));
		    });
		Add(TEXT("Fish.Cast"), TEXT("[Power=0.6 | out]"), TEXT("Take the rod out (a rod is given if needed) and cast along your view; 'out' only takes it out."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    AKGPlayerState* PS = MyPS(C);
			    AKGCharacter* Body = CharacterOf(PS);
			    UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(Body);
			    KG_DEV_REQUIRE(Fishing, TEXT("No villager body with fishing"));
			    UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(PS);
			    if (Pockets && !Pockets->Has(FKGItemIds::FishingRod))
			    {
				    Pockets->AddItem(FKGItemIds::FishingRod, 1);
			    }
			    if (Fishing->GetServerPhase() == EKGFishPhase::Stowed)
			    {
				    KG_DEV_REQUIRE(Fishing->ServerTrySetRod(true), TEXT("Cannot fish now"));
			    }
			    if (A.IsValidIndex(0) && A[0].Equals(TEXT("out"), ESearchCase::IgnoreCase))
			    {
				    return FKGDevResult::Ok(TEXT("Rod out"));
			    }
			    const FVector View = Body->GetFirstPersonCamera()->GetComponentLocation();
			    KG_DEV_REQUIRE(Fishing->ServerTryCast(View, Body->GetControlRotation().Vector(), FMath::Clamp(FloatArg(A, 0, 0.6f), 0.0f, 1.0f)),
			                   TEXT("Cast refused (rod busy / cooldown / phase)"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("Cast -> %s"), *StaticEnum<EKGFishWater>()->GetNameStringByValue(static_cast<int64>(Fishing->GetLine().Water))));
		    });
		Add(TEXT("Fish.Hook"), TEXT(""), TEXT("Strike for you (server side) when a fish bites: starts the reel minigame."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(CharacterOf(MyPS(C)));
			    KG_DEV_REQUIRE(Fishing, TEXT("No villager body with fishing"));
			    KG_DEV_REQUIRE(Fishing->GetServerPhase() == EKGFishPhase::Bite, TEXT("Nothing is biting (Fish.Bite first)"));
			    Fishing->ServerForceHook();
			    return FKGDevResult::Ok(TEXT("Hooked - reel!"));
		    });
		Add(TEXT("Fish.BotCast"), TEXT("[Power=0.6]"), TEXT("Every bot gets a rod and casts where it looks (watch them fish; Bot.AI 0 keeps them still)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AGameStateBase* GS = C.World ? C.World->GetGameState() : nullptr;
			    KG_DEV_REQUIRE(GS, TEXT("No game state"));
			    int32 Cast = 0;
			    for (APlayerState* PS : GS->PlayerArray)
			    {
				    AKGCharacter* Body = PS && PS->IsABot() ? CharacterOf(PS) : nullptr;
				    UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(Body);
				    UKGInventoryComponent* Pockets = UKGInventoryComponent::FindForPlayer(PS);
				    if (!Fishing || !Pockets)
				    {
					    continue;
				    }
				    if (!Pockets->Has(FKGItemIds::FishingRod))
				    {
					    Pockets->AddItem(FKGItemIds::FishingRod, 1);
				    }
				    if (Fishing->ServerTrySetRod(true) &&
				        Fishing->ServerTryCast(Body->GetFirstPersonCamera()->GetComponentLocation(), Body->GetControlRotation().Vector(),
				                               FMath::Clamp(FloatArg(A, 0, 0.6f), 0.0f, 1.0f)))
				    {
					    ++Cast;
				    }
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d bots cast"), Cast));
		    });
		Add(TEXT("Fish.Land"), TEXT(""), TEXT("Land the fish on your line at once (skips the reel minigame)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGFishingComponent* Fishing = UKGFishingComponent::FindFor(CharacterOf(MyPS(C)));
			    KG_DEV_REQUIRE(Fishing, TEXT("No villager body with fishing"));
			    KG_DEV_REQUIRE(Fishing->ServerForceLand(), TEXT("Nothing on the line (Fish.Bite, then strike)"));
			    return FKGDevResult::Ok(FString::Printf(TEXT("Landed %s"), *Fishing->GetLastCatch().ItemId.ToString()));
		    });
		Add(TEXT("Fish.Tension"), TEXT("[0|1]"), TEXT("Fishing debug overlay: tension, stamina, server vs prediction (kg.Fish.Debug)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.Fish.Debug"));
			    KG_DEV_REQUIRE(CVar, TEXT("kg.Fish.Debug missing"));
			    CVar->Set(BoolArg(A, 0, CVar->GetInt() != 0) ? 1 : 0, ECVF_SetByConsole);
			    return FKGDevResult::Ok(CVar->GetInt() ? TEXT("Fishing overlay on") : TEXT("Fishing overlay off"));
		    });
		Add(TEXT("Fish.Sell"), TEXT(""), TEXT("Sell every fish and sea find in your pockets (as at the Fish Market)."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    KG_DEV_REQUIRE(MyPS(C), TEXT("No player"));
			    int32 Items = 0;
			    const int32 Coins = AKGFishMarketStall::SellAll(MyPS(C), Items);
			    return FKGDevResult{Items > 0, FString::Printf(TEXT("Sold %d for %d gold"), Items, Coins)};
		    });
		Add(TEXT("Fish.Market"), TEXT(""), TEXT("Put Madam Brine's stall (sell fish, borrow a rod) in front of you."), Server,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const APawn* Pawn = CharacterOf(MyPS(C));
			    KG_DEV_REQUIRE(Pawn, TEXT("No body"));
			    const FVector Fwd = Pawn->GetActorForwardVector().GetSafeNormal2D();
			    const FVector At = FKGDev::GroundAt(C.World, FVector2D(Pawn->GetActorLocation() + Fwd * 260.0f), Pawn->GetActorLocation().Z);
			    // Customers stand on the stall's local -Y side: turn that side to us.
			    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Fwd.X, Fwd.Y));
			    KG_DEV_REQUIRE(AKGFishMarketStall::SpawnStall(C.World, At, Yaw), TEXT("Could not spawn the stall"));
			    return FKGDevResult::Ok(TEXT("Madam Brine's stall is in front of you (E: sell / borrow a rod)"));
		    });
		Add(TEXT("Fish.Species"), TEXT(""), TEXT("Log the species table (water, time of day, weight, fight)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    for (const FKGFishSpecies& S : FKGFishingRules::Species())
			    {
				    UE_LOG(LogKillGodot, Log, TEXT("KG_FISH_SPECIES %s item=%s base=%.1f sea=%.2f basin=%.2f brook=%.2f koi=%.0f day=%.2f dawn=%.2f night=%.2f kg=%.2f-%.2f pull=%.2f stamina=%.0fs agility=%.2f hook=%.2fs%s"),
				           *S.Id.ToString(), *S.ItemId.ToString(), S.Base, S.WaterWeight(EKGFishWater::Sea),
				           S.WaterWeight(EKGFishWater::Basin), S.WaterWeight(EKGFishWater::Brook), S.WaterWeight(EKGFishWater::KoiPond),
				           S.Day, S.Dawn, S.Night, S.MinKg, S.MaxKg, S.Pull, S.StaminaSec, S.Agility, S.HookWindow,
				           S.bSacred ? TEXT(" SACRED") : TEXT(""));
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d species (see log)"), FKGFishingRules::Species().Num()));
		    });

		// ---- Debug (this machine) ----
		// ---- KG_DIG: digging + the underground (Source/KillGodot/Dig, Docs/01_GDD_Core.md section 16) ----
		KGDigDev::AddVerbs(Add);
		KGTrapDev::AddVerbs(Add);    // SPRINT-040 hook
		KGManorDev::AddVerbs(Add);   // SPRINT-040 hook

		Add(TEXT("Debug.HUDDemo"), TEXT("<0-5>"), TEXT("HUD preview states (kg.HUDDemo)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("kg.HUDDemo"));
			    KG_DEV_REQUIRE(CVar, TEXT("kg.HUDDemo missing"));
			    CVar->Set(FMath::Clamp(IntArg(A, 0, 0), 0, 9), ECVF_SetByConsole);
			    return FKGDevResult::Ok(FString::Printf(TEXT("kg.HUDDemo %d"), CVar->GetInt()));
		    });
		Add(TEXT("Debug.VM"), TEXT("<Preset|FOV|X|Y|Z|Bob|Sway|Left> <Value>"), TEXT("Viewmodel settings (through ApplySettings)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGViewmodelComponent* VM = LocalViewmodel(C);
			    KG_DEV_REQUIRE(VM, TEXT("No first-person body"));
			    KG_DEV_REQUIRE(A.Num() >= 2, TEXT("Usage: Debug.VM <Preset|FOV|X|Y|Z|Bob|Sway|Left> <Value>"));
			    const FString Key = A[0].ToLower();
			    const float Value = FloatArg(A, 1, 0.0f);
			    FKGViewmodelSettings S = VM->GetSettings();
			    if (Key == TEXT("preset"))
			    {
				    S = FKGViewmodelSettings::FromPreset(FMath::Clamp(FMath::RoundToInt(Value), 1, 3));
			    }
			    else
			    {
				    S.Preset = 0;   // custom
				    if (Key == TEXT("fov")) { S.ViewmodelFOV = FMath::Clamp(Value, 54.0f, 68.0f); }
				    else if (Key == TEXT("x")) { S.Offset.X = FMath::Clamp(Value, -2.5f, 2.5f); }
				    else if (Key == TEXT("y")) { S.Offset.Y = FMath::Clamp(Value, -2.5f, 2.5f); }
				    else if (Key == TEXT("z")) { S.Offset.Z = FMath::Clamp(Value, -2.5f, 2.5f); }
				    else if (Key == TEXT("bob")) { S.BobScale = FMath::Clamp(Value, 0.0f, 1.0f); }
				    else if (Key == TEXT("sway")) { S.SwayScale = FMath::Clamp(Value, 0.0f, 1.0f); }
				    else if (Key == TEXT("left")) { S.bLeftHanded = Value != 0.0f; }
				    else { return FKGDevResult::Fail(TEXT("Keys: Preset FOV X Y Z Bob Sway Left")); }
			    }
			    VM->ApplySettings(S);
			    return FKGDevResult::Ok(FString::Printf(TEXT("VM preset %d fov %.0f offset %s bob %.2f sway %.2f"), S.Preset,
			                                            S.ViewmodelFOV, *S.Offset.ToCompactString(), S.BobScale, S.SwayScale));
		    });
		Add(TEXT("Debug.VMTune"), TEXT("<Property> <Value>"), TEXT("Live-tune a float tuning value of the viewmodel (reflection)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    UKGViewmodelComponent* VM = LocalViewmodel(C);
			    KG_DEV_REQUIRE(VM, TEXT("No first-person body"));
			    KG_DEV_REQUIRE(A.Num() >= 2, TEXT("Usage: Debug.VMTune <Property> <Value>"));
			    FFloatProperty* Prop = FindFProperty<FFloatProperty>(UKGViewmodelComponent::StaticClass(), FName(*A[0]));
			    KG_DEV_REQUIRE(Prop, FString::Printf(TEXT("No float property '%s'"), *A[0]));
			    Prop->SetPropertyValue_InContainer(VM, FloatArg(A, 1, 0.0f));
			    return FKGDevResult::Ok(FString::Printf(TEXT("%s = %g"), *A[0], Prop->GetPropertyValue_InContainer(VM)));
		    });
		Add(TEXT("Debug.Speed"), TEXT(""), TEXT("SPRINT-026: current ground speed, hop streak and stamina (dev panel readout)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const AKGCharacter* Me = C.Requester ? Cast<AKGCharacter>(C.Requester->GetPawn()) : nullptr;
			    KG_DEV_REQUIRE(Me, TEXT("No pawn"));
			    const FString Line = FString::Printf(TEXT("speed %.0f uu/s (%.2f m/s) | hop streak %d | stamina %.0f%%"),
			                                         Me->GetHorizontalSpeed(), Me->GetHorizontalSpeed() / 100.0f,
			                                         Me->GetHopChainStreak(), Me->GetStaminaAlpha() * 100.0f);
			    UE_LOG(LogKillGodot, Log, TEXT("KG_SPEED %s"), *Line);
			    return FKGDevResult::Ok(Line);
		    });
		Add(TEXT("Debug.Dump"), TEXT(""), TEXT("Match state to the log and the clipboard."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const FString Dump = FKGDev::DumpState(C.World);
			    TArray<FString> Lines;
			    Dump.ParseIntoArrayLines(Lines);
			    for (const FString& Line : Lines)
			    {
				    UE_LOG(LogKillGodot, Log, TEXT("KG_DUMP %s"), *Line);
			    }
			    FPlatformApplicationMisc::ClipboardCopy(*Dump);
			    return FKGDevResult::Ok(FString::Printf(TEXT("State dumped (%d lines, copied to clipboard)"), Lines.Num()));
		    });
		Add(TEXT("Debug.Net"), TEXT(""), TEXT("Log network stats (mode, ping, bandwidth)."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    const UNetDriver* Driver = C.World ? C.World->GetNetDriver() : nullptr;
			    const APlayerState* PS = C.Requester ? C.Requester->PlayerState.Get() : nullptr;
			    const FString Line = FString::Printf(TEXT("net %s | ping %.0f ms | in %.1f KB/s out %.1f KB/s | connections %d"),
			                                         C.World->GetNetMode() == NM_Client ? TEXT("client") : C.World->GetNetMode() == NM_ListenServer ? TEXT("listen server") : TEXT("standalone"),
			                                         PS ? PS->GetPingInMilliseconds() : 0.0f,
			                                         Driver ? Driver->InBytesPerSecond / 1024.0f : 0.0f,
			                                         Driver ? Driver->OutBytesPerSecond / 1024.0f : 0.0f,
			                                         Driver ? Driver->ClientConnections.Num() : 0);
			    UE_LOG(LogKillGodot, Log, TEXT("%s"), *Line);
			    return FKGDevResult::Ok(Line);
		    });
		Add(TEXT("Dev.Help"), TEXT(""), TEXT("Log every dev verb."), Local,
		    [](const FKGDevContext& C, const TArray<FString>& A)
		    {
			    TArray<FString> Lines;
			    FKGDev::GetHelpLines(Lines);
			    for (const FString& Line : Lines)
			    {
				    UE_LOG(LogKillGodot, Log, TEXT("%s"), *Line);
			    }
			    return FKGDevResult::Ok(FString::Printf(TEXT("%d verbs (see log)"), Lines.Num()));
		    });
		return V;
	}

	const TArray<FVerb>& Verbs()
	{
		static const TArray<FVerb> Table = BuildVerbs();
		return Table;
	}

	const FVerb* FindVerb(const FString& Name)
	{
		return Verbs().FindByPredicate([&Name](const FVerb& Verb) { return Verb.Name.Equals(Name, ESearchCase::IgnoreCase); });
	}

	// ---- teleport targets ---------------------------------------------------------------------------------------

	/** Fallback when the level has no AKGMapInfo: the level layout JSON that best matches the placed chore stations. */
	void AddLayoutPlaces(UWorld* World, TArray<FKGDevLocation>& Out)
	{
		TMap<FName, FVector> Stations;
		for (TActorIterator<AKGTaskStation> It(World); It; ++It)
		{
			Stations.Add(It->TaskId, It->GetActorLocation());
		}
		double BestError = TNumericLimits<double>::Max();
		TSharedPtr<FJsonObject> Best;
		for (const TCHAR* File : {TEXT("morrowmere_layout_v2.json"), TEXT("morrowmere_layout.json")})
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Tools/Level") / File)) ||
			    !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
			{
				continue;
			}
			double Error = 0.0;
			int32 Matched = 0;
			const TArray<TSharedPtr<FJsonValue>>* Tasks = nullptr;
			if (Root->TryGetArrayField(TEXT("tasks"), Tasks))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Tasks)
				{
					const TSharedPtr<FJsonObject> Task = Value->AsObject();
					FString Id;
					const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
					if (Task && Task->TryGetStringField(TEXT("id"), Id) && Task->TryGetArrayField(TEXT("at"), At) && At->Num() >= 2)
					{
						if (const FVector* Station = Stations.Find(FName(*Id)))
						{
							Error += FVector2D::Distance(FVector2D(*Station), FVector2D((*At)[0]->AsNumber() * 100.0, (*At)[1]->AsNumber() * 100.0));
							++Matched;
						}
					}
				}
			}
			Error = Matched > 0 ? Error / Matched : 1e12;   // no stations: the first file that parses wins
			if (!Best.IsValid() || Error < BestError)
			{
				BestError = Error;
				Best = Root;
			}
		}
		if (!Best.IsValid())
		{
			return;
		}
		double PlateauZ = 5.0;
		Best->TryGetNumberField(TEXT("plateau_z"), PlateauZ);
		auto Place = [&Out, World, PlateauZ](const FString& Name, const FString& Group, const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
		{
			const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
			if (!Obj || !Obj->TryGetArrayField(Field, At) || At->Num() < 2)
			{
				return;
			}
			double Z = PlateauZ;
			Obj->TryGetNumberField(TEXT("z"), Z);
			const FVector2D XY((*At)[0]->AsNumber() * 100.0, (*At)[1]->AsNumber() * 100.0);
			Out.Add({Pretty(Name), Group, FKGDev::GroundAt(World, XY, Z * 100.0)});
		};
		const TArray<TSharedPtr<FJsonValue>>* Areas = nullptr;
		if (Best->TryGetArrayField(TEXT("areas"), Areas))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Areas)
			{
				const TSharedPtr<FJsonObject> Area = Value->AsObject();
				FString Name;
				if (Area && Area->TryGetStringField(TEXT("name"), Name))
				{
					Place(Name, TEXT("Area"), Area, TEXT("center"));
				}
			}
		}
		const TSharedPtr<FJsonObject>* Landmarks = nullptr;
		if (Best->TryGetObjectField(TEXT("landmarks"), Landmarks))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Landmarks)->Values)
			{
				const TSharedPtr<FJsonObject> Mark = Pair.Value->AsObject();
				// Buildings: stand at the door ("face"), not on the roof.
				Place(Pair.Key, TEXT("Landmark"), Mark, Mark && Mark->HasField(TEXT("face")) ? TEXT("face") : TEXT("at"));
			}
		}
	}

	struct FLocationCache
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FKGDevLocation> Places;
	};
}

// ---- FKGDev --------------------------------------------------------------------------------------------------------

APlayerController* FKGDev::LocalController(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get(); PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

UWorld* FKGDev::AuthorityWorld(UWorld* World)
{
	if (!World || World->GetNetMode() != NM_Client || !GEngine)
	{
		return World;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* Other = Context.World();
		if (Other && Other != World && Context.WorldType == EWorldType::PIE && Other->GetNetMode() != NM_Client &&
		    Other->GetAuthGameMode<AKGGameMode>())
		{
			return Other;
		}
	}
	return World;
}

bool FKGDev::MayRun(const APlayerController* Requester)
{
	using namespace KGDevPrivate;
	return !Requester || Requester->IsLocalController() || CVarAllowClients.GetValueOnGameThread() != 0;
}

bool FKGDev::IsServerVerb(const FString& Line)
{
	using namespace KGDevPrivate;
	const TArray<FString> Tokens = Tokenize(Line);
	const FVerb* Verb = Tokens.Num() > 0 ? FindVerb(Tokens[0]) : nullptr;
	return Verb && Verb->Scope == EKGDevScope::Server;
}

FKGDevResult FKGDev::Execute(const FKGDevContext& Context, const FString& Line)
{
	using namespace KGDevPrivate;
	TArray<FString> Tokens = Tokenize(Line);
	if (Tokens.Num() == 0)
	{
		return FKGDevResult::Fail(TEXT("Empty dev command"));
	}
	const FVerb* Verb = FindVerb(Tokens[0]);
	if (!Verb)
	{
		return FKGDevResult::Fail(FString::Printf(TEXT("Unknown dev verb '%s' (kg.Dev.Help)"), *Tokens[0]));
	}
	if (!Context.World)
	{
		return FKGDevResult::Fail(TEXT("No world"));
	}
	if (Verb->Scope == EKGDevScope::Server && Context.World->GetNetMode() == NM_Client)
	{
		return FKGDevResult::Fail(TEXT("Server verb on a client world (use FKGDev::Submit)"));
	}
	Tokens.RemoveAt(0);
	return Verb->Run(Context, Tokens);
}

FKGDevResult FKGDev::Submit(UWorld* World, const FString& Line)
{
	FKGDevResult Result;
	APlayerController* PC = LocalController(World);
	if (!World)
	{
		Result = FKGDevResult::Fail(TEXT("No world"));
	}
	else if (IsServerVerb(Line) && World->GetNetMode() == NM_Client)
	{
		UKGDevComponent* Dev = UKGDevComponent::FindFor(PC);
		if (!Dev)
		{
			Result = FKGDevResult::Fail(TEXT("Dev link not replicated yet"));
		}
		else if (!Dev->bMayRun)
		{
			Result = FKGDevResult::Fail(TEXT("Read-only: only the host can run this (kg.Dev.AllowClients 1 on the host)"));
		}
		else
		{
			Dev->ServerRun(Line);
			return FKGDevResult::Ok(TEXT("Sent to host: ") + Line);   // the host's answer arrives via ClientReport
		}
	}
	else
	{
		Result = Execute({World, PC}, Line);
	}
	Report(World, Result);
	return Result;
}

void FKGDev::Report(UWorld* World, const FKGDevResult& Result)
{
	if (Result.bOk)
	{
		UE_LOG(LogKillGodot, Log, TEXT("Dev: %s"), *Result.Message);
	}
	else
	{
		UE_LOG(LogKillGodot, Warning, TEXT("Dev: %s"), *Result.Message);
	}
	if (UKGDevSubsystem* Dev = UKGDevSubsystem::Get(World))
	{
		Dev->PushMessage(Result.bOk, Result.Message);
	}
}

FVector FKGDev::GroundAt(UWorld* World, const FVector2D& XY, float HintZ)
{
	if (!World)
	{
		return FVector(XY.X, XY.Y, HintZ + 200.0);
	}
	// Every static surface under XY, top to bottom; stand on the lowest one with head room above it. That is the
	// terrain outside, the ground floor inside a building (never its roof), the deck under a bridge.
	TArray<FHitResult> Hits;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KGDevGround), true);
	World->LineTraceMultiByObjectType(Hits, FVector(XY.X, XY.Y, 60000.0), FVector(XY.X, XY.Y, -20000.0), Objects, Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.ImpactPoint.Z > B.ImpactPoint.Z; });
	// KG_DIG hook: the underground (well cellar, catacombs) lies below the village; places above it are on the surface.
	Hits.RemoveAll([World](const FHitResult& H) { return AKGUndergroundInfo::IsBelowGround(World, H.ImpactPoint); });
	for (int32 i = Hits.Num() - 1; i >= 0; --i)
	{
		const double Above = i > 0 ? Hits[i - 1].ImpactPoint.Z : TNumericLimits<double>::Max();
		if (Hits[i].ImpactNormal.Z > 0.5 && Above - Hits[i].ImpactPoint.Z >= 230.0)
		{
			return Hits[i].ImpactPoint + FVector(0.0, 0.0, 110.0);
		}
	}
	return Hits.Num() > 0 ? Hits[0].ImpactPoint + FVector(0.0, 0.0, 110.0) : FVector(XY.X, XY.Y, HintZ + 200.0);
}

const TArray<FKGDevLocation>& FKGDev::GatherLocations(UWorld* World)
{
	using namespace KGDevPrivate;
	static FLocationCache Cache;
	if (Cache.World.Get() == World && Cache.Places.Num() > 0)
	{
		return Cache.Places;
	}
	Cache.World = World;
	Cache.Places.Reset();
	if (!World)
	{
		return Cache.Places;
	}
	TArray<FKGDevLocation>& Out = Cache.Places;

	// 1. The level's map regions (AKGMapInfo, generated with the minimap).
	if (const AKGMapInfo* Map = AKGMapInfo::Find(World))
	{
		for (const FKGMapRegion& Region : Map->Regions)
		{
			const FVector2D XY = !Region.Center.IsNearlyZero() ? Region.Center : Region.LabelPos;
			const FString Name = Region.Name.IsEmpty() ? Pretty(Region.Id.ToString()) : Region.Name.ToString();
			Out.Add({Name, Region.Kind.IsNone() ? FString(TEXT("Place")) : Pretty(Region.Kind.ToString()),
			         GroundAt(World, XY, 500.0f)});
		}
	}
	// 2. Designer markers: any actor tagged KG_Loc_<Name>.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (const FName Tag : It->Tags)
		{
			const FString TagText = Tag.ToString();
			if (TagText.StartsWith(TEXT("KG_Loc_")))
			{
				Out.Add({Pretty(TagText.Mid(7)), TEXT("Marker"), It->GetActorLocation() + FVector(0, 0, 100.0)});
			}
			else if (TagText == TEXT("KG_Gallows"))
			{
				Out.Add({TEXT("Gallows"), TEXT("Marker"), It->GetActorLocation() + FVector(0, 400.0, 150.0)});
			}
		}
	}
	// 3. No map info yet: the layout JSON the level was built from.
	if (!AKGMapInfo::Find(World))
	{
		AddLayoutPlaces(World, Out);
	}
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Out.Add({TEXT("Spawn"), TEXT("Marker"), It->GetActorLocation()});
		break;
	}
	Out.StableSort([](const FKGDevLocation& A, const FKGDevLocation& B)
	{
		return A.Group != B.Group ? A.Group < B.Group : A.Name < B.Name;
	});
	return Out;
}

FString FKGDev::DumpState(UWorld* World)
{
	using namespace KGDevPrivate;
	UWorld* Auth = AuthorityWorld(World);
	const AKGGameState* GS = Auth ? Auth->GetGameState<AKGGameState>() : nullptr;
	const AKGGameMode* GM = Auth ? Auth->GetAuthGameMode<AKGGameMode>() : nullptr;
	if (!GS)
	{
		return TEXT("No KG match in this world");
	}
	FString Out;
	Out += FString::Printf(TEXT("Kill Godot state @ %s (%s, world %s)\n"), *FDateTime::Now().ToString(),
	                       GM ? TEXT("authority") : TEXT("client view"), *Auth->GetMapName());
	Out += FString::Printf(TEXT("phase %s  day %d  left %.1fs/%.0fs%s  clock x%g  seed %s\n"), *PhaseName(GS->GetPhase()),
	                       GS->GetDayIndex(), GS->Clock.RemainingSeconds, GS->Clock.PhaseDuration,
	                       GS->Clock.bPaused ? TEXT(" (frozen)") : TEXT(""), GM ? GM->DevClockScale : 1.0f,
	                       *SeedText(GM ? GM->GetMatchSeed() : 0));
	Out += FString::Printf(TEXT("preparation %.0f%%  winner %s  on trial %s\n"), GS->Preparation * 100.0f,
	                       GS->bHasWinner ? *StaticEnum<EKGAlignment>()->GetNameStringByValue(int64(GS->Winner)) : TEXT("-"),
	                       GS->OnTrial ? *GS->OnTrial->GetPlayerName() : TEXT("-"));
	for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
	{
		const AKGPlayerState* PS = Cast<AKGPlayerState>(GS->PlayerArray[i]);
		if (!PS)
		{
			continue;
		}
		int32 Done = 0;
		for (const bool b : PS->TaskDone)
		{
			Done += b ? 1 : 0;
		}
		const AKGCharacter* Character = CharacterOf(PS);
		const AKGBotController* Bot = Cast<AKGBotController>(ControllerOf(PS));
		Out += FString::Printf(TEXT("#%-2d %-22s %-5s role %-18s %-8s hp %3.0f chores %d/%d %s %s\n"), i, *PS->GetPlayerName(),
		                       PS->IsABot() ? TEXT("bot") : TEXT("human"), *PS->GetPrivateRoleId().ToString(),
		                       *StaticEnum<EKGLifeState>()->GetNameStringByValue(int64(PS->LifeState)),
		                       Character && Character->GetHealth() ? Character->GetHealth()->GetHealth() : 0.0f, Done,
		                       PS->TaskIds.Num(), Character ? *Character->GetActorLocation().ToCompactString() : TEXT("(no body)"),
		                       Bot ? *Bot->GetDevStatus() : TEXT(""));
	}
	return Out;
}

void FKGDev::GetHelpLines(TArray<FString>& OutLines)
{
	using namespace KGDevPrivate;
	OutLines.Add(TEXT("kg.Dev : toggle the dev panel (F1).  kg.Dev <Verb> [args] : run any verb.  kg.Dev.AllowClients 0|1"));
	for (const FVerb& Verb : Verbs())
	{
		OutLines.Add(FString::Printf(TEXT("kg.%s %s : %s%s"), *Verb.Name, *Verb.Usage, *Verb.Help,
		                             Verb.Scope == EKGDevScope::Server ? TEXT(" [host]") : TEXT("")));
	}
}

TArray<FName> FKGDev::GetViewmodelTuningProperties()
{
	TArray<FName> Out;
	for (TFieldIterator<FFloatProperty> It(UKGViewmodelComponent::StaticClass()); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Edit))
		{
			Out.Add(It->GetFName());
		}
	}
	return Out;
}

// ---- console commands ------------------------------------------------------------------------------------------

namespace KGDevPrivate
{
	/** Registers kg.<Verb> for every verb plus kg.Dev (toggle / run). Unregisters on module unload (Live Coding). */
	struct FConsoleRegistrar
	{
		TArray<IConsoleObject*> Objects;

		FConsoleRegistrar()
		{
			IConsoleManager& Console = IConsoleManager::Get();
			Objects.Add(Console.RegisterConsoleCommand(
				TEXT("kg.Dev"), TEXT("Toggle the dev panel (F1). kg.Dev <Verb> [args] runs a dev verb, e.g. kg.Dev Bot.Add 5. kg.Dev.Help lists them."),
				FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
				{
					if (Args.Num() == 0)
					{
						UKGDevSubsystem* Dev = UKGDevSubsystem::Get(World);
						APlayerController* PC = FKGDev::LocalController(World);
						if (Dev && PC)
						{
							Dev->TogglePanel(PC);
						}
						else
						{
							UE_LOG(LogKillGodot, Warning, TEXT("kg.Dev: no local player in a match world"));
						}
						return;
					}
					FKGDev::Submit(World, FString::Join(Args, TEXT(" ")));
				}),
				ECVF_Default));
			for (const FVerb& Verb : Verbs())
			{
				const FString VerbName = Verb.Name;
				Objects.Add(Console.RegisterConsoleCommand(
					*(TEXT("kg.") + Verb.Name), *FString::Printf(TEXT("kg.%s %s : %s%s"), *Verb.Name, *Verb.Usage, *Verb.Help,
					                                              Verb.Scope == EKGDevScope::Server ? TEXT(" [host]") : TEXT("")),
					FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([VerbName](const TArray<FString>& Args, UWorld* World)
					{
						FString Line = VerbName;
						for (const FString& Arg : Args)
						{
							Line += Arg.Contains(TEXT(" ")) ? FString::Printf(TEXT(" \"%s\""), *Arg) : TEXT(" ") + Arg;
						}
						FKGDev::Submit(World, Line);
					}),
					ECVF_Default));
			}
		}

		~FConsoleRegistrar()
		{
			if (IConsoleManager* Console = &IConsoleManager::Get())
			{
				for (IConsoleObject* Object : Objects)
				{
					if (Object)
					{
						Console->UnregisterConsoleObject(Object, false);
					}
				}
			}
		}
	};

	static FConsoleRegistrar GConsoleRegistrar;
}

#undef KG_DEV_REQUIRE

#endif
