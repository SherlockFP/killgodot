#pragma once

#include "CoreMinimal.h"
#include "KGRng.generated.h"

/**
 * Deterministic, snapshot-able RNG (PCG32). All gameplay randomness goes through this so host migration can
 * restore the exact stream position (Docs/05_Tech_Architecture.md §4).
 */
USTRUCT(BlueprintType)
struct KILLGODOT_API FKGRng
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	uint64 State = 0x853c49e6748fea9bULL;

	UPROPERTY(SaveGame)
	uint64 Increment = 0xda3e39cb94b95bdbULL;

	FKGRng() = default;
	explicit FKGRng(uint64 Seed, uint64 Stream = 54u) { Reseed(Seed, Stream); }

	void Reseed(uint64 Seed, uint64 Stream = 54u)
	{
		State = 0u;
		Increment = (Stream << 1u) | 1u;
		NextUInt32();
		State += Seed;
		NextUInt32();
	}

	uint32 NextUInt32()
	{
		const uint64 Old = State;
		State = Old * 6364136223846793005ULL + Increment;
		const uint32 XorShifted = static_cast<uint32>(((Old >> 18u) ^ Old) >> 27u);
		const uint32 Rot = static_cast<uint32>(Old >> 59u);
		return (XorShifted >> Rot) | (XorShifted << ((32u - Rot) & 31u));
	}

	/** Uniform integer in [0, Bound). Unbiased (rejection sampling). */
	uint32 NextBounded(uint32 Bound)
	{
		if (Bound <= 1u)
		{
			return 0u;
		}
		const uint32 Threshold = (0u - Bound) % Bound;
		for (;;)
		{
			const uint32 R = NextUInt32();
			if (R >= Threshold)
			{
				return R % Bound;
			}
		}
	}

	/** Uniform integer in [Min, Max] (inclusive). */
	int32 RandRange(int32 Min, int32 Max)
	{
		return Max <= Min ? Min : Min + static_cast<int32>(NextBounded(static_cast<uint32>(Max - Min + 1)));
	}

	/** Uniform float in [0, 1). */
	float FRand()
	{
		return static_cast<float>(NextUInt32() >> 8) * (1.0f / 16777216.0f);
	}

	template <typename T>
	void Shuffle(TArray<T>& Array)
	{
		for (int32 i = Array.Num() - 1; i > 0; --i)
		{
			Array.Swap(i, RandRange(0, i));
		}
	}
};
