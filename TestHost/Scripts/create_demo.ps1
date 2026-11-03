param([string]$EngineRoot="C:\Program Files\Epic Games\UE_5.3",[switch]$Regenerate)
$ErrorActionPreference="Stop"
$root=Split-Path -Parent $PSScriptRoot
$args=@((Join-Path $root "DTDeviceTestHost.uproject"),"-run=CreateDTDeviceDemo","-unattended","-NoSound","-NullRHI",("-UserDir="+(Join-Path $root "Saved/DemoGenerationUser")))
if($Regenerate){$args+="-Regenerate"}
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" @args
if($LASTEXITCODE -ne 0){throw "Demo creation failed ($LASTEXITCODE)"}
