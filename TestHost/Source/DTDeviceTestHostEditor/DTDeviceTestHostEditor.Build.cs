using UnrealBuildTool;
public class DTDeviceTestHostEditor : ModuleRules
{
    public DTDeviceTestHostEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "DeviceRuntime", "DeviceUI", "UMG", "Slate", "SlateCore" });
    }
}
