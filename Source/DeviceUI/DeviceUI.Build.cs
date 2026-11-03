using UnrealBuildTool;
public class DeviceUI : ModuleRules
{
    public DeviceUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core","CoreUObject","Engine","DTCore","DeviceRuntime","DeviceNative","UMG","Slate","SlateCore"});
        PrivateDependencyModuleNames.AddRange(new[] {"ApplicationCore","InputCore","EnhancedInput"});
    }
}
