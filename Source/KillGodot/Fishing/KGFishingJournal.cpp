#include "Fishing/KGFishingJournal.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace KGFishingJournalPrivate
{
	const TCHAR* Slot = TEXT("KGFishing");
}

UKGFishingJournal* UKGFishingJournal::Get()
{
	static TStrongObjectPtr<UKGFishingJournal> Journal;
	if (!Journal.IsValid())
	{
		UKGFishingJournal* Loaded = nullptr;
		if (UGameplayStatics::DoesSaveGameExist(KGFishingJournalPrivate::Slot, 0))
		{
			Loaded = Cast<UKGFishingJournal>(UGameplayStatics::LoadGameFromSlot(KGFishingJournalPrivate::Slot, 0));
		}
		if (!Loaded)
		{
			Loaded = Cast<UKGFishingJournal>(UGameplayStatics::CreateSaveGameObject(UKGFishingJournal::StaticClass()));
		}
		Journal.Reset(Loaded);
	}
	return Journal.Get();
}

int32 UKGFishingJournal::Record(FName Species, int32 Grams)
{
	const int32 Previous = GetBest(Species);
	Caught.FindOrAdd(Species) += 1;
	if (Grams > Previous)
	{
		BestGrams.Add(Species, Grams);
	}
	UGameplayStatics::AsyncSaveGameToSlot(this, KGFishingJournalPrivate::Slot, 0);
	return Previous;
}
