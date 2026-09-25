using UnrealBuildTool;

public class KillGodot : ModuleRules
{
	public KillGodot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NetCore",
			"PhysicsCore",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"SignalProcessing",
			"AudioMixer",
			"EngineSettings",   // UGameMapsSettings, UGeneralProjectSettings (menus)
			"Sockets",          // ISocketSubsystem (LAN address in the host screen)
			"Json",             // dev panel: teleport targets from Tools/Level layout JSON
			"ApplicationCore",  // dev panel: copy state to the clipboard
			"NavigationSystem"  // bots: level-aware roaming (UNavigationSystemV1 queries)
		});

		PublicIncludePaths.Add(ModuleDirectory);
	}
}
