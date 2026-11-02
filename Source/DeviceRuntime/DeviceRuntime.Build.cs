using UnrealBuildTool;

public class DeviceRuntime : ModuleRules
{
	public DeviceRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// DTCore is mandatory: device actors derive from its InteractableActor and use its data/WebSocket subsystems.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DTCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "DeviceNative" });
	}
}
