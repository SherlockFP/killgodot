#include "Chores/WorldChores/KGWorldChoreTypes.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "KillGodot.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Chores/WorldChores/KGWorldChoreData.gen.inl"
#include "Chores/WorldChores/KGWorldChoreData_StormManor.gen.inl"

namespace KGWorldChoreData
{
	constexpr double M = 100.0;

	/** One entry per map: the embedded JSON chunks and the resolved JSON dev builds read from disk. */
	struct FSource
	{
		const TCHAR* const* Chunks;
		int32 NumChunks;
		const TCHAR* ResolvedFile;
	};

	const TArray<FSource>& Sources()
	{
		static const TArray<FSource> S = {
			{GKGWorldChoreJsonChunks, UE_ARRAY_COUNT(GKGWorldChoreJsonChunks), TEXT("Tools/Level/morrowmere_world_chores.resolved.json")},
			{GKGWorldChoreJsonChunks_StormManor, UE_ARRAY_COUNT(GKGWorldChoreJsonChunks_StormManor),
			 TEXT("Tools/Level/stormmanor_world_chores.resolved.json")},
		};
		return S;
	}

	FString EmbeddedJson(int32 Index = 0)
	{
		FString Out;
		const FSource& Src = Sources()[Index];
		for (int32 i = 0; i < Src.NumChunks; ++i)
		{
			Out += Src.Chunks[i];
		}
		return Out;
	}

	FString ResolvedPath(int32 Index = 0)
	{
		return FPaths::ProjectDir() / Sources()[Index].ResolvedFile;
	}

	FVector ReadVec(const TArray<TSharedPtr<FJsonValue>>* Arr, double Scale, double DefZ = 0.0)
	{
		FVector V(0.0, 0.0, DefZ);
		if (Arr)
		{
			for (int32 i = 0; i < FMath::Min(3, Arr->Num()); ++i)
			{
				V[i] = (*Arr)[i]->AsNumber();
			}
		}
		return V * Scale;
	}

	EKGWorldCue CueFrom(const FString& S)
	{
		static const TMap<FString, EKGWorldCue> Map = {
			{TEXT("Thud"), EKGWorldCue::Thud}, {TEXT("Crank"), EKGWorldCue::Crank}, {TEXT("Pour"), EKGWorldCue::Pour},
			{TEXT("Knock"), EKGWorldCue::Knock}, {TEXT("Flame"), EKGWorldCue::Flame}, {TEXT("Whoosh"), EKGWorldCue::Whoosh},
			{TEXT("Knot"), EKGWorldCue::Knot}, {TEXT("Chop"), EKGWorldCue::Chop}, {TEXT("Paper"), EKGWorldCue::Paper},
			{TEXT("Grind"), EKGWorldCue::Grind}, {TEXT("Splash"), EKGWorldCue::Splash}};
		const EKGWorldCue* Found = Map.Find(S);
		return Found ? *Found : EKGWorldCue::None;
	}
}

// ---- chore def ---------------------------------------------------------------------------------------------------------

FName FKGWorldChoreDef::Resolve(FName AnchorOrVar, int32 Variant) const
{
	const FString S = AnchorOrVar.ToString();
	if (!S.StartsWith(TEXT("$")) || Variants.Num() == 0)
	{
		return AnchorOrVar;
	}
	const FKGWorldVariant& V = Variants[FMath::Clamp(Variant, 0, Variants.Num() - 1)];
	const FName* Found = V.Vars.Find(FName(*S.Mid(1)));
	return Found ? *Found : AnchorOrVar;
}

TArray<FName> FKGWorldChoreDef::Targets(int32 Step, int32 Variant) const
{
	TArray<FName> Out;
	if (Steps.IsValidIndex(Step))
	{
		for (const FName At : Steps[Step].At)
		{
			Out.Add(Resolve(At, Variant));
		}
	}
	return Out;
}

FString FKGWorldChoreDef::StepLabel(int32 Step, int32 Variant) const
{
	if (!Steps.IsValidIndex(Step))
	{
		return FString();
	}
	FString Label = Steps[Step].Label;
	if (Variants.Num() > 0)
	{
		const FKGWorldVariant& V = Variants[FMath::Clamp(Variant, 0, Variants.Num() - 1)];
		for (const TPair<FName, FName>& Pair : V.Vars)
		{
			const FKGWorldAnchor* A = FKGWorldChoreCatalog::Get().FindAnchor(Pair.Value);
			Label.ReplaceInline(*(TEXT("$") + Pair.Key.ToString()), A ? *A->Label : *Pair.Value.ToString());
		}
	}
	return Label;
}

