#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Chat/KGChatRules.h"
#include "Core/KGPlayerState.h"
#include "Emote/KGEmoteCatalog.h"
#include "Voice/KGVoiceCommands.h"
#include "Voice/KGVoiceRules.h"

namespace KGVoiceTests
{
	FKGChatParticipant At(const FVector& Loc, EKGLifeState Life = EKGLifeState::Alive)
	{
		FKGChatParticipant P;
		P.Life = Life;
		P.bHasLocation = Life != EKGLifeState::Ghost;
		P.Location = Loc;
		return P;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGVoiceAttenuationTest, "KillGodot.Voice.Attenuation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FKGVoiceAttenuationTest::RunTest(const FString& Parameters)
{
	using namespace KGVoiceTests;
	TestEqual(TEXT("touching = full"), FKGVoiceRules::Attenuation(0.0f), 1.0f);
	TestEqual(TEXT("8 m = full"), FKGVoiceRules::Attenuation(800.0f), 1.0f);
	TestEqual(TEXT("25 m = silent"), FKGVoiceRules::Attenuation(2500.0f), 0.0f);
	TestEqual(TEXT("40 m = silent"), FKGVoiceRules::Attenuation(4000.0f), 0.0f);
	TestTrue(TEXT("16.5 m = half"), FMath::IsNearlyEqual(FKGVoiceRules::Attenuation(1650.0f), 0.5f, 0.01f));
	TestTrue(TEXT("monotonic"), FKGVoiceRules::Attenuation(1000.0f) > FKGVoiceRules::Attenuation(2000.0f));

	const FKGChatParticipant A = At(FVector::ZeroVector);
	TestEqual(TEXT("3 m living: 1"), FKGVoiceRules::HearGain(A, At(FVector(300, 0, 0)), EKGPhase::Day, false), 1.0f);
	TestEqual(TEXT("40 m living: 0"), FKGVoiceRules::HearGain(A, At(FVector(4000, 0, 0)), EKGPhase::Day, false), 0.0f);
	TestEqual(TEXT("never yourself"), FKGVoiceRules::HearGain(A, A, EKGPhase::Day, true), 0.0f);
	TestEqual(TEXT("meeting: the square hears everyone"), FKGVoiceRules::HearGain(A, At(FVector(4000, 0, 0)), EKGPhase::Meeting, false), 1.0f);
	TestEqual(TEXT("trial too"), FKGVoiceRules::HearGain(A, At(FVector(4000, 0, 0)), EKGPhase::Trial, false), 1.0f);
	TestEqual(TEXT("quantise round trip"), FKGVoiceRules::DequantizeGain(FKGVoiceRules::QuantizeGain(1.0f)), 1.0f);
	TestTrue(TEXT("quantise half"), FMath::IsNearlyEqual(FKGVoiceRules::DequantizeGain(FKGVoiceRules::QuantizeGain(0.5f)), 0.5f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGVoiceGhostRulesTest, "KillGodot.Voice.GhostRules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FKGVoiceGhostRulesTest::RunTest(const FString& Parameters)
{
	using namespace KGVoiceTests;
	const FKGChatParticipant Living = At(FVector::ZeroVector);
	const FKGChatParticipant Ghost = At(FVector::ZeroVector, EKGLifeState::Ghost);
	const FKGChatParticipant Ghost2 = At(FVector(9000, 0, 0), EKGLifeState::Ghost);
	FKGChatParticipant Revenant = Living;
	Revenant.Life = EKGLifeState::Revenant;

	TestEqual(TEXT("ghost -> living: silent"), FKGVoiceRules::HearGain(Ghost, Living, EKGPhase::Day, false), 0.0f);
	TestEqual(TEXT("ghost -> living in a meeting: still silent"), FKGVoiceRules::HearGain(Ghost, Living, EKGPhase::Meeting, false), 0.0f);
	TestEqual(TEXT("ghost -> ghost anywhere: full"), FKGVoiceRules::HearGain(Ghost, Ghost2, EKGPhase::Night, false), 1.0f);
	TestEqual(TEXT("living -> ghost: spectators hear the living"), FKGVoiceRules::HearGain(Living, Ghost2, EKGPhase::Day, false), 1.0f);
	TestEqual(TEXT("revenant cannot speak"), FKGVoiceRules::HearGain(Revenant, Living, EKGPhase::Day, false), 0.0f);
	TestFalse(TEXT("nobody mid-migration"), FKGVoiceRules::CanSpeak(Living, EKGPhase::Migrating));
	TestFalse(TEXT("curfew at the role reveal"), FKGVoiceRules::CanSpeak(Living, EKGPhase::RoleReveal));
	TestTrue(TEXT("ghosts talk during the reveal"), FKGVoiceRules::CanSpeak(Ghost, EKGPhase::RoleReveal));
	TestTrue(TEXT("whispering at night is allowed"), FKGVoiceRules::CanSpeak(Living, EKGPhase::Night));
	TestEqual(TEXT("channel label ghost"), static_cast<int32>(FKGVoiceRules::ChannelOf(Ghost, EKGPhase::Day)), static_cast<int32>(EKGVoiceChannel::Ghost));
	TestEqual(TEXT("channel label square"), static_cast<int32>(FKGVoiceRules::ChannelOf(Living, EKGPhase::Meeting)), static_cast<int32>(EKGVoiceChannel::Square));
	TestEqual(TEXT("channel label nearby"), static_cast<int32>(FKGVoiceRules::ChannelOf(Living, EKGPhase::Day)), static_cast<int32>(EKGVoiceChannel::Nearby));

	const int16 Loud[4] = {32767, -32768, 32767, -32768};
	const int16 Quiet[4] = {0, 0, 0, 0};
	TestTrue(TEXT("full-scale square = 1"), FMath::IsNearlyEqual(FKGVoiceRules::Amplitude(Loud, 4), 1.0f, 0.001f));
	TestEqual(TEXT("silence = 0"), FKGVoiceRules::Amplitude(Quiet, 4), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGVoiceCommandCatalogTest, "KillGodot.Voice.CommandCatalog",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FKGVoiceCommandCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FKGVoiceCommandDef>& All = FKGVoiceCommandCatalog::GetAll();
	const TArray<FKGVoiceMenuDef>& Menus = FKGVoiceCommandCatalog::GetMenus();
	TestEqual(TEXT("three menus"), Menus.Num(), FKGVoiceCommandCatalog::MenuCount);
	TestEqual(TEXT("24 lines"), All.Num(), FKGVoiceCommandCatalog::MenuCount * FKGVoiceCommandCatalog::SlotsPerMenu);
	TSet<FName> Ids;
	for (const FKGVoiceCommandDef& Def : All)
	{
		TestFalse(*FString::Printf(TEXT("duplicate id %s"), *Def.Id.ToString()), Ids.Contains(Def.Id));
		Ids.Add(Def.Id);
		TestTrue(*FString::Printf(TEXT("%s has a line"), *Def.Id.ToString()), !Def.Line.IsEmpty());
		TestTrue(*FString::Printf(TEXT("%s syllables 1..8"), *Def.Id.ToString()), Def.Syllables >= 1 && Def.Syllables <= 8);
		if (!Def.Gesture.IsNone())
		{
			TestNotNull(*FString::Printf(TEXT("%s gesture %s is an emote"), *Def.Id.ToString(), *Def.Gesture.ToString()), FKGEmoteCatalog::Find(Def.Gesture));
		}
		TestTrue(TEXT("wave path"), FKGVoiceCommandCatalog::WavePath(EKGBarkVoice::Female, Def.Id).StartsWith(TEXT("/Game/KillGodot/Audio/Barks/S_Bark_Female_")));
	}
	for (int32 m = 0; m < Menus.Num(); ++m)
	{
		TestEqual(TEXT("8 slots"), Menus[m].Commands.Num(), FKGVoiceCommandCatalog::SlotsPerMenu);
		TestEqual(TEXT("key -> menu"), FKGVoiceCommandCatalog::MenuForKey(Menus[m].Key), m);
	}
	TestEqual(TEXT("Z X C"), Menus[0].Key, EKeys::Z);
	TestEqual(TEXT("Z X C"), Menus[1].Key, EKeys::X);
	TestEqual(TEXT("Z X C"), Menus[2].Key, EKeys::C);
	TestEqual(TEXT("G is not a radial"), FKGVoiceCommandCatalog::MenuForKey(EKeys::G), INDEX_NONE);

	const FKGVoiceCommandDef* Place = FKGVoiceCommandCatalog::Find(TEXT("iwasat"));
	if (TestNotNull(TEXT("iwasat exists"), Place))
	{
		TestTrue(TEXT("uses the place"), Place->UsesPlace());
		TestEqual(TEXT("place filled"), FKGVoiceCommandCatalog::FormatLine(*Place, FText::FromString(TEXT("Harbour"))), FString(TEXT("I was at the Harbour")));
		TestEqual(TEXT("unknown place"), FKGVoiceCommandCatalog::FormatLine(*Place, FText::GetEmpty()), FString(TEXT("I was at the square")));
	}
	const FKGVoiceCommandDef* Here = FKGVoiceCommandCatalog::Find(TEXT("overhere"));
	if (TestNotNull(TEXT("overhere exists"), Here))
	{
		TestFalse(TEXT("no place"), Here->UsesPlace());
		TestEqual(TEXT("plain line"), FKGVoiceCommandCatalog::FormatLine(*Here, FText::GetEmpty()), FString(TEXT("Over here!")));
	}
	// Mouth envelope: closed before and after, opens inside, deterministic, three syllables = three humps.
	const float D = FKGVoiceCommandCatalog::BarkDuration(3);
	TestEqual(TEXT("closed before"), FKGVoiceCommandCatalog::MouthEnvelope(-0.1f, 3, 7), 0.0f);
	TestEqual(TEXT("closed after"), FKGVoiceCommandCatalog::MouthEnvelope(D + 0.01f, 3, 7), 0.0f);
	int32 Humps = 0;
	bool bWasOpen = false;
	float Peak = 0.0f;
	for (float T = 0.0f; T < D; T += 0.005f)
	{
		const float E = FKGVoiceCommandCatalog::MouthEnvelope(T, 3, 7);
		TestTrue(TEXT("0..1"), E >= 0.0f && E <= 1.0f);
		TestEqual(TEXT("deterministic"), E, FKGVoiceCommandCatalog::MouthEnvelope(T, 3, 7));
		Peak = FMath::Max(Peak, E);
		const bool bOpen = E > 0.05f;
		Humps += bOpen && !bWasOpen ? 1 : 0;
		bWasOpen = bOpen;
	}
	TestEqual(TEXT("three humps"), Humps, 3);
	TestTrue(TEXT("opens wide enough to read"), Peak > 0.5f);
	TestTrue(TEXT("voice by look"), FKGVoiceCommandCatalog::VoiceFor(true, false, 3) == EKGBarkVoice::Female);
	TestTrue(TEXT("old voice"), FKGVoiceCommandCatalog::VoiceFor(false, true, 3) == EKGBarkVoice::Old);
	TestTrue(TEXT("two male voices"), FKGVoiceCommandCatalog::VoiceFor(false, false, 2) != FKGVoiceCommandCatalog::VoiceFor(false, false, 3));
	FKGChatRateLimiter L = FKGVoiceCommandCatalog::MakeLimiter();
	int32 Passed = 0;
	for (int32 i = 0; i < 10; ++i)
	{
		Passed += L.TryConsume(100.0, FString()) == EKGChatReject::None ? 1 : 0;
	}
	TestEqual(TEXT("bark burst of 3"), Passed, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGPartnerRulesTest, "KillGodot.Emote.PartnerRules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FKGPartnerRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("four partner emotes"), FKGPartnerCatalog::GetAll().Num(), 4);
	for (const FKGPartnerEmoteDef& Def : FKGPartnerCatalog::GetAll())
	{
		TestNotNull(*FString::Printf(TEXT("%s clip A"), *Def.Id.ToString()), FKGEmoteCatalog::Find(Def.EmoteA));
		TestNotNull(*FString::Printf(TEXT("%s clip B"), *Def.Id.ToString()), FKGEmoteCatalog::Find(Def.EmoteB));
		TestTrue(TEXT("found by id"), FKGPartnerCatalog::Find(Def.Id) == &Def);
	}
	TestTrue(TEXT("rps alias"), FKGPartnerCatalog::Find(TEXT("RockPaperScissors")) == FKGPartnerCatalog::Get(EKGPartnerKind::RockPaperScissors));
	TestTrue(TEXT("resolved kinds"), FKGPartnerCatalog::Get(EKGPartnerKind::RockPaperScissors)->IsResolved() && FKGPartnerCatalog::Get(EKGPartnerKind::DanceOff)->IsResolved());
	TestFalse(TEXT("high five has no result"), FKGPartnerCatalog::Get(EKGPartnerKind::HighFive)->IsResolved());

	// rock 0, paper 1, scissors 2
	TestEqual(TEXT("tie"), FKGPartnerRules::RpsWinner(1, 1), 0);
	TestEqual(TEXT("paper beats rock"), FKGPartnerRules::RpsWinner(1, 0), 1);
	TestEqual(TEXT("rock loses to paper"), FKGPartnerRules::RpsWinner(0, 1), 2);
	TestEqual(TEXT("scissors beat paper"), FKGPartnerRules::RpsWinner(2, 1), 1);
	TestEqual(TEXT("rock beats scissors"), FKGPartnerRules::RpsWinner(0, 2), 1);
	TestEqual(TEXT("scissors lose to rock"), FKGPartnerRules::RpsWinner(2, 0), 2);
	uint8 A, B, W;
	FKGPartnerRules::UnpackRps(FKGPartnerRules::PackRps(2, 0), A, B, W);
	TestEqual(TEXT("pack A"), static_cast<int32>(A), 2);
	TestEqual(TEXT("pack B"), static_cast<int32>(B), 0);
	TestEqual(TEXT("pack winner"), static_cast<int32>(W), 2);
	TestEqual(TEXT("deterministic roll"), static_cast<int32>(FKGPartnerRules::RollRps(1234, 5)), static_cast<int32>(FKGPartnerRules::RollRps(1234, 5)));
	bool bVaries = false;
	for (uint8 S = 0; S < 40 && !bVaries; ++S)
	{
		bVaries = FKGPartnerRules::RollRps(1234, S) != FKGPartnerRules::RollRps(1234, 0);
	}
	TestTrue(TEXT("different serials, different throws"), bVaries);
	const uint8 Dance = FKGPartnerRules::RollDanceOff(99, 1);
	TestTrue(TEXT("dance-off picks a winner"), Dance == 1 || Dance == 2);

	TestTrue(TEXT("2 m accepts"), FKGPartnerRules::WithinAcceptRadius(FVector::ZeroVector, FVector(200, 0, 0)));
	TestFalse(TEXT("5 m too far"), FKGPartnerRules::WithinAcceptRadius(FVector::ZeroVector, FVector(500, 0, 0)));
	TestFalse(TEXT("other floor"), FKGPartnerRules::WithinAcceptRadius(FVector::ZeroVector, FVector(100, 0, 400)));
	FVector Loc;
	float YawA, YawB;
	FKGPartnerRules::Align(FVector::ZeroVector, FVector(250, 40, 10), 90.0f, Loc, YawA, YawB);
	TestTrue(TEXT("acceptor 90 cm away"), FMath::IsNearlyEqual(static_cast<float>(FVector::Dist2D(FVector::ZeroVector, Loc)), 90.0f, 0.5f));
	TestTrue(TEXT("keeps its height"), FMath::IsNearlyEqual(static_cast<float>(Loc.Z), 10.0f, 0.01f));
	TestTrue(TEXT("facing each other"), FMath::IsNearlyEqual(FMath::Abs(FMath::FindDeltaAngleDegrees(YawA, YawB)), 180.0f, 0.5f));
	TestTrue(TEXT("attack ends it"), FKGPartnerRules::StopEndsPartner(EKGEmoteStop::Attacked));
	TestTrue(TEXT("damage ends it"), FKGPartnerRules::StopEndsPartner(EKGEmoteStop::Damaged));
	TestTrue(TEXT("walking ends it"), FKGPartnerRules::StopEndsPartner(EKGEmoteStop::Moved));
	TestFalse(TEXT("a finished clip does not"), FKGPartnerRules::StopEndsPartner(EKGEmoteStop::Finished));
	TestFalse(TEXT("the partner clip replacing ours does not"), FKGPartnerRules::StopEndsPartner(EKGEmoteStop::Replaced));
	return true;
}

#endif
