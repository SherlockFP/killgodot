#include "Dev/KGDevSubsystem.h"
#include "Core/KGGameMode.h"
#include "Core/KGGameState.h"
#include "Core/KGPlayerState.h"
#include "Dev/KGDevCommands.h"
#include "Dev/KGDevComponent.h"
#include "Dev/SKGDevPanel.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "KillGodot.h"
#include "World/KGTaskStation.h"

bool UKGDevSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer);
#endif
}

bool UKGDevSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UKGDevSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKGDevSubsystem, STATGROUP_Tickables);
}

UKGDevSubsystem* UKGDevSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UKGDevSubsystem>() : nullptr;
}

void UKGDevSubsystem::Deinitialize()
{
	ClosePanel();
	Super::Deinitialize();
}

void UKGDevSubsystem::EnsureDevComponent(APlayerController* PC)
{
	if (!IsValid(PC) || !PC->HasAuthority() || PC->IsActorBeingDestroyed() || UKGDevComponent::FindFor(PC))
	{
		return;
	}
	UKGDevComponent* Component = NewObject<UKGDevComponent>(PC, TEXT("KGDev"));
	Component->SetIsReplicated(true);
	PC->AddInstanceComponent(Component);
	Component->RegisterComponent();
}

void UKGDevSubsystem::PushMessage(bool bOk, const FString& Text)
{
	FMessage& Message = Messages.AddDefaulted_GetRef();
	Message.bOk = bOk;
	Message.Text = Text;
	Message.Time = FPlatformTime::Seconds();
	if (Messages.Num() > 40)
	{
		Messages.RemoveAt(0, Messages.Num() - 40);
	}
}

void UKGDevSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Only match maps (the front end has its own game state).
	if (!World->GetGameState<AKGGameState>())
	{
		return;
	}

	// Server: every human controller gets the dev link (bots are AI controllers and never need one).
	if (World->GetNetMode() != NM_Client)
	{
		SweepSeconds -= DeltaTime;
		if (SweepSeconds <= 0.0f)
		{
			SweepSeconds = 0.5f;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				EnsureDevComponent(It->Get());
			}
		}
	}

	// Local players: F1 toggles the panel (polled, so neither the character's nor the controller's input changes).
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController() && PC->WasInputKeyJustPressed(EKeys::F1))
			{
				TogglePanel(PC);
			}
		}
		if (Panel.IsValid() && (!PanelPC.IsValid() || !PanelViewport.IsValid()))
		{
			ClosePanel();
		}
		if (bShowChoreMarkers)
		{
			DrawChoreMarkers();
		}
	}
#endif
}

void UKGDevSubsystem::TogglePanel(APlayerController* PC)
{
	// A key press can reach both the panel (Slate) and the polled input in the same frame: toggle once.
	if (GFrameCounter == LastToggleFrame)
	{
		return;
	}
	LastToggleFrame = GFrameCounter;
	if (Panel.IsValid())
	{
		ClosePanel();
	}
	else
	{
		OpenPanel(PC);
	}
}

void UKGDevSubsystem::OpenPanel(APlayerController* PC)
{
#if !UE_BUILD_SHIPPING
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
	if (Panel.IsValid() || !Viewport)
	{
		return;
	}
	TSharedRef<SKGDevPanel> Widget = SNew(SKGDevPanel).PlayerController(PC);
	Viewport->AddViewportWidgetContent(Widget, 90);
	Panel = Widget;
	PanelViewport = Viewport;
	PanelPC = PC;
	bPanelRestoreCursor = PC->ShouldShowMouseCursor();

	// Game and UI: WASD still walks (keys the panel does not use fall through to the game), the mouse drives the panel.
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Widget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
#endif
}

void UKGDevSubsystem::ClosePanel()
{
	UGameViewportClient* Viewport = PanelViewport.Get();
	if (Viewport && Panel.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
	}
	if (APlayerController* PC = PanelPC.Get(); PC && Panel.IsValid())
	{
		if (bPanelRestoreCursor)
		{
			FInputModeGameAndUI Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(Mode);
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}
	Panel.Reset();
	PanelViewport = nullptr;
	PanelPC = nullptr;
}

void UKGDevSubsystem::DrawChoreMarkers() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	const APlayerController* PC = FKGDev::LocalController(World);
	const AKGPlayerState* Me = PC ? PC->GetPlayerState<AKGPlayerState>() : nullptr;
	for (TActorIterator<AKGTaskStation> It(World); It; ++It)
	{
		const bool bMine = Me && Me->HasOpenTask(It->TaskId);
		const bool bDone = Me && Me->TaskIds.Contains(It->TaskId) && !bMine;
		const FColor Color = bMine ? FColor(242, 194, 48) : bDone ? FColor(139, 209, 96) : FColor(79, 209, 197);
		const FVector At = It->GetActorLocation();
		DrawDebugCylinder(World, At, At + FVector(0, 0, 400.0f), It->WorkRadius, 24, Color, false, -1.0f, 0, 2.0f);
		DrawDebugString(World, At + FVector(0, 0, 430.0f),
		                FString::Printf(TEXT("%s%s"), *It->TaskId.ToString(), bMine ? TEXT("  (yours)") : bDone ? TEXT("  (done)") : TEXT("")),
		                nullptr, Color, 0.0f, true, 1.2f);
	}
#endif
}
