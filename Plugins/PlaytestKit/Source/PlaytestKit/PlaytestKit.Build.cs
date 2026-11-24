using UnrealBuildTool;

public class PlaytestKit : ModuleRules
{
	public PlaytestKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "Slate", "SlateCore", "HTTP", "Json", "RenderCore", "RHI", "ImageWrapper", "EngineSettings" });
	}
}
