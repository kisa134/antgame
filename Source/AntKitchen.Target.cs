using UnrealBuildTool;
using System.Collections.Generic;

public class AntKitchenTarget : TargetRules
{
	public AntKitchenTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AntKitchen");
	}
}
