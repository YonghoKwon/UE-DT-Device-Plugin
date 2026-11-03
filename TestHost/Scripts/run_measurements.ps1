param(
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.3",
    # Minimal device measurements that need only DTCore + DTDevice (no UE-DT-Project content).
    [string[]]$Tests = @(
        "MA0T10.Architecture",                      # wire golden bytes (PCD 33-byte, headers) and context snapshots
        "MA0T10.LidarGeometry",                     # ray/beam geometry
        "MA0T10.LidarWallClock",                    # acquisition rates, cadence, backpressure
        "MA0T10.LidarRegression.ReadbackReuseRhi",  # GPU readback reuse on D3D12
        "MA0T10.SensorStream.Pcd20Hz",              # PCD 20 Hz stream diagnostics
        "MA0T10.SensorFiles.PeriodicCadence",       # periodic capture cadence
        "MA0T10.CameraGroundTruth.IntrinsicsAndMaskBoxes", # camera intrinsics / boxes
        "HATCH26.StartupOracleRhi",                 # native vs RHI exact hit records
        "HATCH26.NativeSafetyPolicy",               # recovery/ban policy
        "MA0T10.DevicePlugin"                       # shader folder resolves to the plugin
        # Not here: MA0T10.LidarRegression.SemanticPsoTimeoutRecoveryRhi needs the host's semantic
        # material asset and Slab label registration; it runs in UE-DT-Project until the assets move.
        # MA0T10.SensorReplay.* read UE-DT-Project Samples/*.csv fixtures; add them once fixtures move into the plugin.
    ),

    [switch]$NullRhi
)
$ErrorActionPreference = "Stop"
$hostRoot = Split-Path -Parent $PSScriptRoot
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$out = Join-Path $hostRoot "Saved\Measurements\$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$log = Join-Path $out "editor.log"
$rhi = if ($NullRhi) { "-NullRHI" } else { "-d3d12" }
$filter = ($Tests -join "+")
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" (Join-Path $hostRoot "DTDeviceTestHost.uproject") $rhi -Unattended -NoSplash -NoSound -ResX=1920 -ResY=1080 `
    "-ExecCmds=Automation RunTests $filter; Quit" "-TestExit=Automation Test Queue Empty" "-ReportExportPath=$out" "-abslog=$log" | Out-Null

$index = Join-Path $out "index.json"
if (-not (Test-Path -LiteralPath $index)) { throw "No automation report at $index (see $log)" }
$data = Get-Content -LiteralPath $index -Raw -Encoding UTF8 | ConvertFrom-Json
$rows = @($data.tests | ForEach-Object { [PSCustomObject]@{ Test = $_.fullTestPath; State = $_.state } })
# Keep the measured values the tests print (Hz, frames, records) next to pass/fail.
$values = @(Select-String -LiteralPath $log -Pattern "actual[0-9.]+Hz|requested[0-9]+Hz|exact native/RHI|native=1 reason" | ForEach-Object { ($_.Line -replace '^\[.*?\]\[.*?\]', '').Trim() } | Select-Object -Unique)
$failed = @($rows | Where-Object { $_.State -ne "Success" })
$summary = [PSCustomObject]@{
    Utc = $stamp; Rhi = $rhi; Requested = $Tests; Ran = $rows.Count; Failed = $failed.Count
    Results = $rows; MeasuredValues = $values; Report = $out
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out "summary.json") -Encoding UTF8
$rows | Format-Table -AutoSize | Out-String | Write-Host
$values | ForEach-Object { Write-Host $_ }
Write-Host "Summary: $out\summary.json"
if ($rows.Count -eq 0 -or $failed.Count -gt 0) { exit 1 }
