#include "Tabletop/KGTableRules.h"

// Dispatcher over FKGBoardState::Game plus the shared end-of-game verdict (repetition, no-progress, material).

void FKGTableRules::Generate(const FKGBoardState& S, TArray<FKGTableMove>& Out)
{
	if (S.Game == EKGTableGame::Chess)
	{
		FKGChessRules::Generate(S, Out);
	}
	else
	{
		FKGDraughtsRules::Generate(S, Out);
	}
}

void FKGTableRules::Make(FKGBoardState& S, const FKGTableMove& M)
{
	if (S.Game == EKGTableGame::Chess)
	{
		FKGChessRules::Make(S, M);
	}
	else
	{
		FKGDraughtsRules::Make(S, M);
	}
}

bool FKGTableRules::FindLegal(const FKGBoardState& S, const FString& Text, FKGTableMove& Out)
{
	FKGTableMove Wanted;
	if (!FKGTableMove::Parse(Text, Wanted))
	{
		return false;
	}
	TArray<FKGTableMove> Legal;
	Generate(S, Legal);
	// Exact match first (a full jump sequence, or a promotion with its piece).
	for (const FKGTableMove& M : Legal)
	{
		if (M == Wanted)
		{
			Out = M;
			return true;
		}
	}
	// "e7e8" without a piece = queen; "c3e5" for a single jump; a bare From/To that names exactly one legal move.
	const FKGTableMove* Only = nullptr;
	int32 Count = 0;
	for (const FKGTableMove& M : Legal)
	{
		if (M.From != Wanted.From || M.To != Wanted.To)
		{
			continue;
		}
		if (S.Game == EKGTableGame::Chess)
		{
			if (Wanted.Promo == 0 && M.Promo == KGPiece::Queen)
			{
				Out = M;
				return true;
			}
			continue;
		}
		if (Wanted.NumHops <= 1)
		{
			Only = &M;
			++Count;
		}
	}
	if (Count == 1)
	{
		Out = *Only;
		return true;
	}
	return false;
}

bool FKGTableRules::IsLegal(const FKGBoardState& S, const FKGTableMove& M)
{
	TArray<FKGTableMove> Legal;
	Generate(S, Legal);
	return Legal.Contains(M);
}

FKGTableVerdict FKGTableRules::Evaluate(const FKGBoardState& S, const TArray<uint64>& History)
{
	FKGTableVerdict V;
	const EKGTableStatus MoverWins = S.Side == 0 ? EKGTableStatus::BlackWins : EKGTableStatus::WhiteWins;
	TArray<FKGTableMove> Legal;
	Generate(S, Legal);
	if (Legal.Num() == 0)
	{
		if (S.Game == EKGTableGame::Chess)
		{
			if (FKGChessRules::InCheck(S))
			{
				V.Status = MoverWins;
				V.Reason = EKGTableEndReason::Checkmate;
			}
			else
			{
				V.Status = EKGTableStatus::Draw;
				V.Reason = EKGTableEndReason::Stalemate;
			}
		}
		else
		{
			V.Status = MoverWins;
			V.Reason = EKGTableEndReason::NoMoves;
		}
		return V;
	}
	const uint64 Key = S.Hash();
	int32 Seen = 0;
	for (const uint64 H : History)
	{
		Seen += H == Key ? 1 : 0;
	}
	if (Seen >= 3)
	{
		V.Status = EKGTableStatus::Draw;
		V.Reason = EKGTableEndReason::Repetition;
		return V;
	}
	const int32 NoProgressLimit = S.Game == EKGTableGame::Chess ? 100 : 80;
	if (S.HalfClock >= NoProgressLimit)
	{
		V.Status = EKGTableStatus::Draw;
		V.Reason = EKGTableEndReason::FiftyMove;
		return V;
	}
	if (S.Game == EKGTableGame::Chess && FKGChessRules::InsufficientMaterial(S))
	{
		V.Status = EKGTableStatus::Draw;
		V.Reason = EKGTableEndReason::Material;
	}
	return V;
}

int64 FKGTableRules::Perft(const FKGBoardState& S, int32 Depth)
{
	return S.Game == EKGTableGame::Chess ? FKGChessRules::Perft(S, Depth) : FKGDraughtsRules::Perft(S, Depth);
}

const TCHAR* FKGTableRules::StatusText(EKGTableStatus Status)
{
	switch (Status)
	{
	case EKGTableStatus::Waiting: return TEXT("Waiting");
	case EKGTableStatus::Playing: return TEXT("Playing");
	case EKGTableStatus::Adjourned: return TEXT("Adjourned");
	case EKGTableStatus::WhiteWins: return TEXT("WhiteWins");
	case EKGTableStatus::BlackWins: return TEXT("BlackWins");
	case EKGTableStatus::Draw: return TEXT("Draw");
	}
	return TEXT("?");
}

const TCHAR* FKGTableRules::ReasonText(EKGTableEndReason Reason)
{
	switch (Reason)
	{
	case EKGTableEndReason::None: return TEXT("None");
	case EKGTableEndReason::Checkmate: return TEXT("Checkmate");
	case EKGTableEndReason::Stalemate: return TEXT("Stalemate");
	case EKGTableEndReason::NoMoves: return TEXT("NoMoves");
	case EKGTableEndReason::Resign: return TEXT("Resign");
	case EKGTableEndReason::Flag: return TEXT("Flag");
	case EKGTableEndReason::Repetition: return TEXT("Repetition");
	case EKGTableEndReason::FiftyMove: return TEXT("FiftyMove");
	case EKGTableEndReason::Material: return TEXT("Material");
	case EKGTableEndReason::Agreement: return TEXT("Agreement");
	case EKGTableEndReason::Abandoned: return TEXT("Abandoned");
	}
	return TEXT("?");
}

const TCHAR* FKGTableRules::ClockText(EKGTableClock Clock)
{
	switch (Clock)
	{
	case EKGTableClock::Untimed: return TEXT("Untimed");
	case EKGTableClock::Bullet: return TEXT("Bullet 1+1");
	case EKGTableClock::Blitz: return TEXT("Blitz 3+2");
	case EKGTableClock::Rapid: return TEXT("Rapid 5+3");
	}
	return TEXT("?");
}

void FKGTableRules::ClockPreset(EKGTableClock Clock, float& OutBase, float& OutIncrement)
{
	switch (Clock)
	{
	case EKGTableClock::Bullet: OutBase = 60.0f; OutIncrement = 1.0f; break;
	case EKGTableClock::Blitz: OutBase = 180.0f; OutIncrement = 2.0f; break;
	case EKGTableClock::Rapid: OutBase = 300.0f; OutIncrement = 3.0f; break;
	default: OutBase = 0.0f; OutIncrement = 0.0f; break;
	}
}
