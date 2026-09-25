#include "Misc/AutomationTest.h"
#include "Chat/KGChatRules.h"
#include "Chat/KGEmoji.h"
#include "Styling/SlateBrush.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace KGChatTestsPrivate
{
	FKGChatParticipant Alive(EKGFaction Faction = EKGFaction::Town, bool bHasRole = true, FVector Where = FVector::ZeroVector)
	{
		FKGChatParticipant P;
		P.Life = EKGLifeState::Alive;
		P.Faction = Faction;
		P.bHasRole = bHasRole;
		P.Location = Where;
		P.bHasLocation = true;
		return P;
	}

	FKGChatParticipant Ghost(EKGFaction Faction = EKGFaction::Town)
	{
		FKGChatParticipant P;
		P.Life = EKGLifeState::Ghost;
		P.Faction = Faction;
		P.bHasRole = true;
		return P;
	}

	FString E(const TCHAR* Id)
	{
		return FString::Chr(FKGEmoji::ToChar(FKGEmoji::Find(Id)));
	}

	bool Ok(EKGChatReject R)
	{
		return R == EKGChatReject::None;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatEmojiTest, "KillGodot.Chat.Emoji",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatEmojiTest::RunTest(const FString& Parameters)
{
	using namespace KGChatTestsPrivate;
	TestEqual(TEXT("32 emojis (atlas is 8x4)"), FKGEmoji::Num(), 32);
	for (int32 i = 0; i < FKGEmoji::Num(); ++i)
	{
		TestEqual(TEXT("Char round-trip"), FKGEmoji::FromChar(FKGEmoji::ToChar(i)), i);
		TestEqual(TEXT("Id lookup"), FKGEmoji::Find(FKGEmoji::GetId(i)), i);
	}
	TestEqual(TEXT("Alias rip -> skull"), FKGEmoji::Find(TEXT("RIP")), FKGEmoji::Find(TEXT("skull")));
	TestEqual(TEXT("Alias +1"), FKGEmoji::Find(TEXT("+1")), FKGEmoji::Find(TEXT("thumbsup")));
	TestEqual(TEXT("Unknown"), FKGEmoji::Find(TEXT("pineapple")), INDEX_NONE);
	TestFalse(TEXT("Plain letters are not emojis"), FKGEmoji::IsEmojiChar(TEXT('a')));
	TestFalse(TEXT("Private use outside the set"), FKGEmoji::IsEmojiChar(static_cast<TCHAR>(FKGEmoji::FirstChar + 200)));

	TestEqual(TEXT("Shortcode"), FKGEmoji::ConvertShortcodes(TEXT("rip :skull: bro")), TEXT("rip ") + E(TEXT("skull")) + TEXT(" bro"));
	TestEqual(TEXT("Unknown shortcode stays"), FKGEmoji::ConvertShortcodes(TEXT(":nope: 10:30")), FString(TEXT(":nope: 10:30")));
	TestEqual(TEXT("Adjacent shortcodes"), FKGEmoji::ConvertShortcodes(TEXT(":eyes::knife:")), E(TEXT("eyes")) + E(TEXT("knife")));
	TestEqual(TEXT("Emoticons"), FKGEmoji::ConvertEmoticons(TEXT("hi :) <3 :( ;) :D")),
	          TEXT("hi ") + E(TEXT("smile")) + TEXT(" ") + E(TEXT("heart")) + TEXT(" ") + E(TEXT("cry")) + TEXT(" ") +
	          E(TEXT("wink")) + TEXT(" ") + E(TEXT("laugh")));
	TestEqual(TEXT("Emoticons need word boundaries"), FKGEmoji::ConvertEmoticons(TEXT("http://x.y a:)b")),
	          FString(TEXT("http://x.y a:)b")));
	TestEqual(TEXT("Angry wins over sad"), FKGEmoji::ConvertEmoticons(TEXT(">:(")), E(TEXT("angry")));
	// U+1F480 SKULL as a surrogate pair, U+2764 + VS16 heart
	const FString Unicode = FString(TEXT("a ")) + TEXT("\xD83D\xDC80") + TEXT(" ") + TEXT("\x2764\xFE0F");
	TestEqual(TEXT("Unicode emoji mapped"), FKGEmoji::ConvertUnicode(Unicode), TEXT("a ") + E(TEXT("skull")) + TEXT(" ") + E(TEXT("heart")));
	TestEqual(TEXT("Plain text for logs"), FKGEmoji::ToPlainText(E(TEXT("fish")) + TEXT("!")), FString(TEXT(":fish:!")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatEmojiAtlasTest, "KillGodot.Chat.EmojiAtlas",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatEmojiAtlasTest::RunTest(const FString& Parameters)
{
	// The imported atlases (Tools/Unreal/kg_import_emoji.py) load through the runtime path and give UV-mapped brushes.
	for (int32 i = 0; i < FKGEmoji::Num(); ++i)
	{
		const FSlateBrush* Small = FKGEmoji::GetInlineBrush(i, 20);
		const FSlateBrush* Large = FKGEmoji::GetLargeBrush(i, 36);
		if (!TestNotNull(TEXT("Inline brush"), Small) || !TestNotNull(TEXT("Large brush"), Large))
		{
			return false;
		}
		const FBox2f SmallUV = Small->GetUVRegion();
		const FBox2f LargeUV = Large->GetUVRegion();
		TestTrue(TEXT("UV region set"), SmallUV.bIsValid && LargeUV.bIsValid);
		TestEqual(TEXT("Inline size"), static_cast<float>(Small->ImageSize.X), 20.0f);
		TestTrue(TEXT("Same brush for the same key (runs keep raw pointers)"), FKGEmoji::GetInlineBrush(i, 20) == Small);
	}
	const FBox2f LastUV = FKGEmoji::GetLargeBrush(FKGEmoji::Num() - 1, 36)->GetUVRegion();
	TestEqual(TEXT("Last cell ends at the atlas corner (U)"), LastUV.Max.X, 1.0f);
	TestEqual(TEXT("Last cell ends at the atlas corner (V)"), LastUV.Max.Y, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatSanitizeTest, "KillGodot.Chat.Sanitize",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatSanitizeTest::RunTest(const FString& Parameters)
{
	using namespace KGChatTestsPrivate;
	TestEqual(TEXT("Trim + collapse"), FKGChatRules::Sanitize(TEXT("   hello \t\t  world \n ")), FString(TEXT("hello world")));
	TestEqual(TEXT("Only whitespace -> empty"), FKGChatRules::Sanitize(TEXT(" \t\r\n ")), FString());
	TestEqual(TEXT("Control chars stripped"), FKGChatRules::Sanitize(FString(TEXT("a\x0001\x0007")) + TEXT("b\x007F") + TEXT("c")), FString(TEXT("abc")));
	TestEqual(TEXT("Bidi override / zero width stripped"),
	          FKGChatRules::Sanitize(FString(TEXT("ab\x202E")) + TEXT("cd\x200B") + TEXT("ef\xFEFF")), FString(TEXT("abcdef")));
	TestEqual(TEXT("Foreign private-use stripped"), FKGChatRules::Sanitize(TEXT("x\xE000y")), FString(TEXT("xy")));
	TestEqual(TEXT("Lone surrogate stripped"), FKGChatRules::Sanitize(TEXT("x\xD83Dy")), FString(TEXT("xy")));
	TestEqual(TEXT("Turkish and Cyrillic kept"), FKGChatRules::Sanitize(TEXT("G\x00FCnayd\x0131n, \x00E7ok \x015F\x00FCpheli! \x041F\x0440\x0438\x0432\x0435\x0442")),
	          FString(TEXT("G\x00FCnayd\x0131n, \x00E7ok \x015F\x00FCpheli! \x041F\x0440\x0438\x0432\x0435\x0442")));
	TestEqual(TEXT("Zalgo capped to one mark"), FKGChatRules::Sanitize(TEXT("e\x0301\x0301\x0301")), FString(TEXT("e\x0301")));
	TestEqual(TEXT("Repeats capped"), FKGChatRules::Sanitize(TEXT("nooooooooooo!!!!!!!!!!")), FString(TEXT("noooooo!!!!!!")));

	const FString Long = FString::ChrN(400, TEXT('a'));
	TestEqual(TEXT("Length cap (repeats first)"), FKGChatRules::Sanitize(Long).Len(), FKGChatRules::MaxRepeat);
	FString Words;
	for (int32 i = 0; i < 60; ++i)
	{
		Words += TEXT("abc ");
	}
	TestEqual(TEXT("Length cap 140"), FKGChatRules::Sanitize(Words).Len(), FKGChatRules::MaxChars - 1);   // trailing space trimmed

	TestEqual(TEXT("Shortcodes + emoticons converted"), FKGChatRules::Sanitize(TEXT("sus :sus: :)")),
	          TEXT("sus ") + E(TEXT("sus")) + TEXT(" ") + E(TEXT("smile")));
	FString ManyEmoji;
	for (int32 i = 0; i < 20; ++i)
	{
		ManyEmoji += FString::Printf(TEXT(":%s:"), FKGEmoji::GetId(i));
	}
	TestEqual(TEXT("Emoji cap"), FKGEmoji::CountEmojis(FKGChatRules::Sanitize(ManyEmoji)), FKGChatRules::MaxEmojis);
	TestEqual(TEXT("An emoji counts as one character"), FKGChatRules::Sanitize(TEXT(":skull:")).Len(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatChannelRulesTest, "KillGodot.Chat.ChannelRules",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatChannelRulesTest::RunTest(const FString& Parameters)
{
	using namespace KGChatTestsPrivate;
	using C = EKGChatChannel;
	using P = EKGPhase;
	const FKGChatParticipant Town = Alive();
	const FKGChatParticipant Killer = Alive(EKGFaction::Clockbreakers);
	const FKGChatParticipant Solo = Alive(EKGFaction::SoloKiller);
	const FKGChatParticipant Dead = Ghost();
	FKGChatParticipant Revenant = Alive();
	Revenant.Life = EKGLifeState::Revenant;
	FKGChatParticipant Silenced = Alive();
	Silenced.bSilenced = true;
	FKGChatParticipant Accused = Alive();
	Accused.bOnTrial = true;

	// Town square.
	for (const P Phase : {P::Lobby, P::Warmup, P::Day, P::Meeting, P::Epilogue})
	{
		TestTrue(FString::Printf(TEXT("All open in phase %d"), static_cast<int32>(Phase)), Ok(FKGChatRules::CanSend(Town, C::All, Phase, 60.0f)));
	}
	for (const P Phase : {P::RoleReveal, P::Dawn, P::Night, P::Migrating})
	{
		TestFalse(FString::Printf(TEXT("All closed in phase %d"), static_cast<int32>(Phase)), Ok(FKGChatRules::CanSend(Town, C::All, Phase, 60.0f)));
	}
	TestTrue(TEXT("Night: town chat closed"), FKGChatRules::CanSend(Town, C::All, P::Night, 60.0f) == EKGChatReject::Closed);

	// Trial: defence 43..23 s left, judgement 23..8 s, last words 8..0 s.
	TestTrue(TEXT("Defence stage"), FKGChatRules::GetTrialStage(40.0f) == EKGTrialStage::Defense);
	TestTrue(TEXT("Judgement stage"), FKGChatRules::GetTrialStage(15.0f) == EKGTrialStage::Judgement);
	TestTrue(TEXT("Last words stage"), FKGChatRules::GetTrialStage(5.0f) == EKGTrialStage::LastWords);
	TestTrue(TEXT("Crowd quiet during defence"), FKGChatRules::CanSend(Town, C::All, P::Trial, 40.0f) == EKGChatReject::AccusedSpeaking);
	TestTrue(TEXT("Accused speaks during defence"), Ok(FKGChatRules::CanSend(Accused, C::All, P::Trial, 40.0f)));
	TestTrue(TEXT("Crowd speaks during judgement"), Ok(FKGChatRules::CanSend(Town, C::All, P::Trial, 15.0f)));
	TestTrue(TEXT("Crowd quiet at last words"), FKGChatRules::CanSend(Town, C::All, P::Trial, 5.0f) == EKGChatReject::AccusedSpeaking);
	TestTrue(TEXT("Accused has last words"), Ok(FKGChatRules::CanSend(Accused, C::All, P::Trial, 5.0f)));

	// Nearby.
	TestTrue(TEXT("Nearby at dawn"), Ok(FKGChatRules::CanSend(Town, C::Nearby, P::Dawn, 5.0f)));
	TestTrue(TEXT("Nearby by day"), Ok(FKGChatRules::CanSend(Town, C::Nearby, P::Day, 5.0f)));
	TestFalse(TEXT("No nearby in the meeting"), Ok(FKGChatRules::CanSend(Town, C::Nearby, P::Meeting, 5.0f)));
	TestFalse(TEXT("No nearby at night"), Ok(FKGChatRules::CanSend(Town, C::Nearby, P::Night, 5.0f)));

	// Team (the Impatient).
	TestTrue(TEXT("Killers whisper at night"), Ok(FKGChatRules::CanSend(Killer, C::Team, P::Night, 5.0f)));
	TestTrue(TEXT("Killers meet at role reveal"), Ok(FKGChatRules::CanSend(Killer, C::Team, P::RoleReveal, 5.0f)));
	TestTrue(TEXT("No team chat by day"), FKGChatRules::CanSend(Killer, C::Team, P::Day, 5.0f) == EKGChatReject::Closed);
	TestTrue(TEXT("Town has no team"), FKGChatRules::CanSend(Town, C::Team, P::Night, 5.0f) == EKGChatReject::NoTeam);
	TestTrue(TEXT("Solo killer has no team"), FKGChatRules::CanSend(Solo, C::Team, P::Night, 5.0f) == EKGChatReject::NoTeam);
	TestTrue(TEXT("No role yet (warm-up)"), FKGChatRules::CanSend(Alive(EKGFaction::Clockbreakers, false), C::Team, P::Night, 5.0f) == EKGChatReject::NoTeam);

	// Dead.
	for (const P Phase : {P::Day, P::Meeting, P::Trial, P::Night, P::Dawn})
	{
		TestTrue(TEXT("Ghosts always talk among themselves"), Ok(FKGChatRules::CanSend(Dead, C::Dead, Phase, 30.0f)));
		TestTrue(TEXT("Ghosts cannot use town chat"), FKGChatRules::CanSend(Dead, C::All, Phase, 30.0f) == EKGChatReject::WrongLifeState);
		TestTrue(TEXT("Ghosts cannot use nearby"), FKGChatRules::CanSend(Dead, C::Nearby, Phase, 30.0f) == EKGChatReject::WrongLifeState);
	}
	TestTrue(TEXT("Dead killer loses the team channel"), FKGChatRules::CanSend(Ghost(EKGFaction::Clockbreakers), C::Team, P::Night, 5.0f) == EKGChatReject::WrongLifeState);
	TestTrue(TEXT("The living cannot write to the dead"), FKGChatRules::CanSend(Town, C::Dead, P::Day, 5.0f) == EKGChatReject::WrongLifeState);
	TestTrue(TEXT("Epilogue: ghosts rejoin town chat"), Ok(FKGChatRules::CanSend(Dead, C::All, P::Epilogue, 5.0f)));

	// Special states.
	TestTrue(TEXT("Blackmailed"), FKGChatRules::CanSend(Silenced, C::All, P::Day, 5.0f) == EKGChatReject::Silenced);
	TestTrue(TEXT("Blackmailed may still react"), FKGChatRules::CanReact(Silenced, P::Day, 5.0f));
	TestTrue(TEXT("Revenant mute"), FKGChatRules::CanSend(Revenant, C::All, P::Day, 5.0f) == EKGChatReject::Revenant);
	TestFalse(TEXT("Revenant no reactions"), FKGChatRules::CanReact(Revenant, P::Day, 5.0f));
	TestTrue(TEXT("System is never sendable"), FKGChatRules::CanSend(Town, C::System, P::Day, 5.0f) == EKGChatReject::Closed);

	// Reactions.
	TestFalse(TEXT("No reactions at night"), FKGChatRules::CanReact(Town, P::Night, 5.0f));
	TestTrue(TEXT("Reactions during a defence"), FKGChatRules::CanReact(Town, P::Trial, 40.0f));
	TestTrue(TEXT("Ghost reactions (dead chat)"), FKGChatRules::CanReact(Dead, P::Night, 5.0f));

	// Tab order.
	const TArray<C> DayChannels = FKGChatRules::GetSendableChannels(Killer, P::Day, 60.0f);
	TestEqual(TEXT("Day: All + Nearby"), DayChannels.Num(), 2);
	const TArray<C> NightChannels = FKGChatRules::GetSendableChannels(Killer, P::Night, 60.0f);
	TestTrue(TEXT("Night killer: only Team"), NightChannels.Num() == 1 && NightChannels[0] == C::Team);
	TestEqual(TEXT("Night town: nothing"), FKGChatRules::GetSendableChannels(Town, P::Night, 60.0f).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatReceiversTest, "KillGodot.Chat.Receivers",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatReceiversTest::RunTest(const FString& Parameters)
{
	using namespace KGChatTestsPrivate;
	using C = EKGChatChannel;
	const FKGChatParticipant Town = Alive(EKGFaction::Town, true, FVector(0, 0, 0));
	const FKGChatParticipant Near = Alive(EKGFaction::Town, true, FVector(1500, 0, 0));
	const FKGChatParticipant Far = Alive(EKGFaction::Town, true, FVector(5000, 0, 0));
	const FKGChatParticipant Killer = Alive(EKGFaction::Clockbreakers, true, FVector(9000, 0, 0));
	const FKGChatParticipant Killer2 = Alive(EKGFaction::Clockbreakers, true, FVector(-9000, 0, 0));
	const FKGChatParticipant Vampire = Alive(EKGFaction::Vampires);
	const FKGChatParticipant Dead = Ghost();
	const FKGChatParticipant DeadKiller = Ghost(EKGFaction::Clockbreakers);
	FKGChatParticipant Revenant = Alive();
	Revenant.Life = EKGLifeState::Revenant;

	// All: everyone, the dead included.
	for (const FKGChatParticipant* R : TArray<const FKGChatParticipant*>{&Near, &Far, &Killer, &Dead, &Revenant})
	{
		TestTrue(TEXT("Town square reaches everyone"), FKGChatRules::CanReceive(Town, *R, C::All, false));
	}
	// Nearby: 20 m, ghosts overhear.
	TestTrue(TEXT("Nearby within 20 m"), FKGChatRules::CanReceive(Town, Near, C::Nearby, false));
	TestFalse(TEXT("Nearby not at 50 m"), FKGChatRules::CanReceive(Town, Far, C::Nearby, false));
	TestTrue(TEXT("Ghosts overhear nearby"), FKGChatRules::CanReceive(Town, Dead, C::Nearby, false));
	// Team: same faction, alive.
	TestTrue(TEXT("Killer reaches killer anywhere"), FKGChatRules::CanReceive(Killer, Killer2, C::Team, false));
	TestFalse(TEXT("Town never reads team chat"), FKGChatRules::CanReceive(Killer, Town, C::Team, false));
	TestFalse(TEXT("Other killing faction does not"), FKGChatRules::CanReceive(Killer, Vampire, C::Team, false));
	TestFalse(TEXT("Ghosts cannot hear the radio"), FKGChatRules::CanReceive(Killer, Dead, C::Team, false));
	TestFalse(TEXT("Not even a dead teammate"), FKGChatRules::CanReceive(Killer, DeadKiller, C::Team, false));
	// Dead: ghosts only.
	TestTrue(TEXT("Ghost to ghost"), FKGChatRules::CanReceive(Dead, DeadKiller, C::Dead, false));
	for (const FKGChatParticipant* R : TArray<const FKGChatParticipant*>{&Town, &Near, &Killer, &Revenant})
	{
		TestFalse(TEXT("The living never read dead chat"), FKGChatRules::CanReceive(Dead, *R, C::Dead, false));
	}
	TestTrue(TEXT("Own echo always"), FKGChatRules::CanReceive(Dead, Dead, C::Dead, true));
	// Reactions.
	TestTrue(TEXT("Bubble seen nearby"), FKGChatRules::CanSeeReaction(Town, Near, false));
	TestFalse(TEXT("Bubble not seen across the map"), FKGChatRules::CanSeeReaction(Town, Killer, false));
	TestTrue(TEXT("Ghosts see living bubbles"), FKGChatRules::CanSeeReaction(Town, Dead, false));
	TestFalse(TEXT("The living never see ghost reactions"), FKGChatRules::CanSeeReaction(Dead, Near, false));
	TestTrue(TEXT("Ghosts see ghost reactions"), FKGChatRules::CanSeeReaction(Dead, DeadKiller, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatRateLimitTest, "KillGodot.Chat.RateLimit",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatRateLimitTest::RunTest(const FString& Parameters)
{
	FKGChatRateLimiter L;
	TestTrue(TEXT("1"), L.TryConsume(100.0, TEXT("a")) == EKGChatReject::None);
	TestTrue(TEXT("Duplicate"), L.TryConsume(100.1, TEXT("A")) == EKGChatReject::Duplicate);
	TestTrue(TEXT("2"), L.TryConsume(100.2, TEXT("b")) == EKGChatReject::None);
	TestTrue(TEXT("3"), L.TryConsume(100.3, TEXT("c")) == EKGChatReject::None);
	TestTrue(TEXT("4 (burst)"), L.TryConsume(100.4, TEXT("d")) == EKGChatReject::None);
	TestTrue(TEXT("5th too fast"), L.TryConsume(100.5, TEXT("e")) == EKGChatReject::RateLimited);
	TestTrue(TEXT("Refilled after 1.5 s"), L.TryConsume(102.2, TEXT("f")) == EKGChatReject::None);
	TestTrue(TEXT("Empty again"), L.TryConsume(102.3, TEXT("g")) == EKGChatReject::RateLimited);
	TestTrue(TEXT("Duplicate allowed after the window"), L.TryConsume(115.0, TEXT("a")) == EKGChatReject::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKGChatColorTest, "KillGodot.Chat.NameColor",
                                 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKGChatColorTest::RunTest(const FString& Parameters)
{
	const int32 A = FKGChatRules::MakeColorSeed(FString(), TEXT("Wanderer"));
	TestEqual(TEXT("Stable seed"), A, FKGChatRules::MakeColorSeed(FString(), TEXT("Wanderer")));
	TestEqual(TEXT("PUID wins over the name"), FKGChatRules::MakeColorSeed(TEXT("PUID-1"), TEXT("Wanderer")),
	          FKGChatRules::MakeColorSeed(TEXT("PUID-1"), TEXT("Renamed")));
	TestTrue(TEXT("Opaque colour"), FKGChatRules::NameColor(A).A > 0.99f);
	// No name colour may equal the Impatient crimson channel colour (colours must never hint at a side).
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		TestFalse(TEXT("Never the team colour"), FKGChatRules::NameColor(Seed).Equals(FKGChatRules::ChannelColor(EKGChatChannel::Team)));
		TestFalse(TEXT("Never the ghost colour"), FKGChatRules::NameColor(Seed).Equals(FKGChatRules::ChannelColor(EKGChatChannel::Dead)));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
