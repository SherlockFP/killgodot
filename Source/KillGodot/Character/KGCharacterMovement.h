#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KGCharacterMovement.generated.h"

/**
 * Swimming without water volumes: "in water" means the capsule centre is below the wave surface (FKGWaves), and
 * immersion (which drives buoyancy) is measured against the same moving surface, so players bob with the waves.
 * Wading in shallows stays walking; from ~1.2 m depth the character swims.
 *
 * SPRINT-026: Source-style air strafing / skill bunny-hop. No auto-hop: DoJump() only ever fires from
 * ACharacter::CheckJumpInput's normal grounded path (unchanged engine gating), or from our own OnMovementModeChanged
 * landing hook below, and that hook only fires for a *fresh* jump press seen within JumpBufferSeconds of landing
 * (holding the key down does not keep re-triggering it: CharacterOwner->bPressedJump is engine-cleared every move
 * once JumpMaxHoldTime is 0, so "held" cannot be told apart from "fresh" except by our own edge timer below, which is
 * exactly the point). All of it runs inside CalcVelocity/OnMovementModeChanged/UpdateCharacterStateBeforeMovement -
 * the same virtuals stock CMC prediction replays move-for-move, so no new compressed flags are needed and replays
 * reproduce it exactly (no corrections from this system alone).
 */
UCLASS()
class KILLGODOT_API UKGCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UKGCharacterMovement();

	virtual bool IsInWater() const override;
	virtual float ImmersionDepth() const override;
	virtual float GetMaxSpeed() const override;
	virtual bool CanAttemptJump() const override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	                                        FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData,
	                                        FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
	                                        uint8 ServerMovementMode, FVector ServerGravityDirection) override;

	/** Dev panel speed multiplier (kg.Me.Speed). Set on the server and the owning client alike (UKGDevComponent). */
	float DevSpeedScale = 1.0f;

	/** Water surface height above the capsule centre at which swimming starts (negative = centre below). */
	UPROPERTY(EditAnywhere, Category = "Swimming")
	float SwimStartOffset = -25.0f;

	// --- SPRINT-026: air strafing / skill bunny-hop --------------------------------------------------------------

	/** Quake/Source "accelerate" constant: how fast an air strafe closes the gap to AirWishSpeed (pure math, tested
	 *  by FKGAirStrafeTest). Independent of the engine's own AirControl (we bypass it - see CalcVelocity). */
	UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
	float AirAccelerate = 8.0f;

	/** Per-tick wish speed while airborne: the speed a single strafe tries to reach in the wish direction. Real
	 *  strafe-jumping exceeds this by re-aiming the wish direction every tick (the classic Quake trick). */
	UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
	float AirWishSpeed = 320.0f;

	/** Sprint speed used to derive the soft cap (set once from AKGCharacter::BeginPlay; kept out of the CMC to avoid
	 *  a Character<->Movement coupling on every tick). */
	UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
	float SprintSpeedForCap = 580.0f;

	/** Soft cap multiplier vs. sprint speed (Docs/01_GDD_Core.md movement section: ~1.35x). Above the cap, gains
	 *  taper instead of hard-clamping, so only well-timed strafes reach it. */
	UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
	float BunnyHopSoftCapMultiplier = 1.35f;

	/** Set every tick by AKGCharacter from context (stamina, blade): 1 = full soft cap (BunnyHopSoftCapMultiplier x
	 *  sprint), 0 = capped at plain sprint speed (no bhop advantage, but strafing still feels responsive). */
	float HopGainScale = 1.0f;

	/** Set every tick by AKGCharacter: true while carrying an item, fishing rod out, or doing a chore - hard-disables
	 *  the custom air-strafe accel for the tick (falls back to the engine's own small AirControl, no bhop gain at
	 *  all), rather than merely capping it, so these contexts truly give zero advantage. */
	bool bAirStrafeDisabled = false;

	/** Set every tick by AKGCharacter: true blocks the landing-buffer re-hop outright (stamina exhausted). A plain
	 *  grounded jump is never blocked by this - only the skill-chain re-hop is. */
	bool bHopChainBlocked = false;

	/** Jump-press buffer window (contract: "about 80 ms"). A press up to this long before landing still lands the
	 *  hop; holding the key past that (or past the following landing) needs a fresh press. */
	UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
	float JumpBufferSeconds = 0.08f;

	/** Incremented once per hop actually executed (grounded press, or a buffered landing re-hop). AKGCharacter polls
	 *  this each Tick to charge stamina - deliberately outside the saved-move stream (see .cpp) since stamina is a
	 *  slow-varying game value, not position-critical, and already follows this same "both sides tick it locally"
	 *  pattern for sprint. */
	int32 HopCounter = 0;
	/** True when the hop HopCounter just counted was a landing-buffer chain hop (costs more stamina) rather than a
	 *  fresh grounded jump. */
	bool bLastHopWasChain = false;
	/** Corrections received by OnClientCorrectionReceived, for the net smoke (KG_MOVE_CORRECTIONS). Client only. */
	int32 CorrectionCount = 0;

	/** Pure strafe math, unit tested directly (FKGAirStrafeTest): Quake-style air acceleration with a soft cap.
	 *  CurrentVel2D/WishDir2D/the return value are all horizontal (Z is handled by the caller). WishDir2D need not be
	 *  normalized (zero = no input, decelerates by doing nothing since AddSpeed will not exceed 0). */
	static FVector2D ComputeAirStrafeVelocity2D(const FVector2D& CurrentVel2D, FVector2D WishDir2D, float WishSpeed,
	                                            float Accelerate, float DeltaTime, float SoftCapSpeed);

	/** True if a jump press seen TimeSinceJumpPressed ago is still within the buffer window. */
	static bool IsWithinJumpBuffer(float TimeSinceJumpPressed, float BufferSeconds);

private:
	/** Seconds since CharacterOwner->bPressedJump was last seen true (a fresh press: see UpdateCharacterStateBeforeMovement).
	 *  Reset to a large value once consumed so holding the key cannot re-buffer without a new edge. */
	float TimeSinceJumpPressed = 1000.0f;
};
