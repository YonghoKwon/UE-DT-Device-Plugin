param(
    # Same DTCore commit UE-DT-Project pins; change only together with Docs/Compatibility.md.
    [string]$DTCoreCommit = "bc60f05e2d1e64f4ffe1410cf3239fe24cb4d4c9",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$hostRoot = Split-Path -Parent $PSScriptRoot
$dtcore = Join-Path $hostRoot "Plugins\DTCore"

# DTCore is fetched (not vendored): it stays a separate repository and is ignored here.
if (-not (Test-Path -LiteralPath (Join-Path $dtcore ".git"))) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dtcore) | Out-Null
    git clone --quiet https://github.com/YonghoKwon/UE-DT-Core-Plugin.git $dtcore
    if ($LASTEXITCODE -ne 0) { throw "DTCore clone failed" }
}
git -C $dtcore fetch --quiet origin
git -C $dtcore -c advice.detachedHead=false checkout --quiet $DTCoreCommit
if ($LASTEXITCODE -ne 0) { throw "DTCore commit $DTCoreCommit not available" }
$actual = (git -C $dtcore rev-parse HEAD).Trim()
if ($actual -ne $DTCoreCommit) { throw "DTCore is at $actual, expected $DTCoreCommit" }
if (@(git -C $dtcore status --porcelain).Count -gt 0) { throw "TestHost DTCore checkout has local changes" }
Write-Host "DTCore pinned at $actual"

# This repository is the plugin itself: expose it as Plugins/DTDevice through a directory junction so
# edits are tested in place. Plugin discovery stops at DTDevice.uplugin, so the junction does not recurse.
$device = Join-Path $hostRoot "Plugins\DTDevice"
$pluginRoot = Split-Path -Parent $hostRoot
if (-not (Test-Path -LiteralPath (Join-Path $device "DTDevice.uplugin"))) {
    if (Test-Path -LiteralPath $device) { throw "$device exists but is not the DTDevice junction" }
    New-Item -ItemType Junction -Path $device -Target $pluginRoot | Out-Null
}
Write-Host "DTDevice linked from $pluginRoot"

if (-not $SkipBuild) {
    & "$EngineRoot\Engine\Build\BatchFiles\Build.bat" DTDeviceTestHostEditor Win64 Development (Join-Path $hostRoot "DTDeviceTestHost.uproject") -WaitMutex
    if ($LASTEXITCODE -ne 0) { throw "TestHost Editor build failed ($LASTEXITCODE)" }
}
