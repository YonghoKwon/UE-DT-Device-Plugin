using UnrealBuildTool;

public class DTDeviceTestHost : ModuleRules
{
	public DTDeviceTestHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// The host only proves that DTCore + DTDevice build and run without UE-DT-Project.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DTCore", "DeviceRuntime", "DeviceUI" });
	}
}
