#include "Chores/UI/SKGChorePanel.h"

#include "Chores/KGChoreTypes.h"
#include "Chores/UI/KGMinigame.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundBase.h"
#include "UI/Menu/KGMenuStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/SOverlay.h"

namespace KGChorePanel
{
	// Window layout in logical units (scaled to the screen as a whole).
	constexpr float Pad = 18.0f;
	constexpr float HeaderH = 98.0f;
	constexpr float FooterH = 44.0f;
	constexpr float WinW = KGMg::W + Pad * 2.0f;
	constexpr float WinH = HeaderH + KGMg::H + FooterH;
	constexpr float CelebrateSeconds = 0.8f;
	const FVector2f CloseCentre(WinW - 34.0f, 34.0f);
	constexpr float CloseRadius = 17.0f;

	USoundBase* FindSound(FName Name)
	{
		static TMap<FName, TWeakObjectPtr<USoundBase>> Cache;
		static TSet<FName> Missing;
		if (Name.IsNone() || Missing.Contains(Name))
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<USoundBase>* Hit = Cache.Find(Name); Hit && Hit->IsValid())
		{
			return Hit->Get();
		}
		const FString S = Name.ToString();
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/KillGodot/Audio/%s.%s"), *S, *S), nullptr,
		                                           LOAD_NoWarn | LOAD_Quiet);
		if (!Sound)
		{
			Missing.Add(Name);
			return nullptr;
		}
		Cache.Add(Name, Sound);
		return Sound;
	}

	void Check(FKGMgPainter& P, const FVector2f& C, float Size, const FLinearColor& Color)
	{
		const FVector2f A = C + FVector2f(-0.45f, 0.02f) * Size;
		const FVector2f B = C + FVector2f(-0.12f, 0.34f) * Size;
		const FVector2f D = C + FVector2f(0.48f, -0.34f) * Size;
		P.Bar(A, B, Size * 0.2f, Color);
		P.Bar(B - FVector2f(0.06f, -0.03f) * Size, D, Size * 0.2f, Color);
	}

	void Cross(FKGMgPainter& P, const FVector2f& C, float Size, const FLinearColor& Color, float Width)
	{
		P.Bar(C - FVector2f(Size, Size), C + FVector2f(Size, Size), Width, Color);
		P.Bar(C - FVector2f(Size, -Size), C + FVector2f(Size, -Size), Width, Color);
	}
}

SKGChorePanel::SKGChorePanel() = default;
SKGChorePanel::~SKGChorePanel() = default;

void SKGChorePanel::Construct(const FArguments& InArgs)
{
	ChoreId = InArgs._ChoreId;
	Def = FKGChoreCatalog::Find(ChoreId);
	bFake = InArgs._bFake;
	bPractice = InArgs._bPractice;
	bShot = InArgs._bShot;
	SoundContext = InArgs._SoundContext;
	OnStageDone = InArgs._OnStageDone;
	OnClosed = InArgs._OnClosed;
	Game = FKGMinigameFactory::Create(ChoreId);
	if (Game)
	{
		Game->Start(static_cast<uint32>(InArgs._Seed), InArgs._StartStage);
	}
	SetCanTick(true);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			// Slightly blurred, dimmed world: you still see what happens around you (and who walks up behind you).
			SNew(SBackgroundBlur)
			.bApplyAlphaToBlur(true)
			.BlurStrength_Lambda([this]() { return bShot ? 0.0f : 3.5f * Appear * (bClosing ? 1.0f - FMath::Min(1.0f, CloseAge / 0.2f) : 1.0f); })
			.Padding(0.0f)
			[
				SNew(SImage)
				.Image(&FKGMenuStyle::Get().WhiteBrush)
				.ColorAndOpacity_Lambda([this]()
				{
					const float Fade = bClosing && CloseAge > CloseDelay - 0.2f ? FMath::Max(0.0f, (CloseDelay - CloseAge) / 0.2f) : 1.0f;
					return FSlateColor(FKGMenuStyle::WithAlpha(FKGMenuStyle::Get().Ink, 0.38f * Appear * Fade));
				})
			]
		]
	];
}

