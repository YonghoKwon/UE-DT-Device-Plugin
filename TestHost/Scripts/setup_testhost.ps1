param(
    # Where DTCore comes from. A git URL (GitHub, internal GitLab/Bitbucket, file share) is cloned;
    # an existing local folder containing DTCore.uplugin is linked as-is (no clone, no checkout).
    # Empty = TestHost.local.ini [DTCore] Source, then env DTCORE_SOURCE, then the public GitHub repo.
    [string]$DTCoreSource = "",
    # Branch, tag or commit to check out for git sources. Empty = TestHost.local.ini [DTCore] Ref,
    # then env DTCORE_REF, then the commit this plugin was validated with (Docs/Compatibility.md).
    [string]$DTCoreRef = "",
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$hostRoot = Split-Path -Parent $PSScriptRoot
$validatedCommit = "f05f25f6ebb8bacda0e5b0dc6e1c5d5d88ae44ee"
$defaultSource = "https://github.com/YonghoKwon/UE-DT-Core-Plugin.git"

# Per-PC settings live in TestHost.local.ini (git-ignored) so an internal repository URL is never committed.
function Get-LocalSetting([string]$Key) {
    $file = Join-Path $hostRoot "TestHost.local.ini"
    if (-not (Test-Path -LiteralPath $file)) { return "" }
    $section = ""
    foreach ($line in Get-Content -LiteralPath $file -Encoding UTF8) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') { $section = $Matches[1]; continue }
        if ($section -eq "DTCore" -and $t -match "^$Key\s*=\s*(.*)$") { return $Matches[1].Trim() }
    }
    return ""
}
if (-not $DTCoreSource) { $DTCoreSource = Get-LocalSetting "Source" }
if (-not $DTCoreSource) { $DTCoreSource = $env:DTCORE_SOURCE }
if (-not $DTCoreSource) { $DTCoreSource = $defaultSource }
if (-not $DTCoreRef) { $DTCoreRef = Get-LocalSetting "Ref" }
if (-not $DTCoreRef) { $DTCoreRef = $env:DTCORE_REF }
if (-not $DTCoreRef) { $DTCoreRef = $validatedCommit }

$plugins = Join-Path $hostRoot "Plugins"
$dtcore = Join-Path $plugins "DTCore"
New-Item -ItemType Directory -Force -Path $plugins | Out-Null

function Test-Junction([string]$Path) { (Test-Path -LiteralPath $Path) -and ((Get-Item -LiteralPath $Path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) }

$localFolder = (Test-Path -LiteralPath $DTCoreSource -PathType Container) -and (Test-Path -LiteralPath (Join-Path $DTCoreSource "DTCore.uplugin"))
if ($localFolder) {
    # Use the caller's own DTCore checkout in place (e.g. managed from an internal repository).
    $target = (Resolve-Path -LiteralPath $DTCoreSource).Path
    if (Test-Path -LiteralPath $dtcore) {
        if (-not (Test-Junction $dtcore)) { throw "$dtcore is a real folder; remove it to link $target" }
        if ((Get-Item -LiteralPath $dtcore -Force).Target -ne $target) { (Get-Item -LiteralPath $dtcore -Force).Delete(); New-Item -ItemType Junction -Path $dtcore -Target $target | Out-Null }
    }
    else { New-Item -ItemType Junction -Path $dtcore -Target $target | Out-Null }
    Write-Host "DTCore linked from local folder $target (not checked out by this script)"
}
else {
    if (Test-Junction $dtcore) { (Get-Item -LiteralPath $dtcore -Force).Delete() }
    if (-not (Test-Path -LiteralPath (Join-Path $dtcore ".git"))) {
        git clone --quiet $DTCoreSource $dtcore
        if ($LASTEXITCODE -ne 0) { throw "DTCore clone failed from $DTCoreSource" }
    }
    else {
        $origin = (git -C $dtcore remote get-url origin).Trim()
        if ($origin -ne $DTCoreSource) { git -C $dtcore remote set-url origin $DTCoreSource; Write-Host "DTCore origin changed: $origin -> $DTCoreSource" }
    }
    if (@(git -C $dtcore status --porcelain).Count -gt 0) { throw "TestHost DTCore checkout has local changes; commit or discard them first" }
    git -C $dtcore fetch --quiet origin
    if ($LASTEXITCODE -ne 0) { throw "DTCore fetch failed from $DTCoreSource (check the URL and access)" }
    git -C $dtcore -c advice.detachedHead=false checkout --quiet $DTCoreRef 2>$null
    if ($LASTEXITCODE -ne 0) { git -C $dtcore -c advice.detachedHead=false checkout --quiet "origin/$DTCoreRef" }
    if ($LASTEXITCODE -ne 0) { throw "DTCore ref '$DTCoreRef' not found in $DTCoreSource" }
    Write-Host "DTCore $DTCoreRef from $DTCoreSource"
}
$actual = ""
if (Test-Path -LiteralPath (Join-Path $dtcore ".git")) { $actual = (git -C $dtcore rev-parse HEAD).Trim() }
if ($actual -and $actual -ne $validatedCommit) {
    Write-Warning "DTCore is at $actual; this plugin was validated with $validatedCommit (Docs/Compatibility.md). Run run_measurements.ps1 before relying on it."
}

# This repository is the plugin itself: expose it as Plugins/DTDevice through a directory junction so
# edits are tested in place. Plugin discovery stops at DTDevice.uplugin, so the junction does not recurse.
$device = Join-Path $plugins "DTDevice"
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
