using UnrealBuildTool;

public class DTDeviceTestHostTarget : TargetRules
{
	public DTDeviceTestHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("DTDeviceTestHost");
	}
}