SKGChorePanel::FLayout SKGChorePanel::ComputeLayout(const FGeometry& Geometry) const
{
	using namespace KGChorePanel;
	const FVector2f Size = UE::Slate::CastToVector2f(Geometry.GetLocalSize());
	float Scale = FMath::Min(Size.X * 0.74f / WinW, Size.Y * 0.84f / WinH);
	// Pop-in: a little growth while appearing.
	Scale *= 0.94f + 0.06f * KGMg::EaseOutBack(Appear);
	const FVector2f Origin = (Size - FVector2f(WinW, WinH) * Scale) * 0.5f;
	FLayout L;
	L.Scale = Scale;
	L.Window = Geometry.MakeChild(FVector2f(WinW, WinH), FSlateLayoutTransform(Scale, Origin));
	const float ShakeX = Shake > 0.0f ? FMath::Sin(ShakeT * 70.0f) * Shake : 0.0f;
	L.Content = L.Window.MakeChild(FVector2f(KGMg::W, KGMg::H), FSlateLayoutTransform(1.0f, FVector2f(Pad + ShakeX, HeaderH)));
	return L;
}

void SKGChorePanel::PlaySound(FName Name, float Volume, float Pitch) const
{
	UObject* Ctx = SoundContext.Get();
	if (!Ctx || !Ctx->GetWorld() || bShot)
	{
		return;
	}
	if (USoundBase* Sound = KGChorePanel::FindSound(Name))
	{
		UGameplayStatics::PlaySound2D(Ctx, Sound, Volume, Pitch);
	}
}

void SKGChorePanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	if (!bFrozen)
	{
		Step(FMath::Min(InDeltaTime, 0.1f));
	}
}

void SKGChorePanel::Step(float Dt)
{
	using namespace KGChorePanel;
	Appear = FMath::Min(1.0f, Appear + Dt / 0.22f);
	OpenAge += Dt;
	ShakeT += Dt;
	Shake = FMath::Max(0.0f, Shake - Dt * 30.0f);
	BannerAge += Dt;
	for (int32 i = Pops.Num() - 1; i >= 0; --i)
	{
		Pops[i].Age += Dt;
		if (Pops[i].Age > 1.1f)
		{
			Pops.RemoveAt(i);
		}
	}
	if (bClosing)
	{
		CloseAge += Dt;
		if (!bClosedFired && CloseAge >= CloseDelay)
		{
			bClosedFired = true;
			OnClosed.ExecuteIfBound(CloseReason);
		}
		if (Game)
		{
			Game->HostTick(Dt, false);
		}
		return;
	}
	if (!Game)
	{
		return;
	}

	Game->HostTick(Dt, bAuto);
	if (bAuto && Def && !Game->IsStageSolved() && Def->StageMinSeconds.IsValidIndex(Game->GetStage()) &&
	    Game->GetStageTime() >= Def->StageMinSeconds[Game->GetStage()] + 0.6f)
	{
		Game->ForceSolve();
	}
	for (const FKGMgFeedback& F : Game->DrainFeedback())
	{
		if (!F.Sound.IsNone())
		{
			PlaySound(F.Sound, F.Volume, F.Pitch);
		}
		if (!F.Pop.IsEmpty())
		{
			Pops.Add({F.Pop, F.At, F.Color, 0.0f});
		}
		if (F.Shake > 0.0f)
		{
			Shake = FMath::Max(Shake, F.Shake);
			ShakeT = 0.0f;
		}
	}

	if (Game->IsStageSolved() && !bStageReported)
	{
		bStageReported = true;
		SolveAge = 0.0f;
		SolvedStage = Game->GetStage();
		const bool bLast = SolvedStage + 1 >= Game->NumStages();
		PlaySound(bLast ? FName(TEXT("S_TaskDone")) : FName(TEXT("S_Chore_Stage")), bLast ? 0.8f : 0.9f, 1.0f);
		OnStageDone.ExecuteIfBound(SolvedStage);
		if (bLast)
		{
			bDoneStamp = true;
			BeginClose(EKGChoreClose::Completed, 1.45f);
		}
	}
	else if (bStageReported)
	{
		SolveAge += Dt;
		if (SolveAge >= CelebrateSeconds)
		{
			bStageReported = false;
			Game->AdvanceStage();
		}
	}
}

