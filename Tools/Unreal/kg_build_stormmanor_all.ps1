<#
  Storm Manor (map 2), full headless pipeline (no window; the editor may stay closed):
    1. Python   : Tools/Level/gen_stormmanor_chores.py + render_stormmanor_minimap.py (world-chore data, minimap PNG)
    2. UE cmdlet: Tools/Unreal/kg_build_stormmanor.py -> /Game/KillGodot/Maps/L_StormManor (+ materials, minimap,
                  Saved/KG_SM_BuildReport.json, Saved/KG_SM_Props.json)
    3. UE cmdlet: ResavePackages -BuildNavigationData -> the navmesh saved into the level
    4. -game    : Tools/Unreal/kg_capture_stormmanor.ps1 -Mode probe -> Saved/KG_SM_Probe.json (nav paths, placement
                  traces) + Saved/KG_WorldChoreRoutes.json (kg.WorldChore.Routes)
    5. -game    : Tools/Unreal/kg_capture_stormmanor.ps1 -> Saved/Screenshots/SM/*.png (render tour)
    6. Python   : Tools/Level/verify_stormmanor_build.py (acceptance checks) [+ contact sheet]
  Usage: powershell -File Tools/Unreal/kg_build_stormmanor_all.ps1 [-From 1] [-To 6] [-Shots "great_hall,aerial_se"]
  Logs: Saved/Logs/kg_sm_*.log. UE steps wait while a C++ build runs (a commandlet would hold the DLLs the linker needs).
#>
param([int]$From = 1, [int]$To = 6, [string]$Shots = "")
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Cmd = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$R = $Root -replace '\\', '/'

function Step($n, $name, [scriptblock]$body) {
    if ($n -lt $From -or $n -gt $To) { return }
    $t = Get-Date
    Write-Host "== $n $name"
    & $body
    Write-Host ("   {0:N0}s" -f ((Get-Date) - $t).TotalSeconds)
}
function Grep($log, $pat) {
    Select-String -CaseSensitive -Path $log -Pattern $pat | Select-Object -Unique | ForEach-Object { "   " + $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
}
function WaitBuild() {
    $told = $false
    while (Get-Process -Name "UnrealBuildTool", "link" -ErrorAction SilentlyContinue) {
        if (-not $told) { Write-Host "   waiting for a C++ build to finish..."; $told = $true }
        Start-Sleep -Seconds 10
    }
}

Step 1 "chore data + minimap (system Python)" {
    python "$R/Tools/Level/gen_stormmanor_chores.py" | ForEach-Object { "   " + $_ }
    python "$R/Tools/Level/render_stormmanor_minimap.py" | ForEach-Object { "   " + $_ }
}
Step 2 "manor (UE commandlet)" {
    WaitBuild
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_build_stormmanor.py" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_sm_build.log" | Out-Null
    Grep "$R/Saved/Logs/kg_sm_build.log" "LogPython: KG_SM (saved|report|flush|dress:)|FAILED|Traceback"
}
Step 3 "navmesh (ResavePackages -BuildNavigationData)" {
    WaitBuild
    & $Cmd $Proj -run=ResavePackages -Package=/Game/KillGodot/Maps/L_StormManor -BuildNavigationData -IgnoreChangelist `
        "-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False" `
        -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_sm_nav.log" | Out-Null
    Grep "$R/Saved/Logs/kg_sm_nav.log" "Build total execution time|NOT building|Error: [A-Z]"
}
Step 4 "probe (-game -nullrhi): nav, placement, chore routes" {
    WaitBuild
    powershell -File "$R/Tools/Unreal/kg_capture_stormmanor.ps1" -Mode probe
}
Step 5 "render tour (-game -RenderOffScreen)" {
    WaitBuild
    if ($Shots) { powershell -File "$R/Tools/Unreal/kg_capture_stormmanor.ps1" -Shots $Shots }
    else { powershell -File "$R/Tools/Unreal/kg_capture_stormmanor.ps1" }
}
Step 6 "verify" {
    python "$R/Tools/Level/verify_stormmanor_build.py"
}
