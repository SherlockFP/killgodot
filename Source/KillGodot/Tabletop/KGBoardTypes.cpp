#include "Tabletop/KGBoardTypes.h"

#include "Core/KGRng.h"

namespace KGPiece
{
	FString SquareName(int32 Sq)
	{
		if (Sq < 0 || Sq > 63)
		{
			return TEXT("??");
		}
		return FString::Printf(TEXT("%c%d"), TCHAR('a' + FileOf(Sq)), RankOf(Sq) + 1);
	}

	int32 SquareFromName(const FString& Name)
	{
		if (Name.Len() < 2)
		{
			return -1;
		}
		const TCHAR F = FChar::ToLower(Name[0]);
		const TCHAR R = Name[1];
		if (F < 'a' || F > 'h' || R < '1' || R > '8')
		{
			return -1;
		}
		return SquareAt(F - 'a', R - '1');
	}
}

// ---- FKGTableMove ----------------------------------------------------------------------------------------------

bool FKGTableMove::operator==(const FKGTableMove& O) const
{
	if (From != O.From || To != O.To || Promo != O.Promo || NumHops != O.NumHops)
	{
		return false;
	}
	for (int32 i = 0; i < NumHops; ++i)
	{
		if (Hops[i] != O.Hops[i])
		{
			return false;
		}
	}
	return true;
}

FString FKGTableMove::ToString() const
{
	using namespace KGPiece;
	if (NumHops > 0)
	{
		FString S = SquareName(From);
		int32 Prev = From;
		for (int32 i = 0; i < NumHops; ++i)
		{
			const bool bJump = FMath::Abs(RankOf(Hops[i]) - RankOf(Prev)) == 2;
			S += bJump ? TEXT("x") : TEXT("");
			S += SquareName(Hops[i]);
			Prev = Hops[i];
		}
		return S;
	}
	FString S = SquareName(From) + SquareName(To);
	static const TCHAR* Promos = TEXT(" pnbrqk");
	if (Promo >= Knight && Promo <= Queen)
	{
		S.AppendChar(Promos[Promo]);
	}
	return S;
}

bool FKGTableMove::Parse(const FString& Text, FKGTableMove& Out)
{
	using namespace KGPiece;
	Out = FKGTableMove();
	FString T = Text.TrimStartAndEnd().ToLower();
	T.ReplaceInline(TEXT("="), TEXT(""));
	T.ReplaceInline(TEXT("-"), TEXT(""));
	if (T.Contains(TEXT("x")))
	{
		TArray<FString> Parts;
		T.ParseIntoArray(Parts, TEXT("x"), true);
		if (Parts.Num() < 2 || Parts.Num() > 13)
		{
			return false;
		}
		const int32 From = SquareFromName(Parts[0]);
		if (From < 0)
		{
			return false;
		}
		Out.From = static_cast<uint8>(From);
		for (int32 i = 1; i < Parts.Num(); ++i)
		{
			const int32 Sq = SquareFromName(Parts[i]);
			if (Sq < 0)
			{
				return false;
			}
			Out.Hops[Out.NumHops++] = static_cast<uint8>(Sq);
		}
		Out.To = Out.Hops[Out.NumHops - 1];
		return true;
	}
	if (T.Len() < 4)
	{
		return false;
	}
	const int32 From = SquareFromName(T.Mid(0, 2));
	const int32 To = SquareFromName(T.Mid(2, 2));
	if (From < 0 || To < 0)
	{
		return false;
	}
	Out.From = static_cast<uint8>(From);
	Out.To = static_cast<uint8>(To);
	if (T.Len() >= 5)
	{
		switch (T[4])
		{
		case 'n': Out.Promo = Knight; break;
		case 'b': Out.Promo = Bishop; break;
		case 'r': Out.Promo = Rook; break;
		case 'q': Out.Promo = Queen; break;
		default: return false;
		}
	}
	return true;
}

// ---- FKGBoardState ---------------------------------------------------------------------------------------------

