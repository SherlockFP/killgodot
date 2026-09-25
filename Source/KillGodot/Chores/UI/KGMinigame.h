#pragma once

#include "CoreMinimal.h"
#include "Core/KGRng.h"
#include "InputCoreTypes.h"
#include "Layout/Geometry.h"

class FSlateWindowElementList;

/**
 * Chore minigames (Among Us style). Each minigame is plain C++ (no UObject, no widget): logic in a fixed logical
 * canvas of KGMg::W x KGMg::H units, painted through FKGMgPainter. SKGChorePanel hosts it (window, header, stage pips,
 * feedback, networking hooks) and scales the canvas to the screen. Everything is drawn in code: no textures.
 *
 * Writing a minigame (see Chores/UI/KGMinigames_*.cpp):
 *  - derive from FKGMinigame, override NumStages / GetInstruction / BeginStage / Tick / Paint and the input you need;
 *  - call Solve() when the current stage is done (the host celebrates, reports it to the server, then calls
 *    BeginStage() for the next one); Nice()/Oops() give feedback (pop text, sound, shake);
 *  - AutoPlay() drives the stage for kg.Chore.AutoWin / tests; DebugPose() freezes a readable mid-stage moment for
 *    the kg.ChoreShot screenshots;
 *  - add the id to KG_MINIGAME_LIST below and define KGMakeMinigame_<Id>().
 * Randomness only through Rng (seeded by the server: every machine could rebuild the same layout).
 */
namespace KGMg
{
	/** Logical canvas size every minigame paints in. */
	inline constexpr float W = 640.0f;
	inline constexpr float H = 400.0f;

	// Game palette (Docs/08_UI_UX.md section 1) + scene colours shared by the minigames.
	KILLGODOT_API FLinearColor C(uint32 RGB, float Alpha = 1.0f);
	KILLGODOT_API FLinearColor A(const FLinearColor& Color, float Alpha);
	KILLGODOT_API FLinearColor Mix(const FLinearColor& X, const FLinearColor& Y, float T);

	float Saturate(float X);
	float EaseOutBack(float T);
	float EaseOutCubic(float T);
	/** 0..1..0 triangle pulse with period Period. */
	float Ping(float Time, float Period);
	/** Smooth exponential approach (frame-rate independent). */
	float Approach(float Current, float Target, float Dt, float Speed);
	bool InRect(const FVector2f& P, const FVector2f& Pos, const FVector2f& Size);
	/** Distance from P to segment AB. */
	float DistToSegment(const FVector2f& P, const FVector2f& A, const FVector2f& B);

	extern KILLGODOT_API const FLinearColor Ink, Night, Panel, PanelHi, Cream, CreamDim, Muted, Gold, Lantern, Crimson,
	                                        Ghost, Good, Wood, WoodDark, WoodLight, Stone, StoneDark, Iron, Water, WaterDeep,
	                                        Grass, Soil, Sky, SkyNight, Fire, Paper;
}

/** Immediate-mode painter in the minigame's logical space. Draw order = call order (each call takes a new layer). */
class KILLGODOT_API FKGMgPainter
{
public:
	FKGMgPainter(FSlateWindowElementList& InOut, const FGeometry& InLogical, int32 InLayer, float InOpacity = 1.0f);

	int32 GetLayer() const { return Layer; }
	float GetOpacity() const { return Opacity; }

