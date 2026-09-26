#include "Character/KGCharacterMovement.h"
#include "Character/KGStamina.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "World/KGWaves.h"
#include "KillGodot.h"

UKGCharacterMovement::UKGCharacterMovement()
{
	// Float with the head out of the water; swim a bit slower than walking.
	Buoyancy = 1.35f;   // head and shoulders above the waves
	MaxSwimSpeed = 330.0f;
	GetNavAgentPropertiesRef().bCanSwim = true;
	// Corrections carry the predicted stamina/hop state (SPRINT-026 stabilisation).
	SetMoveResponseDataContainer(KGMoveResponseData);
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
                                                           float SoftCapSpeed, float HardCapSpeed)
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

	// Soft cap: past SoftCapSpeed the gain tapers quadratically across the band up to the hard cap (factor 1 at the
	// soft cap - continuous, no step - and 0 at the hard cap), down to a small floor that keeps air steering alive.
	// Without a hard cap the band is SoftCapSpeed wide (legacy behaviour for callers that pass none).
	constexpr float SteerFloor = 0.08f;
	const float CurrentSpeed = CurrentVel2D.Size();
	if (SoftCapSpeed > 0.0f && CurrentSpeed > SoftCapSpeed)
	{
		const float Band = HardCapSpeed > SoftCapSpeed ? HardCapSpeed - SoftCapSpeed : SoftCapSpeed;
		const float T = FMath::Clamp((CurrentSpeed - SoftCapSpeed) / Band, 0.0f, 1.0f);
		AccelSpeed *= FMath::Max(FMath::Square(1.0f - T), SteerFloor);
	}

	FVector2D Out = CurrentVel2D + WishDir2D * AccelSpeed;

	// Hard ceiling: a strafe never pushes horizontal speed past HardCapSpeed (nor past the current speed if something
	// else - a launch, a slope - already put us above it); it can still turn the velocity there.
	if (HardCapSpeed > 0.0f)
	{
		const float Limit = FMath::Max(HardCapSpeed, CurrentSpeed);
		if (Out.SizeSquared() > FMath::Square(Limit))
		{
			Out = Out.GetSafeNormal() * Limit;
		}
	}
	return Out;
}

bool UKGCharacterMovement::IsWithinJumpBuffer(float TimeSinceJumpPressedIn, float BufferSeconds)
{
	return TimeSinceJumpPressedIn >= 0.0f && TimeSinceJumpPressedIn <= BufferSeconds;
}

float UKGCharacterMovement::NextChainHopCost() const
{
	return ChainHopBaseCost + ChainHopStepCost * FMath::Min(HopStreak + 1, ChainHopStreakCostCap);
}

bool UKGCharacterMovement::IsHopChainBlocked() const
{
	return Stamina && (Stamina->bExhausted || Stamina->Current < NextChainHopCost());
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
	// Context (blade) from AKGCharacter, exhaustion from the predicted stamina: both collapse the caps to sprint speed.
	const float Gain = (Stamina && Stamina->bExhausted) ? 0.0f : FMath::Clamp(HopGainScale, 0.0f, 1.0f);
	const float SoftCap = FMath::Lerp(SprintSpeedForCap, SprintSpeedForCap * BunnyHopSoftCapMultiplier, Gain);
	const float HardCap = FMath::Lerp(SprintSpeedForCap, SprintSpeedForCap * BunnyHopHardCapMultiplier, Gain);
	const FVector2D NewVel2D = ComputeAirStrafeVelocity2D(CurrentVel2D, WishDir2D, AirWishSpeed, AirAccelerate,
	                                                      DeltaTime, SoftCap, HardCap);
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

bool UKGCharacterMovement::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (!Super::DoJump(bReplayingMoves, DeltaTime))
	{
		return false;
	}
	// Predicted charge: DoJump runs from CheckJumpInput on the client's live move, inside MoveAutonomous on the server
	// and again on a replay - which starts from the server's corrected stamina, so nothing is charged twice. A plain
	// jump is never blocked for lack of stamina (TrySpend simply fails).
	if (Stamina)
	{
		Stamina->TrySpend(JumpStaminaCost);
	}
	return true;
}

bool UKGCharacterMovement::ClientUpdatePositionAfterServerUpdate()
{
	// Replaying saved moves rewrites bWantsToSprint from each move's flags; keep the live input (the engine does the
	// same for bWantsToCrouch / bPressedJump).
	const bool bRealWantsToSprint = bWantsToSprint;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealWantsToSprint;
	return bResult;
}