void FKGBoardState::Reset(EKGTableGame InGame)
{
	using namespace KGPiece;
	FMemory::Memzero(Sq, sizeof(Sq));
	Game = InGame;
	Side = 0;
	Castling = 0;
	Ep = -1;
	HalfClock = 0;
	Ply = 0;
	if (Game == EKGTableGame::Chess)
	{
		static const uint8 Back[8] = {Rook, Knight, Bishop, Queen, King, Bishop, Knight, Rook};
		for (int32 F = 0; F < 8; ++F)
		{
			Sq[SquareAt(F, 0)] = Make(Back[F], 0);
			Sq[SquareAt(F, 1)] = Make(Pawn, 0);
			Sq[SquareAt(F, 6)] = Make(Pawn, 1);
			Sq[SquareAt(F, 7)] = Make(Back[F], 1);
		}
		Castling = 15;
	}
	else
	{
		for (int32 S = 0; S < 64; ++S)
		{
			if ((FileOf(S) + RankOf(S)) % 2 != 0)
			{
				continue;
			}
			if (RankOf(S) <= 2)
			{
				Sq[S] = Make(Man, 0);
			}
			else if (RankOf(S) >= 5)
			{
				Sq[S] = Make(Man, 1);
			}
		}
	}
}

namespace KGZobrist
{
	struct FTable
	{
		uint64 Piece[16][64];
		uint64 Side;
		uint64 Castling[16];
		uint64 EpFile[8];

		FTable()
		{
			FKGRng Rng(0x5EA7AB1E5ull, 36u);
			auto Next = [&Rng]() { return (static_cast<uint64>(Rng.NextUInt32()) << 32) | Rng.NextUInt32(); };
			for (int32 P = 0; P < 16; ++P)
			{
				for (int32 S = 0; S < 64; ++S)
				{
					Piece[P][S] = Next();
				}
			}
			Side = Next();
			for (int32 C = 0; C < 16; ++C)
			{
				Castling[C] = Next();
			}
			for (int32 F = 0; F < 8; ++F)
			{
				EpFile[F] = Next();
			}
		}
	};

	const FTable& Get()
	{
		static const FTable Table;
		return Table;
	}
}

uint64 FKGBoardState::Hash() const
{
	const KGZobrist::FTable& T = KGZobrist::Get();
	uint64 H = Game == EKGTableGame::Chess ? 0x1ull : 0x2ull;
	for (int32 S = 0; S < 64; ++S)
	{
		if (Sq[S])
		{
			H ^= T.Piece[Sq[S] & 15][S];
		}
	}
	if (Side)
	{
		H ^= T.Side;
	}
	H ^= T.Castling[Castling & 15];
	if (Ep >= 0)
	{
		H ^= T.EpFile[KGPiece::FileOf(Ep)];
	}
	return H;
}

int32 FKGBoardState::CountPieces(uint8 Colour) const
{
	int32 N = 0;
	for (int32 S = 0; S < 64; ++S)
	{
		N += (Sq[S] && KGPiece::ColourOf(Sq[S]) == Colour) ? 1 : 0;
	}
	return N;
}

FString FKGBoardState::ToFen() const
{
	using namespace KGPiece;
	FString Out;
	static const TCHAR* ChessChars = TEXT(" PNBRQK");
	for (int32 R = 7; R >= 0; --R)
	{
		int32 EmptyRun = 0;
		for (int32 F = 0; F < 8; ++F)
		{
			const uint8 P = Sq[SquareAt(F, R)];
			if (!P)
			{
				if (Game == EKGTableGame::Chess)
				{
					++EmptyRun;
				}
				else
				{
					Out.AppendChar('.');
				}
				continue;
			}
			if (EmptyRun)
			{
				Out.AppendInt(EmptyRun);
				EmptyRun = 0;
			}
			TCHAR C;
			if (Game == EKGTableGame::Chess)
			{
				C = ChessChars[TypeOf(P)];
			}
			else
			{
				C = TypeOf(P) == DKing ? 'W' : 'w';
			}
			if (IsBlack(P))
			{
				C = Game == EKGTableGame::Chess ? FChar::ToLower(C) : (TypeOf(P) == DKing ? 'B' : 'b');
			}
			Out.AppendChar(C);
		}
		if (EmptyRun)
		{
			Out.AppendInt(EmptyRun);
		}
		if (R > 0)
		{
			Out.AppendChar('/');
		}
	}
	Out += Side ? TEXT(" b ") : TEXT(" w ");
	if (Game == EKGTableGame::Chess)
	{
		FString C;
		if (Castling & 1) C.AppendChar('K');
		if (Castling & 2) C.AppendChar('Q');
		if (Castling & 4) C.AppendChar('k');
		if (Castling & 8) C.AppendChar('q');
		Out += C.IsEmpty() ? TEXT("-") : C;
		Out += TEXT(" ");
		Out += Ep >= 0 ? SquareName(Ep) : TEXT("-");
		Out += FString::Printf(TEXT(" %d %d"), HalfClock, FullMove());
	}
	else
	{
		Out += FString::Printf(TEXT("%d %d"), HalfClock, Ply);
	}
	return Out;
}