bool FKGWorldChoreDef::NeedsClimb() const
{
	const FKGWorldChoreCatalog& C = FKGWorldChoreCatalog::Get();
	for (int32 v = 0; v < NumVariants(); ++v)
	{
		for (int32 s = 0; s < Steps.Num(); ++s)
		{
			for (const FName AnchorId : Targets(s, v))
			{
				const FKGWorldAnchor* A = C.FindAnchor(AnchorId);
				if (A && A->ClimbCm > 1.0f)
				{
					return true;
				}
			}
		}
	}
	return false;
}

// ---- catalog -----------------------------------------------------------------------------------------------------------

namespace KGWorldChoreData
{
	FKGWorldChoreCatalog LoadOne(int32 Index)
	{
		FKGWorldChoreCatalog C;
		FString Error;
		FString Json;
#if !UE_BUILD_SHIPPING
		// Dev: the resolved JSON next to the tools wins, so tuning anchors needs no C++ rebuild.
		if (!FFileHelper::LoadFileToString(Json, *ResolvedPath(Index)))
		{
			Json.Reset();
		}
#endif
		if (Json.IsEmpty() || !C.Parse(Json, Error))
		{
			if (!Json.IsEmpty())
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_WORLDCHORE data on disk broken (%s) - using the embedded copy"), *Error);
			}
			C = FKGWorldChoreCatalog();
			if (!C.Parse(EmbeddedJson(Index), Error))
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_WORLDCHORE embedded data broken: %s"), *Error);
				C = FKGWorldChoreCatalog();
			}
		}
		return C;
	}

	struct FCatalogSet
	{
		TArray<FKGWorldChoreCatalog> All;
		int32 Active = 0;
	};

	FCatalogSet& Set()
	{
		static FCatalogSet S = []
		{
			FCatalogSet Out;
			for (int32 i = 0; i < Sources().Num(); ++i)
			{
				Out.All.Add(LoadOne(i));
			}
			return Out;
		}();
		return S;
	}

	FKGWorldChoreCatalog& Mutable()
	{
		FCatalogSet& S = Set();
		return S.All[S.Active];
	}
}

const FKGWorldChoreCatalog& FKGWorldChoreCatalog::Get()
{
	return KGWorldChoreData::Mutable();
}

bool FKGWorldChoreCatalog::SelectForWorld(const UWorld* World)
{
	KGWorldChoreData::FCatalogSet& S = KGWorldChoreData::Set();
	for (int32 i = 0; i < S.All.Num(); ++i)
	{
		if (S.All[i].ForMap(World))
		{
			S.Active = i;
			return true;
		}
	}
	return false;
}

const FKGWorldChoreCatalog* FKGWorldChoreCatalog::FindByMap(const FString& InMapName)
{
	for (const FKGWorldChoreCatalog& C : KGWorldChoreData::Set().All)
	{
		if (C.MapName.Equals(InMapName, ESearchCase::IgnoreCase))
		{
			return &C;
		}
	}
	return nullptr;
}

bool FKGWorldChoreCatalog::Reload(FString& OutMessage)
{
	KGWorldChoreData::FCatalogSet& S = KGWorldChoreData::Set();
	for (int32 i = 0; i < S.All.Num(); ++i)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *KGWorldChoreData::ResolvedPath(i)))
		{
			Json = KGWorldChoreData::EmbeddedJson(i);
		}
		FKGWorldChoreCatalog Fresh;
		FString Error;
		if (!Fresh.Parse(Json, Error))
		{
			OutMessage = Error;
			return false;
		}
		S.All[i] = MoveTemp(Fresh);
	}
	OutMessage = FString::Printf(TEXT("%s: %d chores, %d anchors"), *Get().MapName, Get().Chores.Num(), Get().Anchors.Num());
	return true;
}