void UKGCharacterMovement::PerformMovement(float DeltaTime)
{
	bInSimulatedMove = true;
	Super::PerformMovement(DeltaTime);
	bInSimulatedMove = false;
}

void UKGCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
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

	// Predicted stamina: sprint drain / regen advance by the MOVE's delta time from the move's own sprint flag, so
	// the server (replaying the client's moves) and the client agree move for move; the sprint speed follows.
	if (Stamina)
	{
		const bool bMoving = Velocity.SizeSquared2D() > 100.0f;
		bIsSprinting = Stamina->Tick(DeltaSeconds, bWantsToSprint && bSprintAllowed && !IsCrouching(), bMoving);
		MaxWalkSpeed = bIsSprinting ? SprintSpeedForCap : WalkSpeedForMove;
	}
}

void UKGCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	// Only a landing simulated inside a move counts: a movement mode applied by a server correction (or by simulated-
	// proxy smoothing) must never charge stamina or re-hop.
	if (!bInSimulatedMove || PreviousMovementMode != MOVE_Falling || MovementMode != MOVE_Walking || !CharacterOwner)
	{
		return;
	}

	// Just landed. A fresh jump press seen within JumpBufferSeconds re-launches immediately (the skill-chain hop);
	// otherwise we fall through to normal ground friction this same frame - no residual "slide" past the buffer.
	// Requires a fresh press (not a held key: TimeSinceJumpPressed only resets on the edge in
	// UpdateCharacterStateBeforeMovement above). A plain grounded jump is never blocked by stamina.
	if (bAirStrafeDisabled || !IsWithinJumpBuffer(TimeSinceJumpPressed, JumpBufferSeconds))
	{
		HopStreak = 0;
		return;
	}
	if (IsHopChainBlocked())
	{
		// Stamina exhaustion stops chaining: trying to chain without the stamina for it empties the bar and
		// exhausts it (no chaining until it recovers past RecoverThreshold).
		if (Stamina && !Stamina->bExhausted)
		{
			Stamina->Current = 0.0f;
			Stamina->bExhausted = true;
			Stamina->RegenCooldown = Stamina->RegenDelay;
		}
		TimeSinceJumpPressed = 1000.0f;
		HopStreak = 0;
		return;
	}

	if (Stamina)
	{
		Stamina->TrySpend(NextChainHopCost());
	}
	++HopStreak;
	TimeSinceJumpPressed = 1000.0f;   // consume: holding the key cannot re-buffer without a new press edge
	Velocity.Z = FMath::Max<FVector::FReal>(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
	if (!CharacterOwner->bClientUpdating)
	{
		++HopCounter;   // cosmetic (viewmodel kick), never on a replay
	}
}

void UKGCharacterMovement::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
                                                       FVector NewLocation, FVector NewVelocity,
                                                       FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
                                                       FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
                                                       uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	++CorrectionCount;
	UE_LOG(LogKillGodot, Log, TEXT("KG_MOVE_CORRECTION count=%d ts=%.3f loc=%s vel=%.0f"), CorrectionCount, TimeStamp,
	       *NewLocation.ToCompactString(), NewVelocity.Size2D());
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewMovementBaseInterfaceData,
	                                  NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode,
	                                  ServerGravityDirection);

	// Adopt the server's predicted state as of the corrected move; the unacknowledged moves are then replayed on top
	// of it (ClientUpdatePositionAfterServerUpdate), each charging exactly what it charged on the server.
	const FKGMoveResponseDataContainer& Response = static_cast<const FKGMoveResponseDataContainer&>(GetMoveResponseDataContainer());
	if (Response.bHasKGState && Response.IsCorrection())
	{
		if (Stamina)
		{
			Stamina->Current = Response.StaminaCurrent;
			Stamina->RegenCooldown = Response.StaminaRegenCooldown;
			Stamina->bExhausted = Response.bStaminaExhausted;
		}
		TimeSinceJumpPressed = Response.TimeSinceJumpPressed;
		HopStreak = Response.HopStreak;
	}
}

// --- Predicted-state plumbing: saved move, prediction data, correction payload -----------------------------------

namespace KGMovePrivate
{
	/** Start-of-move snapshot of the predicted state (restored when a pending move is combined and re-simulated). */
	struct FPredictedState
	{
		float StaminaCurrent = 0.0f;
		float StaminaRegenCooldown = 0.0f;
		bool bStaminaExhausted = false;
		float TimeSinceJumpPressed = 1000.0f;
		int32 HopStreak = 0;

