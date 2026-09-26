// Village map for AKGHUD: corner minimap, full-screen map (M), location toast + current-location line.
// Included by KGHUD.cpp inside its anonymous namespace (reuses FKGPainter, FKGHudFrame and the palette). Data comes
// from the level's AKGMapInfo (Tools/Level/render_minimap.py -> Tools/Unreal/kg_make_minimap.py). State lives in a
// .cpp-side store, so KGHUD.h keeps its layout and all of this iterates with Live Coding.
// Social deduction: the map never shows other players.

// -------------------------------------------------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------------------------------------------------
struct FKGMapFx
{
	TWeakObjectPtr<AKGMapInfo> Info;
	double NextSearch = 0.0;

	// Full map (SPRINT-025: tap toggles, hold shows it while M is down; UI/KGMapInput.h)
	bool bFullOpen = false;
	bool bHolding = false;
	float FullAlpha = 0.0f;
	FKGMapInput Input;

	// Location tracking (indices into AKGMapInfo::Regions)
	int32 Here = INDEX_NONE;          // stable most-specific region
	int32 District = INDEX_NONE;      // stable layer-0 region
	int32 CandHere = INDEX_NONE;
	int32 CandDistrict = INDEX_NONE;
	double CandAt = 0.0;
	bool bPrimed = false;
	TMap<int32, double> ToastedAt;

	// Toast
	FString ToastTitle;
	FString ToastCaption;
	double ToastAt = -100.0;

	// Chore station positions (refreshed every few seconds)
	TMap<FName, FVector> Stations;
	double StationsAt = -100.0;

	// SPRINT-025: this frame's chore pins (tracker, markers, minimap and full map share one gather)
	TArray<struct FKGChorePin> Pins;
	uint64 PinsFrame = 0;

	/** kg.Map.Debug value seen last frame: leaving a forced state (2 -> 0) closes the map it forced open. */
	int32 LastForced = 0;
};

#if !UE_BUILD_SHIPPING
TAutoConsoleVariable<int32> CVarMapDebug(TEXT("kg.Map.Debug"), 0,
                                         TEXT("Dev: force the village map state for screenshots: 1 = as if M were held, 2 = as if tapped open."));
int32 MapDebug() { return CVarMapDebug.GetValueOnGameThread(); }
#else
int32 MapDebug() { return 0; }
#endif

FKGMapFx& MapFx(const AKGHUD* Hud)
{
	static TMap<TObjectKey<AKGHUD>, TUniquePtr<FKGMapFx>> MapStoreV1;   // bump on shape change (Live++ statics)
	const TObjectKey<AKGHUD> Key(Hud);
	if (TUniquePtr<FKGMapFx>* Found = MapStoreV1.Find(Key))
	{
		return **Found;
	}
	for (auto It = MapStoreV1.CreateIterator(); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
	}
	return *MapStoreV1.Add(Key, MakeUnique<FKGMapFx>());
}

FKGMapFx* MapFxFor(const APlayerController* PC)
{
	const AKGHUD* Hud = PC ? Cast<AKGHUD>(PC->GetHUD()) : nullptr;
	return Hud ? &MapFx(Hud) : nullptr;
}

AKGMapInfo* MapInfoFor(const FKGHudFrame& F, FKGMapFx& M)
{
	if (!M.Info.IsValid() && F.Fx->Now >= M.NextSearch)
	{
		M.Info = AKGMapInfo::Find(F.Hud->GetWorld());
		M.NextSearch = F.Fx->Now + 2.0;
		M.Here = M.District = M.CandHere = M.CandDistrict = INDEX_NONE;
		M.bPrimed = false;
		M.ToastedAt.Reset();
	}
	return M.Info.Get();
}

// -------------------------------------------------------------------------------------------------------------------
// Icons: coloured badge + dark glyph, all vector (crisp at any size)
// -------------------------------------------------------------------------------------------------------------------
enum class EKGMapGlyph : uint8
{
	Star, Church, Bell, Grave, Gallows, Hall, Inn, Bread, Market, Notice, Clock, Barn, Well, Fountain, Koi, Anchor, Boat,
	Fish, Lighthouse, Farm, Tree, Windmill, Mill, Axe, Anvil, Torii, Pagoda, Telescope, House
};

EKGMapGlyph GlyphOf(FName Icon)
{
	static TMap<FName, EKGMapGlyph> Table;
	if (Table.Num() == 0)
	{
		const TPair<const TCHAR*, EKGMapGlyph> Pairs[] = {
			{TEXT("church"), EKGMapGlyph::Church}, {TEXT("bell"), EKGMapGlyph::Bell}, {TEXT("grave"), EKGMapGlyph::Grave},
			{TEXT("gallows"), EKGMapGlyph::Gallows}, {TEXT("hall"), EKGMapGlyph::Hall}, {TEXT("inn"), EKGMapGlyph::Inn},
			{TEXT("bread"), EKGMapGlyph::Bread}, {TEXT("market"), EKGMapGlyph::Market}, {TEXT("notice"), EKGMapGlyph::Notice},
			{TEXT("clock"), EKGMapGlyph::Clock}, {TEXT("barn"), EKGMapGlyph::Barn}, {TEXT("well"), EKGMapGlyph::Well},
			{TEXT("fountain"), EKGMapGlyph::Fountain}, {TEXT("koi"), EKGMapGlyph::Koi}, {TEXT("anchor"), EKGMapGlyph::Anchor},
			{TEXT("boat"), EKGMapGlyph::Boat}, {TEXT("fish"), EKGMapGlyph::Fish}, {TEXT("lighthouse"), EKGMapGlyph::Lighthouse},
			{TEXT("farm"), EKGMapGlyph::Farm}, {TEXT("tree"), EKGMapGlyph::Tree}, {TEXT("windmill"), EKGMapGlyph::Windmill},
			{TEXT("mill"), EKGMapGlyph::Mill}, {TEXT("axe"), EKGMapGlyph::Axe}, {TEXT("anvil"), EKGMapGlyph::Anvil},
			{TEXT("torii"), EKGMapGlyph::Torii}, {TEXT("pagoda"), EKGMapGlyph::Pagoda}, {TEXT("telescope"), EKGMapGlyph::Telescope},
			{TEXT("house"), EKGMapGlyph::House}, {TEXT("star"), EKGMapGlyph::Star}};
		for (const auto& P : Pairs)
		{
			Table.Add(FName(P.Key), P.Value);
		}
	}
	const EKGMapGlyph* Found = Table.Find(Icon);
	return Found ? *Found : EKGMapGlyph::Star;
}

const FLinearColor MapSea = Srgb(96, 182, 255);
const FLinearColor MapWork = Srgb(255, 150, 78);
const FLinearColor MapShrine = Srgb(255, 128, 160);

FLinearColor GlyphColor(EKGMapGlyph G)
{
	switch (G)
	{
	case EKGMapGlyph::Church: case EKGMapGlyph::Bell: case EKGMapGlyph::Grave: case EKGMapGlyph::Telescope: return NeutralColor;
	case EKGMapGlyph::Gallows: return ImpatientColor;
	case EKGMapGlyph::Well: case EKGMapGlyph::Fountain: case EKGMapGlyph::Koi: return GhostColor;
	case EKGMapGlyph::Anchor: case EKGMapGlyph::Boat: case EKGMapGlyph::Fish: return MapSea;
	case EKGMapGlyph::Lighthouse: return GoldLight;
	case EKGMapGlyph::Farm: case EKGMapGlyph::Tree: case EKGMapGlyph::Windmill: case EKGMapGlyph::Barn: return TownLight;
	case EKGMapGlyph::Mill: case EKGMapGlyph::Axe: case EKGMapGlyph::Anvil: return MapWork;
	case EKGMapGlyph::Torii: case EKGMapGlyph::Pagoda: return MapShrine;
	case EKGMapGlyph::Star: return Cream;
	default: return Gold;
	}
}

FString GlyphLegend(EKGMapGlyph G)
{
	switch (G)
	{
	case EKGMapGlyph::Church: return TEXT("Church");
	case EKGMapGlyph::Bell: return TEXT("Bell tower");
	case EKGMapGlyph::Grave: return TEXT("Graveyard");
	case EKGMapGlyph::Gallows: return TEXT("Gallows");
	case EKGMapGlyph::Hall: return TEXT("Town hall");
	case EKGMapGlyph::Inn: return TEXT("Inn");
	case EKGMapGlyph::Bread: return TEXT("Bakery");
	case EKGMapGlyph::Market: return TEXT("Market");
	case EKGMapGlyph::Notice: return TEXT("Notice board");
	case EKGMapGlyph::Clock: return TEXT("Clock tower");
	case EKGMapGlyph::Barn: return TEXT("Barn");
	case EKGMapGlyph::Well: return TEXT("Well");
	case EKGMapGlyph::Fountain: return TEXT("Fountain");
	case EKGMapGlyph::Koi: return TEXT("Koi pond");
	case EKGMapGlyph::Anchor: return TEXT("Harbour");
	case EKGMapGlyph::Boat: return TEXT("Boathouse");
	case EKGMapGlyph::Fish: return TEXT("Fish market");
	case EKGMapGlyph::Lighthouse: return TEXT("Lighthouse");
	case EKGMapGlyph::Farm: return TEXT("Farm");
	case EKGMapGlyph::Tree: return TEXT("Garden");
	case EKGMapGlyph::Windmill: return TEXT("Windmill");
	case EKGMapGlyph::Mill: return TEXT("Waterwheel");
	case EKGMapGlyph::Axe: return TEXT("Woodcutter");
	case EKGMapGlyph::Anvil: return TEXT("Forge");
	case EKGMapGlyph::Torii: return TEXT("Shrine gate");
	case EKGMapGlyph::Pagoda: return TEXT("Pagoda");
	case EKGMapGlyph::Telescope: return TEXT("Lookout");
	case EKGMapGlyph::House: return TEXT("House");
	default: return TEXT("Landmark");
	}
}