bool FKGWorldChoreCatalog::Parse(const FString& Json, FString& OutError)
{
	using namespace KGWorldChoreData;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("not JSON");
		return false;
	}
	MapName = Root->GetStringField(TEXT("map"));
	WalkSpeed = static_cast<float>(Root->GetNumberField(TEXT("walk_speed")) * M);
	ClimbSpeed = static_cast<float>(Root->GetNumberField(TEXT("climb_speed")) * M);
	const TArray<TSharedPtr<FJsonValue>>* HubArr = nullptr;
	Root->TryGetArrayField(TEXT("hub"), HubArr);
	Hub = ReadVec(HubArr, M);

	const TSharedPtr<FJsonObject>* ItemsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("items"), ItemsObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ItemsObj)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			FKGWorldItemDef& I = Items.AddDefaulted_GetRef();
			I.Kind = FName(*Pair.Key);
			I.Label = O->GetStringField(TEXT("label"));
			I.MeshPath = O->GetStringField(TEXT("mesh"));
			const TArray<TSharedPtr<FJsonValue>>* ScaleArr = nullptr;
			double Scale = 1.0;
			if (O->TryGetArrayField(TEXT("scale"), ScaleArr))
			{
				I.MeshScale = ReadVec(ScaleArr, 1.0, 1.0);
			}
			else if (O->TryGetNumberField(TEXT("scale"), Scale))
			{
				I.MeshScale = FVector(Scale);
			}
			const TArray<TSharedPtr<FJsonValue>>* Box = nullptr;
			O->TryGetArrayField(TEXT("box"), Box);
			I.BoxExtent = ReadVec(Box, 1.0, 15.0);
			I.MeshZ = static_cast<float>(O->GetNumberField(TEXT("mesh_z")));
			I.MassKg = static_cast<float>(O->GetNumberField(TEXT("mass")));
			I.Speed = static_cast<float>(O->GetNumberField(TEXT("speed")));
			O->TryGetBoolField(TEXT("two_person"), I.bTwoPerson);
			O->TryGetBoolField(TEXT("liquid"), I.bLiquid);
			O->TryGetBoolField(TEXT("flame"), I.bFlame);
			O->TryGetNumberField(TEXT("count"), I.Count);
			const TArray<TSharedPtr<FJsonValue>>* Tint = nullptr;
			if (O->TryGetArrayField(TEXT("tint"), Tint))
			{
				const FVector T = ReadVec(Tint, 1.0);
				I.Tint = FLinearColor(T.X, T.Y, T.Z);
				I.bTinted = true;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* AnchorArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("anchors"), AnchorArr))
	{
		OutError = TEXT("no anchors");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& V : *AnchorArr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FKGWorldAnchor& A = Anchors.AddDefaulted_GetRef();
		A.Id = FName(*O->GetStringField(TEXT("id")));
		A.Kind = FName(*O->GetStringField(TEXT("kind")));
		A.Label = O->GetStringField(TEXT("label"));
		O->TryGetStringField(TEXT("name"), A.Name);
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		O->TryGetArrayField(TEXT("at"), At);
		A.Location = ReadVec(At, M, O->GetNumberField(TEXT("z")));
		A.Location.Z = O->GetNumberField(TEXT("z")) * M;
		A.Yaw = static_cast<float>(O->GetNumberField(TEXT("yaw")));
		A.RadiusCm = static_cast<float>(O->GetNumberField(TEXT("r")) * M);
		const TArray<TSharedPtr<FJsonValue>>* Stand = nullptr;
		A.Stand = O->TryGetArrayField(TEXT("stand"), Stand) ? ReadVec(Stand, M) : A.Location;
		double Climb = 0.0;
		O->TryGetNumberField(TEXT("climb_m"), Climb);
		A.ClimbCm = static_cast<float>(Climb * M);
		FString Sab;
		if (O->TryGetStringField(TEXT("sabotage"), Sab))
		{
			A.Sabotage = FName(*Sab);
		}
		AnchorLookup.Add(A.Id, Anchors.Num() - 1);
	}

	const TArray<TSharedPtr<FJsonValue>>* ChoreArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("chores"), ChoreArr))
	{
		OutError = TEXT("no chores");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& V : *ChoreArr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FKGWorldChoreDef& C = Chores.AddDefaulted_GetRef();
		C.Id = FName(*O->GetStringField(TEXT("id")));
		C.Title = O->GetStringField(TEXT("title"));
		O->TryGetStringField(TEXT("blurb"), C.Blurb);
		O->TryGetBoolField(TEXT("bots"), C.bBots);
		{
			// SPRINT-040 hook: manor secret fields (all optional).
			FString Str;
			if (O->TryGetStringField(TEXT("secret"), Str)) { C.SecretId = FName(*Str); }
			if (O->TryGetStringField(TEXT("reward_secret"), Str)) { C.RewardSecret = FName(*Str); }
			if (O->TryGetStringField(TEXT("reward_compartment"), Str)) { C.RewardCompartment = FName(*Str); }
			double Num = 0.0;
			if (O->TryGetNumberField(TEXT("min_players"), Num)) { C.MinPlayers = static_cast<int32>(Num); }
		}
		const TArray<TSharedPtr<FJsonValue>>* Rep = nullptr;
		if (O->TryGetArrayField(TEXT("replaces"), Rep))
		{
			for (const TSharedPtr<FJsonValue>& R : *Rep)
			{
				C.Replaces.Add(FName(*R->AsString()));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Vars = nullptr;
		if (O->TryGetArrayField(TEXT("variants"), Vars))
		{
			for (const TSharedPtr<FJsonValue>& VV : *Vars)
			{
				const TSharedPtr<FJsonObject> VO = VV->AsObject();
				FKGWorldVariant& Var = C.Variants.AddDefaulted_GetRef();
				Var.Name = FName(*VO->GetStringField(TEXT("name")));
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : VO->GetObjectField(TEXT("vars"))->Values)
				{
					Var.Vars.Add(FName(*P.Key), FName(*P.Value->AsString()));
				}
			}
		}
		for (const TSharedPtr<FJsonValue>& SV : O->GetArrayField(TEXT("steps")))
		{
			const TSharedPtr<FJsonObject> S = SV->AsObject();
			FKGWorldStepDef& St = C.Steps.AddDefaulted_GetRef();
			const FString Verb = S->GetStringField(TEXT("verb"));
			St.Verb = Verb == TEXT("take") ? EKGWorldVerb::Take : Verb == TEXT("bring") ? EKGWorldVerb::Bring
			        : Verb == TEXT("panel") ? EKGWorldVerb::Panel : EKGWorldVerb::Work;
			const TArray<TSharedPtr<FJsonValue>>* AtArr = nullptr;
			if (S->TryGetArrayField(TEXT("at"), AtArr))
			{
				for (const TSharedPtr<FJsonValue>& A : *AtArr)
				{
					St.At.Add(FName(*A->AsString()));
				}
			}
			else
			{
				St.At.Add(FName(*S->GetStringField(TEXT("at"))));
			}
			FString Str;
			if (S->TryGetStringField(TEXT("item"), Str))
			{
				St.Item = FName(*Str);
			}
			if (S->TryGetStringField(TEXT("spawn"), Str))
			{
				St.Spawn = FName(*Str);
			}
			if (St.Verb == EKGWorldVerb::Take)
			{
				St.Spawn = St.Item;
			}
			double Num = 0.0;
			if (S->TryGetNumberField(TEXT("secs"), Num)) { St.Secs = static_cast<float>(Num); }
			if (S->TryGetNumberField(TEXT("repeat"), Num)) { St.Repeat = FMath::Max(1, static_cast<int32>(Num)); }
			if (S->TryGetNumberField(TEXT("fill"), Num)) { St.Fill = static_cast<float>(Num); }
			if (S->TryGetNumberField(TEXT("min_fill"), Num)) { St.MinFill = static_cast<float>(Num); }
			if (S->TryGetNumberField(TEXT("stage"), Num)) { St.PanelStage = static_cast<int32>(Num); }
			if (S->TryGetNumberField(TEXT("bot_secs"), Num)) { St.BotSecs = static_cast<float>(Num); }
			S->TryGetBoolField(TEXT("consume"), St.bConsume);
			if (S->TryGetStringField(TEXT("panel"), Str)) { St.Panel = FName(*Str); }
			if (S->TryGetStringField(TEXT("cue"), Str)) { St.Cue = CueFrom(Str); }
			St.Label = S->GetStringField(TEXT("label"));
			if (S->TryGetStringField(TEXT("effect"), Str))
			{
				TArray<FString> Parts;
				Str.ParseIntoArray(Parts, TEXT("+"));
				for (const FString& P : Parts)
				{
					if (P == TEXT("water")) { St.Effects |= KGWorldEffect::Water; }
					else if (P == TEXT("stack")) { St.Effects |= KGWorldEffect::Stack; }
					else if (P == TEXT("light")) { St.Effects |= KGWorldEffect::Light; }
					else if (P == TEXT("chop")) { St.Effects |= KGWorldEffect::Chop; }
					else if (P.StartsWith(TEXT("fx:")))
					{
						St.Effects |= KGWorldEffect::Fx;
						St.FxName = FName(*P.Mid(3));
					}
				}
			}
		}
	}
	return Validate(OutError);
}

