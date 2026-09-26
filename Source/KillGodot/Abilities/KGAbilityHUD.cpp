#include "Abilities/KGAbilityHUD.h"
#include "Abilities/KGAbilityHolder.h"
#include "Abilities/KGAbilityTypes.h"
#include "Abilities/KGWoundComponent.h"
#include "CanvasItem.h"
#include "Character/KGCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Traps/KGMimicTrap.h"
#include "UI/Reveal/KGRoleCardText.h"

namespace KGAbilityHUDPrivate
{
	const FLinearColor Ink(0.03f, 0.02f, 0.03f, 0.72f);
	const FLinearColor Crimson(0.86f, 0.16f, 0.18f, 1.0f);
	const FLinearColor Bone(0.96f, 0.92f, 0.84f, 1.0f);
	const FLinearColor Dim(0.62f, 0.58f, 0.55f, 1.0f);
	const FLinearColor Good(0.45f, 0.9f, 0.5f, 1.0f);

	void Box(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col)
	{
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), Col);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	}

	void Text(UCanvas* C, const FString& S, float X, float Y, float Scale, const FLinearColor& Col, bool bCentre = false)
	{
		UFont* Font = GEngine->GetMediumFont();
		float W = 0.0f, H = 0.0f;
		C->TextSize(Font, S, W, H, Scale, Scale);
		FCanvasTextItem T(FVector2D(bCentre ? X - W * 0.5f : X, Y), FText::FromString(S), Font, Col);
		T.Scale = FVector2D(Scale, Scale);
		T.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		C->DrawItem(T);
	}

	float TextW(UCanvas* C, const FString& S, float Scale)
	{
		float W = 0.0f, H = 0.0f;
		C->TextSize(GEngine->GetMediumFont(), S, W, H, Scale, Scale);
		return W;
	}

	/** Everyone: wounds on the body in front of the camera. */
	void DrawWounds(UCanvas* C, APlayerController* PC, float S, bool bTr)
	{
		FVector Eyes;
		FRotator Rot;
		PC->GetPlayerViewPoint(Eyes, Rot);
		const FVector Dir = Rot.Vector();
		const AKGCharacter* Best = nullptr;
		double BestDot = FMath::Cos(FMath::DegreesToRadians(9.0));
		for (TActorIterator<AKGCharacter> It(PC->GetWorld()); It; ++It)
		{
			if (*It == PC->GetPawn() || !UKGWoundComponent::FindOn(*It))
			{
				continue;
			}
			const FVector To = It->GetActorLocation() - Eyes;
			if (To.SizeSquared() > FMath::Square(350.0))
			{
				continue;
			}
			const double Dot = FVector::DotProduct(To.GetSafeNormal(), Dir);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				Best = *It;
			}
		}
		const UKGWoundComponent* W = UKGWoundComponent::FindOn(Best);
		if (!W)
		{
			return;
		}
		float Y = C->ClipY * 0.56f;
		TSet<FName> Shown;
		for (const FKGWound& X : W->GetWounds())
		{
			if (Shown.Contains(X.Kind))
			{
				continue;
			}
			Shown.Add(X.Kind);
			Text(C, UKGWoundComponent::Describe(X.Kind, bTr), C->ClipX * 0.5f, Y, 1.15f * S, Bone, true);
			Y += 24.0f * S;
		}
	}
}

