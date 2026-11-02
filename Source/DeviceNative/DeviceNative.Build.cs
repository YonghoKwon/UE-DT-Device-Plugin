using UnrealBuildTool;

public class DeviceNative : ModuleRules
{
	public DeviceNative(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Loaded at PostConfigInit for shader directory mapping; must not create DTCore UObjects that early.
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.AddRange(new string[] { "Engine", "RenderCore", "RHI", "Renderer", "Projects" });
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("D3D12RHI");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
		}
	}
}