bool FKGWorldChoreCatalog::Validate(FString& OutError) const
{
	if (Chores.Num() == 0 || Anchors.Num() == 0)
	{
		OutError = TEXT("empty");
		return false;
	}
	for (const FKGWorldChoreDef& C : Chores)
	{
		if (C.Steps.Num() == 0)
		{
			OutError = C.Id.ToString() + TEXT(": no steps");
			return false;
		}
		for (int32 v = 0; v < C.NumVariants(); ++v)
		{
			for (int32 s = 0; s < C.Steps.Num(); ++s)
			{
				const FKGWorldStepDef& St = C.Steps[s];
				for (const FName Id : C.Targets(s, v))
				{
					if (!AnchorLookup.Contains(Id))
					{
						OutError = FString::Printf(TEXT("%s step %d: unknown anchor %s"), *C.Id.ToString(), s, *Id.ToString());
						return false;
					}
				}
				if ((St.Verb == EKGWorldVerb::Bring || St.Verb == EKGWorldVerb::Take) && !FindItem(St.Item))
				{
					OutError = FString::Printf(TEXT("%s step %d: unknown item %s"), *C.Id.ToString(), s, *St.Item.ToString());
					return false;
				}
				if (!St.Spawn.IsNone() && !FindItem(St.Spawn))
				{
					OutError = FString::Printf(TEXT("%s step %d: unknown spawn %s"), *C.Id.ToString(), s, *St.Spawn.ToString());
					return false;
				}
			}
		}
	}
	return true;
}

