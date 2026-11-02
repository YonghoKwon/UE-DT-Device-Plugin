using UnrealBuildTool;

public class DeviceUI : ModuleRules
{
	public DeviceUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Optional common device panels on DTCore's UDxWidget lifecycle. Headless hosts omit this module.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DTCore", "DeviceRuntime", "UMG", "Slate", "SlateCore" });
	}
}
