// SPRINT-016 world chore dev commands (development builds only):
//
//   kg.WorldChore.List                      catalog + your world chore progress (log)
//   kg.WorldChore.Give <Chore> [Variant]    put a world chore on your list (host; clients need kg.Dev.AllowClients 1)
//   kg.WorldChore.Goto <anchor|next>        teleport next to a spot (with whatever you carry)
//   kg.WorldChore.Path <anchor|next>        walk there on the navmesh (client autopilot, like the smoke)
//   kg.WorldChore.Use <anchor|next>         E on a spot without aiming
//   kg.WorldChore.Skip [Chore]              complete the next pending target of a step
//   kg.WorldChore.Spoil <anchor>            [host] sabotage a spot (poisoned trough / snuffed lamp) for testing
//   kg.WorldChore.Reload                    re-read Tools/Level/morrowmere_world_chores.resolved.json
//   kg.WorldChore.Routes                    [standalone, navmesh] every chore x variant: legs on the navmesh, length,
//                                           time at walking / carrying speed (+ ladders, work) -> Saved/KG_WorldChoreRoutes.json
//   kg.WorldChore.Pose <Chore|Twist_*>      [standalone] stage a chore's key moment (spot states, items, villagers)
//   kg.WorldChore.Shots [a,b,...] [quit]    [standalone, -RenderOffScreen] pose + HighResShot for each (WC_<name>.png)
//                                           (Tools/Unreal/kg_worldchore_capture.ps1)

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Character/KGCharacter.h"
#include "Chores/KGChoreFx.h"
#include "Chores/KGChoreTypes.h"
#include "Chores/WorldChores/KGWorldChoreComponent.h"
#include "Chores/WorldChores/KGWorldChoreTypes.h"
#include "Chores/WorldChores/KGWorldChoreWorld.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "KillGodot.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "World/KGChoreItem.h"

