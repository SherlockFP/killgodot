#include "Character/KGBodyAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"

namespace KGBodyAnimPrivate
{
	float AdvanceTime(const UAnimSequence* Seq, float Time, float Delta, bool bLoop)
	{
		const float Len = Seq ? FMath::Max(Seq->GetPlayLength(), KINDA_SMALL_NUMBER) : 0.0f;
		if (!Seq)
		{
			return 0.0f;
		}
		Time += Delta;
		return bLoop ? FMath::Fmod(FMath::Max(Time, 0.0f), Len) : FMath::Clamp(Time, 0.0f, Len);
	}

	float SmoothStep(float X)
	{
		X = FMath::Clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}

	void Sample(UAnimSequence* Seq, float Time, bool bLoop, FPoseContext& Out)
	{
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, FDeltaTimeRecord(), bLoop));
	}

	/** Out = lerp(Out, Other, Weight * Mask[bone]) per bone, local space. */
	void BlendInto(FPoseContext& Out, const FPoseContext& Other, float Weight, const TArray<float>* Mask)
	{
		for (const FCompactPoseBoneIndex Index : Out.Pose.ForEachBoneIndex())
		{
			const float W = Weight * (Mask && Mask->IsValidIndex(Index.GetInt()) ? (*Mask)[Index.GetInt()] : 1.0f);
			if (W > ZERO_ANIMWEIGHT_THRESH)
			{
				Out.Pose[Index].BlendWith(Other.Pose[Index], W);
			}
		}
		if (!Mask && Weight >= 1.0f - ZERO_ANIMWEIGHT_THRESH)
		{
			Out.Curve = Other.Curve;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Proxy (evaluation)
// ---------------------------------------------------------------------------------------------------------------

void FKGBodyAnimInstanceProxy::PreEvaluateAnimation(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::PreEvaluateAnimation(InAnimInstance);
	if (const UKGBodyAnimInstance* Instance = Cast<UKGBodyAnimInstance>(InAnimInstance))
	{
		Snapshot = Instance->MakeSnapshot();
	}
}

const TArray<float>& FKGBodyAnimInstanceProxy::GetUpperBodyMask(const FBoneContainer& Bones)
{
	const int32 Num = Bones.GetCompactPoseNumBones();
	if (UpperMaskSerial == Bones.GetSerialNumber() && UpperMaskBones == Num && UpperMask.Num() == Num)
	{
		return UpperMask;
	}
	UpperMaskSerial = Bones.GetSerialNumber();
	UpperMaskBones = Num;
	UpperMask.SetNumZeroed(Num);
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	static const FName Spine1(TEXT("spine_01"));
	static const FName Spine2(TEXT("spine_02"));
	static const FName Spine3(TEXT("spine_03"));
	for (int32 i = 0; i < Num; ++i)
	{
		const FCompactPoseBoneIndex Index(i);
		const FName Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt());
		// Compact poses list parents before children, so the parent's weight is already known.
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
		float W = Parent.IsValid() ? UpperMask[Parent.GetInt()] : 0.0f;
		if (Name == Spine1)
		{
			W = 0.35f;
		}
		else if (Name == Spine2)
		{
			W = 0.75f;
		}
		else if (Name == Spine3)
		{
			W = 1.0f;
		}
		UpperMask[i] = W;
	}
	return UpperMask;
}

bool FKGBodyAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	using namespace KGBodyAnimPrivate;
	const FKGBodyAnimSnapshot& S = Snapshot;
	if (S.Base)
	{
		Sample(S.Base, S.BaseTime, S.bBaseLoop, Output);
	}
	else
	{
		Output.ResetToRefPose();
	}
	if (S.Prev && S.BaseAlpha < 1.0f)
	{
		FPoseContext PrevPose(Output);
		Sample(S.Prev, S.PrevTime, S.bPrevLoop, PrevPose);
		BlendInto(Output, PrevPose, 1.0f - S.BaseAlpha, nullptr);
	}
	if (S.Emote && S.EmoteWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		FPoseContext EmotePose(Output);
		Sample(S.Emote, S.EmoteTime, S.bEmoteLoop, EmotePose);
		BlendInto(Output, EmotePose, S.EmoteWeight, S.bUpperBody ? &GetUpperBodyMask(Output.Pose.GetBoneContainer()) : nullptr);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Instance (game thread)
// ---------------------------------------------------------------------------------------------------------------

FAnimInstanceProxy* UKGBodyAnimInstance::CreateAnimInstanceProxy()
{
	return new FKGBodyAnimInstanceProxy(this);
}

void UKGBodyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FKGBodyAnimInstanceProxy*>(InProxy);
}

void UKGBodyAnimInstance::PlayBase(UAnimSequence* Anim, bool bLoop, float BlendTime)
{
	if (!Anim || (Anim == Base && bLoop && bBaseLoop))
	{
		return;
	}
	if (Base && BlendTime > 0.0f)
	{
		Prev = Base;
		PrevTime = BaseTime;
		bPrevLoop = bBaseLoop;
		BaseAlpha = 0.0f;
		BaseBlendTime = BlendTime;
	}
	else
	{
		Prev = nullptr;
		BaseAlpha = 1.0f;
	}
	Base = Anim;
	BaseTime = 0.0f;
	bBaseLoop = bLoop;
}

void UKGBodyAnimInstance::PlayEmote(UAnimSequence* Intro, UAnimSequence* Main, bool bLoop, bool bUpperBody, float BlendIn,
                                    float PlayRate)
{
	if (!Main)
	{
		return;
	}
	// A new emote over a running one restarts from its first frame but keeps the current weight (no pop to the base).
	EmoteIntro = Intro;
	EmoteMain = Main;
	EmoteTime = 0.0f;
	EmoteRate = FMath::Max(PlayRate, 0.05f);
	bEmoteLoop = bLoop;
	bEmoteUpper = bUpperBody;
	EmoteTarget = 1.0f;
	EmoteBlendIn = FMath::Max(BlendIn, 0.01f);
}

void UKGBodyAnimInstance::StopEmote(float BlendOut)
{
	EmoteTarget = 0.0f;
	EmoteBlendOut = FMath::Max(BlendOut, 0.01f);
	if (BlendOut <= 0.0f)
	{
		EmoteWeight = 0.0f;
		EmoteMain = nullptr;
		EmoteIntro = nullptr;
	}
}

void UKGBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	using namespace KGBodyAnimPrivate;
	Super::NativeUpdateAnimation(DeltaSeconds);
	BaseTime = AdvanceTime(Base, BaseTime, DeltaSeconds, bBaseLoop);
	if (Prev)
	{
		PrevTime = AdvanceTime(Prev, PrevTime, DeltaSeconds, bPrevLoop);
		BaseAlpha = FMath::Min(1.0f, BaseAlpha + DeltaSeconds / FMath::Max(BaseBlendTime, 0.01f));
		if (BaseAlpha >= 1.0f)
		{
			Prev = nullptr;
		}
	}
	if (EmoteMain)
	{
		EmoteTime += DeltaSeconds * EmoteRate;
		if (EmoteWeight < EmoteTarget)
		{
			EmoteWeight = FMath::Min(EmoteTarget, EmoteWeight + DeltaSeconds / EmoteBlendIn);
		}
		else if (EmoteWeight > EmoteTarget)
		{
			EmoteWeight = FMath::Max(EmoteTarget, EmoteWeight - DeltaSeconds / EmoteBlendOut);
		}
		if (EmoteTarget <= 0.0f && EmoteWeight <= 0.0f)
		{
			EmoteMain = nullptr;
			EmoteIntro = nullptr;
		}
	}
}

