using UnrealBuildTool;
public class DeviceRuntime : ModuleRules
{
    public DeviceRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core","CoreUObject","Engine","DeveloperSettings","DTCore","DeviceNative","RHI","RenderCore","Json","JsonUtilities","WebSockets","Stomp","HTTP","ImageWrapper","Niagara","NiagaraCore","PhysicsCore","Sockets","Networking","ProceduralMeshComponent"});
        PrivateDependencyModuleNames.AddRange(new[] {"Renderer","HTTPServer","Projects","InputCore"});
        if(Target.bBuildEditor)PrivateDependencyModuleNames.AddRange(new[] {"UnrealEd","DesktopPlatform","Slate","SlateCore","ApplicationCore","InputCore"});
    }
}