/** Glyph inside a box of edge G centred on C. */
void MapGlyph(const FKGPainter& P, EKGMapGlyph Glyph, const FVector2D& C, float G, const FLinearColor& Col)
{
	const float T = FMath::Max(1.2f, G * 0.13f);   // stroke
	auto V = [&](float X, float Y) { return C + FVector2D(X, Y) * G; };
	switch (Glyph)
	{
	case EKGMapGlyph::Church:
		P.Line(V(0.0f, -0.42f), V(0.0f, 0.42f), T * 1.15f, Col, true);
		P.Line(V(-0.28f, -0.14f), V(0.28f, -0.14f), T * 1.15f, Col, true);
		break;
	case EKGMapGlyph::Bell:
	{
		const FVector2D Body[4] = {V(-0.18f, -0.26f), V(0.18f, -0.26f), V(0.34f, 0.24f), V(-0.34f, 0.24f)};
		P.Poly(MakeArrayView(Body), Col, Col);
		P.Circle(V(0.0f, -0.26f), G * 0.18f, Col);
		P.Line(V(-0.4f, 0.24f), V(0.4f, 0.24f), T, Col, true);
		P.Circle(V(0.0f, 0.38f), G * 0.09f, Col);
		break;
	}
	case EKGMapGlyph::Grave:
		P.RoundRect(C.X - G * 0.24f, C.Y - G * 0.4f, G * 0.48f, G * 0.74f, G * 0.22f, Col);
		P.Line(V(-0.4f, 0.38f), V(0.4f, 0.38f), T, Col, true);
		break;
	case EKGMapGlyph::Gallows:
		P.Line(V(-0.24f, 0.42f), V(-0.24f, -0.4f), T, Col, true);
		P.Line(V(-0.3f, -0.4f), V(0.26f, -0.4f), T, Col, true);
		P.Line(V(0.2f, -0.4f), V(0.2f, -0.08f), T * 0.7f, Col, true);
		P.Arc(V(0.2f, 0.04f), G * 0.1f, T * 0.7f, 0.0f, UE_TWO_PI, Col);
		P.Line(V(-0.42f, 0.42f), V(0.1f, 0.42f), T, Col, true);
		break;
	case EKGMapGlyph::Hall:
	{
		const FVector2D Roof[3] = {V(0.0f, -0.44f), V(0.44f, -0.16f), V(-0.44f, -0.16f)};
		P.Poly(MakeArrayView(Roof), Col, Col);
		for (int32 i = -1; i <= 1; ++i)
		{
			P.Line(V(i * 0.26f, -0.06f), V(i * 0.26f, 0.3f), T, Col, false);
		}
		P.Line(V(-0.44f, 0.38f), V(0.44f, 0.38f), T * 1.1f, Col, true);
		break;
	}
	case EKGMapGlyph::Inn:
		P.RoundRect(C.X - G * 0.3f, C.Y - G * 0.32f, G * 0.44f, G * 0.7f, G * 0.08f, Col);
		P.Arc(V(0.18f, 0.02f), G * 0.16f, T, -UE_HALF_PI, UE_HALF_PI, Col);
		break;
	case EKGMapGlyph::Bread:
		P.RoundRect(C.X - G * 0.42f, C.Y - G * 0.2f, G * 0.84f, G * 0.4f, G * 0.2f, Col);
		break;
	case EKGMapGlyph::Market:
	{
		P.RoundRect(C.X - G * 0.42f, C.Y - G * 0.4f, G * 0.84f, G * 0.26f, G * 0.06f, Col);
		const FVector2D Scallop[3] = {V(-0.42f, -0.16f), V(0.42f, -0.16f), V(0.0f, -0.02f)};
		P.Poly(MakeArrayView(Scallop), Col, Col);
		P.Line(V(-0.32f, -0.1f), V(-0.32f, 0.42f), T, Col, false);
		P.Line(V(0.32f, -0.1f), V(0.32f, 0.42f), T, Col, false);
		P.Line(V(-0.36f, 0.16f), V(0.36f, 0.16f), T, Col, true);
		break;
	}
	case EKGMapGlyph::Notice:
		P.Line(V(0.0f, 0.0f), V(0.0f, 0.44f), T, Col, true);
		P.RoundRect(C.X - G * 0.34f, C.Y - G * 0.38f, G * 0.68f, G * 0.44f, G * 0.06f, Col);
		break;
	case EKGMapGlyph::Clock:
		P.Arc(C, G * 0.36f, T, 0.0f, UE_TWO_PI, Col);
		P.Line(C, V(0.0f, -0.24f), T, Col, true);
		P.Line(C, V(0.16f, 0.06f), T, Col, true);
		break;
	case EKGMapGlyph::Barn:
	case EKGMapGlyph::House:
		P.House(C, G * 0.92f, Col);
		break;
	case EKGMapGlyph::Well:
	case EKGMapGlyph::Fountain:
	{
		// Droplet (+ a basin line for the fountain).
		const float Dy = Glyph == EKGMapGlyph::Fountain ? -0.08f : 0.0f;
		P.Circle(V(0.0f, 0.1f + Dy), G * 0.24f, Col);
		const FVector2D Tip[3] = {V(0.0f, -0.42f + Dy), V(0.215f, 0.0f + Dy), V(-0.215f, 0.0f + Dy)};
		P.Poly(MakeArrayView(Tip), Col, Col);
		if (Glyph == EKGMapGlyph::Fountain)
		{
			P.Line(V(-0.4f, 0.38f), V(0.4f, 0.38f), T, Col, true);
		}
		break;
	}
	case EKGMapGlyph::Fish:
	case EKGMapGlyph::Koi:
	{
		FPts Body;
		for (int32 k = 0; k < 20; ++k)
		{
			const float A = UE_TWO_PI * k / 20.0f;
			Body.Add(V(-0.06f + FMath::Cos(A) * 0.3f, FMath::Sin(A) * 0.17f));
		}
		P.Poly(Body, Col, Col);
		const FVector2D Tail[3] = {V(0.16f, 0.0f), V(0.44f, -0.2f), V(0.44f, 0.2f)};
		P.Poly(MakeArrayView(Tail), Col, Col);
		break;
	}
	case EKGMapGlyph::Anchor:
		P.Arc(V(0.0f, -0.3f), G * 0.1f, T * 0.8f, 0.0f, UE_TWO_PI, Col);
		P.Line(V(0.0f, -0.2f), V(0.0f, 0.4f), T, Col, true);
		P.Line(V(-0.2f, -0.08f), V(0.2f, -0.08f), T, Col, true);
		P.Arc(V(0.0f, 0.06f), G * 0.34f, T, UE_PI * 0.15f, UE_PI * 0.85f, Col);
		break;
	case EKGMapGlyph::Boat:
	{
		const FVector2D Hull[4] = {V(-0.44f, 0.12f), V(0.44f, 0.12f), V(0.28f, 0.36f), V(-0.28f, 0.36f)};
		P.Poly(MakeArrayView(Hull), Col, Col);
		const FVector2D Sail[3] = {V(0.02f, -0.44f), V(0.3f, 0.04f), V(0.02f, 0.04f)};
		P.Poly(MakeArrayView(Sail), Col, Col);
		P.Line(V(-0.04f, -0.4f), V(-0.04f, 0.1f), T * 0.7f, Col, false);
		break;
	}
	case EKGMapGlyph::Lighthouse:
	{
		const FVector2D Tower[4] = {V(-0.13f, -0.2f), V(0.13f, -0.2f), V(0.22f, 0.42f), V(-0.22f, 0.42f)};
		P.Poly(MakeArrayView(Tower), Col, Col);
		P.Circle(V(0.0f, -0.32f), G * 0.12f, Col);
		P.Line(V(-0.42f, -0.32f), V(-0.26f, -0.32f), T * 0.7f, Col, true);
		P.Line(V(0.26f, -0.32f), V(0.42f, -0.32f), T * 0.7f, Col, true);
		break;
	}
	case EKGMapGlyph::Farm:
		P.Line(V(0.0f, 0.44f), V(0.0f, -0.4f), T * 0.8f, Col, true);
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = -0.26f + i * 0.2f;
			P.Diamond(V(-0.13f, Y), G * 0.11f, Col);
			P.Diamond(V(0.13f, Y), G * 0.11f, Col);
		}
		break;
	case EKGMapGlyph::Tree:
		P.Line(V(0.0f, 0.1f), V(0.0f, 0.44f), T * 1.2f, Col, true);
		P.Circle(V(0.0f, -0.1f), G * 0.3f, Col);
		break;
	case EKGMapGlyph::Windmill:
		P.Line(V(-0.36f, -0.36f), V(0.36f, 0.36f), T, Col, true);
		P.Line(V(0.36f, -0.36f), V(-0.36f, 0.36f), T, Col, true);
		P.Circle(C, G * 0.1f, Col);
		break;
	case EKGMapGlyph::Mill:
		P.Arc(C, G * 0.34f, T, 0.0f, UE_TWO_PI, Col);
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = UE_PI * 0.25f * i;
			const FVector2D D(FMath::Cos(A), FMath::Sin(A));
			P.Line(C - D * (G * 0.34f), C + D * (G * 0.34f), T * 0.7f, Col, false);
		}
		break;
	case EKGMapGlyph::Axe:
	{
		P.Line(V(-0.3f, 0.4f), V(0.22f, -0.34f), T, Col, true);
		const FVector2D Head[4] = {V(0.02f, -0.3f), V(0.3f, -0.44f), V(0.42f, -0.06f), V(0.2f, -0.06f)};
		P.Poly(MakeArrayView(Head), Col, Col);
		break;
	}
	case EKGMapGlyph::Anvil:
	{
		const FVector2D Top[5] = {V(-0.44f, -0.2f), V(0.36f, -0.2f), V(0.24f, 0.0f), V(-0.2f, 0.0f), V(-0.36f, -0.08f)};
		P.Poly(MakeArrayView(Top), Col, Col);
		const FVector2D Base[4] = {V(-0.1f, 0.0f), V(0.12f, 0.0f), V(0.26f, 0.34f), V(-0.24f, 0.34f)};
		P.Poly(MakeArrayView(Base), Col, Col);
		break;
	}
	case EKGMapGlyph::Torii:
		P.Line(V(-0.44f, -0.32f), V(0.44f, -0.32f), T * 1.1f, Col, true);
		P.Line(V(-0.34f, -0.12f), V(0.34f, -0.12f), T * 0.8f, Col, true);
		P.Line(V(-0.24f, -0.32f), V(-0.24f, 0.42f), T, Col, false);
		P.Line(V(0.24f, -0.32f), V(0.24f, 0.42f), T, Col, false);
		break;
	case EKGMapGlyph::Pagoda:
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = -0.34f + i * 0.26f;
			const float W = 0.2f + i * 0.1f;
			const FVector2D Tier[4] = {V(-W, Y), V(W, Y), V(W + 0.1f, Y + 0.12f), V(-W - 0.1f, Y + 0.12f)};
			P.Poly(MakeArrayView(Tier), Col, Col);
		}
		P.Line(V(0.0f, -0.46f), V(0.0f, 0.42f), T * 0.7f, Col, false);
		break;
	case EKGMapGlyph::Telescope:
		P.Line(V(-0.36f, 0.0f), V(0.3f, -0.34f), T * 1.6f, Col, true);
		P.Line(V(-0.04f, -0.12f), V(-0.24f, 0.42f), T * 0.7f, Col, true);
		P.Line(V(-0.04f, -0.12f), V(0.16f, 0.42f), T * 0.7f, Col, true);
		break;
	default:
		P.Star(C, G * 0.95f, Col);
		break;
	}
}