void SKGChorePanel::BeginClose(EKGChoreClose Reason, float Delay)
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	CloseReason = Reason;
	CloseDelay = Delay;
	CloseAge = 0.0f;
}

void SKGChorePanel::ServerRejected(int32 Stage, const FString& Why)
{
	if (!Game)
	{
		return;
	}
	if (bClosing && CloseReason == EKGChoreClose::Completed && !bClosedFired)
	{
		bClosing = false;   // the last stage was refused: stay open and replay it
		bDoneStamp = false;
	}
	bStageReported = false;
	Game->RestartAt(Stage);
	Banner = Why;
	BannerAge = 0.0f;
	BannerColor = KGMg::Crimson;
	PlaySound(TEXT("S_UI_Bad"), 1.0f, 0.8f);
}

void SKGChorePanel::ForceClose(EKGChoreClose Reason)
{
	if (bClosing && bClosedFired)
	{
		return;
	}
	switch (Reason)
	{
	case EKGChoreClose::Hit: Banner = TEXT("You were hit!"); break;
	case EKGChoreClose::Moved: Banner = TEXT("Pushed away from the chore"); break;
	case EKGChoreClose::Phase: Banner = TEXT("The meeting bell! Chores wait."); break;
	case EKGChoreClose::Died: Banner = TEXT(""); break;
	default: Banner = TEXT("Interrupted"); break;
	}
	BannerAge = 0.0f;
	BannerColor = KGMg::Crimson;
	bClosing = false;   // restart the close with the interrupt reason
	bDoneStamp = false;
	BeginClose(Reason, Reason == EKGChoreClose::Died ? 0.0f : 0.7f);
	PlaySound(TEXT("S_UI_Bad"), 0.8f, 0.7f);
}

void SKGChorePanel::DebugAdvance(float Seconds, bool bPose)
{
	Appear = 1.0f;
	const float StepDt = 1.0f / 30.0f;
	for (float T = 0.0f; T < Seconds; T += StepDt)
	{
		Step(StepDt);
	}
	if (bPose && Game)
	{
		Game->DebugPose();
		Game->DrainFeedback();
		bFrozen = true;
	}
	Appear = 1.0f;
}

bool SKGChorePanel::IsCloseHovered() const
{
	return FVector2f::Distance(WindowMouse, KGChorePanel::CloseCentre) <= KGChorePanel::CloseRadius + 4.0f;
}

// ---- input -------------------------------------------------------------------------------------------------------------

FReply SKGChorePanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (bClosing)
	{
		return FReply::Handled();
	}
	const bool bLeaveKey = Key == EKeys::Escape || Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D ||
	                       Key == EKeys::Gamepad_Special_Right || (Key == EKeys::E && OpenAge > 0.4f && !InKeyEvent.IsRepeat());
	if (bLeaveKey && OpenAge > 0.15f)
	{
		BeginClose(EKGChoreClose::Left, 0.18f);
		return FReply::Handled();
	}
	if (Game && !InKeyEvent.IsRepeat())
	{
		Game->HostKey(Key);
	}
	return FReply::Handled();
}