namespace KGWorldChoreDebug
{
	UWorld* GameWorld(UWorld* Hint)
	{
		if (Hint && (Hint->WorldType == EWorldType::Game || Hint->WorldType == EWorldType::PIE))
		{
			return Hint;
		}
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.World() && (C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE))
			{
				return C.World();
			}
		}
		return Hint;
	}

	/** Runs a dev line for the local player's body: directly on the host, through ServerDev on a client. */
	void RunForMe(UWorld* World, const FString& Line)
	{
		World = GameWorld(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		UKGWorldChoreComponent* WC = PC ? UKGWorldChoreComponent::FindFor(PC->GetPawn()) : nullptr;
		if (!WC)
		{
			UE_LOG(LogKillGodot, Warning, TEXT("KG_WORLDCHORE_DEV no world chore component on your body (not on the world chore map?)"));
			return;
		}
		if (WC->GetOwner()->HasAuthority())
		{
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_DEV %s -> %s"), *Line, *WC->AuthDev(Line));
		}
		else
		{
			WC->ServerDev(Line);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GList(TEXT("kg.WorldChore.List"), TEXT("World chores: catalog and your progress."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			World = GameWorld(World);
			const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_LIST map=%s chores=%d anchors=%d items=%d here=%d"), *Cat.MapName, Cat.Chores.Num(),
			       Cat.Anchors.Num(), Cat.Items.Num(), Cat.ForMap(World) ? 1 : 0);
			for (const FKGWorldChoreDef& D : Cat.Chores)
			{
				FString Steps;
				for (int32 s = 0; s < D.NumSteps(); ++s)
				{
					Steps += FString::Printf(TEXT("%s%s@%s"), s ? TEXT(" > ") : TEXT(""), *KGWorldChores::VerbName(D.Steps[s].Verb),
					                         *FString::JoinBy(D.Targets(s, 0), TEXT("+"), [](const FName& N) { return N.ToString(); }));
				}
				UE_LOG(LogKillGodot, Log, TEXT("  %s \"%s\" variants=%d bots=%d climb=%d: %s"), *D.Id.ToString(), *D.Title, D.NumVariants(),
				       D.bBots ? 1 : 0, D.NeedsClimb() ? 1 : 0, *Steps);
			}
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (const UKGWorldChoreComponent* WC = PC ? UKGWorldChoreComponent::FindFor(PC->GetPawn()) : nullptr)
			{
				for (const FKGWorldProgress& P : WC->GetProgress())
				{
					UE_LOG(LogKillGodot, Log, TEXT("  mine: %s step=%d variant=%d mask=%d reps=%d item=%s"), *P.Chore.ToString(), P.Step + 1, P.Variant,
					       P.DoneMask, P.Reps, *GetNameSafe(P.Item));
				}
				UE_LOG(LogKillGodot, Log, TEXT("  now: %s"), *WC->GetActiveLabel());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GGive(TEXT("kg.WorldChore.Give"), TEXT("kg.WorldChore.Give <Chore> [Variant]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunForMe(World, TEXT("Give ") + FString::Join(Args, TEXT(" ")));
		}));

	FAutoConsoleCommandWithWorldAndArgs GGoto(TEXT("kg.WorldChore.Goto"), TEXT("kg.WorldChore.Goto <anchor|next>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunForMe(World, TEXT("Goto ") + (Args.Num() ? Args[0] : FString(TEXT("next"))));
		}));

	FAutoConsoleCommandWithWorldAndArgs GPath(TEXT("kg.WorldChore.Path"), TEXT("kg.WorldChore.Path <anchor|next> (walks there)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunForMe(World, TEXT("Path ") + (Args.Num() ? Args[0] : FString(TEXT("next"))));
		}));

	FAutoConsoleCommandWithWorldAndArgs GUse(TEXT("kg.WorldChore.Use"), TEXT("kg.WorldChore.Use <anchor|next>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunForMe(World, TEXT("Use ") + (Args.Num() ? Args[0] : FString(TEXT("next"))));
		}));

	FAutoConsoleCommandWithWorldAndArgs GSkip(TEXT("kg.WorldChore.Skip"), TEXT("kg.WorldChore.Skip [Chore]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunForMe(World, TEXT("Skip ") + FString::Join(Args, TEXT(" ")));
		}));

	FAutoConsoleCommandWithWorldAndArgs GReload(TEXT("kg.WorldChore.Reload"), TEXT("Re-read the world chore data from disk."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FString Msg;
			const bool bOk = FKGWorldChoreCatalog::Reload(Msg);
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_RELOAD %s: %s (respawn the map to move spots)"), bOk ? TEXT("ok") : TEXT("FAILED"), *Msg);
		}));

	FAutoConsoleCommandWithWorldAndArgs GSpoil(TEXT("kg.WorldChore.Spoil"), TEXT("[host] kg.WorldChore.Spoil <anchor>: sabotage a spot"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			World = GameWorld(World);
			AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(World);
			const int32 Index = Args.Num() ? FKGWorldChoreCatalog::Get().AnchorIndex(FName(*Args[0])) : INDEX_NONE;
			if (FKGSpotState* S = Dir ? Dir->AuthMutableSpot(Index) : nullptr)
			{
				S->bSpoiled = true;
				S->bLit = false;
				S->Level = FMath::Max(S->Level, 0.6f);
				Dir->AuthDirty();
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SABOTAGE dev at %s"), *Args[0]);
			}
		}));

	// ---- route check ---------------------------------------------------------------------------------------------------

	FVector NavPoint(UWorld* World, const FVector& P)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FNavLocation Out;
		if (Nav && Nav->ProjectPointToNavigation(P + FVector(0.0f, 0.0f, 60.0f), Out, FVector(150.0f, 150.0f, 250.0f)))
		{
			return Out.Location;
		}
		return P + FVector(0.0f, 0.0f, 60.0f);
	}

	/** Navmesh path length (cm) between two stand points, -1 when unreachable. */
	float PathCm(UWorld* World, const FVector& A, const FVector& B)
	{
		if (FVector::DistSquared(A, B) < FMath::Square(30.0f))
		{
			return 0.0f;
		}
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, NavPoint(World, A), NavPoint(World, B));
		return Path && Path->IsValid() && !Path->IsPartial() ? static_cast<float>(Path->GetPathLength()) : -1.0f;
	}

	TSharedRef<FJsonObject> RouteOne(UWorld* World, const FKGWorldChoreDef& Def, int32 Variant, bool& bOutOk, float& OutTotal)
	{
		const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
		const UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(World);
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Legs;
		float WalkS = 0.0f, WorkS = 0.0f, ClimbS = 0.0f, LenCm = 0.0f;
		bool bOk = true;
		const TArray<FName> First = Def.Targets(0, Variant);
		FVector At = Sub->StandLocation(Cat.AnchorIndex(First[0]));
		const float FromHub = PathCm(World, Cat.Hub, At);
		for (int32 s = 0; s < Def.NumSteps(); ++s)
		{
			const FKGWorldStepDef& S = Def.Steps[s];
			const FKGWorldItemDef* Item = S.Verb == EKGWorldVerb::Bring ? Cat.FindItem(S.Item) : nullptr;
			const float Speed = Cat.WalkSpeed * (Item ? FKGWorldChoreRules::CarrySpeedFactor(*Item, 1) : 1.0f);
			TArray<FName> Todo = Def.Targets(s, Variant);
			while (Todo.Num() > 0)
			{
				// Nearest next target first (how a player walks an "all of" list).
				int32 Pick = 0;
				float Best = TNumericLimits<float>::Max();
				for (int32 i = 0; i < Todo.Num(); ++i)
				{
					const float D = FVector::Dist(At, Sub->StandLocation(Cat.AnchorIndex(Todo[i])));
					if (D < Best)
					{
						Best = D;
						Pick = i;
					}
				}
				const int32 Index = Cat.AnchorIndex(Todo[Pick]);
				const FKGWorldAnchor& A = Cat.Anchors[Index];
				const FVector To = Sub->StandLocation(Index);
				const float Cm = PathCm(World, At, To);
				TSharedRef<FJsonObject> L = MakeShared<FJsonObject>();
				L->SetNumberField(TEXT("step"), s + 1);
				L->SetStringField(TEXT("verb"), KGWorldChores::VerbName(S.Verb));
				L->SetStringField(TEXT("to"), A.Id.ToString());
				L->SetNumberField(TEXT("len_m"), FMath::RoundToFloat(Cm) / 100.0f);
				L->SetBoolField(TEXT("reachable"), Cm >= 0.0f);
				L->SetStringField(TEXT("carry"), Item ? Item->Kind.ToString() : TEXT("-"));
				if (Cm < 0.0f)
				{
					bOk = false;
				}
				else
				{
					LenCm += Cm;
					WalkS += Cm / Speed;
				}
				if (A.ClimbCm > 1.0f)
				{
					// Up the ladder, and back down only when another step follows (the chore ends at the top otherwise).
					const bool bLastLeg = s == Def.NumSteps() - 1 && Todo.Num() == 1;
					ClimbS += (bLastLeg ? 1.0f : 2.0f) * A.ClimbCm / Cat.ClimbSpeed;
					L->SetNumberField(TEXT("climb_m"), A.ClimbCm / 100.0f);
				}
				Legs.Add(MakeShared<FJsonValueObject>(L));
				At = To;
				Todo.RemoveAt(Pick);
				WorkS += S.Verb == EKGWorldVerb::Panel ? S.BotSecs : S.Secs * S.Repeat;
			}
		}
		const float Total = WalkS + WorkS + ClimbS;
		O->SetStringField(TEXT("variant"), Def.Variants.IsValidIndex(Variant) ? Def.Variants[Variant].Name.ToString() : TEXT("default"));
		O->SetNumberField(TEXT("length_m"), FMath::RoundToFloat(LenCm / 10.0f) / 10.0f);
		O->SetNumberField(TEXT("walk_s"), FMath::RoundToFloat(WalkS * 10.0f) / 10.0f);
		O->SetNumberField(TEXT("work_s"), FMath::RoundToFloat(WorkS * 10.0f) / 10.0f);
		O->SetNumberField(TEXT("climb_s"), FMath::RoundToFloat(ClimbS * 10.0f) / 10.0f);
		O->SetNumberField(TEXT("total_s"), FMath::RoundToFloat(Total * 10.0f) / 10.0f);
		O->SetNumberField(TEXT("from_square_m"), FMath::RoundToFloat(FromHub / 10.0f) / 10.0f);
		O->SetBoolField(TEXT("reachable"), bOk);
		O->SetBoolField(TEXT("in_30_75"), Total >= 30.0f && Total <= 75.0f);
		// User target (2026-09-25, "keep it simple but fun"): 25-50 s; with_travel adds the walk from the square.
		const float WithTravel = Total + FMath::Max(0.0f, FromHub) / Cat.WalkSpeed;
		O->SetNumberField(TEXT("with_travel_s"), FMath::RoundToFloat(WithTravel * 10.0f) / 10.0f);
		O->SetBoolField(TEXT("in_25_50"), WithTravel >= 25.0f && WithTravel <= 50.0f);
		O->SetArrayField(TEXT("legs"), Legs);
		bOutOk = bOk;
		OutTotal = Total;
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_ROUTE %s/%s reachable=%d length=%.1fm walk=%.1fs work=%.1fs climb=%.1fs total=%.1fs in_25_50=%d from_square=%.1fm with_travel=%.1fs"),
		       *Def.Id.ToString(), *O->GetStringField(TEXT("variant")), bOk ? 1 : 0, LenCm / 100.0f, WalkS, WorkS, ClimbS, Total,
		       O->GetBoolField(TEXT("in_25_50")) ? 1 : 0, FromHub / 100.0f, WithTravel);
		return O;
	}

	FAutoConsoleCommandWithWorldAndArgs GRoutes(TEXT("kg.WorldChore.Routes"),
		TEXT("Navmesh route check of every world chore x variant -> Saved/KG_WorldChoreRoutes.json"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			World = GameWorld(World);
			UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(World);
			if (!Sub || !FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_WORLDCHORE_ROUTE needs the world chore map with its navmesh"));
				return;
			}
			Sub->SetupWorld(true);
			const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
			TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
			int32 Pass = 0, Fail = 0, InTarget = 0;
			for (const FKGWorldChoreDef& Def : Cat.Chores)
			{
				TArray<TSharedPtr<FJsonValue>> Vars;
				for (int32 v = 0; v < Def.NumVariants(); ++v)
				{
					bool bOk = false;
					float Total = 0.0f;
					Vars.Add(MakeShared<FJsonValueObject>(RouteOne(World, Def, v, bOk, Total)));
					// Pass = every step reachable (acceptance 2). Target (user, 2026-09-25): 25-50 s with the walk from the square.
					(bOk ? Pass : Fail) += 1;
					const TSharedPtr<FJsonObject> Last = Vars.Last()->AsObject();
					InTarget += Last->GetBoolField(TEXT("in_25_50")) ? 1 : 0;
				}
				Root->SetArrayField(Def.Id.ToString(), Vars);
			}
			Root->SetNumberField(TEXT("pass"), Pass);
			Root->SetNumberField(TEXT("fail"), Fail);
			Root->SetNumberField(TEXT("in_25_50"), InTarget);
			FString Out;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
			FJsonSerializer::Serialize(Root, Writer);
			FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("KG_WorldChoreRoutes.json")));
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_ROUTES done: %d pass, %d fail (reachable); %d of %d within 25-50 s incl. the walk from the square -> Saved/KG_WorldChoreRoutes.json"),
			       Pass, Fail, InTarget, Pass + Fail);
		}));

	// ---- poses + shots -------------------------------------------------------------------------------------------------

	TArray<TWeakObjectPtr<AActor>> GPoseActors;

	void ClearPose(UWorld* World)
	{
		for (const TWeakObjectPtr<AActor>& A : GPoseActors)
		{
			if (A.IsValid())
			{
				A->Destroy();
			}
		}
		GPoseActors.Reset();
		if (AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(World))
		{
			for (int32 i = 0; i < FKGWorldChoreCatalog::Get().Anchors.Num(); ++i)
			{
				if (FKGSpotState* S = Dir->AuthMutableSpot(i))
				{
					*S = FKGSpotState();
				}
			}
		}
	}

	FKGSpotState* Spot(UWorld* World, const TCHAR* Id)
	{
		AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(World);
		return Dir ? Dir->AuthMutableSpot(FKGWorldChoreCatalog::Get().AnchorIndex(FName(Id))) : nullptr;
	}

	FVector SpotAt(UWorld* World, const TCHAR* Id)
	{
		return UKGWorldChoreSubsystem::Get(World)->SpotLocation(FKGWorldChoreCatalog::Get().AnchorIndex(FName(Id)));
	}

	FVector StandAt(UWorld* World, const TCHAR* Id)
	{
		return UKGWorldChoreSubsystem::Get(World)->StandLocation(FKGWorldChoreCatalog::Get().AnchorIndex(FName(Id)));
	}

	AKGChoreItem* PoseItem(UWorld* World, const TCHAR* Kind, const FVector& At, float Yaw = 0.0f)
	{
		AKGChoreItem* Item = AKGChoreItem::AuthSpawn(World, FName(Kind), At, Yaw, nullptr, TEXT("Pose"));
		if (Item)
		{
			GPoseActors.Add(Item);
		}
		return Item;
	}

	/** A villager standing at At (ground) looking toward LookAt, optionally carrying an item. */
	AKGCharacter* PoseVillager(UWorld* World, const FVector& At, const FVector& LookAt, const TCHAR* Carry = nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Face = (LookAt - At).GetSafeNormal2D().Rotation();
		AKGCharacter* Body = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(Face, At + FVector(0.0f, 0.0f, 95.0f)), Params);
		if (!Body)
		{
			return nullptr;
		}
		GPoseActors.Add(Body);
		if (Carry)
		{
			if (AKGChoreItem* Item = PoseItem(World, Carry, At + Face.Vector() * 50.0f))
			{
				Item->AuthAttachTo(Body);
			}
		}
		return Body;
	}

	struct FShotCam
	{
		FVector Eye;
		FVector Target;
		float Fov = 70.0f;
	};

	FShotCam Around(const FVector& Focus, float Yaw, float Dist, float Height, float LookUp = 60.0f, float Fov = 70.0f)
	{
		const FVector Dir = FRotator(0.0f, Yaw, 0.0f).Vector();
		return {Focus + Dir * Dist + FVector(0.0f, 0.0f, Height), Focus + FVector(0.0f, 0.0f, LookUp), Fov};
	}

	/** Stages a key moment; returns the camera. False = unknown pose. */
	bool Pose(UWorld* World, const FString& Name, FShotCam& Cam)
	{
		ClearPose(World);
		const FKGWorldChoreCatalog& Cat = FKGWorldChoreCatalog::Get();
		auto Yaw = [&Cat](const TCHAR* Id) { const FKGWorldAnchor* A = Cat.FindAnchor(FName(Id)); return A ? A->Yaw : 0.0f; };
		if (Name == TEXT("WaterRun"))
		{
			// Pouring the well water into the fountain trough; the trough three quarters full.
			if (FKGSpotState* S = Spot(World, TEXT("fountain_trough"))) { S->Level = 0.75f; }
			const FVector T = SpotAt(World, TEXT("fountain_trough"));
			PoseVillager(World, T + FVector(-120.0f, 30.0f, 0.0f), T, TEXT("Bucket"));
			if (AKGChoreItem* B = PoseItem(World, TEXT("Bucket"), T + FVector(-60.0f, -95.0f, 0.0f))) { B->AuthSetFill(0.9f); }
			for (AKGChoreItem* I : TActorRange<AKGChoreItem>(World)) { I->AuthSetFill(0.95f); }
			Cam = Around(T, 150.0f, 330.0f, 170.0f, 40.0f);
		}
		else if (Name == TEXT("FishToMarket"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("brine_table"))) { S->Count = 3; }
			const FVector T = SpotAt(World, TEXT("brine_table"));
			const FVector Dir = FRotator(0.0f, Yaw(TEXT("brine_table")), 0.0f).Vector();
			PoseVillager(World, T - Dir * 260.0f, T, TEXT("Crate"));
			Cam = Around(T - Dir * 150.0f, Yaw(TEXT("brine_table")) + 70.0f, 420.0f, 190.0f, 50.0f);
		}
		else if (Name == TEXT("BreadDelivery"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("knock_H05"))) { S->Count = 1; }
			const FVector T = SpotAt(World, TEXT("knock_H05"));
			const FVector Out = FRotator(0.0f, Yaw(TEXT("knock_H05")), 0.0f).Vector();
			PoseVillager(World, T + Out * 120.0f, T, TEXT("Loaves"));
			for (AKGChoreItem* I : TActorRange<AKGChoreItem>(World)) { I->AuthSetPieces(2); }
			Cam = Around(T, Yaw(TEXT("knock_H05")) + 35.0f, 360.0f, 150.0f, 70.0f);
		}
		else if (Name == TEXT("Lamplighter"))
		{
			for (const TCHAR* L : {TEXT("lamp_1"), TEXT("lamp_2"), TEXT("lamp_3")})
			{
				if (FKGSpotState* S = Spot(World, L)) { S->bLit = true; }
			}
			const FVector T = SpotAt(World, TEXT("lamp_3"));
			PoseVillager(World, StandAt(World, TEXT("lamp_4")), SpotAt(World, TEXT("lamp_4")), TEXT("Taper"));
			Cam = Around(T, Yaw(TEXT("lamp_3")) + 150.0f, 650.0f, 260.0f, 200.0f, 75.0f);
		}
		else if (Name == TEXT("BellAndClock"))
		{
			const FVector Bell = SpotAt(World, TEXT("bell_rope"));
			if (AKGChoreFx* Fx = AKGChoreFx::Get(World, true))
			{
				Fx->AuthTrigger(TEXT("BellAndClock"), EKGChoreFx::BellRing, Bell, FRotator::ZeroRotator);
			}
			PoseVillager(World, Bell + FVector(-60.0f, 60.0f, 0.0f), Bell);
			Cam = Around(Bell, Yaw(TEXT("bell_rope")) + 200.0f, 380.0f, 120.0f, 120.0f, 80.0f);
		}
		else if (Name == TEXT("Nets"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("quay_posts"))) { S->Count = 1; }
			const FVector T = SpotAt(World, TEXT("quay_posts"));
			PoseVillager(World, T + FVector(150.0f, 150.0f, 0.0f), T, TEXT("Net"));
			Cam = Around(T, Yaw(TEXT("quay_posts")) - 60.0f, 450.0f, 180.0f, 110.0f);
		}
		else if (Name == TEXT("Firewood"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("chop_block"))) { S->Count = 2; }
			const FVector T = SpotAt(World, TEXT("chop_block"));
			PoseVillager(World, T + FVector(90.0f, 60.0f, 0.0f), T);
			PoseItem(World, TEXT("Bundle"), T + FVector(40.0f, -110.0f, 0.0f), 30.0f);
			Cam = Around(T, 30.0f, 380.0f, 160.0f, 50.0f);
		}
		else if (Name == TEXT("Letters"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("box_H14"))) { S->Count = 1; }
			const FVector T = SpotAt(World, TEXT("box_H14"));
			const FVector Out = FRotator(0.0f, Yaw(TEXT("box_H14")), 0.0f).Vector();
			PoseVillager(World, T + Out * 110.0f + FRotator(0.0f, Yaw(TEXT("box_H14")) + 90.0f, 0.0f).Vector() * 60.0f, T, TEXT("Letters"));
			Cam = Around(T, Yaw(TEXT("box_H14")) - 25.0f, 230.0f, 120.0f, 110.0f, 65.0f);
		}
		else if (Name == TEXT("GrainToMill"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("mill_hopper"))) { S->Count = 1; }
			const FVector T = SpotAt(World, TEXT("mill_hopper"));
			const FVector Out = FRotator(0.0f, Yaw(TEXT("mill_hopper")), 0.0f).Vector();
			PoseVillager(World, T + Out * 150.0f, T, TEXT("Sack"));
			Cam = Around(T, Yaw(TEXT("mill_hopper")) + 40.0f, 520.0f, 220.0f, 120.0f);
		}
		else if (Name == TEXT("LighthouseOil"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("lighthouse_lamp"))) { S->bLit = true; }
			const FVector T = SpotAt(World, TEXT("lighthouse_lamp"));
			if (AKGChoreFx* Fx = AKGChoreFx::Get(World, true))
			{
				Fx->AuthTrigger(TEXT("LighthouseOil"), EKGChoreFx::LighthouseGlow, T, FRotator::ZeroRotator);
			}
			PoseVillager(World, T + FVector(80.0f, -60.0f, 0.0f), T, TEXT("OilCan"));
			Cam = Around(T, Yaw(TEXT("lighthouse_lamp")) + 180.0f, 380.0f, 60.0f, 100.0f, 80.0f);
		}
		else if (Name == TEXT("Twist_Poison"))
		{
			if (FKGSpotState* S = Spot(World, TEXT("inn_trough"))) { S->Level = 0.8f; S->bSpoiled = true; }
			const FVector T = SpotAt(World, TEXT("inn_trough"));
			PoseVillager(World, T + FVector(-110.0f, 90.0f, 0.0f), T);
			Cam = Around(T, 200.0f, 300.0f, 170.0f, 30.0f);
		}
		else if (Name == TEXT("Twist_TwoCarry"))
		{
			const FVector T = StandAt(World, TEXT("jetty_crates"));
			const FVector Along = (SpotAt(World, TEXT("brine_table")) - T).GetSafeNormal2D();
			const FVector Side = FVector(-Along.Y, Along.X, 0.0f);
			AKGCharacter* A = PoseVillager(World, T + Along * 300.0f + Side * 55.0f, T + Along * 800.0f + Side * 55.0f);
			PoseVillager(World, T + Along * 300.0f - Side * 55.0f, T + Along * 800.0f - Side * 55.0f);
			if (AKGChoreItem* Crate = PoseItem(World, TEXT("Crate"), T + Along * 350.0f))
			{
				if (A)
				{
					Crate->AuthAttachTo(A);
					Crate->SetActorRelativeLocation(FVector(45.0f, -55.0f, 15.0f));
				}
			}
			Cam = Around(T + Along * 320.0f, (Along.Rotation().Yaw) + 20.0f, 420.0f, 170.0f, 90.0f);
		}
		else
		{
			return false;
		}
		if (AKGWorldChoreDirector* Dir = AKGWorldChoreDirector::Get(World))
		{
			Dir->AuthDirty();
		}
		return true;
	}

	void AimCamera(UWorld* World, const FShotCam& Cam)
	{
		ACameraActor* CamActor = nullptr;
		for (TActorIterator<ACameraActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("KG_CaptureCam")))
			{
				CamActor = *It;
				break;
			}
		}
		if (!CamActor)
		{
			FActorSpawnParameters Params;
			CamActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Cam.Eye), Params);
			CamActor->Tags.Add(TEXT("KG_CaptureCam"));
		}
		CamActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		CamActor->SetActorLocationAndRotation(Cam.Eye, (Cam.Target - Cam.Eye).Rotation());
		CamActor->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Perspective);
		CamActor->GetCameraComponent()->SetFieldOfView(Cam.Fov);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(CamActor, 0.0f);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GPose(TEXT("kg.WorldChore.Pose"), TEXT("[standalone] kg.WorldChore.Pose <Chore|Twist_Poison|Twist_TwoCarry>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			World = GameWorld(World);
			if (UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(World))
			{
				Sub->SetupWorld(true);
			}
			FShotCam Cam;
			if (Args.Num() && World && World->GetNetMode() != NM_Client && Pose(World, Args[0], Cam))
			{
				AimCamera(World, Cam);
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_POSE %s eye=%s"), *Args[0], *Cam.Eye.ToCompactString());
			}
			else
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_WORLDCHORE_POSE unknown or not the host: %s"), Args.Num() ? *Args[0] : TEXT("-"));
			}
		}));

	struct FShots
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FString> Todo;
		FString Current;
		double NextAt = 0.0;
		int32 State = 0;
		bool bQuit = false;
		FTSTicker::FDelegateHandle Handle;
	};
	TSharedPtr<FShots> GShots;

	bool TickShots(float Dt)
	{
		FShots& S = *GShots;
		UWorld* World = S.World.Get();
		if (!World)
		{
			GShots.Reset();
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		if (Now < S.NextAt)
		{
			return true;
		}
		if (S.State == 0)
		{
			if (S.Todo.Num() == 0)
			{
				ClearPose(World);
				UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SHOTS done"));
				if (S.bQuit)
				{
					GEngine->Exec(World, TEXT("quit"));
				}
				GShots.Reset();
				return false;
			}
			S.Current = S.Todo[0];
			S.Todo.RemoveAt(0);
			FShotCam Cam;
			if (Pose(World, S.Current, Cam))
			{
				AimCamera(World, Cam);
				S.State = 1;
				S.NextAt = Now + 2.5;   // items settle, lights and streaming catch up
			}
			return true;
		}
		GEngine->Exec(World, *FString::Printf(TEXT("HighResShot 1600x900 filename=WC_%s"), *S.Current));
		UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SHOT %s"), *S.Current);
		S.State = 0;
		S.NextAt = Now + 2.0;
		return true;
	}

	FAutoConsoleCommandWithWorldAndArgs GShotsCmd(TEXT("kg.WorldChore.Shots"),
		TEXT("[standalone, -RenderOffScreen] kg.WorldChore.Shots [a,b,...|all] [quit]: pose + HighResShot WC_<name>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			World = GameWorld(World);
			if (UKGWorldChoreSubsystem* Sub = UKGWorldChoreSubsystem::Get(World))
			{
				Sub->SetupWorld(true);
			}
			GShots = MakeShared<FShots>();
			GShots->World = World;
			const FString What = Args.Num() ? Args[0] : FString(TEXT("all"));
			if (What.Equals(TEXT("all"), ESearchCase::IgnoreCase))
			{
				for (const FKGWorldChoreDef& D : FKGWorldChoreCatalog::Get().Chores)
				{
					GShots->Todo.Add(D.Id.ToString());
				}
				GShots->Todo.Add(TEXT("Twist_Poison"));
				GShots->Todo.Add(TEXT("Twist_TwoCarry"));
			}
			else
			{
				What.ParseIntoArray(GShots->Todo, TEXT(","));
			}
			GShots->bQuit = Args.Contains(TEXT("quit"));
			GShots->NextAt = FPlatformTime::Seconds() + 1.0;
			GEngine->Exec(World, TEXT("DisableAllScreenMessages"));
			GShots->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickShots), 0.1f);
			UE_LOG(LogKillGodot, Log, TEXT("KG_WORLDCHORE_SHOTS %d poses"), GShots->Todo.Num());
		}));
}

#endif