void MapBadge(const FKGPainter& P, EKGMapGlyph Glyph, const FVector2D& C, float D, float Opacity)
{
	const float R = D * 0.5f;
	P.Circle(C + FVector2D(0.0f, D * 0.08f), R + 1.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Opacity), 2.0f);
	P.Circle(C, R + FMath::Max(1.0f, D * 0.08f), WithAlpha(InkBottom, 0.95f * Opacity));
	P.Circle(C, R, WithAlpha(GlyphColor(Glyph), Opacity));
	MapGlyph(P, Glyph, C, D * 0.62f, WithAlpha(InkText, Opacity));
}

/** Gold map pin with "!" (a chore), tip at Tip. */
void ChorePin(const FKGPainter& P, const FVector2D& Tip, float D, float Opacity, float Pulse = 0.0f)
{
	const float R = D * 0.5f;
	const FVector2D Head = Tip - FVector2D(0.0f, D * 0.95f);
	if (Pulse > 0.0f)
	{
		P.Arc(Tip, D * (0.3f + 0.5f * Pulse), FMath::Max(1.0f, D * 0.08f), 0.0f, UE_TWO_PI,
		      WithAlpha(Gold, 0.6f * (1.0f - Pulse) * Opacity));
	}
	P.Circle(Tip, D * 0.16f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Opacity), 2.0f);
	const FVector2D Stem[3] = {Head + FVector2D(-R * 0.72f, R * 0.5f), Head + FVector2D(R * 0.72f, R * 0.5f), Tip};
	const float Rim = FMath::Max(1.0f, D * 0.09f);
	const FVector2D StemO[3] = {Stem[0] + FVector2D(-Rim, 0.0f), Stem[1] + FVector2D(Rim, 0.0f), Tip + FVector2D(0.0f, Rim * 1.4f)};
	P.Poly(MakeArrayView(StemO), WithAlpha(InkBottom, 0.95f * Opacity), WithAlpha(InkBottom, 0.95f * Opacity));
	P.Circle(Head, R + Rim, WithAlpha(InkBottom, 0.95f * Opacity));
	P.Poly(MakeArrayView(Stem), WithAlpha(Gold, Opacity), WithAlpha(Srgb(236, 150, 40), Opacity));
	P.Circle(Head, R, WithAlpha(GoldLight, Opacity));
	P.Circle(Head, R * 0.86f, WithAlpha(Gold, Opacity));
	P.Exclaim(Head, D * 0.62f, WithAlpha(InkText, Opacity));
}

/** "You": a chevron pointing along ScreenAngle (0 = up, clockwise, radians). */
void PlayerArrow(const FKGPainter& P, const FVector2D& C, float Size, float ScreenAngle, float Opacity)
{
	const FVector2D Unit[4] = {FVector2D(0.0, -0.62), FVector2D(0.46, 0.46), FVector2D(0.0, 0.22), FVector2D(-0.46, 0.46)};
	const float Sn = FMath::Sin(ScreenAngle);
	const float Cs = FMath::Cos(ScreenAngle);
	auto Xf = [&](const FVector2D& U, float K)
	{
		const FVector2D L = U * (Size * K);
		return C + FVector2D(L.X * Cs - L.Y * Sn, L.X * Sn + L.Y * Cs);
	};
	FVector2D Outer[4];
	FVector2D Inner[4];
	for (int32 i = 0; i < 4; ++i)
	{
		Outer[i] = Xf(Unit[i], 1.28f);
		Inner[i] = Xf(Unit[i], 1.0f);
	}
	const FVector2D KO = Xf(FVector2D(0.0, 0.0), 1.0f);
	P.PolyEx(MakeArrayView(Outer), WithAlpha(InkBottom, 0.95f * Opacity), WithAlpha(InkBottom, 0.95f * Opacity), 1.5f, &KO,
	         true, 0.0, 0.0);
	P.PolyEx(MakeArrayView(Inner), WithAlpha(FLinearColor::White, Opacity), WithAlpha(GoldLight, Opacity), 1.0f, &KO, true,
	         0.0, 0.0);
}

/** Crisp label with an 8-way dark halo, centred on (X, MidY). Returns its box. */
FBox2D HaloLabel(const FKGPainter& P, const FString& Str, float X, float MidY, float Px, const FLinearColor& Col, bool bBold,
                 float Tracking = 0.0f, bool bDraw = true)
{
	const FVector2D Size = P.Measure(Str, Px, bBold, Tracking);
	const float X0 = X - static_cast<float>(Size.X) * 0.5f;
	const float Y0 = MidY - static_cast<float>(Size.Y) * 0.5f;
	if (bDraw && Col.A > 0.004f)
	{
		const float O = FMath::Max(1.0f, FMath::RoundToFloat(1.4f * P.S));
		const FLinearColor Halo(InkBottom.R, InkBottom.G, InkBottom.B, 0.85f * Col.A);
		static const FVector2D Dirs[8] = {FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1),
		                                  FVector2D(0.7, 0.7), FVector2D(-0.7, 0.7), FVector2D(0.7, -0.7), FVector2D(-0.7, -0.7)};
		for (const FVector2D& D : Dirs)
		{
			P.Text(Str, X0 + D.X * O, Y0 + D.Y * O + O * 0.5f, Px, Halo, 0.0f, bBold, Tracking, 0.0f);
		}
		P.Text(Str, X0, Y0, Px, Col, 0.0f, bBold, Tracking, 0.0f);
	}
	return FBox2D(FVector2D(X0 - 3.0f * P.S, Y0), FVector2D(X0 + Size.X + 3.0f * P.S, Y0 + Size.Y));
}