const FKGWorldChoreDef* FKGWorldChoreCatalog::FindChore(FName Id) const
{
	return Chores.FindByPredicate([Id](const FKGWorldChoreDef& C) { return C.Id == Id; });
}

const FKGWorldItemDef* FKGWorldChoreCatalog::FindItem(FName Kind) const
{
	return Items.FindByPredicate([Kind](const FKGWorldItemDef& I) { return I.Kind == Kind; });
}

int32 FKGWorldChoreCatalog::AnchorIndex(FName Id) const
{
	const int32* Found = AnchorLookup.Find(Id);
	return Found ? *Found : INDEX_NONE;
}

const FKGWorldAnchor* FKGWorldChoreCatalog::FindAnchor(FName Id) const
{
	const int32 Index = AnchorIndex(Id);
	return Anchors.IsValidIndex(Index) ? &Anchors[Index] : nullptr;
}

bool FKGWorldChoreCatalog::ForMap(const UWorld* World) const
{
	if (!World || MapName.IsEmpty())
	{
		return false;
	}
	FString Map = World->GetMapName();
	Map.RemoveFromStart(World->StreamingLevelsPrefix);
	return Map.Equals(MapName, ESearchCase::IgnoreCase);
}

// ---- rules -------------------------------------------------------------------------------------------------------------

TFunction<bool(const FKGWorldChoreDef&)> FKGWorldChoreRules::DealFilter;   // SPRINT-040 hook
FKGOnWorldChoreDone FKGWorldChoreRules::OnChoreDone;                       // SPRINT-040 hook

TArray<FName> FKGWorldChoreRules::Deal(const TArray<FName>& Pool, const FKGWorldChoreCatalog& Catalog, bool bBot, int32 Count, FKGRng& Rng)
{
	TArray<FName> World;
	TArray<FName> Panel;
	bool bSkipped = false;   // SPRINT-040 hook
	for (const FName Id : Pool)
	{
		if (const FKGWorldChoreDef* Def = Catalog.FindChore(Id))
		{
			if (!Def->SecretId.IsNone() || (DealFilter && !DealFilter(*Def)))
			{
				bSkipped = true;
				continue;   // SPRINT-040 hook: secret chores are given, never dealt; the manor filter may veto
			}
			if (!bBot || (Def->bBots && !Def->NeedsClimb()))
			{
				World.AddUnique(Id);
			}
		}
		else
		{
			Panel.AddUnique(Id);
		}
	}
	if (World.Num() == 0)
	{
		// No world chores on this level (greybox, v1): exactly the SPRINT-014 deal (minus SPRINT-040 skipped chores).
		TArray<FName> Mine = bSkipped ? Panel : Pool;
		Rng.Shuffle(Mine);
		Mine.SetNum(FMath::Min(Count, Mine.Num()));
		return Mine;
	}
	// About 70 % world: 4 chores -> 3 world (80 %) or 2 world (20 %) = 2.8 on average.
	const float Want = Count * WorldShare;
	int32 NumWorld = FMath::FloorToInt(Want) + (Rng.FRand() < FMath::Frac(Want) ? 1 : 0);
	NumWorld = FMath::Clamp(NumWorld, 0, World.Num());
	Rng.Shuffle(World);
	TArray<FName> Mine(World.GetData(), NumWorld);
	TSet<FName> Replaced;
	for (const FName Id : Mine)
	{
		Replaced.Append(Catalog.FindChore(Id)->Replaces);
	}
	Rng.Shuffle(Panel);
	Panel.StableSort([&Replaced](const FName& A, const FName& B) { return !Replaced.Contains(A) && Replaced.Contains(B); });
	for (int32 i = 0; i < Panel.Num() && Mine.Num() < Count; ++i)
	{
		Mine.Add(Panel[i]);
	}
	for (int32 i = NumWorld; i < World.Num() && Mine.Num() < Count; ++i)
	{
		Mine.Add(World[i]);   // a panel-poor level: more world chores
	}
	return Mine;
}

