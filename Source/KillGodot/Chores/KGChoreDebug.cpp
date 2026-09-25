// Development-only chore minigame capture (the kg.UIShot technique from UI/Menu/KGMenuDebug.cpp): renders chore panels
// off screen over a painted stand-in for the game view and saves PNGs, so every minigame can be reviewed headless.
//
//   kg.ChoreShot <ChoreId|all>[@Stage] [WxH ...] [fake] [practice] [t=Seconds]
//     all          every chore, every stage (DrawWater@0, DrawWater@1, ...)
//     @Stage       only that stage (0-based)
//     t=Seconds    simulated seconds before the minigame's DebugPose (default 1.2)
//   Output: Saved/UIShots/chore_<Id>_s<Stage>[_fake]_<W>x<H>.png   (log lines: KG_CHORESHOT)
//
// Needs a real RHI: UnrealEditor.exe KillGodot.uproject /Game/KillGodot/Maps/L_MainMenu -game -RenderOffScreen
//   -unattended -nosound -ExecCmds="kg.ChoreShot all 1280x720; quit"   (Tools/Unreal/kg_chore_shots.ps1)

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Chores/KGChoreTypes.h"
#include "Chores/UI/KGMinigame.h"
#include "Chores/UI/SKGChorePanel.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "KillGodot.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Slate/WidgetRenderer.h"

namespace KGChoreShot
{
	/** bGamma: the kg.UIShot setting (gamma-corrected render target). Off = raw sRGB colours as the game shows them. */
	bool GGamma = false;

	bool Capture(FName ChoreId, int32 Stage, const FIntPoint& Size, bool bFake, bool bPractice, float Seconds)
	{
		TSharedRef<SKGChorePanel> Panel = SNew(SKGChorePanel)
			.ChoreId(ChoreId)
			.Seed(1234 + Stage)
			.StartStage(Stage)
			.bFake(bFake)
			.bPractice(bPractice)
			.bShot(true);
		Panel->DebugAdvance(Seconds, true);
		const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
		const FVector2D DrawSize(Size.X, Size.Y);
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, GGamma);
		Target->AddToRoot();
		{
			FWidgetRenderer Renderer(GGamma, true);
			for (int32 Frame = 0; Frame < 4; ++Frame)
			{
				Renderer.DrawWidget(Target, Panel, Scale, DrawSize, 0.016f);
			}
		}
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("UIShots");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / FString::Printf(TEXT("chore_%s_s%d%s_%dx%d.png"), *ChoreId.ToString(), Stage, bFake ? TEXT("_fake") : TEXT(""),
		                                           Size.X, Size.Y);
		bool bSaved = false;
		if (TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*File)))
		{
			bSaved = FImageUtils::ExportRenderTarget2DAsPNG(Target, *Ar);
		}
		Target->RemoveFromRoot();
		UE_LOG(LogKillGodot, Log, TEXT("KG_CHORESHOT %s stage %d %dx%d -> %s %s"), *ChoreId.ToString(), Stage, Size.X, Size.Y, *File,
		       bSaved ? TEXT("ok") : TEXT("FAILED"));
		return bSaved;
	}

	FAutoConsoleCommandWithWorldAndArgs GCommand(
		TEXT("kg.ChoreShot"),
		TEXT("Renders chore minigame panels off screen: kg.ChoreShot <ChoreId|all>[@Stage] [WxH ...] [fake] [practice] [t=Seconds]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!FApp::CanEverRender())
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_CHORESHOT needs a renderer (run with -RenderOffScreen, not -nullrhi)"));
				return;
			}
			FString What = Args.Num() > 0 ? Args[0] : FString(TEXT("all"));
			int32 OnlyStage = INDEX_NONE;
			FString StagePart;
			if (What.Split(TEXT("@"), &What, &StagePart))
			{
				OnlyStage = FCString::Atoi(*StagePart);
			}
			TArray<FIntPoint> Sizes;
			bool bFake = false;
			bool bPractice = false;
			GGamma = false;
			float Seconds = 1.2f;
			for (int32 Index = 1; Index < Args.Num(); ++Index)
			{
				FString W, H;
				if (Args[Index].Equals(TEXT("fake"), ESearchCase::IgnoreCase))
				{
					bFake = true;
				}
				else if (Args[Index].Equals(TEXT("practice"), ESearchCase::IgnoreCase))
				{
					bPractice = true;
				}
				else if (Args[Index].Equals(TEXT("gamma"), ESearchCase::IgnoreCase))
				{
					GGamma = true;
				}
				else if (Args[Index].StartsWith(TEXT("t=")))
				{
					Seconds = FCString::Atof(*Args[Index].Mid(2));
				}
				else if (Args[Index].Split(TEXT("x"), &W, &H))
				{
					Sizes.Add(FIntPoint(FCString::Atoi(*W), FCString::Atoi(*H)));
				}
			}
			if (Sizes.Num() == 0)
			{
				Sizes.Add(FIntPoint(1280, 720));
			}
			int32 Saved = 0;
			for (const FKGChoreDef& Def : FKGChoreCatalog::GetAll())
			{
				if (!What.Equals(TEXT("all"), ESearchCase::IgnoreCase) && !Def.Id.ToString().Equals(What, ESearchCase::IgnoreCase))
				{
					continue;
				}
				if (!FKGMinigameFactory::Has(Def.Id))
				{
					continue;
				}
				for (int32 Stage = 0; Stage < Def.NumStages(); ++Stage)
				{
					if (OnlyStage != INDEX_NONE && Stage != OnlyStage)
					{
						continue;
					}
					for (const FIntPoint& Size : Sizes)
					{
						Saved += Capture(Def.Id, Stage, Size, bFake, bPractice, Seconds) ? 1 : 0;
					}
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_CHORESHOT done: %d image(s)"), Saved);
		}));
}

#endif