	void Rect(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Color);
	void RectV(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Top, const FLinearColor& Bottom);
	void RectH(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Left, const FLinearColor& Right);
	/** Rounded box (Radius < 0 = pill). Outline drawn inside. */
	void RoundRect(const FVector2f& Pos, const FVector2f& Size, const FLinearColor& Fill, float Radius,
	               const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);
	void Circle(const FVector2f& Centre, float Radius, const FLinearColor& Fill,
	            const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);
	/** Radial gradient ellipse. */
	void Disc(const FVector2f& Centre, const FVector2f& Radii, const FLinearColor& Inner, const FLinearColor& Outer,
	          int32 Segments = 32);
	/** Soft additive-looking glow (colour at the centre, transparent rim). */
	void Glow(const FVector2f& Centre, float Radius, const FLinearColor& Color);
	/** Anti-aliased polyline. */
	void Line(const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness = 2.0f);
	void Segment(const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness = 2.0f);
	/** Solid quad strip between A and B (for thick ropes, blades, planks). */
	void Bar(const FVector2f& A, const FVector2f& B, float Width, const FLinearColor& Color);
	void RotRect(const FVector2f& Centre, const FVector2f& Size, float AngleRad, const FLinearColor& Color);
	void Tri(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FLinearColor& Color);
	void Quad(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FVector2f& D, const FLinearColor& Color);
	/** Convex polygon. */
	void Poly(const TArray<FVector2f>& Points, const FLinearColor& Color);
	/** Outline of a circle arc (radians, 0 = +X, clockwise on screen because +Y is down). */
	void Arc(const FVector2f& Centre, float Radius, float A0, float A1, const FLinearColor& Color, float Thickness = 2.0f,
	         int32 Segments = 40);
	/** Filled ring sector between radii R0..R1. */
	void ArcBand(const FVector2f& Centre, float R0, float R1, float A0, float A1, const FLinearColor& Color, int32 Segments = 40);
	/** Text; Align 0 = left, 0.5 = centre, 1 = right (Pos.Y is the top). */
	void Text(const FVector2f& Pos, const FString& Text, float Size, const FLinearColor& Color, float Align = 0.0f,
	          FName Typeface = TEXT("Bold"), int32 Spacing = 0);
	FVector2f MeasureText(const FString& Text, float Size, FName Typeface = TEXT("Bold"), int32 Spacing = 0) const;
	/** Soft drop shadow for a box (draw before the box). */
	void Shadow(const FVector2f& Pos, const FVector2f& Size, float Radius, float Alpha = 0.35f, float Offset = 6.0f);
	/** Horizontal gauge with an optional target zone [Z0, Z1] (0..1). */
	void Gauge(const FVector2f& Pos, const FVector2f& Size, float Fill, const FLinearColor& FillColor, float Z0 = -1.0f,
	           float Z1 = -1.0f);
	/** Vertical gauge (fills bottom-up) with an optional target zone. */
	void GaugeV(const FVector2f& Pos, const FVector2f& Size, float Fill, const FLinearColor& FillColor, float Z0 = -1.0f,
	            float Z1 = -1.0f);
	/** Small rounded tag with text (labels, counters). */
	void Tag(const FVector2f& Centre, const FString& Text, const FLinearColor& Fill, const FLinearColor& TextColor, float Size = 13.0f);
	/** Big instruction arrow / chevron hint pointing along Dir, pulsing with Time. */
	void HintArrow(const FVector2f& From, const FVector2f& To, float Time, const FLinearColor& Color = FLinearColor(1, 1, 1, 0.8f));
	/** Round cursor hint ("click here"): a pulsing ring. */
	void HintRing(const FVector2f& Centre, float Radius, float Time, const FLinearColor& Color = FLinearColor(1, 1, 1, 0.8f));

private:
	FLinearColor Fade(const FLinearColor& Color) const;
	FSlateWindowElementList& Out;
	FGeometry G;
	int32 Layer;
	float Opacity;
};

/** One moment of feedback queued by a minigame (the host plays it). */
struct FKGMgFeedback
{
	FName Sound;
	float Volume = 1.0f;
	float Pitch = 1.0f;
	FString Pop;
	FVector2f At = FVector2f::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Shake = 0.0f;
};

class KILLGODOT_API FKGMinigame
{
public:
	virtual ~FKGMinigame() = default;

