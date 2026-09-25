#pragma once

#include "CoreMinimal.h"

/**
 * The sea surface, shared by the M_KG_Ocean material (world position offset) and gameplay (swimming, buoyancy,
 * bobbers). Three travelling sine waves with deep-water speed c = sqrt(g / k).
 * KEEP IN SYNC with OCEAN_HLSL in Tools/Unreal/kg_make_ocean.py (same directions, amplitudes, wavelengths, calm mask).
 * Time is world time in seconds (the material's Time node uses the same clock).
 *
 * Calm zone (sheltered harbour basin): inside a circle the swell is scaled down to CalmScale and replaced by small
 * ripples. The level's AKGMapInfo owns the numbers (bCalmWater, CalmCentre...) and pushes them both here and into the
 * material parameter collection MPC_KG_Water, so the rendered and the gameplay surface agree. Off by default (v1).
 */
struct FKGWaves
{
	static constexpr float SeaLevel = 0.0f;

	struct FWave
	{
		float DirX;
		float DirY;
		float Amplitude;   // cm
		float Wavelength;  // cm
	};

	/** Mirrors MPC_KG_Water: CalmZone = (CentreX, CentreY, Radius, Fade), CalmParams = (Scale, RippleAmp, Tint, 0). */
	struct FCalmZone
	{
		float CentreX = 0.0f;
		float CentreY = 0.0f;
		float Radius = 0.0f;     // cm; <= 0 = no calm zone
		float Fade = 1000.0f;    // cm, ramp width inside the radius (full calm within Radius - Fade)
		float Scale = 0.25f;     // swell multiplier at full calm
		float RippleAmp = 2.5f;  // cm, small wind ripples at full calm
	};

	static FCalmZone& Calm()
	{
		static FCalmZone Zone;
		return Zone;
	}

	static const FWave* GetWaves(int32& OutCount)
	{
		static const FWave Waves[] = {
			{0.78f, 0.62f, 26.0f, 2600.0f},
			{-0.35f, 0.94f, 13.0f, 1300.0f},
			{0.95f, -0.31f, 6.0f, 650.0f},
		};
		OutCount = UE_ARRAY_COUNT(Waves);
		return Waves;
	}

	/** 0 in the open sea .. 1 in the sheltered basin (smoothstep over Fade inside the radius). */
	static float CalmWeight(float X, float Y)
	{
		const FCalmZone& Z = Calm();
		if (Z.Radius <= 0.0f)
		{
			return 0.0f;
		}
		const float D = FMath::Sqrt(FMath::Square(X - Z.CentreX) + FMath::Square(Y - Z.CentreY));
		const float Fade = FMath::Max(Z.Fade, 1.0f);
		const float T = FMath::Clamp((D - (Z.Radius - Fade)) / Fade, 0.0f, 1.0f);
		return 1.0f - T * T * (3.0f - 2.0f * T);
	}

	/** World Z (cm) of the water surface at X, Y. */
	static float HeightAt(float X, float Y, float Time)
	{
		int32 Count = 0;
		const FWave* Waves = GetWaves(Count);
		float H = 0.0f;
		for (int32 i = 0; i < Count; ++i)
		{
			const FWave& W = Waves[i];
			const float K = 2.0f * PI / W.Wavelength;
			const float C = FMath::Sqrt(980.0f / K);
			H += W.Amplitude * FMath::Sin(K * (W.DirX * X + W.DirY * Y) - K * C * Time);
		}
		const float Wc = CalmWeight(X, Y);
		if (Wc > 0.0f)
		{
			const FCalmZone& Z = Calm();
			const float Ripple = 0.6f * FMath::Sin(0.021f * (0.6f * X + 0.8f * Y) - 1.9f * Time) +
			                     0.4f * FMath::Sin(0.029f * (-0.7f * X + 0.71f * Y) - 2.3f * Time);
			H = H * FMath::Lerp(1.0f, Z.Scale, Wc) + Wc * Z.RippleAmp * Ripple;
		}
		return SeaLevel + H;
	}

	/** True if a point is under the (moving) surface. */
	static bool IsUnderwater(const FVector& P, float Time) { return P.Z < HeightAt(P.X, P.Y, Time); }
};
