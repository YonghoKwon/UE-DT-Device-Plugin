using UnrealBuildTool;

public class DeviceNative : ModuleRules
{
	public DeviceNative(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Loaded at PostConfigInit for shader directory mapping; must not create DTCore UObjects that early.
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Engine", "RenderCore", "RHI", "Projects" });
	}
}