bool Overlaps(const TArray<FBox2D>& Placed, const FBox2D& B)
{
	for (const FBox2D& O : Placed)
	{
		if (B.Min.X < O.Max.X && B.Max.X > O.Min.X && B.Min.Y < O.Max.Y && B.Max.Y > O.Min.Y)
		{
			return true;
		}
	}
	return false;
}

/** Textured fan: Ring (screen, star-shaped around Centre) with UVs from ToUV; 1 px feathered edge. */
template <typename FToUV>
void DrawMapFill(const FKGPainter& P, const UTexture2D* Tex, TConstArrayView<FVector2D> Ring, const FVector2D& Centre,
                 FToUV&& ToUV, float Opacity)
{
	const FTexture* Res = Tex ? Tex->GetResource() : nullptr;
	const int32 N = Ring.Num();
	if (!Res || N < 3)
	{
		return;
	}
	FPts Off;
	OutwardOffsets(Ring, Off);
	const FLinearColor On(1.0f, 1.0f, 1.0f, Opacity);
	const FLinearColor Clear(1.0f, 1.0f, 1.0f, 0.0f);
	FTris T;
	T.Reserve(N * 3);
	auto Vtx = [&](FCanvasUVTri& Tri, int32 Slot, const FVector2D& Pos, const FLinearColor& Col)
	{
		const FVector2D UV = ToUV(Pos);
		(Slot == 0 ? Tri.V0_Pos : Slot == 1 ? Tri.V1_Pos : Tri.V2_Pos) = Pos;
		(Slot == 0 ? Tri.V0_UV : Slot == 1 ? Tri.V1_UV : Tri.V2_UV) = UV;
		(Slot == 0 ? Tri.V0_Color : Slot == 1 ? Tri.V1_Color : Tri.V2_Color) = Col;
	};
	auto Add = [&](const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB,
	               const FLinearColor& CC)
	{
		FCanvasUVTri& Tri = T.AddDefaulted_GetRef();
		Vtx(Tri, 0, A, CA);
		Vtx(Tri, 1, B, CB);
		Vtx(Tri, 2, C, CC);
	};
	for (int32 i = 0; i < N; ++i)
	{
		const int32 j = (i + 1) % N;
		Add(Centre, Ring[i], Ring[j], On, On, On);
		const FVector2D Oi = Ring[i] + Off[i];
		const FVector2D Oj = Ring[j] + Off[j];
		Add(Ring[i], Ring[j], Oj, On, On, Clear);
		Add(Ring[i], Oj, Oi, On, Clear, Clear);
	}
	FCanvasTriangleItem Item(T, Res);
	Item.BlendMode = SE_BLEND_Translucent;
	P.C->DrawItem(Item);
}

// -------------------------------------------------------------------------------------------------------------------
// Location tracking + toast
// -------------------------------------------------------------------------------------------------------------------
FString RegionName(const AKGMapInfo& Info, int32 Index)
{
	return Info.Regions.IsValidIndex(Index) ? Info.Regions[Index].Name.ToString() : FString();
}

FString MapTitleOf(const AKGMapInfo& Info)
{
	return Info.MapTitle.IsEmpty() ? FString(TEXT("Morrowmere")) : Info.MapTitle.ToString();
}

int32 DistrictAt(const AKGMapInfo& Info, const FVector2D& P)
{
	int32 Best = INDEX_NONE;
	double BestArea = 0.0;
	for (int32 i = 0; i < Info.Regions.Num(); ++i)
	{
		const FKGMapRegion& R = Info.Regions[i];
		if (R.Layer == 0 && R.Contains(P))
		{
			const double A = R.Area();
			if (Best == INDEX_NONE || A < BestArea)
			{
				Best = i;
				BestArea = A;
			}
		}
	}
	return Best;
}

void StartToast(FKGMapFx& M, double Now, const FString& Title, const FString& Caption)
{
	M.ToastTitle = Title;
	M.ToastCaption = Caption;
	M.ToastAt = Now;
}

void UpdateLocation(const FKGHudFrame& F, FKGMapFx& M, const AKGMapInfo& Info, const FVector2D& Me, bool bQuiet)
{
	const double Now = F.Fx->Now;
	const int32 Here = Info.FindRegionAt(Me);
	const int32 District = DistrictAt(Info, Me);
	if (Here != M.CandHere || District != M.CandDistrict)
	{
		M.CandHere = Here;
		M.CandDistrict = District;
		M.CandAt = Now;
	}
	const bool bStable = Now - M.CandAt >= 0.35;
	if (!M.bPrimed)
	{
		// First sample: adopt silently (the loading card covers the spawn); the next change toasts.
		M.Here = Here;
		M.District = District;
		M.bPrimed = !bQuiet;
		return;
	}
	if (!bStable || (M.CandHere == M.Here && M.CandDistrict == M.District))
	{
		return;
	}
	const int32 OldDistrict = M.District;
	M.Here = M.CandHere;
	M.District = M.CandDistrict;
	if (bQuiet)
	{
		return;
	}
	const FString DistrictName = RegionName(Info, M.District);
	const FKGMapRegion* R = Info.Regions.IsValidIndex(M.Here) ? &Info.Regions[M.Here] : nullptr;
	// Re-entering within 8 s (boundary shuffles, doorways) stays quiet.
	auto Recently = [&](int32 Index) { const double* At = M.ToastedAt.Find(Index); return At && Now - *At < 8.0; };
	if (R && R->bToast && R->Layer >= 2 && !Recently(M.Here))
	{
		StartToast(M, Now, R->Name.ToString(), DistrictName.IsEmpty() ? MapTitleOf(Info) : DistrictName);
		M.ToastedAt.Add(M.Here, Now);
	}
	else if (M.District != OldDistrict && M.District != INDEX_NONE && !Recently(M.District))
	{
		StartToast(M, Now, DistrictName, MapTitleOf(Info));
		M.ToastedAt.Add(M.District, Now);
	}
}

void DrawLocationToast(const FKGHudFrame& F, const FKGMapFx& M)
{
	constexpr float In = 0.35f;
	constexpr float Hold = 1.75f;
	constexpr float Out = 0.6f;
	const float Age = static_cast<float>(F.Fx->Now - M.ToastAt);
	if (Age < 0.0f || Age > In + Hold + Out || M.ToastTitle.IsEmpty())
	{
		return;
	}
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float A = Age < In ? EaseOutCubic(Age / In) : Age < In + Hold ? 1.0f : 1.0f - Saturate((Age - In - Hold) / Out);
	const float Rise = (1.0f - EaseOutCubic(Age / (In * 1.4f))) * 10.0f * S;
	const float MidY = 250.0f * S + Rise;
	const FString Title = M.ToastTitle.ToUpper();
	const FString Caption = M.ToastCaption.ToUpper();
	const float Track = 3.0f + 3.0f * (1.0f - EaseOutCubic(Age / 0.9f));   // letters settle in
	const float TitleW = static_cast<float>(P.Measure(Title, 40.0f, true, Track).X);
	// Soft ink band so it reads over bright sky and sand.
	const float BandW = TitleW + 260.0f * S;
	const FVector2D Band[4] = {FVector2D(F.CX - BandW * 0.5f, MidY - 44.0f * S), FVector2D(F.CX + BandW * 0.5f, MidY - 44.0f * S),
	                           FVector2D(F.CX + BandW * 0.5f, MidY + 34.0f * S), FVector2D(F.CX - BandW * 0.5f, MidY + 34.0f * S)};
	const FLinearColor Ink = WithAlpha(InkBottom, 0.34f * A);
	P.PolyEx(MakeArrayView(Band), Ink, Ink, 70.0f * S, nullptr, true, 0.0, 0.0);
	// Caption with rules and diamonds.
	const float CapY = MidY - 30.0f * S;
	const float CapW = static_cast<float>(P.Measure(Caption, 14.0f, true, 3.0f).X);
	P.TextMid(Caption, F.CX, CapY, 14.0f, WithAlpha(Gold, A), 0.5f, true, 3.0f, 0.5f);
	const float RuleLen = 70.0f * S * A;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float X0 = F.CX + Side * (CapW * 0.5f + 14.0f * S);
		P.Line(FVector2D(X0, CapY), FVector2D(X0 + Side * RuleLen, CapY), FMath::Max(1.0f, 1.6f * S), WithAlpha(Gold, 0.8f * A));
		P.Diamond(FVector2D(X0 + Side * (RuleLen + 6.0f * S), CapY), 3.5f * S, WithAlpha(Gold, A));
	}
	P.TextMid(Title, F.CX, MidY + 4.0f * S, 40.0f, WithAlpha(Cream, A), 0.5f, true, Track, 0.7f);
}

