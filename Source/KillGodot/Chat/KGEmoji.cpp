#include "Chat/KGEmoji.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/UObjectGlobals.h"

namespace KGEmojiPrivate
{
	struct FDef
	{
		const TCHAR* Id;
		/** Comma-separated aliases (shortcodes that also work). */
		const TCHAR* Aliases;
	};

	// ORDER = atlas cell order (Tools/UI/kg_make_emoji.py ORDER). Append only: indices travel over the network.
	const FDef Defs[] = {
		{TEXT("laugh"), TEXT("lol,joy,haha,xd,rofl")},
		{TEXT("sus"), TEXT("thinking,hmm,suspicious,sideeye")},
		{TEXT("skull"), TEXT("dead,rip,skeleton")},
		{TEXT("thumbsup"), TEXT("+1,yes,like,ok")},
		{TEXT("thumbsdown"), TEXT("-1,no,dislike")},
		{TEXT("heart"), TEXT("love,<3")},
		{TEXT("wave"), TEXT("hi,hello,bye,hey")},
		{TEXT("angry"), TEXT("mad,rage")},
		{TEXT("smile"), TEXT("happy,smiley")},
		{TEXT("cry"), TEXT("sad,tears,sob")},
		{TEXT("shock"), TEXT("wow,omg,surprised,gasp")},
		{TEXT("sweat"), TEXT("nervous,awkward,phew")},
		{TEXT("zzz"), TEXT("sleep,sleepy,afk")},
		{TEXT("wink"), TEXT("winky")},
		{TEXT("clap"), TEXT("applause,gg,bravo")},
		{TEXT("pray"), TEXT("please,thanks,pls")},
		{TEXT("knife"), TEXT("dagger,blade,stab")},
		{TEXT("eyes"), TEXT("look,watching,see")},
		{TEXT("ghost"), TEXT("boo,spirit")},
		{TEXT("candle"), TEXT("light,vigil")},
		{TEXT("fire"), TEXT("lit,flame,hot")},
		{TEXT("fish"), TEXT("catch,sardine")},
		{TEXT("bell"), TEXT("alarm,ring")},
		{TEXT("anchor"), TEXT("harbour,harbor,ship")},
		{TEXT("question"), TEXT("?,what,huh")},
		{TEXT("exclamation"), TEXT("!,alert,important")},
		{TEXT("crown"), TEXT("king,queen,mayor")},
		{TEXT("clock"), TEXT("godot,time,wait")},
		{TEXT("mask"), TEXT("impatient,masked,theatre")},
		{TEXT("lantern"), TEXT("lighthouse,lamp")},
		{TEXT("mug"), TEXT("beer,ale,cheers,tavern")},
		{TEXT("moon"), TEXT("night,nighty")},
	};
	constexpr int32 NumDefs = UE_ARRAY_COUNT(Defs);
	constexpr int32 AtlasColumns = 8;
	constexpr int32 AtlasRows = 4;

	/** Whole-word emoticons. Longest first so ">:(" wins over ":(". */
	const TPair<const TCHAR*, const TCHAR*> Emoticons[] = {
		{TEXT(">:("), TEXT("angry")}, {TEXT(":'("), TEXT("cry")}, {TEXT(":-)"), TEXT("smile")},
		{TEXT(":-("), TEXT("cry")}, {TEXT(":-D"), TEXT("laugh")}, {TEXT(";-)"), TEXT("wink")},
		{TEXT(":)"), TEXT("smile")}, {TEXT("(:"), TEXT("smile")}, {TEXT(":("), TEXT("cry")},
		{TEXT(":D"), TEXT("laugh")}, {TEXT("xD"), TEXT("laugh")}, {TEXT("XD"), TEXT("laugh")},
		{TEXT(";)"), TEXT("wink")}, {TEXT(":o"), TEXT("shock")}, {TEXT(":O"), TEXT("shock")},
		{TEXT("<3"), TEXT("heart")}, {TEXT(":/"), TEXT("sus")}, {TEXT("o/"), TEXT("wave")},
	};

