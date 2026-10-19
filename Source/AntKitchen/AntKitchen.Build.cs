using UnrealBuildTool;

public class AntKitchen : ModuleRules
{
	public AntKitchen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Small module: compile files separately so per-file helper names never collide.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NetCore", "PhysicsCore"
		});
	}
}