// -------------------------------------------------------------------------------------------------------------------
// Chores (own open ones only)
// -------------------------------------------------------------------------------------------------------------------
struct FKGChorePin
{
	FVector2D Pos;          // world XY (map)
	bool bActive = false;
	// SPRINT-025 (item 1): the in-world marker and the tracker line.
	FVector Loc = FVector::ZeroVector;   // world position of the current step
	FName Chore;
	FString Name;           // "Draw water"
	FString Step;           // "Crank", "Carry the bucket to the trough 1/2"
	bool bItem = false;     // your dropped item (pick it up again)
	bool bDone = false;     // finished (tracker only, no marker)
	bool bRowOnly = false;  // tracker row without a marker (a world chore's row; its steps are separate pins)
	float Metres = 0.0f;
};

// SPRINT-016 hooks: world chore waypoints, compass strip and work ring (Chores/WorldChores/KGWorldChoreHud.inl,
// included at the end).
bool IsWorldChoreId(FName Id);
void GatherWorldChorePins(const FKGHudFrame& F, TArray<FKGChorePin>& Out);
void DrawWorldChoreHud(const FKGHudFrame& F);
// SPRINT-025 hooks (UI/KGHUDChoreMarkers.inl, included at the end): in-world markers + the top-left tracker.
FString WorldChoreStepLine(const FKGHudFrame& F, FName Chore, FVector* OutLoc);
FString WorldChoreTitle(FName Chore);
void DrawChoreMarkers(const FKGHudFrame& F, const TArray<FKGChorePin>& Pins, float Opacity);

void GatherChores(const FKGHudFrame& F, FKGMapFx& M, TArray<FKGChorePin>& Out)
{
	if (F.Fx->Now - M.StationsAt > 3.0)
	{
		M.Stations.Reset();
		for (TActorIterator<AKGTaskStation> It(F.Hud->GetWorld()); It; ++It)
		{
			M.Stations.Add(It->TaskId, It->GetActorLocation());
		}
		M.StationsAt = F.Fx->Now;
	}
	if (M.PinsFrame == GFrameCounter && GFrameCounter != 0)
	{
		Out = M.Pins;   // already gathered this frame (tracker, markers and maps share it)
		return;
	}
	const AKGTaskStation* Active = F.Pawn ? F.Pawn->GetActiveTask() : nullptr;
	const UKGChoreComponent* Chores = F.Pawn ? UKGChoreComponent::FindFor(F.Pawn) : nullptr;
	const FVector MeLoc = F.Pawn ? F.Pawn->GetActorLocation() : FVector::ZeroVector;
	if (F.Me && F.GS && F.GS->GetPhase() != EKGPhase::Epilogue)
	{
		for (int32 i = 0; i < F.Me->TaskIds.Num(); ++i)
		{
			const FName Id = F.Me->TaskIds[i];
			const bool bDone = F.Me->TaskDone.IsValidIndex(i) && F.Me->TaskDone[i];
			FKGChorePin Pin;
			Pin.Chore = Id;
			Pin.bDone = bDone;
			if (IsWorldChoreId(Id))
			{
				// SPRINT-016: world chores pin their current step, not where they start (GatherWorldChorePins).
				Pin.Name = WorldChoreTitle(Id);
				Pin.Step = bDone ? FString() : WorldChoreStepLine(F, Id, &Pin.Loc);
				Pin.Pos = FVector2D(Pin.Loc);
				Pin.Metres = static_cast<float>(FVector::Dist2D(MeLoc, Pin.Loc)) / 100.0f;
				Pin.bRowOnly = true;
				Out.Add(Pin);
				continue;
			}
			Pin.Name = ChoreName(F, Id);
			const FKGChoreDef* Def = FKGChoreCatalog::Find(Id);
			const int32 Stage = Chores ? Chores->GetSavedStage(Id) : 0;
			Pin.Step = Def && Def->Stages.IsValidIndex(Stage) ? Def->Stages[Stage] : FString(TEXT("Work"));
			if (Def && Def->NumStages() > 1)
			{
				Pin.Step += FString::Printf(TEXT("  %d/%d"), Stage + 1, Def->NumStages());
			}
			if (const FVector* Pos = M.Stations.Find(Id))
			{
				Pin.Loc = *Pos;
				Pin.Pos = FVector2D(*Pos);
				Pin.Metres = static_cast<float>(FVector::Dist2D(MeLoc, *Pos)) / 100.0f;
			}
			else
			{
				Pin.bRowOnly = true;
			}
			Pin.bActive = Active && Active->TaskId == Id;
			Out.Add(Pin);
		}
		GatherWorldChorePins(F, Out);   // SPRINT-016 hook (markers + map pins of the world chore steps)
	}
	if (Out.Num() == 0 && F.Demo != 0)
	{
		int32 k = 0;
		const TCHAR* DemoSteps[] = {TEXT("Crank  1/2"), TEXT("Carry the bucket to the trough"), TEXT("Ring"), TEXT("Weed  1/2")};
		for (const TPair<FName, FVector>& Pair : M.Stations)
		{
			if (k++ % 4 == 1 && Out.Num() < 4)
			{
				FKGChorePin Pin;
				Pin.Pos = FVector2D(Pair.Value);
				Pin.Loc = Pair.Value;
				Pin.Chore = Pair.Key;
				Pin.Name = ChoreName(F, Pair.Key);
				Pin.Step = DemoSteps[Out.Num()];
				Pin.Metres = static_cast<float>(FVector::Dist2D(MeLoc, Pair.Value)) / 100.0f;
				Pin.bActive = Out.Num() == 0;
				Pin.bDone = Out.Num() == 2;
				Out.Add(Pin);
			}
		}
	}
	M.Pins = Out;
	M.PinsFrame = GFrameCounter;
}

// KG_DIG hooks: digging HUD, treasure-map marks and the underground plan (Dig/KGDigHud.inl, included at the end).
void DrawDigLayer(const FKGHudFrame& F);
const AKGMapInfo* DigResolvePlan(const FKGHudFrame& F, FKGMapFx& M, const AKGMapInfo* Surface);
void DigMapMarks(const FKGHudFrame& F, TFunctionRef<FVector2D(const FVector2D&)> ToScreen, float Size, float Opacity,
                 const FVector2D* ClipCentre, float ClipRadius);
// SPRINT-040 hook: discovered secret passages + pinging traps (Manor/KGManorHud.inl, included at the end).
void ManorMapMarks(const FKGHudFrame& F, TFunctionRef<FVector2D(const FVector2D&)> ToScreen, float Size, float Opacity,
                   const FVector2D* ClipCentre, float ClipRadius);

// -------------------------------------------------------------------------------------------------------------------
// Corner minimap (top right): rotates with the view, you in the centre, nearby names, your chores
// -------------------------------------------------------------------------------------------------------------------
struct FKGMiniXf
{
	FVector2D Ctr;
	FVector2D Me;
	double Scale = 1.0;   // screen px per cm
	FVector2D Fwd;
	FVector2D Right;

	FVector2D ToScreen(const FVector2D& W) const
	{
		const FVector2D D = W - Me;
		return Ctr + FVector2D(FVector2D::DotProduct(D, Right), -FVector2D::DotProduct(D, Fwd)) * Scale;
	}

	FVector2D ToWorld(const FVector2D& Sc) const
	{
		const FVector2D O = (Sc - Ctr) / Scale;
		return Me + Right * O.X - Fwd * O.Y;
	}
};

