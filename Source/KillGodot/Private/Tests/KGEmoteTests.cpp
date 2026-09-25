#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Animation/AnimSequence.h"
#include "AssetCompilingManager.h"
#include "Character/KGBodyAnimInstance.h"
#include "Character/KGCharacter.h"
#include "Chat/KGChatComponent.h"
#include "Chat/KGEmoji.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Emote/KGEmoteCatalog.h"
#include "Emote/KGEmoteComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"

namespace KGEmoteTests
{
	using P = EKGPhase;

	FKGChatParticipant Alive()
	{
		return FKGChatParticipant();
	}

	FKGChatParticipant Ghost()
	{
		FKGChatParticipant Who;
		Who.Life = EKGLifeState::Ghost;
		return Who;
	}

	const FKGEmoteDef& Def(const TCHAR* Id)
	{
		const FKGEmoteDef* Found = FKGEmoteCatalog::Find(FName(Id));
		check(Found);
		return *Found;
	}

	/** Headless server world running AKGGameMode (like KillGodot.Dev.*), plus bodies for bots. */
	struct FServerWorld
	{
		UWorld* World = nullptr;

		bool Create()
		{
			if (!GEngine)
			{
				return false;
			}
			// Unique name: the previous test's world may not be garbage collected yet.
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("KGEmoteTestWorld"));
			World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
			if (!World)
			{
				return false;
			}
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GetTransientPackage()));
			World->GetWorldSettings()->DefaultGameMode = AKGGameMode::StaticClass();
			const FURL URL;
			World->SetGameMode(URL);
			World->InitializeActorsForPlay(URL);
			World->BeginPlay();
			return World->GetAuthGameMode<AKGGameMode>() != nullptr && World->GetGameState<AKGGameState>() != nullptr;
		}

		~FServerWorld()
		{
			if (World && GEngine)
			{
				World->EndPlay(EEndPlayReason::Quit);
				World->BeginTearingDown();
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					It->Destroy();
				}
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		FKGDevResult Run(const FString& Line) const { return FKGDev::Execute({World, nullptr}, Line); }
		AKGGameState* GS() const { return World->GetGameState<AKGGameState>(); }

		/** A standing villager body (no movement ticking: the test drives velocity by hand). */
		AKGCharacter* SpawnBody(const FVector& At) const
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKGCharacter* Body = World->SpawnActor<AKGCharacter>(AKGCharacter::StaticClass(), FTransform(At), Params);
			if (Body)
			{
				Park(Body);
			}
			return Body;
		}

		static void Park(AKGCharacter* Body)
		{
			UCharacterMovementComponent* Move = Body->GetCharacterMovement();
			Move->SetComponentTickEnabled(false);
			Move->SetMovementMode(MOVE_Walking);
			Move->Velocity = FVector::ZeroVector;
		}

		/** Bot #Index with a body (headless worlds have no player starts: spawn + possess one if needed). */
		AKGCharacter* BotBody(int32 Index) const
		{
			AKGPlayerState* PS = GS()->PlayerArray.IsValidIndex(Index) ? Cast<AKGPlayerState>(GS()->PlayerArray[Index]) : nullptr;
			AController* Controller = PS ? Cast<AController>(PS->GetOwner()) : nullptr;
			if (!Controller)
			{
				return nullptr;
			}
			AKGCharacter* Body = Cast<AKGCharacter>(Controller->GetPawn());
			if (!Body)
			{
				Body = SpawnBody(FVector(400.0 * Index, 0.0, 200.0));
				if (Body)
				{
					Controller->Possess(Body);
				}
			}
			if (Body)
			{
				Park(Body);
			}
			return Body;
		}
	};

	void Tick(UKGEmoteComponent* Emote, float Seconds, float Step = 0.05f)
	{
		for (float T = 0.0f; T < Seconds; T += Step)
		{
			Emote->TickComponent(Step, LEVELTICK_All, nullptr);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteCatalogTest, "KillGodot.Emote.Catalog",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FKGEmoteDef>& All = FKGEmoteCatalog::GetAll();
	TestTrue(TEXT("12-16 emotes"), All.Num() >= 12 && All.Num() <= 16);
	TSet<FString> Words;
	// Chat commands an emote alias must never shadow.
	for (const TCHAR* Cmd : {TEXT("all"), TEXT("a"), TEXT("town"), TEXT("say"), TEXT("s"), TEXT("near"), TEXT("n"), TEXT("local"),
	                         TEXT("l"), TEXT("team"), TEXT("t"), TEXT("imp"), TEXT("dead"), TEXT("d"), TEXT("ghost"), TEXT("g"),
	                         TEXT("r"), TEXT("react"), TEXT("e"), TEXT("mute"), TEXT("block"), TEXT("unmute"), TEXT("unblock"),
	                         TEXT("report"), TEXT("help"), TEXT("emojis"), TEXT("emoji"), TEXT("emotes"), TEXT("stop")})
	{
		Words.Add(Cmd);
	}
	int32 FullBody = 0;
	int32 Loops = 0;
	int32 Gestures = 0;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		const FKGEmoteDef& E = All[i];
		const FString Id = E.Id.ToString();
		TestFalse(FString::Printf(TEXT("%s: unique word"), *Id), Words.Contains(Id.ToLower()));
		Words.Add(Id.ToLower());
		TArray<FString> Aliases;
		E.Aliases.ParseIntoArray(Aliases, TEXT(","));
		for (const FString& Alias : Aliases)
		{
			const FString A = Alias.TrimStartAndEnd().ToLower();
			TestFalse(FString::Printf(TEXT("%s: alias '%s' unique"), *Id, *A), Words.Contains(A));
			Words.Add(A);
			TestEqual(FString::Printf(TEXT("alias '%s' finds %s"), *A, *Id), FKGEmoteCatalog::IndexOf(FName(*A)), i);
		}
		TestNotEqual(FString::Printf(TEXT("%s: emoji '%s' exists"), *Id, *E.Emoji), FKGEmoji::Find(E.Emoji), int32(INDEX_NONE));
		TestFalse(FString::Printf(TEXT("%s: has a verb"), *Id), E.Verb.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s: has a clip"), *Id), E.Clip.IsNull());
		TestFalse(FString::Printf(TEXT("%s: has a name"), *Id), E.DisplayName.IsEmpty());
		if (E.bLoop && !E.IsFullBody())
		{
			TestTrue(FString::Printf(TEXT("%s: upper-body loops time out"), *Id), E.MaxSeconds > 0.0f);
		}
		if (!E.FirstPersonClip.IsNull())
		{
			TestFalse(FString::Printf(TEXT("%s: first-person gestures are upper-body only"), *Id), E.IsFullBody());
			TestFalse(FString::Printf(TEXT("%s: gesture keeps first person"), *Id), E.UsesThirdPersonCamera());
			++Gestures;
		}
		FullBody += E.IsFullBody() ? 1 : 0;
		Loops += E.bLoop ? 1 : 0;
	}
	TestTrue(TEXT("Some full-body emotes"), FullBody >= 4);
	TestTrue(TEXT("Loops (dances, sit)"), Loops >= 3);
	TestTrue(TEXT("First-person gestures for wave/point"), Gestures >= 2);
	for (const TCHAR* Required : {TEXT("wave"), TEXT("point"), TEXT("clap"), TEXT("cheer"), TEXT("shrug"), TEXT("laugh"),
	                              TEXT("facepalm"), TEXT("dance"), TEXT("sit"), TEXT("bow"), TEXT("salute"), TEXT("sus"),
	                              TEXT("cry"), TEXT("threaten"), TEXT("accuse")})
	{
		TestNotNull(FString::Printf(TEXT("emote %s"), Required), FKGEmoteCatalog::Find(FName(Required)));
	}
	TestNull(TEXT("Unknown emote"), FKGEmoteCatalog::Find(FName(TEXT("moonwalk"))));
	TestEqual(TEXT("Case-insensitive"), FKGEmoteCatalog::IndexOf(FName(TEXT("WAVE"))), FKGEmoteCatalog::IndexOf(FName(TEXT("wave"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteRulesTest, "KillGodot.Emote.Rules",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteRulesTest::RunTest(const FString& Parameters)
{
	using namespace KGEmoteTests;
	using R = EKGEmoteReject;
	const FKGEmoteDef& Wave = Def(TEXT("wave"));
	const FKGEmoteDef& Dance = Def(TEXT("dance"));
	const FKGEmoteBodyState Standing;

	// Phases: exactly the reaction phases (FKGChatRules::CanReact).
	for (const P Phase : {P::Lobby, P::Warmup, P::Dawn, P::Day, P::Meeting, P::Trial, P::Epilogue})
	{
		TestEqual(FString::Printf(TEXT("Emote allowed in phase %d"), int32(Phase)),
		          FKGEmoteRules::CanStart(Dance, Alive(), Standing, Phase, 30.0f), R::None);
		TestEqual(TEXT("...same as reactions"), FKGChatRules::CanReact(Alive(), Phase, 30.0f), true);
	}
	for (const P Phase : {P::RoleReveal, P::Night, P::Migrating})
	{
		TestEqual(FString::Printf(TEXT("No emotes in phase %d"), int32(Phase)),
		          FKGEmoteRules::CanStart(Wave, Alive(), Standing, Phase, 30.0f), R::Phase);
	}
	// Ghosts can react (Dead chat) but never emote; dead bodies neither; revenants cannot react at all.
	TestEqual(TEXT("Ghost refused"), FKGEmoteRules::CanStart(Wave, Ghost(), Standing, P::Day, 30.0f), R::Dead);
	TestEqual(TEXT("Ghost refused even at night"), FKGEmoteRules::CanStart(Wave, Ghost(), Standing, P::Night, 30.0f), R::Dead);
	FKGEmoteBodyState DeadBody;
	DeadBody.bDead = true;
	TestEqual(TEXT("Dead body refused"), FKGEmoteRules::CanStart(Wave, Alive(), DeadBody, P::Day, 30.0f), R::Dead);
	FKGChatParticipant Revenant;
	Revenant.Life = EKGLifeState::Revenant;
	TestEqual(TEXT("Revenant refused"), FKGEmoteRules::CanStart(Wave, Revenant, Standing, P::Day, 30.0f), R::Phase);
	FKGChatParticipant Silenced;
	Silenced.bSilenced = true;
	TestEqual(TEXT("Blackmailed may still emote"), FKGEmoteRules::CanStart(Wave, Silenced, Standing, P::Day, 30.0f), R::None);
	FKGEmoteBodyState NoBody;
	NoBody.bHasBody = false;
	TestEqual(TEXT("No body"), FKGEmoteRules::CanStart(Wave, Alive(), NoBody, P::Day, 30.0f), R::NoBody);

	// Body state.
	FKGEmoteBodyState Walking;
	Walking.Speed = 300.0f;
	TestEqual(TEXT("Wave while walking (upper body)"), FKGEmoteRules::CanStart(Wave, Alive(), Walking, P::Day, 30.0f), R::None);
	TestEqual(TEXT("No dance while walking"), FKGEmoteRules::CanStart(Dance, Alive(), Walking, P::Day, 30.0f), R::Moving);
	for (int32 Case = 0; Case < 4; ++Case)
	{
		FKGEmoteBodyState Busy;
		Busy.bSwimming = Case == 0;
		Busy.bClimbing = Case == 1;
		Busy.bSeated = Case == 2;
		Busy.bCarrying = Case == 3;
		TestEqual(FString::Printf(TEXT("Busy case %d refuses waves"), Case), FKGEmoteRules::CanStart(Wave, Alive(), Busy, P::Day, 30.0f), R::Busy);
	}
	FKGEmoteBodyState Air;
	Air.bFalling = true;
	TestEqual(TEXT("Wave mid-air"), FKGEmoteRules::CanStart(Wave, Alive(), Air, P::Day, 30.0f), R::None);
	TestEqual(TEXT("No dance mid-air"), FKGEmoteRules::CanStart(Dance, Alive(), Air, P::Day, 30.0f), R::Busy);
	FKGEmoteBodyState Crouched;
	Crouched.bCrouched = true;
	TestEqual(TEXT("No dance crouched"), FKGEmoteRules::CanStart(Dance, Alive(), Crouched, P::Day, 30.0f), R::Busy);

	// Cancel rules.
	EKGEmoteStop Why = EKGEmoteStop::Finished;
	TestFalse(TEXT("Standing dance keeps going"), FKGEmoteRules::ShouldStop(Dance, Alive(), Standing, P::Day, 30.0f, Why));
	TestTrue(TEXT("Moving stops a dance"), FKGEmoteRules::ShouldStop(Dance, Alive(), Walking, P::Day, 30.0f, Why));
	TestEqual(TEXT("...as Moved"), Why, EKGEmoteStop::Moved);
	TestFalse(TEXT("Moving keeps a wave"), FKGEmoteRules::ShouldStop(Wave, Alive(), Walking, P::Day, 30.0f, Why));
	TestTrue(TEXT("Night stops everything"), FKGEmoteRules::ShouldStop(Wave, Alive(), Standing, P::Night, 30.0f, Why));
	TestEqual(TEXT("...as Phase"), Why, EKGEmoteStop::Phase);
	TestTrue(TEXT("Death stops it"), FKGEmoteRules::ShouldStop(Wave, Alive(), DeadBody, P::Day, 30.0f, Why));
	TestEqual(TEXT("...as Died"), Why, EKGEmoteStop::Died);
	FKGEmoteBodyState Carrying;
	Carrying.bCarrying = true;
	TestTrue(TEXT("Picking something up stops it"), FKGEmoteRules::ShouldStop(Wave, Alive(), Carrying, P::Day, 30.0f, Why));

	// Meetings / trials / curfew starting clear every emote; other transitions do not.
	TestTrue(TEXT("Meeting starting"), FKGEmoteRules::PhaseChangeStops(P::Day, P::Meeting));
	TestTrue(TEXT("Trial starting"), FKGEmoteRules::PhaseChangeStops(P::Meeting, P::Trial));
	TestTrue(TEXT("Curfew"), FKGEmoteRules::PhaseChangeStops(P::Day, P::Night));
	TestFalse(TEXT("Dawn -> Day keeps it"), FKGEmoteRules::PhaseChangeStops(P::Dawn, P::Day));
	TestFalse(TEXT("No change"), FKGEmoteRules::PhaseChangeStops(P::Meeting, P::Meeting));

	// Chat mapping.
	TestEqual(TEXT("Phase -> closed"), FKGEmoteRules::ToChatReject(R::Phase), EKGChatReject::Closed);
	TestEqual(TEXT("Moving hint"), FKGEmoteRules::ToChatReject(R::Moving), EKGChatReject::EmoteMoving);
	TestEqual(TEXT("Busy hint"), FKGEmoteRules::ToChatReject(R::Busy), EKGChatReject::EmoteBlocked);
	TestFalse(TEXT("Hints have text"), FKGChatRules::RejectReason(EKGChatReject::EmoteMoving).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteRateLimitTest, "KillGodot.Emote.RateLimit",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteRateLimitTest::RunTest(const FString& Parameters)
{
	FKGChatRateLimiter L = FKGEmoteRules::MakeLimiter();
	double T = 100.0;
	TestEqual(TEXT("1st"), L.TryConsume(T, FString()), EKGChatReject::None);
	TestEqual(TEXT("2nd"), L.TryConsume(T, FString()), EKGChatReject::None);
	TestEqual(TEXT("3rd"), L.TryConsume(T, FString()), EKGChatReject::None);
	TestEqual(TEXT("4th in the same instant refused"), L.TryConsume(T, FString()), EKGChatReject::RateLimited);
	TestEqual(TEXT("Still refused 1 s later"), L.TryConsume(T + 1.0, FString()), EKGChatReject::RateLimited);
	TestEqual(TEXT("One more after 2 s"), L.TryConsume(T + 2.1, FString()), EKGChatReject::None);
	TestEqual(TEXT("...but only one"), L.TryConsume(T + 2.2, FString()), EKGChatReject::RateLimited);
	TestEqual(TEXT("Full burst again after a long pause"), L.TryConsume(T + 60.0, FString()), EKGChatReject::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteChatBridgeTest, "KillGodot.Emote.ChatBridge",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteChatBridgeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Chat lists every catalog emote"), FKGChatEmote::GetAll().Num(), FKGEmoteCatalog::GetAll().Num());
	const FKGChatEmote* Cheers = FKGChatEmote::Find(FName(TEXT("cheers")));
	if (TestNotNull(TEXT("/cheers still works (alias)"), Cheers))
	{
		TestEqual(TEXT("...as cheer"), FString(Cheers->Id), FString(TEXT("cheer")));
	}
	for (const TCHAR* Old : {TEXT("wave"), TEXT("clap"), TEXT("cry"), TEXT("laugh"), TEXT("pray"), TEXT("point"), TEXT("shrug"),
	                         TEXT("think"), TEXT("cheers"), TEXT("bow"), TEXT("sleep"), TEXT("dance"), TEXT("angry")})
	{
		const FKGChatEmote* E = FKGChatEmote::Find(FName(Old));
		if (TestNotNull(FString::Printf(TEXT("old chat emote /%s still resolves"), Old), E))
		{
			TestNotEqual(FString::Printf(TEXT("/%s has a bubble emoji"), Old), FKGEmoji::Find(E->Emoji), int32(INDEX_NONE));
		}
	}
	TestNull(TEXT("Unknown"), FKGChatEmote::Find(FName(TEXT("moonwalk"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteAssetsTest, "KillGodot.Emote.Assets",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteAssetsTest::RunTest(const FString& Parameters)
{
	const USkeletalMesh* Villager = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/KillGodot/Characters/Villager/SK_KG_Villager_M.SK_KG_Villager_M"));
	const USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/KillGodot/Characters/FPArms2/SK_KG_FPArms2.SK_KG_FPArms2"));
	if (!TestNotNull(TEXT("Villager mesh"), Villager) || !TestNotNull(TEXT("FPArms2 mesh"), Arms))
	{
		return false;
	}
	for (const FKGEmoteDef& E : FKGEmoteCatalog::GetAll())
	{
		for (const FSoftObjectPath& Path : {E.Clip, E.IntroClip})
		{
			if (Path.IsNull())
			{
				continue;
			}
			const UAnimSequence* Seq = Cast<UAnimSequence>(Path.TryLoad());
			if (TestNotNull(FString::Printf(TEXT("%s: clip %s"), *E.Id.ToString(), *Path.ToString()), Seq))
			{
				TestTrue(FString::Printf(TEXT("%s: on the villager skeleton"), *E.Id.ToString()), Seq->GetSkeleton() == Villager->GetSkeleton());
				TestTrue(FString::Printf(TEXT("%s: has length"), *E.Id.ToString()), Seq->GetPlayLength() > 0.2f);
			}
		}
		if (!E.FirstPersonClip.IsNull())
		{
			const UAnimSequence* Seq = Cast<UAnimSequence>(E.FirstPersonClip.TryLoad());
			if (TestNotNull(FString::Printf(TEXT("%s: gesture %s"), *E.Id.ToString(), *E.FirstPersonClip.ToString()), Seq))
			{
				TestTrue(FString::Printf(TEXT("%s: gesture on the FPArms2 skeleton"), *E.Id.ToString()), Seq->GetSkeleton() == Arms->GetSkeleton());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteServerTest, "KillGodot.Emote.Server",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteServerTest::RunTest(const FString& Parameters)
{
	using namespace KGEmoteTests;
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless server world"), Server.Create()))
	{
		return false;
	}
	Server.Run(TEXT("Bot.Add 3"));
	Server.Run(TEXT("Bot.AI 0"));
	Server.Run(TEXT("Match.Phase Day"));
	Server.Run(TEXT("Match.Freeze 1"));
	AKGCharacter* Body = Server.BotBody(0);
	AKGCharacter* Other = Server.BotBody(1);
	if (!TestNotNull(TEXT("Bot body"), Body) || !TestNotNull(TEXT("Second bot body"), Other))
	{
		return false;
	}
	UKGEmoteComponent* Emote = Body->GetEmote();
	if (!TestNotNull(TEXT("Emote component on the character"), Emote))
	{
		return false;
	}
	AKGPlayerState* PS = Body->GetPlayerState<AKGPlayerState>();
	TestNotNull(TEXT("Bot player state"), PS);
	TestNotNull(TEXT("Layered body anim instance"), Body->GetBodyAnim());

	// Start, replicated state, presentation on the server's own copy.
	TestEqual(TEXT("Dance starts"), Emote->ServerTryStart(TEXT("dance"), true), EKGEmoteReject::None);
	TestTrue(TEXT("Emoting"), Emote->IsEmoting());
	TestEqual(TEXT("Replicated id"), Emote->GetActiveEmoteId(), FName(TEXT("dance")));
	const uint8 Serial = Emote->GetSerial();
	TestTrue(TEXT("Body plays the emote layer"), Body->GetBodyAnim() && Body->GetBodyAnim()->IsEmoteWanted());
	TestFalse(TEXT("Full-body emote frees the body yaw"), Body->bUseControllerRotationYaw);
	Tick(Emote, 3.0f);
	TestTrue(TEXT("A looping dance keeps going while standing"), Emote->IsEmoting());

	// Moving cancels a full-body emote.
	Body->GetCharacterMovement()->Velocity = FVector(300.0, 0.0, 0.0);
	Tick(Emote, 0.1f);
	TestFalse(TEXT("Moving stopped the dance"), Emote->IsEmoting());
	TestEqual(TEXT("...reason Moved"), Emote->GetLastStopReason(), EKGEmoteStop::Moved);
	TestTrue(TEXT("Yaw follows the controller again"), Body->bUseControllerRotationYaw);
	TestFalse(TEXT("Emote layer blending out"), Body->GetBodyAnim() && Body->GetBodyAnim()->IsEmoteWanted());

	// Upper-body emotes survive walking; a full-body one is refused while moving.
	TestEqual(TEXT("Dance refused while walking"), Emote->ServerTryStart(TEXT("dance"), true), EKGEmoteReject::Moving);
	TestEqual(TEXT("Wave while walking"), Emote->ServerTryStart(TEXT("wave"), true), EKGEmoteReject::None);
	TestTrue(TEXT("New serial"), Emote->GetSerial() != Serial);
	Tick(Emote, 0.5f);
	TestTrue(TEXT("Still waving while walking"), Emote->IsEmoting());
	Tick(Emote, UKGEmoteComponent::ComputeDuration(Def(TEXT("wave"))) + 0.2f);
	TestFalse(TEXT("One-shot wave finished"), Emote->IsEmoting());
	TestEqual(TEXT("...reason Finished"), Emote->GetLastStopReason(), EKGEmoteStop::Finished);
	Body->GetCharacterMovement()->Velocity = FVector::ZeroVector;

	// Being hit cancels.
	TestEqual(TEXT("Sit"), Emote->ServerTryStart(TEXT("sit"), true), EKGEmoteReject::None);
	UGameplayStatics::ApplyDamage(Body, 1.0f, nullptr, Other, UDamageType::StaticClass());
	TestFalse(TEXT("Hit stopped it"), Emote->IsEmoting());
	TestEqual(TEXT("...reason Damaged"), Emote->GetLastStopReason(), EKGEmoteStop::Damaged);

	// A meeting starting cancels; emotes are allowed again inside the meeting.
	TestEqual(TEXT("Cheer"), Emote->ServerTryStart(TEXT("reel"), true), EKGEmoteReject::None);
	Server.Run(TEXT("Match.Phase Meeting"));
	Server.Run(TEXT("Match.Freeze 1"));
	FServerWorld::Park(Body);
	Tick(Emote, 0.1f);
	TestFalse(TEXT("Meeting cleared the emote"), Emote->IsEmoting());
	TestEqual(TEXT("...reason Phase"), Emote->GetLastStopReason(), EKGEmoteStop::Phase);
	TestEqual(TEXT("Accuse during the meeting"), Emote->ServerTryStart(TEXT("accuse"), true), EKGEmoteReject::None);

	// Curfew: refused.
	Server.Run(TEXT("Match.Phase Night"));
	Server.Run(TEXT("Match.Freeze 1"));
	FServerWorld::Park(Body);
	Tick(Emote, 0.1f);
	TestFalse(TEXT("Night cleared it"), Emote->IsEmoting());
	TestEqual(TEXT("Refused at night"), Emote->ServerTryStart(TEXT("wave"), true), EKGEmoteReject::Phase);
	Server.Run(TEXT("Match.Phase Day"));
	Server.Run(TEXT("Match.Freeze 1"));
	FServerWorld::Park(Body);

	// Attacking cancels (server path of the melee RPC).
	TestEqual(TEXT("Laugh"), Emote->ServerTryStart(TEXT("laugh"), true), EKGEmoteReject::None);
	Emote->ServerStop(EKGEmoteStop::Attacked);
	TestEqual(TEXT("Attack reason recorded"), Emote->GetLastStopReason(), EKGEmoteStop::Attacked);

	// Ghosts cannot emote.
	if (PS)
	{
		PS->LifeState = EKGLifeState::Ghost;
		TestEqual(TEXT("Ghost refused"), Emote->ServerTryStart(TEXT("wave"), true), EKGEmoteReject::Dead);
		PS->LifeState = EKGLifeState::Alive;
	}

	// Rate limit (a second body with a fresh limiter): 3 in a burst, then refused.
	UKGEmoteComponent* Spam = Other->GetEmote();
	int32 Started = 0;
	int32 Limited = 0;
	for (int32 i = 0; i < 6; ++i)
	{
		const EKGEmoteReject R = Spam->ServerTryStart(TEXT("clap"));
		Started += R == EKGEmoteReject::None ? 1 : 0;
		Limited += R == EKGEmoteReject::RateLimited ? 1 : 0;
	}
	TestEqual(TEXT("Burst of 3"), Started, 3);
	TestEqual(TEXT("Then rate limited"), Limited, 3);
	TestEqual(TEXT("Unknown emote"), Spam->ServerTryStart(TEXT("moonwalk"), true), EKGEmoteReject::Unknown);

	// Dev verb: every bot dances (the phase jumps above may have re-dropped the bodies: stand them up again).
	FServerWorld::Park(Other);
	TestTrue(TEXT("Emote.Bots dance ok"), Server.Run(TEXT("Emote.Bots dance")).bOk);
	TestEqual(TEXT("Bot dances"), Other->GetEmote()->GetActiveEmoteId(), FName(TEXT("dance")));
	TestTrue(TEXT("Emote.Bots stop ok"), Server.Run(TEXT("Emote.Bots stop")).bOk);
	TestFalse(TEXT("Bot stopped"), Other->GetEmote()->IsEmoting());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGEmoteBodyLayerTest, "KillGodot.Emote.BodyLayer",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGEmoteBodyLayerTest::RunTest(const FString& Parameters)
{
	using namespace KGEmoteTests;
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/KillGodot/Characters/Villager/Anims/A_KG_Idle_Loop.A_KG_Idle_Loop"));
	UAnimSequence* Dance = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/KillGodot/Characters/Villager/Anims/A_KG_Dance_Loop.A_KG_Dance_Loop"));
	if (!TestNotNull(TEXT("Idle clip"), Idle) || !TestNotNull(TEXT("Dance clip"), Dance))
	{
		return false;
	}
	FServerWorld Server;
	if (!TestTrue(TEXT("Headless world"), Server.Create()))
	{
		return false;
	}
	// A: idle base + dance on the upper-body layer. B: dance only. C: idle only. Same clock for all three.
	AKGCharacter* Bodies[3] = {Server.SpawnBody(FVector(0, 0, 200)), Server.SpawnBody(FVector(300, 0, 200)),
	                           Server.SpawnBody(FVector(600, 0, 200))};
	UKGBodyAnimInstance* Anim[3] = {};
	for (int32 i = 0; i < 3; ++i)
	{
		if (!TestNotNull(TEXT("Body"), Bodies[i]))
		{
			return false;
		}
		Bodies[i]->GetEmote()->SetComponentTickEnabled(false);
		Bodies[i]->SetActorTickEnabled(false);
		Anim[i] = Bodies[i]->GetBodyAnim();
		if (!TestNotNull(TEXT("UKGBodyAnimInstance on the villager mesh"), Anim[i]))
		{
			return false;
		}
		Bodies[i]->GetMesh()->bEnableUpdateRateOptimizations = false;
	}
	// Editor builds compile skeletal meshes asynchronously; a mesh still compiling skips TickAnimation.
	FAssetCompilingManager::Get().FinishAllCompilation();
	Anim[0]->PlayBase(Idle, true, 0.0f);
	Anim[0]->PlayEmote(nullptr, Dance, true, true, 0.01f);
	Anim[1]->PlayBase(Dance, true, 0.0f);
	Anim[2]->PlayBase(Idle, true, 0.0f);
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		for (AKGCharacter* Body : Bodies)
		{
			Body->GetMesh()->TickAnimation(0.137f, false);
			Body->GetMesh()->RefreshBoneTransforms();
		}
	}
	if (!TestTrue(TEXT("Emote fully blended in"), Anim[0]->GetEmoteWeight() >= 0.999f))
	{
		AddInfo(FString::Printf(TEXT("weight %.3f time %.3f emote %s base %s"), Anim[0]->GetEmoteWeight(), Anim[0]->GetEmoteTime(),
		                        *GetNameSafe(Anim[0]->GetEmoteMain()), *GetNameSafe(Anim[0]->GetBase())));
	}
	USkeletalMeshComponent* Mesh = Bodies[0]->GetMesh();
	auto Local = [](AKGCharacter* Body, FName Bone) -> FTransform
	{
		USkeletalMeshComponent* M = Body->GetMesh();
		const int32 Index = M->GetBoneIndex(Bone);
		const TArray<FTransform> Pose = M->GetBoneSpaceTransforms();
		return Pose.IsValidIndex(Index) ? Pose[Index] : FTransform::Identity;
	};
	for (const TCHAR* Upper : {TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("Head"), TEXT("spine_03")})
	{
		TestTrue(FString::Printf(TEXT("%s follows the emote (upper body)"), Upper),
		         Local(Bodies[0], Upper).Equals(Local(Bodies[1], Upper), 0.01));
	}
	for (const TCHAR* Lower : {TEXT("thigh_l"), TEXT("calf_r"), TEXT("pelvis")})
	{
		TestTrue(FString::Printf(TEXT("%s keeps the base clip (legs)"), Lower),
		         Local(Bodies[0], Lower).Equals(Local(Bodies[2], Lower), 0.01));
	}
	TestFalse(TEXT("The clips differ at all (sanity)"),
	          Local(Bodies[1], TEXT("upperarm_r")).Equals(Local(Bodies[2], TEXT("upperarm_r")), 0.01));
	TestTrue(TEXT("Mesh evaluated"), Mesh->GetBoneSpaceTransforms().Num() > 50);

	// Full body: everything follows the emote; stopping blends back to the base.
	Anim[0]->PlayEmote(nullptr, Dance, true, false, 0.01f);
	Anim[0]->PlayBase(Dance, true, 0.0f);   // same clock trick is not needed for a full-body check of the stop
	Anim[0]->StopEmote(0.2f);
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		Bodies[0]->GetMesh()->TickAnimation(0.137f, false);
	}
	TestFalse(TEXT("Emote layer gone after its blend-out"), Anim[0]->IsEmoteActive());
	return true;
}

#endif
