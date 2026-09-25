#include "Chat/KGEmojiText.h"
#include "Chat/KGEmoji.h"
#include "Framework/Text/SlateImageRun.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/TextLayout.h"

TSharedRef<FKGEmojiTextMarshaller> FKGEmojiTextMarshaller::Create(int32 InEmojiSize, bool bInLiveUpdate)
{
	return MakeShareable(new FKGEmojiTextMarshaller(InEmojiSize, bInLiveUpdate));
}

FKGEmojiTextMarshaller::FKGEmojiTextMarshaller(int32 InEmojiSize, bool bInLiveUpdate)
	: EmojiSize(InEmojiSize)
	, bLiveUpdate(bInLiveUpdate)
{
}

void FKGEmojiTextMarshaller::SetSpans(TArray<FSpan> InSpans)
{
	Spans = MoveTemp(InSpans);
	MakeDirty();
}

const FKGEmojiTextMarshaller::FSpan* FKGEmojiTextMarshaller::SpanAt(int32 Index) const
{
	for (const FSpan& Span : Spans)
	{
		if (Index >= Span.Begin && Index < Span.End)
		{
			return &Span;
		}
	}
	return nullptr;
}

void FKGEmojiTextMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	const FTextBlockStyle& DefaultStyle = static_cast<FSlateTextLayout&>(TargetTextLayout).GetDefaultTextStyle();

	TArray<FTextRange> LineRanges;
	FTextRange::CalculateLineRangesFromString(SourceString, LineRanges);

	TArray<FTextLayout::FNewLineData> Lines;
	Lines.Reserve(LineRanges.Num());
	// Emoji bottom sits a little under the text baseline so it centres on lower-case letters.
	const int16 Baseline = static_cast<int16>(-FMath::RoundToInt(EmojiSize * 0.2f));

	for (const FTextRange& LineRange : LineRanges)
	{
		TSharedRef<FString> LineText = MakeShared<FString>(SourceString.Mid(LineRange.BeginIndex, LineRange.Len()));
		TArray<TSharedRef<IRun>> Runs;

		auto StyleFor = [this, &DefaultStyle](const FSpan* Span) -> FTextBlockStyle
		{
			FTextBlockStyle Style = DefaultStyle;
			if (Span)
			{
				Style.SetColorAndOpacity(FSlateColor(Span->Color));
				if (Span->bBold && BoldFont.HasValidFont())
				{
					Style.SetFont(BoldFont);
				}
			}
			return Style;
		};

		int32 RunStart = 0;
		const FSpan* RunSpan = SpanAt(LineRange.BeginIndex);
		auto FlushText = [&](int32 End)
		{
			if (End > RunStart)
			{
				Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, StyleFor(RunSpan), FTextRange(RunStart, End)));
			}
			RunStart = End;
		};

		for (int32 i = 0; i < LineText->Len(); ++i)
		{
			const FSpan* Span = SpanAt(LineRange.BeginIndex + i);
			const int32 Emoji = FKGEmoji::FromChar((*LineText)[i]);
			const FSlateBrush* Brush = Emoji != INDEX_NONE ? FKGEmoji::GetInlineBrush(Emoji, EmojiSize) : nullptr;
			if (Brush)
			{
				FlushText(i);
				Runs.Add(FSlateImageRun::Create(FRunInfo(), LineText, Brush, Baseline, FTextRange(i, i + 1)));
				RunStart = i + 1;
				RunSpan = Span;
				continue;
			}
			if (Span != RunSpan)
			{
				FlushText(i);
				RunSpan = Span;
			}
		}
		FlushText(LineText->Len());
		if (Runs.Num() == 0)
		{
			// Editable text needs at least one run per line, even an empty one.
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, StyleFor(nullptr), FTextRange(0, 0)));
		}
		Lines.Emplace(MoveTemp(LineText), MoveTemp(Runs));
	}
	TargetTextLayout.AddLines(Lines);
}

void FKGEmojiTextMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	SourceTextLayout.GetAsText(TargetString);
}