	/** Unicode code point -> our id. */
	const TPair<uint32, const TCHAR*> UnicodeMap[] = {
		{0x1F600, TEXT("smile")}, {0x1F603, TEXT("smile")}, {0x1F604, TEXT("smile")}, {0x1F642, TEXT("smile")},
		{0x263A, TEXT("smile")}, {0x1F60A, TEXT("smile")}, {0x1F601, TEXT("smile")},
		{0x1F602, TEXT("laugh")}, {0x1F923, TEXT("laugh")}, {0x1F606, TEXT("laugh")},
		{0x1F622, TEXT("cry")}, {0x1F62D, TEXT("cry")}, {0x1F641, TEXT("cry")}, {0x2639, TEXT("cry")},
		{0x1F620, TEXT("angry")}, {0x1F621, TEXT("angry")}, {0x1F92C, TEXT("angry")},
		{0x1F480, TEXT("skull")}, {0x2620, TEXT("skull")},
		{0x1F52A, TEXT("knife")}, {0x1F5E1, TEXT("knife")},
		{0x1F440, TEXT("eyes")},
		{0x1F914, TEXT("sus")}, {0x1F9D0, TEXT("sus")}, {0x1F928, TEXT("sus")}, {0x1F612, TEXT("sus")},
		{0x2764, TEXT("heart")}, {0x2665, TEXT("heart")}, {0x1F496, TEXT("heart")}, {0x1F495, TEXT("heart")},
		{0x1F44D, TEXT("thumbsup")}, {0x1F44E, TEXT("thumbsdown")},
		{0x1F41F, TEXT("fish")}, {0x1F420, TEXT("fish")}, {0x1F421, TEXT("fish")},
		{0x1F514, TEXT("bell")}, {0x1F47B, TEXT("ghost")}, {0x1F56F, TEXT("candle")}, {0x1F525, TEXT("fire")},
		{0x2693, TEXT("anchor")}, {0x2753, TEXT("question")}, {0x2754, TEXT("question")},
		{0x2757, TEXT("exclamation")}, {0x2755, TEXT("exclamation")},
		{0x1F44B, TEXT("wave")}, {0x1F64F, TEXT("pray")}, {0x1F44F, TEXT("clap")},
		{0x1F605, TEXT("sweat")}, {0x1F630, TEXT("sweat")}, {0x1F613, TEXT("sweat")},
		{0x1F4A4, TEXT("zzz")}, {0x1F634, TEXT("zzz")}, {0x1F451, TEXT("crown")},
		{0x1F62E, TEXT("shock")}, {0x1F632, TEXT("shock")}, {0x1F631, TEXT("shock")}, {0x1F633, TEXT("shock")},
		{0x1F609, TEXT("wink")}, {0x1F61C, TEXT("wink")},
		{0x1F570, TEXT("clock")}, {0x23F0, TEXT("clock")}, {0x231B, TEXT("clock")}, {0x23F3, TEXT("clock")},
		{0x1F3AD, TEXT("mask")}, {0x1F3EE, TEXT("lantern")}, {0x1F37A, TEXT("mug")}, {0x1F37B, TEXT("mug")},
		{0x1F319, TEXT("moon")}, {0x1F31B, TEXT("moon")}, {0x1F31C, TEXT("moon")},
	};

	bool IsWordBoundary(const FString& Text, int32 Index)
	{
		return !Text.IsValidIndex(Index) || FChar::IsWhitespace(Text[Index]);
	}

	bool IsShortcodeChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('+') || C == TEXT('-');
	}

	struct FBrushSet
	{
		TArray<TSharedPtr<FSlateBrush>> Brushes;
	};

	UTexture2D* LoadAtlas(bool bLarge)
	{
		const TCHAR* Path = bLarge ? TEXT("/Game/KillGodot/UI/Emoji/T_KG_EmojiAtlas.T_KG_EmojiAtlas")
		                           : TEXT("/Game/KillGodot/UI/Emoji/T_KG_EmojiAtlas_Small.T_KG_EmojiAtlas_Small");
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Texture)
		{
			Texture->AddToRoot();   // brushes hold a raw pointer for the whole session
		}
		return Texture;
	}

	const FSlateBrush* GetBrush(bool bLarge, int32 Index, int32 Size)
	{
		if (Index < 0 || Index >= NumDefs || !IsInGameThread())
		{
			return nullptr;
		}
		// One brush per (atlas, size, index): brushes are referenced by pointer from text runs, so they never move.
		static TMap<int32, FBrushSet> Sets;
		static UTexture2D* Textures[2] = {nullptr, nullptr};
		static bool bTried[2] = {false, false};
		const int32 A = bLarge ? 1 : 0;
		if (!bTried[A] || (Textures[A] == nullptr && GFrameCounter % 600 == 0))
		{
			bTried[A] = true;
			Textures[A] = LoadAtlas(bLarge);
		}
		if (!Textures[A])
		{
			return nullptr;
		}
		FBrushSet& Set = Sets.FindOrAdd(A * 10000 + Size);
		if (Set.Brushes.Num() != NumDefs)
		{
			Set.Brushes.SetNum(NumDefs);
		}
		TSharedPtr<FSlateBrush>& Brush = Set.Brushes[Index];
		if (!Brush.IsValid())
		{
			Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Textures[A]);
			Brush->ImageSize = FVector2D(Size, Size);
			Brush->DrawAs = ESlateBrushDrawType::Image;
			Brush->Tiling = ESlateBrushTileType::NoTile;
			const int32 Col = Index % AtlasColumns;
			const int32 Row = Index / AtlasColumns;
			Brush->SetUVRegion(FBox2f(FVector2f(static_cast<float>(Col) / AtlasColumns, static_cast<float>(Row) / AtlasRows),
			                          FVector2f(static_cast<float>(Col + 1) / AtlasColumns,
			                                    static_cast<float>(Row + 1) / AtlasRows)));
		}
		return Brush.Get();
	}
}

int32 FKGEmoji::Num()
{
	return KGEmojiPrivate::NumDefs;
}

const TCHAR* FKGEmoji::GetId(int32 Index)
{
	return Index >= 0 && Index < KGEmojiPrivate::NumDefs ? KGEmojiPrivate::Defs[Index].Id : nullptr;
}