void KGAbilityHUD::Draw(AHUD* Hud, UCanvas* PostCanvas)
{
	using namespace KGAbilityHUDPrivate;
	// AHUD::Canvas is protected, so the canvas handed to the post-render hook is used.
	UCanvas* Canvas = PostCanvas;
	APlayerController* PC = Hud ? Hud->PlayerOwner.Get() : nullptr;
	if (!PC || !Canvas || !GEngine || !PC->IsLocalController())
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.0f;
	const bool bTr = KGRoleCard::IsTurkish();
	DrawWounds(Canvas, PC, S, bTr);

	AKGAbilityHolder* H = AKGAbilityHolder::FindFor(PC);
	if (!H || H->GetStates().Num() == 0)
	{
		return;
	}
	const float Now = PC->GetWorld()->GetTimeSeconds();

	// ---- the bar (bottom right, above the stamina / chat corner) ----
	const TArray<FKGAbilityState>& States = H->GetStates();
	const float SlotW = 150.0f * S;
	const float SlotH = 74.0f * S;
	const float Gap = 8.0f * S;
	const float BarW = States.Num() * SlotW + (States.Num() - 1) * Gap;
	const float X0 = Canvas->ClipX - BarW - 36.0f * S;
	const float Y0 = Canvas->ClipY - SlotH - 150.0f * S;
	Text(Canvas, bTr ? TEXT("TUZAKÇI") : TEXT("TRAPPER"), X0, Y0 - 26.0f * S, 1.0f * S, Crimson);
	for (int32 i = 0; i < States.Num(); ++i)
	{
		const FKGAbilityState& St = States[i];
		const FKGAbilityDef* D = FKGAbilityCatalog::Find(St.AbilityId);
		const float X = X0 + i * (SlotW + Gap);
		const bool bAim = H->AimingAbility == St.AbilityId;
		const bool bReady = St.Charges > 0 && St.Cooldown.RemainingSeconds <= 0.0f;
		Box(Canvas, X, Y0, SlotW, SlotH, Ink);
		Box(Canvas, X, Y0 + SlotH - 4.0f * S, SlotW, 4.0f * S, bAim ? Bone : (bReady ? Crimson : Dim * FLinearColor(1, 1, 1, 0.5f)));
		if (St.Cooldown.RemainingSeconds > 0.0f && D && D->CooldownSecs > 0.0f)
		{
			const float A = St.Cooldown.RemainingSeconds / D->CooldownSecs;
			Box(Canvas, X, Y0, SlotW * A, SlotH - 4.0f * S, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
		}
		Text(Canvas, FString::Printf(TEXT("Alt+%d"), i + 1), X + 8.0f * S, Y0 + 6.0f * S, 0.8f * S, Dim);
		Text(Canvas, FKGAbilityCatalog::DisplayName(St.AbilityId, bTr), X + 8.0f * S, Y0 + 24.0f * S, 1.25f * S, bReady ? Bone : Dim);
		FString Pips;
		for (int32 c = 0; c < (D ? D->ChargesPerCycle : St.Charges); ++c)
		{
			Pips += c < St.Charges ? TEXT("o ") : TEXT(". ");
		}
		const FString Right = St.Cooldown.RemainingSeconds > 0.0f ? FString::Printf(TEXT("%.0fs"), FMath::CeilToFloat(St.Cooldown.RemainingSeconds)) : Pips;
		Text(Canvas, Right, X + SlotW - TextW(Canvas, Right, 1.0f * S) - 8.0f * S, Y0 + 6.0f * S, 1.0f * S, St.Charges > 0 ? Crimson : Dim);
	}

	// ---- the targeting prompt ----
	if (!H->AimingAbility.IsNone())
	{
		const FKGAbilityDef* D = FKGAbilityCatalog::Find(H->AimingAbility);
		const int32 Slot = States.IndexOfByPredicate([&](const FKGAbilityState& X) { return X.AbilityId == H->AimingAbility; }) + 1;
		FVector Eyes;
		FRotator Rot;
		PC->GetPlayerViewPoint(Eyes, Rot);
		FString Target;
		bool bOk = false;
		if (D && D->Target == EKGAbilityTarget::Container)
		{
			float Dist = 0.0f;
			const AActor* A = AKGAbilityHolder::PickContainer(PC->GetWorld(), Eyes, Rot.Vector(), D->RangeCm, Dist);
			const AKGMimicTrap* M = A ? AKGMimicTrap::FindOn(A) : nullptr;
			bOk = A && Dist <= D->RangeCm && (!M || M->GetState() == EKGTrapState::Idle);
			Target = !A ? (bTr ? TEXT("hedef yok") : TEXT("no container"))
			       : Dist > D->RangeCm ? (bTr ? TEXT("çok uzak") : TEXT("too far"))
			       : !bOk ? (bTr ? TEXT("zaten mimik") : TEXT("already a mimic"))
			       : (bTr ? TEXT("hazır") : TEXT("ready"));
		}
		else if (D)
		{
			FVector P;
			bOk = AKGAbilityHolder::PickGround(PC->GetWorld(), PC->GetPawn(), Eyes, Rot.Vector(), D->RangeCm, P);
			Target = bOk ? (bTr ? TEXT("hazır") : TEXT("ready")) : (bTr ? TEXT("zemin yok") : TEXT("no ground"));
		}
		const float Y = Canvas->ClipY * 0.63f;
		const FString Head = FString::Printf(TEXT("%s - %s"), *FKGAbilityCatalog::DisplayName(H->AimingAbility, bTr),
		                                     *FKGAbilityCatalog::TargetHint(H->AimingAbility, bTr));
		const float W = FMath::Max(TextW(Canvas, Head, 1.3f * S), 420.0f * S) + 40.0f * S;
		Box(Canvas, Canvas->ClipX * 0.5f - W * 0.5f, Y - 10.0f * S, W, 84.0f * S, Ink);
		Text(Canvas, Head, Canvas->ClipX * 0.5f, Y, 1.3f * S, Bone, true);
		Text(Canvas, FString::Printf(TEXT("[%s]"), *Target), Canvas->ClipX * 0.5f, Y + 26.0f * S, 1.1f * S, bOk ? Good : Crimson, true);
		Text(Canvas, bTr ? FString::Printf(TEXT("Kurmak için tekrar Alt+%d  ·  Sağ tık: vazgeç"), Slot) : FString::Printf(TEXT("Alt+%d again to set  ·  Right click: cancel"), Slot),
		     Canvas->ClipX * 0.5f, Y + 50.0f * S, 0.9f * S, Dim, true);
		// A small reticle ring so the aim point reads.
		Box(Canvas, Canvas->ClipX * 0.5f - 7.0f * S, Canvas->ClipY * 0.5f - 1.0f * S, 14.0f * S, 2.0f * S, bOk ? Good : Crimson);
		Box(Canvas, Canvas->ClipX * 0.5f - 1.0f * S, Canvas->ClipY * 0.5f - 7.0f * S, 2.0f * S, 14.0f * S, bOk ? Good : Crimson);
	}

	// ---- the last verdict + private notes ----
	if (Now - H->LastFeedbackAt < 3.0f && !H->LastFeedback.IsEmpty())
	{
		Text(Canvas, H->LastFeedback, X0, Y0 + SlotH + 8.0f * S, 1.0f * S, Bone);
	}
	float NY = Canvas->ClipY * 0.22f;
	for (const FKGAbilityNote& N : H->GetNotes())
	{
		if (Now - N.ShownAt < 10.0f)
		{
			const float W = TextW(Canvas, N.Text, 1.05f * S) + 24.0f * S;
			Box(Canvas, Canvas->ClipX - W - 30.0f * S, NY - 4.0f * S, W, 28.0f * S, Ink);
			Text(Canvas, N.Text, Canvas->ClipX - W - 18.0f * S, NY, 1.05f * S, Crimson);
			NY += 32.0f * S;
		}
	}
}