FReply SKGChorePanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FLayout L = ComputeLayout(MyGeometry);
	WindowMouse = UE::Slate::CastToVector2f(L.Window.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	if (!bClosing && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (IsCloseHovered())
		{
			BeginClose(EKGChoreClose::Left, 0.18f);
		}
		else if (Game && !bStageReported)
		{
			Game->HostPress(UE::Slate::CastToVector2f(L.Content.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
		}
	}
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SKGChorePanel::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SKGChorePanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FLayout L = ComputeLayout(MyGeometry);
	if (Game && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Game->HostRelease(UE::Slate::CastToVector2f(L.Content.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SKGChorePanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FLayout L = ComputeLayout(MyGeometry);
	WindowMouse = UE::Slate::CastToVector2f(L.Window.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	if (Game)
	{
		const FVector2f Pos = UE::Slate::CastToVector2f(L.Content.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
		const FVector2f Delta = UE::Slate::CastToVector2f(MouseEvent.GetCursorDelta()) / FMath::Max(0.01f, L.Scale * MyGeometry.Scale);
		Game->HostMove(Pos, Delta);
	}
	return FReply::Handled();
}

FCursorReply SKGChorePanel::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(IsCloseHovered() ? EMouseCursor::Hand : EMouseCursor::Default);
}

// ---- paint -------------------------------------------------------------------------------------------------------------

void SKGChorePanel::PaintBackdrop(FKGMgPainter& P, const FVector2f& Size) const
{
	// Screenshot stand-in for the game view: dusk sky, the cove, rooftops (what the blur would show in game).
	using namespace KGMg;
	P.RectV(FVector2f::ZeroVector, FVector2f(Size.X, Size.Y * 0.62f), C(0x3B4D8F), C(0xF0A868));
	P.RectV(FVector2f(0.0f, Size.Y * 0.62f), FVector2f(Size.X, Size.Y * 0.38f), C(0x2C5C7A), C(0x173347));
	P.Glow(FVector2f(Size.X * 0.72f, Size.Y * 0.58f), Size.Y * 0.35f, A(C(0xFFD27A), 0.8f));
	for (int32 i = 0; i < 12; ++i)
	{
		const float X = Size.X * (i / 11.0f);
		const float Hgt = Size.Y * (0.14f + 0.06f * FMath::Sin(i * 1.7f));
		const float Wd = Size.X * 0.09f;
		P.Rect(FVector2f(X - Wd * 0.5f, Size.Y * 0.66f - Hgt * 0.4f), FVector2f(Wd, Hgt), C(0x3A2B42));
		P.Tri(FVector2f(X - Wd * 0.6f, Size.Y * 0.66f - Hgt * 0.4f), FVector2f(X + Wd * 0.6f, Size.Y * 0.66f - Hgt * 0.4f),
		      FVector2f(X, Size.Y * 0.66f - Hgt * 0.4f - Wd * 0.45f), C(0x7A3B3B));
	}
	P.RectV(FVector2f(0.0f, Size.Y * 0.8f), FVector2f(Size.X, Size.Y * 0.2f), C(0x4E7A3A), C(0x2F4A24));
	P.Rect(FVector2f::ZeroVector, Size, A(Ink, 0.38f));
}

void SKGChorePanel::PaintHeader(FKGMgPainter& P) const
{
	using namespace KGMg;
	using namespace KGChorePanel;
	const int32 NumStages = Game ? Game->NumStages() : 1;
	const int32 Current = Game ? Game->GetStage() : 0;

	// Caption + title + instruction.
	FString Caption = TEXT("CHORE");
	if (Def && Def->IsVisual())
	{
		Caption += TEXT("   ·   VISIBLE TO OTHERS");
	}
	P.Text(FVector2f(Pad + 2.0f, 16.0f), Caption, 11.0f, Lantern, 0.0f, TEXT("Bold"), 220);
	P.Text(FVector2f(Pad, 30.0f), Def ? Def->Title : ChoreId.ToString(), 25.0f, Cream, 0.0f, TEXT("Black"), 10);
	// Instruction: one line across the whole header, shrunk to fit when long.
	const FString Instruction = Game ? Game->GetInstruction() : FString(TEXT("Hold E to work"));
	float InstrSize = 15.0f;
	const float InstrRoom = WinW - Pad * 2.0f - 20.0f;
	while (InstrSize > 11.0f && P.MeasureText(Instruction, InstrSize, TEXT("Bold")).X > InstrRoom)
	{
		InstrSize -= 0.5f;
	}
	P.Tri(FVector2f(Pad + 2.0f, 72.0f), FVector2f(Pad + 2.0f, 84.0f), FVector2f(Pad + 10.0f, 78.0f), Gold);
	P.Text(FVector2f(Pad + 16.0f, 78.0f - InstrSize * 0.68f), Instruction, InstrSize, Gold, 0.0f, TEXT("Bold"));

	// Stage pips on the title row, left of the close button: done = green check, current = pulsing gold ring,
	// later = muted; the stage names sit under the pips.
	if (NumStages > 1)
	{
		TArray<float> Widths;
		for (int32 i = 0; i < NumStages; ++i)
		{
			const FString Name = Def && Def->Stages.IsValidIndex(i) ? Def->Stages[i].ToUpper() : FString::FromInt(i + 1);
			Widths.Add(FMath::Max(28.0f, P.MeasureText(Name, 9.0f, TEXT("Bold"), 60).X + 12.0f));
		}
		TArray<float> Xs;
		Xs.SetNum(NumStages);
		float Cursor = CloseCentre.X - 40.0f - Widths.Last() * 0.5f + 10.0f;
		for (int32 i = NumStages - 1; i >= 0; --i)
		{
			Xs[i] = Cursor;
			if (i > 0)
			{
				Cursor -= FMath::Max(46.0f, (Widths[i] + Widths[i - 1]) * 0.5f + 8.0f);
			}
		}
		const float Y = 34.0f;
		for (int32 i = 0; i < NumStages; ++i)
		{
			const float X = Xs[i];
			const float Gap = i + 1 < NumStages ? Xs[i + 1] - X : 0.0f;
			const bool bDone = i < Current || (i == Current && Game && Game->IsStageSolved());
			const bool bNow = i == Current && !bDone;
			if (i + 1 < NumStages)
			{
				P.Bar(FVector2f(X + 11.0f, Y), FVector2f(X + Gap - 11.0f, Y), 3.0f, bDone ? A(Good, 0.8f) : A(Cream, 0.18f));
			}
			if (bDone)
			{
				P.Circle(FVector2f(X, Y), 10.0f, Good);
				Check(P, FVector2f(X, Y), 11.0f, Night);
			}
			else if (bNow)
			{
				const float Pulse = 0.5f + 0.5f * FMath::Sin(OpenAge * 5.0f);
				P.Circle(FVector2f(X, Y), 10.0f + Pulse * 1.5f, A(Gold, 0.22f), Gold, 2.5f);
			}
			else
			{
				P.Circle(FVector2f(X, Y), 9.0f, A(Ink, 0.6f), A(Cream, 0.3f), 1.5f);
			}
			const FString Name = Def && Def->Stages.IsValidIndex(i) ? Def->Stages[i].ToUpper() : FString::FromInt(i + 1);
			P.Text(FVector2f(X, Y + 13.0f), Name, 9.0f, bNow ? Gold : bDone ? A(Good, 0.9f) : Muted, 0.5f, TEXT("Bold"), 60);
		}
	}

	// Close button.
	const bool bHot = IsCloseHovered();
	P.Circle(CloseCentre, CloseRadius, bHot ? A(Crimson, 0.9f) : A(Cream, 0.08f), A(Cream, bHot ? 0.0f : 0.25f), 1.5f);
	Cross(P, CloseCentre, 6.0f, bHot ? Cream : CreamDim, 3.0f);
}

void SKGChorePanel::PaintOverlays(FKGMgPainter& P) const
{
	using namespace KGMg;
	using namespace KGChorePanel;
	// Floating pop text (in content space).
	for (const FPop& Pop : Pops)
	{
		const float T = Pop.Age / 1.1f;
		const float Rise = 34.0f * EaseOutCubic(T);
		const float Alpha = T < 0.7f ? 1.0f : 1.0f - (T - 0.7f) / 0.3f;
		const float Size = 18.0f * (0.7f + 0.3f * EaseOutBack(FMath::Min(1.0f, Pop.Age / 0.25f)));
		P.Text(Pop.At + FVector2f(1.5f, 1.5f - Rise), Pop.Text, Size, FLinearColor(0, 0, 0, 0.6f * Alpha), 0.5f, TEXT("Black"));
		P.Text(Pop.At + FVector2f(0.0f, -Rise), Pop.Text, Size, A(Pop.Color, Alpha), 0.5f, TEXT("Black"));
	}

	// Stage solved: green wash + stamp.
	if (bStageReported || bDoneStamp)
	{
		const float Age = bDoneStamp ? CloseAge : SolveAge;
		const float Wash = FMath::Max(0.0f, 1.0f - Age / 0.5f);
		P.Rect(FVector2f::ZeroVector, FVector2f(W, H), A(Good, 0.22f * Wash));
		const float S = EaseOutBack(FMath::Min(1.0f, Age / 0.3f));
		const float Fade = bDoneStamp ? 1.0f : FMath::Clamp((CelebrateSeconds - Age) / 0.2f, 0.0f, 1.0f);
		const FString Label = bDoneStamp ? FString(TEXT("CHORE DONE")) :
		                      (Def && Def->Stages.IsValidIndex(SolvedStage) ? Def->Stages[SolvedStage].ToUpper() + TEXT("  DONE") : FString(TEXT("GOOD")));
		const float TextSize = bDoneStamp ? 34.0f : 24.0f;
		const FVector2f M = P.MeasureText(Label, TextSize, TEXT("Black"), 60);
		const FVector2f Box = FVector2f(M.X + 90.0f, M.Y + 30.0f) * FMath::Max(0.05f, S);
		const FVector2f Centre(W * 0.5f, H * 0.5f);
		P.Shadow(Centre - Box * 0.5f, Box, 22.0f, 0.4f * Fade);
		P.RoundRect(Centre - Box * 0.5f, Box, A(Night, 0.94f * Fade), 22.0f, A(Good, 0.95f * Fade), 3.0f);
		if (S > 0.6f)
		{
			P.Circle(Centre + FVector2f(-Box.X * 0.5f + 34.0f, 0.0f), 16.0f, A(Good, Fade));
			Check(P, Centre + FVector2f(-Box.X * 0.5f + 34.0f, 0.0f), 17.0f, A(Night, Fade));
			P.Text(Centre + FVector2f(22.0f, -M.Y * 0.5f), Label, TextSize, A(Cream, Fade), 0.5f, TEXT("Black"), 60);
		}
	}

	// Reject / interrupt banner (top of the content).
	if (!Banner.IsEmpty() && BannerAge < 2.4f)
	{
		const float In = EaseOutBack(FMath::Min(1.0f, BannerAge / 0.25f));
		const float Out = FMath::Clamp((2.4f - BannerAge) / 0.3f, 0.0f, 1.0f);
		const FVector2f M = P.MeasureText(Banner, 17.0f, TEXT("Black"));
		const FVector2f Box(M.X + 40.0f, 42.0f);
		const FVector2f Pos(W * 0.5f - Box.X * 0.5f, 14.0f + (1.0f - In) * -30.0f);
		P.Shadow(Pos, Box, 14.0f, 0.4f * Out);
		P.RoundRect(Pos, Box, A(Night, 0.96f * Out), 14.0f, A(BannerColor, Out), 2.5f);
		P.Text(FVector2f(W * 0.5f, Pos.Y + 10.0f), Banner, 17.0f, A(Cream, Out), 0.5f, TEXT("Black"));
	}
}

int32 SKGChorePanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
                             FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
                             bool bParentEnabled) const
{
	using namespace KGMg;
	using namespace KGChorePanel;
	const FVector2f Size = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize());
	int32 Layer = LayerId;
	if (bShot)
	{
		FKGMgPainter Back(OutDrawElements, AllottedGeometry, Layer, 1.0f);
		PaintBackdrop(Back, Size);
		Layer = Back.GetLayer();
	}
	Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Layer, InWidgetStyle, bParentEnabled) + 1;

	float Opacity = FMath::Min(1.0f, Appear * 1.4f);
	if (bClosing && CloseAge > CloseDelay - 0.2f)
	{
		Opacity *= FMath::Max(0.0f, (CloseDelay - CloseAge) / 0.2f);
	}
	if (Opacity <= 0.001f)
	{
		return Layer;
	}
	const FLayout L = ComputeLayout(AllottedGeometry);

	// Window frame.
	FKGMgPainter Win(OutDrawElements, L.Window, Layer, Opacity);
	Win.Shadow(FVector2f::ZeroVector, FVector2f(WinW, WinH), 26.0f, 0.55f, 10.0f);
	Win.RoundRect(FVector2f::ZeroVector, FVector2f(WinW, WinH), A(Night, 0.97f), 26.0f, A(Cream, 0.10f), 1.5f);
	Win.RectV(FVector2f(2.0f, 2.0f), FVector2f(WinW - 4.0f, 84.0f), A(Lantern, 0.10f), A(Lantern, 0.0f));
	PaintHeader(Win);
	// Content well (the minigame paints its own scene inside).
	Win.RoundRect(FVector2f(Pad - 4.0f, HeaderH - 4.0f), FVector2f(W + 8.0f, H + 8.0f), Ink, 16.0f);
	// Footer hints.
	{
		const float Y = HeaderH + H + 14.0f;
		Win.RoundRect(FVector2f(Pad, Y), FVector2f(40.0f, 22.0f), Panel, 6.0f, A(Cream, 0.28f), 1.5f);
		Win.Text(FVector2f(Pad + 20.0f, Y + 3.0f), TEXT("Esc"), 12.0f, Cream, 0.5f, TEXT("Black"));
		Win.Text(FVector2f(Pad + 50.0f, Y + 3.0f), TEXT("Leave  ·  progress is kept"), 13.0f, CreamDim, 0.0f, TEXT("Medium"));
		FString Right;
		if (Game && Game->NumStages() > 1)
		{
			Right = FString::Printf(TEXT("STAGE %d / %d"), FMath::Min(Game->GetStage() + 1, Game->NumStages()), Game->NumStages());
		}
		if (bAuto)
		{
			Right = TEXT("AUTO-WIN   ") + Right;
		}
		Win.Text(FVector2f(WinW - Pad, Y + 4.0f), Right, 11.0f, Muted, 1.0f, TEXT("Bold"), 160);
		// Tags left of the stage counter: practice / faking. Faking is only ever painted on the Impatient's own screen.
		float TagX = WinW - Pad - Win.MeasureText(Right, 11.0f, TEXT("Bold"), 160).X - (Right.IsEmpty() ? 0.0f : 14.0f);
		auto AddTag = [&Win, &TagX, Y](const FString& T, const FLinearColor& Color, const FLinearColor& TextColor)
		{
			const float Wd = Win.MeasureText(T, 11.0f, TEXT("Bold"), 80).X + 20.0f;
			Win.RoundRect(FVector2f(TagX - Wd, Y), FVector2f(Wd, 22.0f), A(Color, 0.25f), -1.0f, A(Color, 0.9f), 1.5f);
			Win.Text(FVector2f(TagX - Wd * 0.5f, Y + 3.5f), T, 11.0f, TextColor, 0.5f, TEXT("Bold"), 80);
			TagX -= Wd + 8.0f;
		};
		if (bFake)
		{
			AddTag(TEXT("FAKING - WON'T COUNT"), Crimson, C(0xFF8A80));
		}
		if (bPractice)
		{
			AddTag(TEXT("PRACTICE"), Ghost, Ghost);
		}
	}
	Layer = Win.GetLayer();

	// Minigame + overlays, clipped to the content well.
	OutDrawElements.PushClip(FSlateClippingZone(L.Content));
	FKGMgPainter Content(OutDrawElements, L.Content, Layer, Opacity);
	if (Game)
	{
		Game->Paint(Content);
	}
	PaintOverlays(Content);
	Layer = Content.GetLayer();
	OutDrawElements.PopClip();

	// Round the content's corners (fillets in the window colour) + a hairline frame.
	FKGMgPainter Frame(OutDrawElements, L.Window, Layer, Opacity);
	{
		const float R = 12.0f;
		const FVector2f P0(Pad, HeaderH);
		const FVector2f P1(Pad + W, HeaderH + H);
		const FVector2f Corners[4] = {P0, FVector2f(P1.X, P0.Y), P1, FVector2f(P0.X, P1.Y)};
		const FVector2f Dirs[4] = {FVector2f(1, 1), FVector2f(-1, 1), FVector2f(-1, -1), FVector2f(1, -1)};
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector2f O = Corners[c] + Dirs[c] * R;
			const float Base = FMath::Atan2(-Dirs[c].Y, -Dirs[c].X);
			for (int32 s = 0; s < 6; ++s)
			{
				const float A0 = Base - UE_HALF_PI * 0.5f + UE_HALF_PI * (s / 6.0f);
				const float A1 = Base - UE_HALF_PI * 0.5f + UE_HALF_PI * ((s + 1) / 6.0f);
				Frame.Tri(Corners[c], O + FVector2f(FMath::Cos(A0), FMath::Sin(A0)) * R, O + FVector2f(FMath::Cos(A1), FMath::Sin(A1)) * R,
				          A(Night, 0.97f));
			}
		}
		Frame.RoundRect(P0 - FVector2f(1.0f, 1.0f), FVector2f(W + 2.0f, H + 2.0f), FLinearColor::Transparent, R + 1.0f,
		                A(Cream, 0.16f), 1.5f);
	}
	return Frame.GetLayer();
}