int32 FKGEmoji::Find(const FString& NameOrAlias)
{
	const FString Key = NameOrAlias.TrimStartAndEnd().ToLower();
	if (Key.IsEmpty())
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < KGEmojiPrivate::NumDefs; ++i)
	{
		if (Key == KGEmojiPrivate::Defs[i].Id)
		{
			return i;
		}
	}
	for (int32 i = 0; i < KGEmojiPrivate::NumDefs; ++i)
	{
		TArray<FString> Aliases;
		FString(KGEmojiPrivate::Defs[i].Aliases).ParseIntoArray(Aliases, TEXT(","));
		if (Aliases.Contains(Key))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 FKGEmoji::FromChar(TCHAR Char)
{
	const int32 Index = static_cast<int32>(Char) - static_cast<int32>(FirstChar);
	return Index >= 0 && Index < KGEmojiPrivate::NumDefs ? Index : INDEX_NONE;
}

int32 FKGEmoji::CountEmojis(const FString& Text)
{
	int32 Count = 0;
	for (const TCHAR C : Text)
	{
		Count += IsEmojiChar(C) ? 1 : 0;
	}
	return Count;
}

FString FKGEmoji::ConvertShortcodes(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len());
	int32 i = 0;
	while (i < Text.Len())
	{
		if (Text[i] == TEXT(':'))
		{
			int32 j = i + 1;
			while (j < Text.Len() && j - i <= 24 && KGEmojiPrivate::IsShortcodeChar(Text[j]))
			{
				++j;
			}
			if (j < Text.Len() && j > i + 1 && Text[j] == TEXT(':'))
			{
				const int32 Index = Find(Text.Mid(i + 1, j - i - 1));
				if (Index != INDEX_NONE)
				{
					Out.AppendChar(ToChar(Index));
					i = j + 1;
					continue;
				}
			}
		}
		Out.AppendChar(Text[i]);
		++i;
	}
	return Out;
}

FString FKGEmoji::ConvertEmoticons(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len());
	int32 i = 0;
	while (i < Text.Len())
	{
		bool bMatched = false;
		if (KGEmojiPrivate::IsWordBoundary(Text, i - 1))
		{
			for (const TPair<const TCHAR*, const TCHAR*>& Pair : KGEmojiPrivate::Emoticons)
			{
				const int32 Len = FCString::Strlen(Pair.Key);
				if (FCString::Strncmp(*Text + i, Pair.Key, Len) == 0 && KGEmojiPrivate::IsWordBoundary(Text, i + Len))
				{
					Out.AppendChar(ToChar(Find(Pair.Value)));
					i += Len;
					bMatched = true;
					break;
				}
			}
		}
		if (!bMatched)
		{
			Out.AppendChar(Text[i]);
			++i;
		}
	}
	return Out;
}

FString FKGEmoji::ConvertUnicode(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len());
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		uint32 Code = Text[i];
		int32 Consumed = 1;
		if (Code >= 0xD800 && Code <= 0xDBFF && i + 1 < Text.Len() && Text[i + 1] >= 0xDC00 && Text[i + 1] <= 0xDFFF)
		{
			Code = 0x10000 + ((Code - 0xD800) << 10) + (static_cast<uint32>(Text[i + 1]) - 0xDC00);
			Consumed = 2;
		}
		if (Code == 0xFE0F || Code == 0xFE0E || Code == 0x200D || (Code >= 0x1F3FB && Code <= 0x1F3FF))
		{
			i += Consumed - 1;   // variation selectors, joiners, skin tones
			continue;
		}
		const TCHAR* Mapped = nullptr;
		for (const TPair<uint32, const TCHAR*>& Pair : KGEmojiPrivate::UnicodeMap)
		{
			if (Pair.Key == Code)
			{
				Mapped = Pair.Value;
				break;
			}
		}
		if (Mapped)
		{
			Out.AppendChar(ToChar(Find(Mapped)));
		}
		else
		{
			Out.AppendChars(*Text + i, Consumed);
		}
		i += Consumed - 1;
	}
	return Out;
}

FString FKGEmoji::ConvertAll(const FString& Text)
{
	return ConvertEmoticons(ConvertShortcodes(ConvertUnicode(Text)));
}

FString FKGEmoji::ToPlainText(const FString& Text)
{
	FString Out;
	Out.Reserve(Text.Len() + 16);
	for (const TCHAR C : Text)
	{
		const int32 Index = FromChar(C);
		if (Index != INDEX_NONE)
		{
			Out += FString::Printf(TEXT(":%s:"), GetId(Index));
		}
		else
		{
			Out.AppendChar(C);
		}
	}
	return Out;
}

const FSlateBrush* FKGEmoji::GetInlineBrush(int32 Index, int32 Size)
{
	return KGEmojiPrivate::GetBrush(false, Index, Size);
}

const FSlateBrush* FKGEmoji::GetLargeBrush(int32 Index, int32 Size)
{
	return KGEmojiPrivate::GetBrush(true, Index, Size);
}