void DrawMinimap(const FKGHudFrame& F, FKGMapFx& M, const AKGMapInfo& Info, const FVector2D& Me, float Yaw,
                 const TArray<FKGChorePin>& Chores, float Opacity)
{
	const FKGPainter& P = F.P;
	const float S = F.S;
	const float R = 124.0f * S;
	const float Rim0 = 7.0f * S;
	const FBox2D Slot = FKGHudLayout::Compute(F.W, F.H, 0, false).Minimap;   // SPRINT-025: shared layout table
	const FVector2D Ctr(static_cast<float>(Slot.Max.X) - Rim0 - R, static_cast<float>(Slot.Min.Y) + Rim0 + R);
	const double ViewRadius = 3800.0;   // cm of world from the centre to the rim
	const float Rad = FMath::DegreesToRadians(Yaw);
	FKGMiniXf X;
	X.Ctr = Ctr;
	X.Me = Me;
	X.Scale = R / ViewRadius;
	X.Fwd = FVector2D(FMath::Cos(Rad), FMath::Sin(Rad));
	X.Right = FVector2D(-FMath::Sin(Rad), FMath::Cos(Rad));

	// Frame: drop shadow, ink rim, the map disc, inner rim light.
	const float Rim = 7.0f * S;
	P.Circle(Ctr + FVector2D(0.0f, 6.0f * S), R + Rim, FLinearColor(0.0f, 0.0f, 0.0f, 0.3f * Opacity), 16.0f * S);
	P.Circle(Ctr, R + Rim, WithAlpha(InkTop, 0.92f * Opacity));
	FPts Ring;
	constexpr int32 Seg = 96;
	for (int32 k = 0; k < Seg; ++k)
	{
		const float A = UE_TWO_PI * k / Seg;
		Ring.Add(Ctr + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
	}
	DrawMapFill(P, Info.MapTexture, Ring, Ctr, [&](const FVector2D& Sc) { return Info.ToUV(X.ToWorld(Sc)); }, Opacity);
	P.Arc(Ctr, R - 1.0f * S, 2.0f * S, 0.0f, UE_TWO_PI, FLinearColor(0.0f, 0.0f, 0.0f, 0.25f * Opacity));
	P.Arc(Ctr, R + Rim - 1.0f * S, 1.5f * S, 0.0f, UE_TWO_PI, FLinearColor(1.0f, 0.93f, 0.8f, 0.12f * Opacity));

	// Names + icons of nearby places (priority order, no overlaps, fully inside the disc).
	TArray<int32> Order;
	for (int32 i = 0; i < Info.Regions.Num(); ++i)
	{
		const FKGMapRegion& Rg = Info.Regions[i];
		if (Rg.Priority >= 0 && Rg.Layer != 0)
		{
			Order.Add(i);
		}
	}
	Order.Sort([&](int32 A, int32 B) { return Info.Regions[A].Priority > Info.Regions[B].Priority; });
	TArray<FBox2D> Placed;
	Placed.Add(FBox2D(Ctr - FVector2D(14.0f, 14.0f) * S, Ctr + FVector2D(14.0f, 14.0f) * S));   // keep "you" clear
	const float Inside = R - 6.0f * S;
	auto InDisc = [&](const FBox2D& B)
	{
		const FVector2D Cs[4] = {B.Min, B.Max, FVector2D(B.Min.X, B.Max.Y), FVector2D(B.Max.X, B.Min.Y)};
		for (const FVector2D& C : Cs)
		{
			if (FVector2D::Distance(C, Ctr) > Inside)
			{
				return false;
			}
		}
		return true;
	};
	for (const int32 i : Order)
	{
		const FKGMapRegion& Rg = Info.Regions[i];
		const FVector2D L = X.ToScreen(Rg.LabelPos);
		if (FVector2D::Distance(L, Ctr) > R + 60.0f * S)
		{
			continue;
		}
		const bool bIcon = !Rg.Icon.IsNone();
		const bool bStreet = Rg.Layer == 1;
		if (bStreet && Rg.Priority < 20)
		{
			continue;
		}
		const float IconD = 19.0f * S;
		FBox2D IconBox(L - FVector2D(IconD, IconD) * 0.55f, L + FVector2D(IconD, IconD) * 0.55f);
		const float Px = bStreet ? 12.0f : 13.0f;
		const float LabelMid = bIcon ? L.Y + IconD * 0.5f + 9.0f * S : L.Y;
		const FString Name = Rg.Name.ToString();
		FBox2D Box = HaloLabel(P, Name, L.X, LabelMid, Px, Cream, !bStreet, 0.0f, false);
		const bool bLabelFits = InDisc(Box) && !Overlaps(Placed, Box) && (!bIcon || !Overlaps(Placed, IconBox));
		// Icons without room for their name only for major landmarks (keeps the square readable).
		if (bIcon && (bLabelFits || Rg.Priority >= 60) && InDisc(IconBox) && !Overlaps(Placed, IconBox))
		{
			MapBadge(P, GlyphOf(Rg.Icon), L, bLabelFits ? IconD : IconD * 0.8f, Opacity);
			Placed.Add(IconBox);
		}
		if (bLabelFits)
		{
			HaloLabel(P, Name, L.X, LabelMid, Px, WithAlpha(bStreet ? WithAlpha(Cream, 0.82f) : Cream, Opacity), !bStreet);
			Placed.Add(Box);
		}
	}

	// Your chores: pins inside, rim arrows outside.
	const float Pulse = static_cast<float>(FMath::Frac(F.Fx->Now * 0.8));
	for (const FKGChorePin& Pin : Chores)
	{
		if (Pin.bDone || Pin.bRowOnly)
		{
			continue;
		}
		const FVector2D Sc = X.ToScreen(Pin.Pos);
		const FVector2D D = Sc - Ctr;
		const float Dist = static_cast<float>(D.Size());
		if (Dist <= R - 16.0f * S)
		{
			ChorePin(P, Sc, 20.0f * S, Opacity, Pin.bActive ? 0.0f : Pulse);
		}
		else
		{
			const FVector2D U = D / FMath::Max(Dist, 1.0f);
			const FVector2D N(-U.Y, U.X);
			const FVector2D Tip = Ctr + U * (R + Rim + 7.0f * S);
			const FVector2D Base = Ctr + U * (R - 3.0f * S);
			const FVector2D Tri[3] = {Tip, Base + N * (8.0f * S), Base - N * (8.0f * S)};
			const FVector2D TriO[3] = {Tip + U * (2.5f * S), Base + N * (10.5f * S) - U * (1.5f * S), Base - N * (10.5f * S) - U * (1.5f * S)};
			P.Poly(MakeArrayView(TriO), WithAlpha(InkBottom, 0.95f * Opacity), WithAlpha(InkBottom, 0.95f * Opacity));
			P.Poly(MakeArrayView(Tri), WithAlpha(Gold, Opacity), WithAlpha(Srgb(236, 150, 40), Opacity));
		}
	}
	DigMapMarks(F, [&X](const FVector2D& W) { return X.ToScreen(W); }, 20.0f * S, Opacity, &Ctr, R);   // KG_DIG hook
	ManorMapMarks(F, [&X](const FVector2D& W) { return X.ToScreen(W); }, 20.0f * S, Opacity, &Ctr, R);   // SPRINT-040 hook

	// You: view cone + arrow (always up: the map turns, you don't).
	FPts Cone;
	Cone.Add(Ctr);
	for (int32 k = 0; k <= 12; ++k)
	{
		const float A = -UE_HALF_PI - 0.62f + 1.24f * k / 12.0f;
		Cone.Add(Ctr + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R * 0.55f));
	}
	P.PolyEx(Cone, WithAlpha(Cream, 0.22f * Opacity), WithAlpha(Cream, 0.0f), 0.0f, &Ctr, true, Ctr.Y - R * 0.55f, Ctr.Y);
	PlayerArrow(P, Ctr, 15.0f * S, 0.0f, Opacity);

	// North marker riding the rim.
	const FVector2D North = X.ToScreen(Me + FVector2D(0.0, -1.0) * ViewRadius) - Ctr;
	const FVector2D NC = Ctr + North.GetSafeNormal() * (R + Rim * 0.5f);
	P.Circle(NC, 11.0f * S, WithAlpha(InkBottom, 0.96f * Opacity));
	P.Arc(NC, 11.0f * S, 1.5f * S, 0.0f, UE_TWO_PI, WithAlpha(Gold, 0.9f * Opacity));
	P.TextMid(TEXT("N"), NC.X, NC.Y, 13.0f, WithAlpha(Gold, Opacity), 0.5f, true, 0.0f, 0.0f);

	// [M] map button on the lower-left rim.
	const FVector2D KC = Ctr + FVector2D(-0.72f, 0.72f) * (R + Rim * 0.3f);
	const float KH = P.KeycapHeight(13.0f);
	const float KW = P.KeycapWidth(TEXT("M"), 13.0f);
	P.Keycap(TEXT("M"), KC.X - KW * 0.5f, KC.Y - KH * 0.5f, 13.0f, Opacity);

	// Current location under the disc.
	FString Where = RegionName(Info, M.Here);
	FString Sub = M.District != INDEX_NONE && M.District != M.Here ? RegionName(Info, M.District) : FString();
	if (Where.IsEmpty())
	{
		Where = Sub.IsEmpty() ? MapTitleOf(Info) : Sub;
		Sub.Reset();
	}
	Where = Where.ToUpper();
	Sub = Sub.ToUpper();
	const float WW = static_cast<float>(P.Measure(Where, 15.0f, true, 1.5f).X);
	const float SW = Sub.IsEmpty() ? 0.0f : static_cast<float>(P.Measure(Sub, 12.0f, true, 1.5f).X) + 22.0f * S;
	const float PillW = FMath::Min(WW + SW + 44.0f * S, 2.0f * R + 60.0f * S);
	const float PillH = 32.0f * S;
	const float PillY = Ctr.Y + R + Rim + 12.0f * S;
	P.RoundRect(Ctr.X - PillW * 0.5f, PillY, PillW, PillH, PillH * 0.5f, WithAlpha(InkBottom, 0.66f * Opacity),
	            WithAlpha(InkBottom, 0.78f * Opacity));
	const float X0 = Ctr.X - (WW + SW) * 0.5f + 8.0f * S;
	P.Diamond(FVector2D(X0 - 12.0f * S, PillY + PillH * 0.5f), 4.0f * S, WithAlpha(Gold, Opacity));
	P.TextMid(Where, X0, PillY + PillH * 0.5f, 15.0f, WithAlpha(Cream, Opacity), 0.0f, true, 1.5f);
	if (!Sub.IsEmpty())
	{
		P.TextMid(Sub, X0 + WW + 22.0f * S, PillY + PillH * 0.5f, 12.0f, WithAlpha(Gold, 0.85f * Opacity), 0.0f, true, 1.5f);
	}
}

