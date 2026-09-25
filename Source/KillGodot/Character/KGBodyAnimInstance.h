#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "KGBodyAnimInstance.generated.h"

class UAnimSequence;

/** What the (possibly worker-thread) evaluation reads; copied on the game thread right before each evaluation. */
struct FKGBodyAnimSnapshot
{
	UAnimSequence* Base = nullptr;
	float BaseTime = 0.0f;
	bool bBaseLoop = true;
	/** Crossfade source (locomotion clip we are leaving) and the weight of the NEW base clip (1 = done). */
	UAnimSequence* Prev = nullptr;
	float PrevTime = 0.0f;
	bool bPrevLoop = true;
	float BaseAlpha = 1.0f;
	/** Emote layer on top: full body, or from spine_01 up (legs keep the locomotion). */
	UAnimSequence* Emote = nullptr;
	float EmoteTime = 0.0f;
	bool bEmoteLoop = false;
	float EmoteWeight = 0.0f;
	bool bUpperBody = false;
};

/** Native evaluation: base clip (+ crossfade) and an optional masked emote layer. No anim graph asset needed. */
struct FKGBodyAnimInstanceProxy : public FAnimInstanceProxy
{
	FKGBodyAnimInstanceProxy() = default;
	explicit FKGBodyAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FKGBodyAnimSnapshot Snapshot;

private:
	/** Per compact-pose bone weight of the upper-body layer (spine_01 0.35 -> spine_02 0.75 -> spine_03 up 1.0). */
	const TArray<float>& GetUpperBodyMask(const FBoneContainer& Bones);
	TArray<float> UpperMask;
	uint16 UpperMaskSerial = 0;
	int32 UpperMaskBones = -1;
};

/**
 * The villager body's animation instance (replaces the single-node PlayAnimation stand-in so clips can LAYER):
 *  - base layer: one clip picked by AKGCharacter (idle/walk/sprint/crouch/fall, attack, death, seats) with a short
 *    crossfade between clips,
 *  - emote layer (UKGEmoteComponent): full body, or upper body only (wave while walking), with blend in/out and an
 *    optional intro clip (sit down -> sitting loop).
 * Everything is driven from the game thread; the proxy only samples + blends (see FKGBodyAnimInstanceProxy).
 */
UCLASS(Transient, NotBlueprintable)
class KILLGODOT_API UKGBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Base clip. The same looping clip again is a no-op; a one-shot restarts. */
	void PlayBase(UAnimSequence* Anim, bool bLoop, float BlendTime = 0.18f);
	UAnimSequence* GetBase() const { return Base; }

	/** Intro (optional, played once) then Main (looping or held on its last frame until StopEmote). */
	void PlayEmote(UAnimSequence* Intro, UAnimSequence* Main, bool bLoop, bool bUpperBody, float BlendIn, float PlayRate = 1.0f);
	void StopEmote(float BlendOut);
	/** The emote layer has a clip and has not fully blended out yet. */
	bool IsEmoteActive() const { return EmoteMain != nullptr; }
	/** True from PlayEmote until StopEmote (the blend-out may still be running afterwards). */
	bool IsEmoteWanted() const { return EmoteMain != nullptr && EmoteTarget > 0.0f; }
	UAnimSequence* GetEmoteMain() const { return EmoteMain; }
	float GetEmoteWeight() const { return EmoteWeight; }
	float GetEmoteTime() const { return EmoteTime; }
	bool IsEmoteUpperBody() const { return bEmoteUpper; }

	/** The pose inputs for this frame (the proxy copies it before evaluating; tests read it too). */
	FKGBodyAnimSnapshot MakeSnapshot() const;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	UPROPERTY() TObjectPtr<UAnimSequence> Base;
	UPROPERTY() TObjectPtr<UAnimSequence> Prev;
	UPROPERTY() TObjectPtr<UAnimSequence> EmoteIntro;
	UPROPERTY() TObjectPtr<UAnimSequence> EmoteMain;

	float BaseTime = 0.0f;
	bool bBaseLoop = true;
	float PrevTime = 0.0f;
	bool bPrevLoop = true;
	float BaseAlpha = 1.0f;
	float BaseBlendTime = 0.18f;

	float EmoteTime = 0.0f;
	float EmoteRate = 1.0f;
	bool bEmoteLoop = false;
	bool bEmoteUpper = false;
	float EmoteWeight = 0.0f;
	float EmoteTarget = 0.0f;
	float EmoteBlendIn = 0.25f;
	float EmoteBlendOut = 0.3f;
};
