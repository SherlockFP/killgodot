#include "Character/KGCharacterMovement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "World/KGWaves.h"
#include "KillGodot.h"

UKGCharacterMovement::UKGCharacterMovement()
{
	// Float with the head out of the water; swim a bit slower than walking.
	Buoyancy = 1.35f;   // head and shoulders above the waves
	MaxSwimSpeed = 330.0f;
	GetNavAgentPropertiesRef().bCanSwim = true;
}

bool UKGCharacterMovement::IsInWater() const
{
	if (!UpdatedComponent || !GetWorld())
	{
		return Super::IsInWater();
	}
	const FVector P = UpdatedComponent->GetComponentLocation();
	const float Surface = FKGWaves::HeightAt(P.X, P.Y, GetWorld()->GetTimeSeconds());
	// Wave-based only (the character switches its physics volume from this answer; asking the volume back would
	// latch "in water" forever).
	return P.Z < Surface + SwimStartOffset;
}

float UKGCharacterMovement::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * DevSpeedScale;
}

float UKGCharacterMovement::ImmersionDepth() const
{
	if (!UpdatedComponent || !CharacterOwner || !GetWorld())
	{
		return Super::ImmersionDepth();
	}
	const FVector P = UpdatedComponent->GetComponentLocation();
	const float Half = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Surface = FKGWaves::HeightAt(P.X, P.Y, GetWorld()->GetTimeSeconds());
	return FMath::Clamp((Surface - (P.Z - Half)) / (2.0f * Half), 0.0f, 1.0f);
}

// --- SPRINT-026: air strafing / skill bunny-hop --------------------------------------------------------------

FVector2D UKGCharacterMovement::ComputeAirStrafeVelocity2D(const FVector2D& CurrentVel2D, FVector2D WishDir2D,
                                                           float WishSpeed, float Accelerate, float DeltaTime,
                                                           float SoftCapSpeed)
{
	if (!WishDir2D.IsNearlyZero())
	{
		WishDir2D.Normalize();
	}
	else
	{
		return CurrentVel2D;
	}

	// Quake PM_AirAccelerate: only the component of velocity ALREADY along the wish direction counts against the
	// target, so re-aiming the wish direction (mouse turn + opposite strafe key, classic strafe-jump) keeps adding
	// speed tick after tick; holding one direction plateaus at WishSpeed. This is what makes "speed gain only from
	// correctly synced strafes" true: a bad strafe (wish direction fighting velocity, or none at all) adds nothing.
	const float CurrentSpeedAlongWish = FVector2D::DotProduct(CurrentVel2D, WishDir2D);
	const float AddSpeed = WishSpeed - CurrentSpeedAlongWish;
	if (AddSpeed <= 0.0f)
	{
		return CurrentVel2D;
	}

	float AccelSpeed = FMath::Min(Accelerate * WishSpeed * DeltaTime, AddSpeed);

	// Soft cap: past SoftCapSpeed, gains taper off (never hard-clamped) so only sustained good strafing reaches it.
	const float CurrentSpeed = CurrentVel2D.Size();
	if (SoftCapSpeed > 0.0f && CurrentSpeed > SoftCapSpeed)
	{
		const float Over = (CurrentSpeed - SoftCapSpeed) / SoftCapSpeed;
		AccelSpeed *= FMath::Clamp(1.0f - Over, 0.08f, 1.0f);
	}

	return CurrentVel2D + WishDir2D * AccelSpeed;
}

bool UKGCharacterMovement::IsWithinJumpBuffer(float TimeSinceJumpPressedIn, float BufferSeconds)
{
	return TimeSinceJumpPressedIn >= 0.0f && TimeSinceJumpPressedIn <= BufferSeconds;
}

void UKGCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (MovementMode != MOVE_Falling || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || bAirStrafeDisabled)
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	// PhysFalling calls us with Acceleration temporarily swapped for GetFallingLateralAcceleration() (direction =
	// the raw wish direction from input; magnitude already scaled by the engine's own AirControl, which we ignore -
	// only the direction matters here, our own AirAccelerate/AirWishSpeed drive the magnitude).
	const FVector2D WishDir2D(Acceleration.X, Acceleration.Y);
	const FVector2D CurrentVel2D(Velocity.X, Velocity.Y);
	const float SoftCap = SprintSpeedForCap * BunnyHopSoftCapMultiplier;
	const float EffectiveSoftCap = FMath::Lerp(SprintSpeedForCap, SoftCap, FMath::Clamp(HopGainScale, 0.0f, 1.0f));
	const FVector2D NewVel2D = ComputeAirStrafeVelocity2D(CurrentVel2D, WishDir2D, AirWishSpeed, AirAccelerate,
	                                                      DeltaTime, EffectiveSoftCap);
	Velocity.X = NewVel2D.X;
	Velocity.Y = NewVel2D.Y;
}

bool UKGCharacterMovement::CanAttemptJump() const
{
	// Unchanged grounded-jump gating (a single fresh press already only fires once: JumpMaxHoldTime is 0, so
	// CharacterOwner->bPressedJump is cleared by the engine every move regardless of whether the key is still held -
	// see ClearJumpInput). The landing-buffer re-hop below is the only place we add new behaviour.
	return Super::CanAttemptJump();
}

void UKGCharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (!CharacterOwner)
	{
		return;
	}
	// CharacterOwner->bPressedJump is still valid here: ClearJumpInput (which zeroes it) runs later in
	// PerformMovement, after physics. So "true" here means "a fresh Jump press was queued for this move" (edge-
	// pulsed by the engine itself, see KGCharacterMovement.h), regardless of whether CheckJumpInput's grounded-jump
	// path actually consumed it this move (e.g. because we are airborne). Feeds the landing buffer below.
	if (CharacterOwner->bPressedJump)
	{
		TimeSinceJumpPressed = 0.0f;
	}
	else if (TimeSinceJumpPressed < 1000.0f)
	{
		TimeSinceJumpPressed += DeltaSeconds;
	}
}

void UKGCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (PreviousMovementMode != MOVE_Falling || MovementMode != MOVE_Walking || !CharacterOwner)
	{
		return;
	}

	// Just landed. A fresh jump press seen within JumpBufferSeconds re-launches immediately (the skill-chain hop);
	// otherwise we fall through to normal ground friction this same frame - no residual "slide" past the buffer.
	// Requires a fresh press (not a held key: TimeSinceJumpPressed only resets on the edge in
	// UpdateCharacterStateBeforeMovement above) and is skipped outright while stamina-exhausted (bHopChainBlocked,
	// set every tick by AKGCharacter from FKGStamina) - "stamina exhaustion stops chaining" without ever blocking a
	// plain grounded jump.
	if (bHopChainBlocked || bAirStrafeDisabled || !IsWithinJumpBuffer(TimeSinceJumpPressed, JumpBufferSeconds))
	{
		return;
	}

	TimeSinceJumpPressed = 1000.0f;   // consume: holding the key cannot re-buffer without a new press edge
	Velocity.Z = FMath::Max<FVector::FReal>(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
	++HopCounter;
	bLastHopWasChain = true;
}

void UKGCharacterMovement::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
                                                       FVector NewLocation, FVector NewVelocity,
                                                       FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
                                                       FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
                                                       uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	++CorrectionCount;
	UE_LOG(LogKillGodot, Verbose, TEXT("KG_MOVE_CORRECTION count=%d ts=%.3f loc=%s"), CorrectionCount, TimeStamp,
	       *NewLocation.ToCompactString());
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewMovementBaseInterfaceData,
	                                  NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode,
	                                  ServerGravityDirection);
}
