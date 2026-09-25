#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"

/**
 * Text layout marshaller that draws our emoji characters (FKGEmoji, U+E100+) as inline images and optionally
 * recolours character spans (the sender name in a chat line). Works for read-only lines and for the editable input
 * box: every emoji char becomes a one-character FSlateImageRun, which editable text treats as a single glyph, and
 * with bLiveUpdate the layout is rebuilt after every edit so a freshly typed emoji turns into an image at once.
 * If the emoji atlas is not loaded the character falls back to a text run (tofu) instead of crashing.
 */
class KILLGODOT_API FKGEmojiTextMarshaller : public FBaseTextLayoutMarshaller
{
public:
	struct FSpan
	{
		int32 Begin = 0;
		int32 End = 0;
		FLinearColor Color = FLinearColor::White;
		bool bBold = false;
	};

	static TSharedRef<FKGEmojiTextMarshaller> Create(int32 InEmojiSize, bool bInLiveUpdate);

	/** Spans index the whole source string (single-line chat text). */
	void SetSpans(TArray<FSpan> InSpans);
	void SetBoldFont(const FSlateFontInfo& InFont) { BoldFont = InFont; }

	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;
	virtual bool RequiresLiveUpdate() const override { return bLiveUpdate; }

protected:
	FKGEmojiTextMarshaller(int32 InEmojiSize, bool bInLiveUpdate);

private:
	const FSpan* SpanAt(int32 Index) const;

	TArray<FSpan> Spans;
	FSlateFontInfo BoldFont;
	int32 EmojiSize = 20;
	bool bLiveUpdate = false;
};