FKGBodyAnimSnapshot UKGBodyAnimInstance::MakeSnapshot() const
{
	using namespace KGBodyAnimPrivate;
	FKGBodyAnimSnapshot S;
	S.Base = Base;
	S.BaseTime = BaseTime;
	S.bBaseLoop = bBaseLoop;
	S.Prev = Prev;
	S.PrevTime = PrevTime;
	S.bPrevLoop = bPrevLoop;
	S.BaseAlpha = SmoothStep(BaseAlpha);
	if (EmoteMain)
	{
		const float IntroLen = EmoteIntro ? EmoteIntro->GetPlayLength() : 0.0f;
		if (EmoteIntro && EmoteTime < IntroLen)
		{
			S.Emote = EmoteIntro;
			S.EmoteTime = EmoteTime;
			S.bEmoteLoop = false;
		}
		else
		{
			const float Len = FMath::Max(EmoteMain->GetPlayLength(), KINDA_SMALL_NUMBER);
			const float T = EmoteTime - IntroLen;
			S.Emote = EmoteMain;
			S.EmoteTime = bEmoteLoop ? FMath::Fmod(FMath::Max(T, 0.0f), Len) : FMath::Clamp(T, 0.0f, Len);
			S.bEmoteLoop = bEmoteLoop;
		}
		S.EmoteWeight = SmoothStep(EmoteWeight);
		S.bUpperBody = bEmoteUpper;
	}
	return S;
}