// -------------------------------------------------------------------------------------------------------------------
// Full-screen map (M): every name, you, your chores, landmark icons, legend
// -------------------------------------------------------------------------------------------------------------------
void DrawFullMap(const FKGHudFrame& F, FKGMapFx& M, const AKGMapInfo& Info, const FVector2D* Me, float Yaw,
                 const TArray<FKGChorePin>& Chores)
{
	const float A = EaseOutCubic(M.FullAlpha);
	if (A <= 0.003f)
	{
		return;
	}
	const FKGPainter& P = F.P;
	const float S = F.S;
	const FVector2D Screen[4] = {FVector2D(0.0f, 0.0f), FVector2D(F.W, 0.0f), FVector2D(F.W, F.H), FVector2D(0.0f, F.H)};
	P.Poly(MakeArrayView(Screen), WithAlpha(InkBottom, 0.6f * A), WithAlpha(InkBottom, 0.78f * A), 0.0f);

	const float Pad = 22.0f * S;
	const float HeadH = 66.0f * S;
	const float LegendW = 318.0f * S;
	const float Ms = FMath::Min(F.H - HeadH - Pad - 90.0f * S, F.W - LegendW - 3.0f * Pad - 80.0f * S);
	const float CardW = Pad + Ms + Pad + LegendW + Pad;
	const float CardH = HeadH + Ms + Pad;
	const float CX0 = F.CX - CardW * 0.5f;
	const float CY0 = F.CY - CardH * 0.5f + (1.0f - A) * 16.0f * S;
	P.Card(CX0, CY0, CardW, CardH, A, 22.0f * S, true);
	P.TopBand(CX0, CY0, CardW, 22.0f * S, 6.0f * S, WithAlpha(Gold, A));

	// Header.
	P.TextMid(MapTitleOf(Info).ToUpper(), CX0 + Pad + 4.0f * S, CY0 + 36.0f * S, 30.0f, WithAlpha(Cream, A), 0.0f, true, 5.0f);
	const float TitleW = static_cast<float>(P.Measure(MapTitleOf(Info).ToUpper(), 30.0f, true, 5.0f).X);
	P.TextMid(TEXT("VILLAGE MAP"), CX0 + Pad + TitleW + 22.0f * S, CY0 + 38.0f * S, 14.0f, WithAlpha(Gold, 0.9f * A), 0.0f,
	          true, 3.0f);
	const FString CloseLabel = M.bHolding ? TEXT("Release to close") : TEXT("Close  ·  hold to peek");
	P.KeyHint(TEXT("M"), CloseLabel, CX0 + CardW - Pad, CY0 + 36.0f * S, 16.0f, WithAlpha(Cream, 0.85f), 1.0f, A);

	// Map.
	const float MX = CX0 + Pad;
	const float MY = CY0 + HeadH;
	const FVector2D Span = (Info.WorldMax - Info.WorldMin).ComponentMax(FVector2D(1.0, 1.0));
	auto ToScreen = [&](const FVector2D& W) { return FVector2D(MX, MY) + (W - Info.WorldMin) / Span * Ms; };
	FPts Frame;
	RoundRectPoints(MX, MY, Ms, Ms, 16.0f * S, Frame);
	P.Shadow(MX, MY + 3.0f * S, Ms, Ms, 16.0f * S, 10.0f * S, 0.4f * A);
	DrawMapFill(P, Info.MapTexture, Frame, FVector2D(MX + Ms * 0.5f, MY + Ms * 0.5f),
	            [&](const FVector2D& Sc) { return (Sc - FVector2D(MX, MY)) / Ms; }, A);
	P.Outline(MX, MY, Ms, Ms, 16.0f * S, FMath::Max(1.0f, 2.0f * S), FLinearColor(0.0f, 0.0f, 0.0f, 0.3f * A));

	auto InMap = [&](const FBox2D& B)
	{
		return B.Min.X >= MX + 4.0f * S && B.Max.X <= MX + Ms - 4.0f * S && B.Min.Y >= MY + 4.0f * S && B.Max.Y <= MY + Ms - 4.0f * S;
	};
	TArray<int32> Order;
	for (int32 i = 0; i < Info.Regions.Num(); ++i)
	{
		if (Info.Regions[i].Priority >= 0)
		{
			Order.Add(i);
		}
	}
	Order.Sort([&](int32 X, int32 Y) { return Info.Regions[X].Priority > Info.Regions[Y].Priority; });
	TArray<FBox2D> Placed;
	TSet<EKGMapGlyph> Used;
	// Districts first as big faint titles (under everything), then icons + place names, then streets.
	for (const int32 i : Order)
	{
		const FKGMapRegion& Rg = Info.Regions[i];
		if (Rg.Layer != 0)
		{
			continue;
		}
		const FVector2D L = ToScreen(Rg.LabelPos);
		const FString Name = Rg.Name.ToString().ToUpper();
		const FBox2D Box = HaloLabel(P, Name, L.X, L.Y, 21.0f, Cream, true, 4.0f, false);
		if (InMap(Box) && !Overlaps(Placed, Box))
		{
			HaloLabel(P, Name, L.X, L.Y, 21.0f, WithAlpha(Srgb(255, 236, 200), 0.92f * A), true, 4.0f);
			Placed.Add(Box);
		}
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const int32 i : Order)
		{
			const FKGMapRegion& Rg = Info.Regions[i];
			const bool bStreet = Rg.Layer == 1;
			if (Rg.Layer == 0 || bStreet != (Pass == 1))
			{
				continue;
			}
			const FVector2D L = ToScreen(Rg.LabelPos);
			const bool bIcon = !Rg.Icon.IsNone();
			const float IconD = 24.0f * S;
			const FBox2D IconBox(L - FVector2D(IconD, IconD) * 0.55f, L + FVector2D(IconD, IconD) * 0.55f);
			const float Px = bStreet ? 12.0f : 15.0f;
			const float Mid = bIcon ? L.Y + IconD * 0.5f + 11.0f * S : L.Y;
			const FString Name = Rg.Name.ToString();
			const FBox2D Box = HaloLabel(P, Name, L.X, Mid, Px, Cream, !bStreet, 0.0f, false);
			if (bIcon && InMap(IconBox) && !Overlaps(Placed, IconBox))
			{
				MapBadge(P, GlyphOf(Rg.Icon), L, IconD, A);
				Placed.Add(IconBox);
				Used.Add(GlyphOf(Rg.Icon));
			}
			if (InMap(Box) && !Overlaps(Placed, Box))
			{
				HaloLabel(P, Name, L.X, Mid, Px, WithAlpha(bStreet ? WithAlpha(Cream, 0.85f) : Cream, A), !bStreet);
				Placed.Add(Box);
			}
		}
	}

	// Chores (SPRINT-025: every open step as a pin with its chore name; the active one in lantern orange), then you.
	const float Pulse = static_cast<float>(FMath::Frac(F.Fx->Now * 0.8));
	for (const FKGChorePin& Pin : Chores)
	{
		if (Pin.bDone || Pin.bRowOnly)
		{
			continue;
		}
		const FVector2D Sc = ToScreen(Pin.Pos);
		ChorePin(P, Sc, (Pin.bActive ? 30.0f : 26.0f) * S, A, Pin.bActive ? 0.0f : Pulse);
		const FString Tag = Pin.bItem ? FString::Printf(TEXT("Your %s"), *Pin.Name) : Pin.Name;
		const FBox2D Box = HaloLabel(P, Tag, Sc.X, Sc.Y + 12.0f * S, 13.0f, Cream, true, 0.0f, false);
		if (InMap(Box))
		{
			HaloLabel(P, Tag, Sc.X, Sc.Y + 12.0f * S, 13.0f, WithAlpha(Pin.bActive ? Lantern : Cream, A), true);
		}
	}
	DigMapMarks(F, [&ToScreen](const FVector2D& W) { return ToScreen(W); }, 26.0f * S, A, nullptr, 0.0f);   // KG_DIG hook
	ManorMapMarks(F, [&ToScreen](const FVector2D& W) { return ToScreen(W); }, 26.0f * S, A, nullptr, 0.0f);   // SPRINT-040 hook
	if (Me)
	{
		const FVector2D You = ToScreen(*Me);
		const float Ring = static_cast<float>(FMath::Frac(F.Fx->Now * 0.9));
		P.Arc(You, (12.0f + 26.0f * Ring) * S, 2.5f * S, 0.0f, UE_TWO_PI, WithAlpha(FLinearColor::White, 0.7f * (1.0f - Ring) * A));
		PlayerArrow(P, You, 17.0f * S, FMath::DegreesToRadians(Yaw + 90.0f), A);
	}

	// Legend column.
	const float LX = MX + Ms + Pad;
	float LY = MY + 4.0f * S;
	P.RoundRect(LX, MY, LegendW, Ms, 16.0f * S, FLinearColor(1.0f, 1.0f, 1.0f, 0.04f * A));
	const float In = 18.0f * S;
	LY += 26.0f * S;
	P.TextMid(TEXT("YOU ARE HERE"), LX + In, LY, 13.0f, WithAlpha(Gold, A), 0.0f, true, 2.5f);
	LY += 28.0f * S;
	FString Where = RegionName(Info, M.Here);
	const FString Dist = M.District != INDEX_NONE && M.District != M.Here ? RegionName(Info, M.District) : FString();
	if (Where.IsEmpty())
	{
		Where = Dist.IsEmpty() ? MapTitleOf(Info) : Dist;
	}
	P.TextMid(Where, LX + In, LY, 22.0f, WithAlpha(Cream, A), 0.0f, true);
	if (!Dist.IsEmpty() && Dist != Where)
	{
		LY += 24.0f * S;
		P.TextMid(Dist.ToUpper(), LX + In, LY, 13.0f, WithAlpha(Cream, 0.6f * A), 0.0f, true, 2.0f);
	}
	LY += 30.0f * S;
	P.Rect(LX + In, LY, LegendW - 2.0f * In, FMath::Max(1.0f, S), FLinearColor(1.0f, 0.93f, 0.8f, 0.1f * A));
	LY += 26.0f * S;
	P.TextMid(TEXT("LEGEND"), LX + In, LY, 13.0f, WithAlpha(Gold, A), 0.0f, true, 2.5f);
	LY += 30.0f * S;
	const float Row = 30.0f * S;
	PlayerArrow(P, FVector2D(LX + In + 11.0f * S, LY), 12.0f * S, 0.0f, A);
	P.TextMid(TEXT("You"), LX + In + 32.0f * S, LY, 15.0f, WithAlpha(Cream, A), 0.0f, true);
	ChorePin(P, FVector2D(LX + LegendW * 0.5f + 11.0f * S, LY + 10.0f * S), 18.0f * S, A);
	P.TextMid(FString::Printf(TEXT("Your chores (%d)"), Chores.Num()), LX + LegendW * 0.5f + 30.0f * S, LY, 15.0f,
	          WithAlpha(Cream, A), 0.0f, true);
	LY += Row + 4.0f * S;
	// Swatches matching the rendered texture.
	struct FSwatch
	{
		const TCHAR* Label;
		FLinearColor Col;
	};
	const FSwatch Swatches[] = {{TEXT("Homes"), Srgb(228, 106, 74)}, {TEXT("Public buildings"), Srgb(242, 177, 60)},
	                            {TEXT("Towers"), Srgb(122, 103, 216)}, {TEXT("Streets"), Srgb(245, 234, 208)},
	                            {TEXT("Water"), Srgb(79, 179, 217)}, {TEXT("Piers"), Srgb(176, 122, 72)}};
	for (int32 k = 0; k < UE_ARRAY_COUNT(Swatches); ++k)
	{
		const float SX = LX + In + (k % 2) * (LegendW - 2.0f * In) * 0.5f;
		const float SY = LY + (k / 2) * Row;
		P.RoundRect(SX, SY - 8.0f * S, 22.0f * S, 16.0f * S, 4.0f * S, WithAlpha(Swatches[k].Col, A));
		P.Outline(SX, SY - 8.0f * S, 22.0f * S, 16.0f * S, 4.0f * S, FMath::Max(1.0f, S), WithAlpha(InkBottom, 0.8f * A));
		P.TextMid(Swatches[k].Label, SX + 32.0f * S, SY, 14.0f, WithAlpha(Cream, 0.9f * A), 0.0f, false);
	}
	LY += Row * 3.0f + 6.0f * S;
	// Icons actually on this map.
	TArray<EKGMapGlyph> Glyphs = Used.Array();
	Glyphs.Sort([](EKGMapGlyph X, EKGMapGlyph Y) { return static_cast<uint8>(X) < static_cast<uint8>(Y); });
	const float Bottom = MY + Ms - 18.0f * S;
	for (int32 k = 0; k < Glyphs.Num(); ++k)
	{
		const float GX = LX + In + (k % 2) * (LegendW - 2.0f * In) * 0.5f;
		const float GY = LY + (k / 2) * Row;
		if (GY > Bottom)
		{
			break;
		}
		MapBadge(P, Glyphs[k], FVector2D(GX + 11.0f * S, GY), 20.0f * S, A);
		P.TextMid(GlyphLegend(Glyphs[k]), GX + 30.0f * S, GY, 14.0f, WithAlpha(Cream, 0.9f * A), 0.0f, false);
	}
}

