#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"

namespace KGAudio
{
	// Sounds synthesised by Tools/Audio/kg_synth_sfx.py, imported to /Game/KillGodot/Audio.
	inline USoundBase* Get(const TCHAR* Name)
	{
		static TMap<FName, TWeakObjectPtr<USoundBase>> Cache;
		const FName Key(Name);
		if (const TWeakObjectPtr<USoundBase>* Hit = Cache.Find(Key); Hit && Hit->IsValid())
		{
			return Hit->Get();
		}
		USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/KillGodot/Audio/%s.%s"), Name, Name));
		Cache.Add(Key, S);
		return S;
	}

	inline USoundAttenuation* Near()
	{
		static TWeakObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("KG_NearAttenuation"));
			A->AddToRoot();
			A->Attenuation.FalloffDistance = 1800.0f;
			A->Attenuation.AttenuationShapeExtents = FVector(250.0f, 0.0f, 0.0f);
			Att = A;
		}
		return Att.Get();
	}

	inline void At(const UObject* Ctx, const TCHAR* Name, const FVector& Loc, float Volume = 1.0f)
	{
		if (USoundBase* S = Get(Name); S && Ctx && Ctx->GetWorld() && Ctx->GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(Ctx, S, Loc, Volume, FMath::FRandRange(0.93f, 1.07f), 0.0f, Near());
		}
	}

	inline void UI(const UObject* Ctx, const TCHAR* Name, float Volume = 1.0f)
	{
		if (USoundBase* S = Get(Name); S && Ctx && Ctx->GetWorld() && Ctx->GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySound2D(Ctx, S, Volume);
		}
	}
}