	// ---- implemented by each minigame ---------------------------------------------------------------------------
	virtual int32 NumStages() const { return 1; }
	/** One short imperative line under the title, for the current stage ("Circle the mouse to crank"). */
	virtual FString GetInstruction() const = 0;
	/** (Re)initialise the current stage (Stage is set). Called on start, after each solved stage and on retry. */
	virtual void BeginStage() = 0;
	virtual void Tick(float Dt) {}
	virtual void Paint(FKGMgPainter& P) const = 0;
	virtual void OnPress(const FVector2f& Pos) {}
	virtual void OnRelease(const FVector2f& Pos) {}
	virtual void OnMove(const FVector2f& Pos, const FVector2f& Delta) {}
	virtual void OnKey(const FKey& Key) {}
	/**
	 * Dev auto-win and tests: optional "plays itself" animation. The host force-solves the stage anyway once the
	 * server's plausibility floor has passed, so this only has to look right.
	 */
	virtual void AutoPlay(float Dt) {}
	/** Screenshots: jump to a representative, readable mid-stage moment. */
	virtual void DebugPose() {}

	// ---- host ---------------------------------------------------------------------------------------------------
	void Start(uint32 Seed, int32 FirstStage);
	void HostTick(float Dt, bool bAuto);
	void HostPress(const FVector2f& Pos);
	void HostRelease(const FVector2f& Pos);
	void HostMove(const FVector2f& Pos, const FVector2f& Delta);
	void HostKey(const FKey& Key);
	/** Next stage (after a solve was celebrated). */
	void AdvanceStage();
	/** The server refused the stage: play it again. */
	void RetryStage();
	/** The server refused an earlier stage than the one on screen: go back to it. */
	void RestartAt(int32 InStage);
	TArray<FKGMgFeedback> DrainFeedback();
	/** Auto-win / tests: mark the current stage solved. */
	void ForceSolve() { Solve(); }

	int32 GetStage() const { return Stage; }
	bool IsStageSolved() const { return bSolved; }
	float GetStageTime() const { return StageTime; }
	float GetTime() const { return Time; }
	const FVector2f& GetMouse() const { return Mouse; }
	bool IsMouseDown() const { return bMouseDown; }

protected:
	/** The current stage is done. */
	void Solve();
	void Nice(const FVector2f& At, const FString& Pop = FString(), FName Sound = TEXT("S_UI_Good"), float Pitch = 1.0f);
	void Oops(const FVector2f& At, const FString& Pop = FString(), FName Sound = TEXT("S_UI_Bad"), float Shake = 6.0f);
	void Sound(FName Name, float Volume = 1.0f, float Pitch = 1.0f);
	void PopText(const FVector2f& At, const FString& Text, const FLinearColor& Color);

	FKGRng Rng;
	int32 Stage = 0;
	float StageTime = 0.0f;
	float Time = 0.0f;
	bool bSolved = false;
	FVector2f Mouse = FVector2f(KGMg::W * 0.5f, KGMg::H * 0.5f);
	bool bMouseDown = false;

private:
	TArray<FKGMgFeedback> Feedback;
	uint32 BaseSeed = 1;
};

// Every chore minigame. Add an entry + define TUniquePtr<FKGMinigame> KGMakeMinigame_<Id>() in a KGMinigames_*.cpp.
#define KG_MINIGAME_LIST(X) \
	X(DrawWater) X(PostNotice) X(FileReports) X(RingBell) X(LightCandles) X(TendGraves) \
	X(ForgeNails) X(SharpenTools) X(BakeBread) X(PourAle) X(StockStall) X(ChopWood) \
	X(MendNets) X(UnloadFish) X(FuelLighthouse) X(FixBoat) X(LightHarbourLamp) X(FeedKoi) \
	X(HarvestCarrots) X(FeedAnimals) X(GrindFlour) X(WindClock)

#define KG_DECLARE_MINIGAME_FACTORY(Id) TUniquePtr<FKGMinigame> KGMakeMinigame_##Id();
KG_MINIGAME_LIST(KG_DECLARE_MINIGAME_FACTORY)
#undef KG_DECLARE_MINIGAME_FACTORY

struct KILLGODOT_API FKGMinigameFactory
{
	/** New minigame for a chore id, or null if the chore has none (hold-E fallback). */
	static TUniquePtr<FKGMinigame> Create(FName ChoreId);
	static bool Has(FName ChoreId);
	static TArray<FName> GetIds();
};