float FKGWorldChoreRules::CarrySpeedFactor(const FKGWorldItemDef& Item, int32 Carriers)
{
	if (Carriers <= 0)
	{
		return 1.0f;
	}
	if (Item.bTwoPerson && Carriers >= 2)
	{
		return 1.0f;   // the social twist: a second pair of hands and you walk at full speed
	}
	return Item.Speed;
}

float FKGWorldChoreRules::SpillFill(float Fill, float Dt, bool bCarried, float CarrierSpeed, float WalkSpeed, bool bFalling, float UpZ)
{
	float Loss = 0.0f;
	if (bCarried)
	{
		if (CarrierSpeed > WalkSpeed * RunThreshold)
		{
			Loss += SprintSpillPerSec;
		}
		if (bFalling)
		{
			Loss += FallSpillPerSec;
		}
	}
	if (UpZ < 0.55f)
	{
		Loss += TippedSpillPerSec;   // on its side (dropped, knocked over, thrown)
	}
	return FMath::Clamp(Fill - Loss * Dt, 0.0f, 1.0f);
}

float FKGWorldChoreRules::DwellSecs(const FKGWorldStepDef& Step, bool bSpoiled)
{
	const bool bLamp = (Step.Effects & KGWorldEffect::Light) != 0;
	return Step.Secs + (bSpoiled && bLamp ? SootExtraSecs : 0.0f);
}

int32 FKGWorldChoreRules::PickVariant(const FKGWorldChoreDef& Def, uint32 PlayerHash)
{
	return Def.Variants.Num() > 1 ? static_cast<int32>(HashCombine(PlayerHash, GetTypeHash(Def.Id)) % static_cast<uint32>(Def.Variants.Num())) : 0;
}

const TCHAR* KGWorldChores::CueSound(EKGWorldCue Cue)
{
	switch (Cue)
	{
	case EKGWorldCue::Thud: return TEXT("S_Chore_Thud");
	case EKGWorldCue::Crank: return TEXT("S_Chore_Crank");
	case EKGWorldCue::Pour: return TEXT("S_Chore_Pour");
	case EKGWorldCue::Knock: return TEXT("S_Chore_Stamp");   // (no dedicated knock yet: the stamp's wooden rap)
	case EKGWorldCue::Flame: return TEXT("S_Chore_Flame");
	case EKGWorldCue::Whoosh: return TEXT("S_Chore_Whoosh");
	case EKGWorldCue::Knot: return TEXT("S_Chore_Knot");
	case EKGWorldCue::Chop: return TEXT("S_Chore_Chop");
	case EKGWorldCue::Paper: return TEXT("S_Chore_Paper");
	case EKGWorldCue::Grind: return TEXT("S_Chore_Grind");
	case EKGWorldCue::Splash: return TEXT("S_Chore_Splash");
	case EKGWorldCue::Poison: return TEXT("S_Chore_Squish");
	case EKGWorldCue::Snuff: return TEXT("S_Chore_Whoosh");
	case EKGWorldCue::StepDone: return TEXT("S_UI_Good");
	default: return nullptr;
	}
}

FString KGWorldChores::VerbName(EKGWorldVerb Verb)
{
	switch (Verb)
	{
	case EKGWorldVerb::Take: return TEXT("take");
	case EKGWorldVerb::Work: return TEXT("work");
	case EKGWorldVerb::Bring: return TEXT("bring");
	case EKGWorldVerb::Panel: return TEXT("panel");
	default: return TEXT("?");
	}
}