bool FKGBoardState::FromFen(const FString& Fen, EKGTableGame InGame)
{
	using namespace KGPiece;
	TArray<FString> Parts;
	Fen.TrimStartAndEnd().ParseIntoArrayWS(Parts);
	if (Parts.Num() < 2)
	{
		return false;
	}
	FMemory::Memzero(Sq, sizeof(Sq));
	Game = InGame;
	Castling = 0;
	Ep = -1;
	HalfClock = 0;
	Ply = 0;
	int32 R = 7, F = 0;
	for (const TCHAR C : Parts[0])
	{
		if (C == '/')
		{
			--R;
			F = 0;
			continue;
		}
		if (R < 0 || F > 7)
		{
			return false;
		}
		if (FChar::IsDigit(C))
		{
			F += C - '0';
			continue;
		}
		uint8 P = 0;
		if (Game == EKGTableGame::Chess)
		{
			switch (FChar::ToUpper(C))
			{
			case 'P': P = Pawn; break;
			case 'N': P = Knight; break;
			case 'B': P = Bishop; break;
			case 'R': P = Rook; break;
			case 'Q': P = Queen; break;
			case 'K': P = King; break;
			default: return false;
			}
			if (FChar::IsLower(C))
			{
				P |= Black;
			}
		}
		else
		{
			switch (C)
			{
			case '.': ++F; continue;
			case 'w': P = Man; break;
			case 'W': P = DKing; break;
			case 'b': P = Man | Black; break;
			case 'B': P = DKing | Black; break;
			default: return false;
			}
		}
		Sq[SquareAt(F, R)] = P;
		++F;
	}
	Side = Parts[1].StartsWith(TEXT("b")) ? 1 : 0;
	if (Game == EKGTableGame::Chess)
	{
		if (Parts.Num() > 2)
		{
			for (const TCHAR C : Parts[2])
			{
				Castling |= C == 'K' ? 1 : C == 'Q' ? 2 : C == 'k' ? 4 : C == 'q' ? 8 : 0;
			}
		}
		if (Parts.Num() > 3 && Parts[3] != TEXT("-"))
		{
			Ep = static_cast<int8>(SquareFromName(Parts[3]));
		}
		if (Parts.Num() > 4)
		{
			HalfClock = static_cast<uint16>(FCString::Atoi(*Parts[4]));
		}
		if (Parts.Num() > 5)
		{
			const int32 Full = FMath::Max(1, FCString::Atoi(*Parts[5]));
			Ply = static_cast<uint16>((Full - 1) * 2 + Side);
		}
	}
	else
	{
		if (Parts.Num() > 2)
		{
			HalfClock = static_cast<uint16>(FCString::Atoi(*Parts[2]));
		}
		if (Parts.Num() > 3)
		{
			Ply = static_cast<uint16>(FCString::Atoi(*Parts[3]));
		}
	}
	return true;
}

void FKGBoardState::Pack(TArray<uint8>& Out) const
{
	Out.SetNumUninitialized(32);
	for (int32 i = 0; i < 32; ++i)
	{
		Out[i] = static_cast<uint8>((Sq[i * 2] & 15) | ((Sq[i * 2 + 1] & 15) << 4));
	}
}

bool FKGBoardState::Unpack(const TArray<uint8>& In)
{
	if (In.Num() != 32)
	{
		return false;
	}
	for (int32 i = 0; i < 32; ++i)
	{
		Sq[i * 2] = In[i] & 15;
		Sq[i * 2 + 1] = In[i] >> 4;
	}
	return true;
}
