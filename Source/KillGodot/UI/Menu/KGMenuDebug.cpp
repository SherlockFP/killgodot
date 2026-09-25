// Development-only UI capture: renders front-end screens off screen at real resolutions (with the project's DPI
// curve) and saves PNGs, so layouts can be checked for every screen size without opening a window.
//
//   kg.UIShot <page|all> [WxH ...]
//     page: home | play | playempty | host | cosmetics | settings | pause | lobby
//     default sizes: 1280x720 1920x1080 2560x1440 3440x1440 1024x768
//   Output: Saved/UIShots/<page>_<W>x<H>.png
//
// Needs a real RHI (e.g. -game -RenderOffScreen); does nothing under -nullrhi.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Core/KGLobbyState.h"
#include "Core/KGPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "KillGodot.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Slate/WidgetRenderer.h"
#include "UI/Menu/KGMenuActions.h"
#include "UI/Menu/SKGLobbyRoom.h"
#include "UI/Menu/SKGMainMenu.h"
#include "UI/Menu/SKGPauseMenu.h"
#include "UI/Menu/SKGSettingsMenu.h"

namespace
{
	TArray<FKGSessionRow> UIShotSampleRows()
	{
		struct FSample
		{
			const TCHAR* Name;
			const TCHAR* Host;
			int32 Players;
			int32 Max;
			int32 Ping;
			bool bInProgress;
			bool bPassword;
			const TCHAR* Region;
			const TCHAR* Code;
		};
		static const FSample Samples[] = {
			{TEXT("Lighthouse Keepers"), TEXT("Marta"), 9, 12, 24, false, false, TEXT("EU"), TEXT("LHK7QZ")},
			{TEXT("No Godot Allowed (EU)"), TEXT("Sergei_RU"), 14, 16, 48, true, false, TEXT("EU"), TEXT("NGA2XP")},
			{TEXT("Chill village, new players welcome"), TEXT("Ayse"), 5, 10, 31, false, false, TEXT("EU"), TEXT("CVW3MK")},
			{TEXT("Friends only"), TEXT("Deniz"), 4, 8, 19, false, true, TEXT("ME"), TEXT("FRN4TY")},
			{TEXT("Harbour Night #3"), TEXT("CaptainNemo"), 12, 12, 88, false, false, TEXT("NA"), TEXT("HN3BVC")},
			{TEXT("Impatient Speedrun Lobby With A Very Long Name That Keeps Going"), TEXT("TheLongestHostNameEver"), 7, 20, 140,
			 false, false, TEXT("NA"), TEXT("ISL9WW")},
			{TEXT("Turkish lobby / Turk lobisi"), TEXT("Emre"), 11, 14, 36, true, false, TEXT("EU"), TEXT("TRK5LB")},
			{TEXT("Ranked practice"), TEXT("Kaito"), 6, 6, 210, true, true, TEXT("AS"), TEXT("RPR8JD")},
			{TEXT("Sunday funday"), TEXT("Lucia"), 3, 10, -1, false, false, TEXT("SA"), TEXT("SFD6HN")},
			{TEXT("Aussie dusk"), TEXT("Bruce"), 8, 12, 290, false, false, TEXT("OC"), TEXT("AUS2KE")},
			{TEXT("Moonlit harbour"), TEXT("Ingrid"), 10, 18, 57, false, false, TEXT("EU"), TEXT("MLH4RA")},
			{TEXT("Bots and friends"), TEXT("Pat"), 2, 8, 12, false, false, TEXT("NA"), TEXT("BAF7UX")},
			{TEXT("Late night village"), TEXT("Olek"), 13, 20, 64, true, false, TEXT("EU"), TEXT("LNV3QE")},
			{TEXT("Password is fish"), TEXT("Mira"), 6, 12, 41, false, true, TEXT("EU"), TEXT("PIF9ZC")},
		};
		TArray<FKGSessionRow> Rows;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Samples); ++Index)
		{
			const FSample& Sample = Samples[Index];
			FKGSessionRow& Row = Rows.AddDefaulted_GetRef();
			Row.SearchIndex = 1000 + Index; // not a real search result: joining is refused as stale
			Row.Name = Sample.Name;
			Row.HostName = Sample.Host;
			Row.MapPath = TEXT("/Game/KillGodot/Maps/L_Morrowmere_v2");
			Row.MapTitle = TEXT("Morrowmere");
			Row.Players = Sample.Players;
			Row.MaxPlayers = Sample.Max;
			Row.PingMs = Sample.Ping;
			Row.bInProgress = Sample.bInProgress;
			Row.bPassword = Sample.bPassword;
			Row.Region = Sample.Region;
			Row.Code = Sample.Code;
		}
		return Rows;
	}

	TSharedPtr<SWidget> UIShotBuild(const FString& Page, APlayerController* PC)
	{
		if (Page == TEXT("pause"))
		{
			return SNew(SKGPauseMenu).OwningPlayer(PC);
		}
		if (Page == TEXT("settings"))
		{
			TSharedRef<SKGMainMenu> Menu = SNew(SKGMainMenu).OwningPlayer(PC);
			Menu->OpenPage(EKGMainMenuPage::Settings);
			return Menu;
		}
		if (Page == TEXT("lobby"))
		{
			AKGPlayerController* KGPC = Cast<AKGPlayerController>(PC);
			if (!KGPC || !AKGLobbyState::Get(PC))
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_UISHOT lobby needs an open lobby (host a game first)"));
				return nullptr;
			}
			return SNew(SKGLobbyRoom).OwningPlayer(KGPC);
		}
		static const TMap<FString, EKGMainMenuPage> Pages = {
			{TEXT("home"), EKGMainMenuPage::Home}, {TEXT("play"), EKGMainMenuPage::Play}, {TEXT("playempty"), EKGMainMenuPage::Play},
			{TEXT("host"), EKGMainMenuPage::Host}, {TEXT("cosmetics"), EKGMainMenuPage::Cosmetics}};
		const EKGMainMenuPage* Found = Pages.Find(Page);
		if (!Found)
		{
			return nullptr;
		}
		TSharedRef<SKGMainMenu> Menu = SNew(SKGMainMenu).OwningPlayer(PC);
		Menu->OpenPage(*Found);
		if (*Found == EKGMainMenuPage::Play)
		{
			Menu->DebugSetRows(Page == TEXT("play") ? UIShotSampleRows() : TArray<FKGSessionRow>());
		}
		return Menu;
	}

	bool UIShotCapture(const FString& Page, APlayerController* PC, const FIntPoint& Size)
	{
		const TSharedPtr<SWidget> Widget = UIShotBuild(Page, PC);
		if (!Widget.IsValid())
		{
			return false;
		}
		const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
		const FVector2D DrawSize(Size.X, Size.Y);
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, true);
		Target->AddToRoot();
		{
			FWidgetRenderer Renderer(true, true);
			// Tick through the intro / page animations (about 1.5 s) before the kept frame.
			for (int32 Frame = 0; Frame < 30; ++Frame)
			{
				Renderer.DrawWidget(Target, Widget.ToSharedRef(), Scale, DrawSize, 0.05f);
			}
		}
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("UIShots");
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / FString::Printf(TEXT("%s_%dx%d.png"), *Page, Size.X, Size.Y);
		bool bSaved = false;
		if (TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*File)))
		{
			bSaved = FImageUtils::ExportRenderTarget2DAsPNG(Target, *Ar);
		}
		Target->RemoveFromRoot();
		UE_LOG(LogKillGodot, Log, TEXT("KG_UISHOT %s %dx%d (dpi %.3f, logical %.0fx%.0f) -> %s %s"), *Page, Size.X, Size.Y, Scale,
		       Size.X / Scale, Size.Y / Scale, *File, bSaved ? TEXT("ok") : TEXT("FAILED"));
		return bSaved;
	}

	FAutoConsoleCommandWithWorldAndArgs GKGUIShotCommand(
		TEXT("kg.UIShot"),
		TEXT("Renders a front-end screen off screen: kg.UIShot <home|play|playempty|host|cosmetics|settings|pause|lobby|all> [WxH ...]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!FApp::CanEverRender())
			{
				UE_LOG(LogKillGodot, Warning, TEXT("KG_UISHOT needs a renderer (run with -RenderOffScreen, not -nullrhi)"));
				return;
			}
			APlayerController* PC = KGMenu::GetLocalPlayerController(World);
			const FString What = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("all"));
			TArray<FIntPoint> Sizes;
			for (int32 Index = 1; Index < Args.Num(); ++Index)
			{
				FString W, H;
				if (Args[Index].Split(TEXT("x"), &W, &H))
				{
					Sizes.Add(FIntPoint(FCString::Atoi(*W), FCString::Atoi(*H)));
				}
			}
			if (Sizes.Num() == 0)
			{
				Sizes = {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3440, 1440), FIntPoint(1024, 768)};
			}
			TArray<FString> Pages;
			if (What == TEXT("all"))
			{
				Pages = {TEXT("home"), TEXT("play"), TEXT("playempty"), TEXT("host"), TEXT("cosmetics"), TEXT("settings"), TEXT("pause")};
				if (AKGLobbyState::Get(World))
				{
					Pages.Add(TEXT("lobby"));
				}
			}
			else
			{
				Pages.Add(What);
			}
			int32 Saved = 0;
			for (const FString& Page : Pages)
			{
				for (const FIntPoint& Size : Sizes)
				{
					Saved += UIShotCapture(Page, PC, Size) ? 1 : 0;
				}
			}
			UE_LOG(LogKillGodot, Log, TEXT("KG_UISHOT done: %d image(s)"), Saved);
		}));
}

#endif