// -------------------------------------------------------------------------------------------------------------------
// Entry point from AKGHUD::DrawHUD
// -------------------------------------------------------------------------------------------------------------------
void DrawMapLayer(const FKGHudFrame& F, bool bLoadingCard)
{
	FKGMapFx& M = MapFx(F.Hud);
	const AKGMapInfo* Info = MapInfoFor(F, M);
	if (!bLoadingCard)
	{
		DrawWorldChoreHud(F);               // SPRINT-016 hook: compass waypoints, current step, work ring, toasts
	}
	DrawDigLayer(F);                        // KG_DIG hook: shovel prompt, stage ring, dug-up card, grave noise
	Info = DigResolvePlan(F, M, Info);      // KG_DIG hook: below ground the map is the underground plan
	if (!Info)
	{
		M.bFullOpen = false;
		M.FullAlpha = 0.0f;
		return;
	}
	const APawn* Pawn = F.Pawn;
	FVector2D Me = FVector2D::ZeroVector;
	float Yaw = 0.0f;
	if (Pawn)
	{
		Me = FVector2D(Pawn->GetActorLocation());
		Yaw = F.PC && F.PC->PlayerCameraManager ? F.PC->PlayerCameraManager->GetCameraRotation().Yaw : Pawn->GetControlRotation().Yaw;
		UpdateLocation(F, M, *Info, Me, bLoadingCard);
	}
	// SPRINT-025: M tap toggles, M held shows the big map while down (UI/KGMapInput.h); Esc closes
	// (AKGPlayerController also closes it before opening the pause menu). kg.Map.Debug forces a state for shots.
	if (F.PC && F.PC->IsLocalController())
	{
		const int32 Forced = MapDebug();
		const bool bKeyDown = Forced == 1 || (Forced == 0 && F.PC->IsInputKeyDown(EKeys::M) && (Pawn || M.Input.bOpen));
		if (KGMapInput::Update(M.Input, bKeyDown, F.Fx->Now))
		{
			KGAudio::UI(F.Hud, TEXT("S_ReelClick"), 0.35f);
		}
		if (Forced == 2)
		{
			M.Input.bOpen = true;
		}
		else if (Forced != M.LastForced)
		{
			// Stabilisation 2026-09-26: a forced "tapped open" (2) behaved like a real tap, so kg.Map.Debug 0 left the
			// map open and the HUD demo shots after it were taken under the map. Leaving a forced state closes it.
			KGMapInput::Close(M.Input);
		}
		M.LastForced = Forced;
		if (M.Input.bOpen && F.PC->WasInputKeyJustPressed(EKeys::Escape))
		{
			KGMapInput::Close(M.Input);
		}
	}
	if (!Pawn)
	{
		KGMapInput::Close(M.Input);
	}
	M.bFullOpen = M.Input.bOpen;
	M.bHolding = KGMapInput::IsHolding(M.Input, F.Fx->Now);
	M.FullAlpha = FMath::FInterpConstantTo(M.FullAlpha, M.bFullOpen ? 1.0f : 0.0f, F.Fx->Dt, 7.0f);

	TArray<FKGChorePin> Chores;
	GatherChores(F, M, Chores);
	if (Pawn && !bLoadingCard && M.FullAlpha < 0.999f)
	{
		DrawChoreMarkers(F, Chores, 1.0f - EaseOutCubic(M.FullAlpha));   // SPRINT-025 item 1: in-world markers
	}
	if (Pawn && M.FullAlpha < 0.999f)
	{
		DrawMinimap(F, M, *Info, Me, Yaw, Chores, 1.0f - EaseOutCubic(M.FullAlpha));
	}
	if (Pawn && !bLoadingCard && M.FullAlpha < 0.5f)
	{
		DrawLocationToast(F, M);
	}
	DrawFullMap(F, M, *Info, Pawn ? &Me : nullptr, Yaw, Chores);
}

// KG_DIG hook: digging HUD + underground plan (leaves and re-opens the anonymous namespace for its includes).
#include "Dig/KGDigHud.inl"

// SPRINT-040 hook: manor secrets + trap pings on the maps (same namespace trick).
#include "Manor/KGManorHud.inl"

// SPRINT-016 hook: world chore HUD (same namespace trick).
#include "Chores/WorldChores/KGWorldChoreHud.inl"

// SPRINT-025 hook: in-world chore markers + the top-left tracker (needs the world chore include above).
#include "UI/KGHUDChoreMarkers.inl"