		void Capture(const UKGCharacterMovement& Move)
		{
			if (Move.Stamina)
			{
				StaminaCurrent = Move.Stamina->Current;
				StaminaRegenCooldown = Move.Stamina->RegenCooldown;
				bStaminaExhausted = Move.Stamina->bExhausted;
			}
			TimeSinceJumpPressed = Move.GetTimeSinceJumpPressed();
			HopStreak = Move.HopStreak;
		}

		void Restore(UKGCharacterMovement& Move) const
		{
			if (Move.Stamina)
			{
				Move.Stamina->Current = StaminaCurrent;
				Move.Stamina->RegenCooldown = StaminaRegenCooldown;
				Move.Stamina->bExhausted = bStaminaExhausted;
			}
			Move.SetTimeSinceJumpPressed(TimeSinceJumpPressed);
			Move.HopStreak = HopStreak;
		}
	};

	class FKGSavedMove : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		bool bSavedWantsToSprint = false;
		FPredictedState Start;

		virtual void Clear() override
		{
			Super::Clear();
			bSavedWantsToSprint = false;
			Start = FPredictedState();
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Result = Super::GetCompressedFlags();
			if (bSavedWantsToSprint)
			{
				Result |= FLAG_Custom_0;
			}
			return Result;
		}

		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
		                        FNetworkPredictionData_Client_Character& ClientData) override
		{
			Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);   // calls SetInitialPosition (captures Start)
			if (const UKGCharacterMovement* Move = Cast<UKGCharacterMovement>(C->GetCharacterMovement()))
			{
				bSavedWantsToSprint = Move->bWantsToSprint;
			}
		}

		virtual void SetInitialPosition(ACharacter* C) override
		{
			Super::SetInitialPosition(C);
			if (const UKGCharacterMovement* Move = Cast<UKGCharacterMovement>(C->GetCharacterMovement()))
			{
				Start.Capture(*Move);
			}
		}

		virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* C, APlayerController* PC,
		                         const FVector& OldStartLocation) override
		{
			// The combined move is re-simulated from the pending move's start, so the predicted state goes back there
			// too (SetInitialPosition then re-captures it); otherwise the pending move's charges would apply twice.
			Super::CombineWith(OldMove, C, PC, OldStartLocation);
			if (UKGCharacterMovement* Move = Cast<UKGCharacterMovement>(C->GetCharacterMovement()))
			{
				static_cast<const FKGSavedMove*>(OldMove)->Start.Restore(*Move);
			}
		}

		virtual void PrepMoveFor(ACharacter* C) override
		{
			Super::PrepMoveFor(C);
			if (UKGCharacterMovement* Move = Cast<UKGCharacterMovement>(C->GetCharacterMovement()))
			{
				Move->bWantsToSprint = bSavedWantsToSprint;
			}
		}
	};

	class FKGNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
	{
	public:
		explicit FKGNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
			: FNetworkPredictionData_Client_Character(ClientMovement)
		{
		}

		virtual FSavedMovePtr AllocateNewMove() override
		{
			return FSavedMovePtr(new FKGSavedMove());
		}
	};
}

FNetworkPredictionData_Client* UKGCharacterMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UKGCharacterMovement* MutableThis = const_cast<UKGCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new KGMovePrivate::FKGNetworkPredictionData_Client(*this);
	}
	return ClientPredictionData;
}

void FKGMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement,
                                                          const FClientAdjustment& PendingAdjustment)
{
	FCharacterMoveResponseDataContainer::ServerFillResponseData(CharacterMovement, PendingAdjustment);
	const UKGCharacterMovement* Move = Cast<UKGCharacterMovement>(&CharacterMovement);
	bHasKGState = Move != nullptr;
	if (Move)
	{
		KGMovePrivate::FPredictedState State;
		State.Capture(*Move);
		StaminaCurrent = State.StaminaCurrent;
		StaminaRegenCooldown = State.StaminaRegenCooldown;
		bStaminaExhausted = State.bStaminaExhausted;
		TimeSinceJumpPressed = State.TimeSinceJumpPressed;
		HopStreak = State.HopStreak;
	}
}

bool FKGMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap)
{
	if (!FCharacterMoveResponseDataContainer::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}
	if (IsCorrection())   // good-move acks stay as small as stock
	{
		Ar.SerializeBits(&bHasKGState, 1);
		if (bHasKGState)
		{
			Ar << StaminaCurrent;
			Ar << StaminaRegenCooldown;
			Ar.SerializeBits(&bStaminaExhausted, 1);
			Ar << TimeSinceJumpPressed;
			Ar << HopStreak;
		}
	}
	return !Ar.IsError();
}
