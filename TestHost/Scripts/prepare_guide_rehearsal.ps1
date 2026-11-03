param([Parameter(Mandatory=$true)][string]$DTCoreSource)
$ErrorActionPreference='Stop'
$testHost=Split-Path -Parent $PSScriptRoot
$plugin=Split-Path -Parent $testHost
$root=Join-Path $testHost 'Saved/GuideRehearsal'
if(Test-Path -LiteralPath $root){throw 'Rehearsal already exists; preserve its evidence and choose a fresh checkout.'}
if(-not(Test-Path -LiteralPath (Join-Path $DTCoreSource 'DTCore.uplugin'))){throw 'An initialized, validated DTCore folder is required.'}
New-Item -ItemType Directory -Path "$root/Source/GuideHost","$root/Config","$root/Plugins" -Force|Out-Null
New-Item -ItemType Junction -Path "$root/Plugins/DTCore" -Target (Resolve-Path -LiteralPath $DTCoreSource).Path|Out-Null
New-Item -ItemType Junction -Path "$root/Plugins/DTDevice" -Target $plugin|Out-Null
@'
{"FileVersion":3,"EngineAssociation":"5.3","Modules":[{"Name":"GuideHost","Type":"Runtime","LoadingPhase":"Default"},{"Name":"DTDeviceTestHostEditor","Type":"Editor","LoadingPhase":"Default"}],"Plugins":[{"Name":"DTCore","Enabled":true},{"Name":"DTDevice","Enabled":true}]}
'@ | Set-Content "$root/GuideHost.uproject" -Encoding UTF8
@'
using UnrealBuildTool;
public class GuideHost : ModuleRules {
 public GuideHost(ReadOnlyTargetRules Target):base(Target) {
 PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
 PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","DTCore","DeviceRuntime","DeviceUI"});
 }}
'@|Set-Content "$root/Source/GuideHost/GuideHost.Build.cs" -Encoding UTF8
# This is the documented two-tag semantic example, not any UE-DT-Project business policy.
(Get-Content "$testHost/Source/DTDeviceTestHost/DTDeviceTestHost.cpp" -Raw -Encoding UTF8).Replace('DTDeviceTestHost','GuideHost')|Set-Content "$root/Source/GuideHost/GuideHost.cpp" -Encoding UTF8
foreach($kind in @('Game','Editor')){
 $suffix=if($kind -eq 'Editor'){'Editor'}else{''}
 $extra=if($kind -eq 'Editor'){'ExtraModuleNames.Add("DTDeviceTestHostEditor");'}else{''}
 "using UnrealBuildTool; public class GuideHost${suffix}Target : TargetRules { public GuideHost${suffix}Target(TargetInfo Target):base(Target) {Type=TargetType.$kind; DefaultBuildSettings=BuildSettingsVersion.V4; IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_3; ExtraModuleNames.Add(`"GuideHost`"); $extra }}" | Set-Content "$root/Source/GuideHost$suffix.Target.cs" -Encoding UTF8
}
# Reuse only the explicitly documented demo creator and its editor test, outside the plugin runtime.
Copy-Item -LiteralPath "$testHost/Source/DTDeviceTestHostEditor" -Destination "$root/Source/DTDeviceTestHostEditor" -Recurse
$engine=Get-Content "$testHost/Config/DefaultEngine.ini" -Raw -Encoding UTF8
# Only documented render/game settings, never a complete machine-specific config copy.
$engine=($engine -split '\[/Script/AndroidFileServerEditor')[0]
$engine|Set-Content "$root/Config/DefaultEngine.ini" -Encoding UTF8
@'
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/DTDevice/Sensor")
'@|Set-Content "$root/Config/DefaultGame.ini" -Encoding UTF8
Write-Host "Prepared fresh C++ consumer: $root/GuideHost.uproject"
