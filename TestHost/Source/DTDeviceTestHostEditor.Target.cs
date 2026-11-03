using UnrealBuildTool;

public class DTDeviceTestHostEditorTarget : TargetRules
{
	public DTDeviceTestHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("DTDeviceTestHost");
	}
}
