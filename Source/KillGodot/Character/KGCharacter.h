#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/KGStamina.h"
#include "KGCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UKGHealthComponent;
class UKGMouthComponent;
class UKGSnapshotComponent;
class UKGViewmodelComponent;
class UKGAppearanceComponent;
class UKGEmoteComponent;
class UKGChoreComponent;
class UKGBodyAnimInstance;
class UAnimSequence;
class UPhysicsHandleComponent;
class UStaticMesh;
class UStaticMeshComponent;
class AKGTaskStation;
class AKGLadder;
class UPostProcessComponent;
struct FInputActionValue;

/**
 * First-person puppet villager. Owner sees only the arms/viewmodel (native First Person Rendering, no wall
 * clipping); everyone else sees the full puppet body with its voice-driven jaw.
 *
 * Networking (M3): input plays cosmetic feedback locally at once, gameplay runs on the server through the
 * Server* RPCs (validated: cooldown, stamina, view origin), other clients get MulticastSwing for the body.
 * The public actions are BlueprintCallable so automation and bots drive exactly the player code path.
 */
UCLASS()
class KILLGODOT_API AKGCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AKGCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Emotes pull the owner's view out to a third-person camera (UKGEmoteComponent::ApplyCamera). */
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
	/** SPRINT-026: landing feel (viewmodel dip + sound) and stamina/footstep bookkeeping. */
	virtual void Landed(const FHitResult& Hit) override;
	/** SPRINT-026: a grounded jump actually happened (engine calls this once per successful DoJump on both the
	 *  predicting client and the server) - charges the base jump stamina cost and kicks the viewmodel. */
	virtual void OnJumped_Implementation() override;

	/** Player actions (input, bots and automation all call these). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void Attack();
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void Shove();
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void Interact();
	/** Draw/sheathe the Impatient blade (B). Server-validated: only Impatient roles once roles are dealt. */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void ToggleDevBlade();
	UFUNCTION(BlueprintPure, Category = "KillGodot|Actions") bool CanDrawBlade() const;
	/** Meeting: accuse whoever is under the crosshair (V). Trial: vote guilty (Y) / innocent (N). */
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void Accuse();
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void AccusePlayer(APlayerState* Target);
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void VoteGuilty();
	UFUNCTION(BlueprintCallable, Category = "KillGodot|Actions") void VoteInnocent();
	/** Authority: start working on a chore (from E on a station, or a bot). */
	void BeginTask(AKGTaskStation* Station);
	AKGTaskStation* GetActiveTask() const { return ActiveTask; }
	float GetTaskProgress() const { return TaskProgress; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	UKGViewmodelComponent* GetViewmodel() const { return Viewmodel; }
	/** SPRINT-027a: per-player villager look, owner-only role cuff, public revealed-role sash. */
	UKGAppearanceComponent* GetAppearance() const { return Appearance; }
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	UKGHealthComponent* GetHealth() const { return Health; }
	/** Owner HUD: seconds left on the crosshair hit marker (0 = none) and whether that hit killed. */
	float GetHitMarkerTime() const { return HitMarkerTime; }
	bool WasHitMarkerKill() const { return bHitMarkerKill; }

	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	UKGEmoteComponent* GetEmote() const { return Emote; }

	/** SPRINT-023: the talking mouth (voice amplitude / barks). */
	UKGMouthComponent* GetMouth() const { return Mouth; }

	/** Chore minigames (server-validated sessions + the owner's panel). */
	UKGChoreComponent* GetChores() const { return Chores; }

	/** The body's layered anim instance (base clip + emote layer); null if something forced single-node mode. */
	UKGBodyAnimInstance* GetBodyAnim() const;
	/** Plays a body clip outside the locomotion picker (seats): the base layer, blended. */
	void PlayBodyClip(UAnimSequence* Anim, bool bLoop, float BlendTime = 0.18f);
	/** The base clip the body plays right now. */
	UAnimSequence* GetBodyClip() const;
	/** First-person emote gesture (wave/point/clap/salute) on the arms rig; the held item hides while it plays. */
	void PlayArmsGesture(UAnimSequence* Clip);

	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	float GetStaminaAlpha() const { return Stamina.GetAlpha(); }

	/** SPRINT-026 dev panel / HUD readout: current horizontal speed in cm/s (kg.Debug.Speed). */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	float GetHorizontalSpeed() const { return GetVelocity().Size2D(); }

	/** SPRINT-026 dev panel / HUD readout: consecutive landing-buffer chain hops since the streak last reset. */
	UFUNCTION(BlueprintPure, Category = "KillGodot|Character")
	int32 GetHopChainStreak() const { return ChainHopStreak; }

	/**
	 * TF2-spy style backstab test, pure geometry (unit-tested):
	 *  - within MaxDistance (2D),
	 *  - the target is in front of the attacker,
	 *  - the attacker is behind the target (target faces away),
	 *  - both look roughly the same way (no side/face stabs).
	 */
	static bool IsBackstabGeometry(const FVector& AttackerLocation, const FVector& AttackerForward,
	                               const FVector& TargetLocation, const FVector& TargetForward, float MaxDistance);

	/** Closest living pawn whose back is exposed to us right now, or nullptr. */
	AKGCharacter* FindBackstabTarget() const;
	AKGCharacter* FindBackstabTarget(const FVector& ViewForward) const;

	/** Set by the role/weapon system (server): true while holding an Impatient blade (knife, clock hand...). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Combat")
	bool bHoldingAssassinBlade = false;

	/** Owner-only mirror of the server-side physics grab (attack throws, interact drops). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "KillGodot|Physics")
	bool bHoldingObject = false;

	UPROPERTY(EditAnywhere, Category = "KillGodot|Combat")
	float BackstabRange = 150.0f;

protected:
	/** Builds IMC + actions at runtime when no assets are assigned. */
	void CreateDefaultInput();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpStart();
	void JumpEnd();
	void StartSprint();
	void StopSprint();
	void ToggleCrouch();
	void Inspect();

	UFUNCTION(Server, Reliable) void ServerAttack(FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir);
	UFUNCTION(Server, Reliable) void ServerShove(FVector_NetQuantizeNormal ViewDir);
	UFUNCTION(Server, Reliable) void ServerInteract(FVector_NetQuantize10 ViewStart, FVector_NetQuantizeNormal ViewDir);
	UFUNCTION(Server, Reliable) void ServerSetSprint(bool bSprint);
	UFUNCTION(Server, Reliable) void ServerSetAssassinBlade(bool bHolding);
	UFUNCTION(Server, Reliable) void ServerAccuse(APlayerState* Target);
	/** Carry (hold E): release drops, R turns the held object 45 degrees. */
	UFUNCTION(Server, Reliable) void ServerReleaseHeld();
	UFUNCTION(Server, Reliable) void ServerRotateHeld();
	void InteractReleased();
	void RotateHeld();
	/** Yaw offset applied to the held object relative to the view (server). */
	float HeldYaw = 0.0f;
	/** Held item's hand-relative rotation and long axis at placement (inspect twirls around that axis). */
	FQuat HeldItemBaseRel = FQuat::Identity;
	FVector HeldItemAxis = FVector::ForwardVector;
	UFUNCTION(Server, Reliable) void ServerVerdict(bool bGuilty);
	/** Chore currently being worked on (server authoritative, progress replicated to the owner for the HUD). */
	UPROPERTY(Replicated)
	TObjectPtr<AKGTaskStation> ActiveTask;

	UPROPERTY(Replicated)
	float TaskProgress = 0.0f;

	void TickTask(float DeltaSeconds);
	/** Swimming (wave surface) + ladders + the underwater look. */
	void TickWaterAndLadders(float DeltaSeconds);
	class APhysicsVolume* GetSeaVolume() const;
	bool bSwimUp = false;
	TWeakObjectPtr<AKGLadder> Ladder;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> UnderwaterPP;
	void UpdateRevealGlow();

	/** After death: the player's controller becomes a free-flying ghost spectator. */
	void BecomeGhost();
	UFUNCTION(NetMulticast, Unreliable) void MulticastSwing(bool bBackstab);
	/** Non-lethal hit: everyone sees the body flinch (A_KG_Hit_Chest / A_KG_Hit_Head) and hears the thud. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastHitReact(FVector_NetQuantize From);
	/** To the attacker only: their hit landed (crosshair hit marker; red when it killed). */
	UFUNCTION(Client, Unreliable) void ClientHitConfirm(bool bKilled);

	/** Server: the camera origin a client claims must be near where we think its head is. */
	FVector ValidatedViewStart(const FVector& Claimed) const;
	void PlaySwingCosmetics(bool bBackstab);

	bool TryGrab(UPrimitiveComponent* Component, const FVector& GrabPoint);
	void Release(bool bThrow);
	bool TraceView(float Distance, FHitResult& OutHit) const;
	bool TraceFrom(const FVector& Start, const FVector& Dir, float Distance, FHitResult& OutHit) const;

	UFUNCTION()
	void HandleDeath(AActor* Killer, FName DamageType);

	/** Third-person body: picks a clip from movement state (M2 stand-in until the AnimBP in M5). */
	void UpdateBodyAnimation(float DeltaSeconds);
	void PlayBodyAnim(UAnimSequence* Anim, bool bLoop);

	/** SPRINT-026 (-KGMoveSmoke, dev builds only): scripted bad-strafe-then-good-strafe run, logging KG_MOVE_CSV
	 *  speed samples and a KG_MOVE_DONE summary with the CMC's correction count (Tools/Unreal/kg_move_smoke.ps1). */
	void TickMoveSmoke(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Everything first-person hangs off this pivot; the viewmodel component animates it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ViewmodelPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	/** Melee item in the first-person hand (frying pan by default). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeldItem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGViewmodelComponent> Viewmodel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGAppearanceComponent> Appearance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGMouthComponent> Mouth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGEmoteComponent> Emote;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGChoreComponent> Chores;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKGSnapshotComponent> Snapshot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> AttackAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> ShoveAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> InspectAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> DevBladeAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> AccuseAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> RotateHeldAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> GuiltyAction;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> InnocentAction;

	/** Docs/01_GDD_Core.md §4: walk 3.2 m/s, sprint 5.8 m/s. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float WalkSpeed = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintSpeed = 580.0f;

	UPROPERTY(EditAnywhere, SaveGame, Category = "Movement")
	FKGStamina Stamina;

	/** SPRINT-026: stamina cost of a plain grounded jump. Spent if available; a jump is never blocked for lack of
	 *  stamina (only the skill-chain re-hop is - see ChainHopBaseCost / FKGStamina::bExhausted). */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float JumpStaminaCost = 6.0f;

	/** SPRINT-026: stamina cost of a landing-buffer chain hop, before the streak surcharge. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float ChainHopBaseCost = 8.0f;

	/** SPRINT-026: extra stamina per consecutive chained hop (streak 1, 2, 3...), so a long bhop run gets
	 *  progressively more expensive. Capped at ChainHopStreakCostCap consecutive steps. */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float ChainHopStepCost = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	int32 ChainHopStreakCostCap = 5;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeDamage = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeRange = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeCooldown = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ShoveStaminaCost = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ShoveImpulse = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Physics")
	float GrabDistance = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Physics")
	float HoldDistance = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Physics")
	float MaxGrabMassKg = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Physics")
	float ThrowSpeed = 1100.0f;

	/** Default melee tool and the Impatient blade (umbrella-sword skin) shown while bHoldingAssassinBlade. */
	UPROPERTY(EditAnywhere, Category = "Combat") TObjectPtr<UStaticMesh> ToolMesh;
	UPROPERTY(EditAnywhere, Category = "Combat") TObjectPtr<UStaticMesh> BladeMesh;
	/** Per-item grip: which end of the mesh's Z extent is the handle, and first-person scale. */
	UPROPERTY(EditAnywhere, Category = "Combat") bool bToolGripAtMaxZ = false;
	UPROPERTY(EditAnywhere, Category = "Combat") float ToolScale = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Combat") bool bBladeGripAtMaxZ = true;
	UPROPERTY(EditAnywhere, Category = "Combat") float BladeScale = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimIdle;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimWalk;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimSprint;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimCrouchIdle;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimCrouchMove;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimFalling;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimAttack;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimDeath;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimHitChest;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimHitHead;
	/** First-person arms: the villager mesh in its "holding a weapon" pose, head and legs hidden. */
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsIdle;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsAttack;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsAttack2;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsDraw;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsShove;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsGrab;
	/** Empty-handed villager (no blade out): relaxed open hand, jab to punch. */
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsEmptyIdle;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimSequence> AnimArmsPunch;

private:
	UPROPERTY()
	TObjectPtr<UAnimSequence> CurrentBodyAnim;

	/** Places HeldItem in camera space once the arms pose is evaluated (bone axes are asset-dependent). */
	void PlaceHeldItemInHand();
	int32 HeldItemPlacementFrames = 3;
	bool bShowingBlade = false;

	float BodyOneShotRemaining = 0.0f;
	float ArmsOneShotRemaining = 0.0f;
	float HitMarkerTime = 0.0f;
	bool bHitMarkerKill = false;
	int32 AttackAlternate = 0;
	void PlayArmsOneShot(UAnimSequence* Anim);
	UAnimSequence* CurrentArmsIdle() const { return (bShowingBlade || !AnimArmsEmptyIdle) ? AnimArmsIdle : AnimArmsEmptyIdle; }
	/** Bone names differ per FP rig; resolved once in BeginPlay. */
	FName RightHandBone;
	FName RightFingerBone;
	bool bWantsSprint = false;
	float AttackCooldownRemaining = 0.0f;
	float ServerAttackCooldown = 0.0f;

	/** SPRINT-026: cosmetic mirrors of the CMC's predicted hop state (HopCounter drives the viewmodel kick, the streak
	 *  is the dev-panel readout); the gameplay state itself lives in UKGCharacterMovement's saved moves. */
	int32 LastSeenHopCounter = 0;
	int32 ChainHopStreak = 0;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> HeldComponent;
};
