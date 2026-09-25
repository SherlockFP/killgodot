<#
  Morrowmere v2, full headless pipeline (the editor may stay closed; nothing opens a window):
    1. Blender  : Tools/Blender/kg_build_terrain_v2.py  -> Art/Packed/KG_Terrain_v2.glb + _heights.json
    2. UE cmdlet: Tools/Unreal/kg_import_terrain_v2.py  -> /Game/KillGodot/Env/Terrain/KG_Terrain_v2 (+ harbour prop pack)
                  + kg_import_dress_pack.py KG_DressTerrace_Clean (plan 11.4 props, when Art/Packed has the pack)
                  + kg_make_ocean.py (MPC_KG_Water for the calm basin, created if missing)
    3. Python   : Tools/Level/prep_v2_placements.py     -> Art/Packed/KG_V2_Placements.json
    4. UE cmdlet: Tools/Unreal/kg_build_village_v2.py   -> /Game/KillGodot/Maps/L_Morrowmere_v2 (+ minimap)
    5. UE cmdlet: Tools/Unreal/kg_dress_v2.py           -> zone dressing (Tools/Unreal/dressing/v2), helper assets,
                  brook water + slate roofs, minimap redrawn with the dressing, Saved/KG_V2_DressReport.json
    6. UE cmdlet: ResavePackages -BuildNavigationData   -> navmesh saved into the level (the editor world's
                  AsyncLoadLock is switched off with an -ini override, it never releases without editor ticks)
    7. UE cmdlet: Tools/Unreal/kg_fix_gltf_materials.py -> project-owned glTF base materials with the ISM/Nanite usage
                  flags + a check of every material on the map's (H)ISMs -> Saved/KG_V2_MaterialCheck.json
    8. Capture  : Tools/Unreal/kg_capture_v2.ps1        -> Saved/Screenshots/V2/*.png + Saved/KG_V2_NavCheck.json
                  (-game -RenderOffScreen -nosound, GameModeBase; real navmesh paths fountain -> doors/chores/starts)
    9. Placement: Tools/Unreal/kg_placement_check_v2.ps1 -> Saved/KG_V2_PlacementSamples.json (SPRINT-022 geometric
                  validator data: traces under every exterior prop; verify_v2_build.py evaluates them)
  Bots: powershell -File Tools/Unreal/kg_bot_test_v2.ps1 (20-bot -nullrhi soak -> Saved/KG_V2_BotTest.json)
  Usage: powershell -File Tools/Unreal/kg_build_v2_all.ps1 [-From 1] [-To 7] [-Shots "top,S1_postcard"] [-Zones "square harbour"]
  Then:  python Tools/Level/verify_v2_build.py
  Logs: Saved/Logs/kg_*_v2.log
  UE steps wait while a C++ build (UnrealBuildTool) runs: a commandlet would hold the editor DLLs the linker needs.
#>
param([int]$From = 1, [int]$To = 9, [string]$Shots = "", [string]$Zones = "")
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Blender = "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
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
    Select-String -CaseSensitive -Path $log -Pattern $pat | Select-Object -Unique | ForEach-Object { "   " + $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
}
function WaitBuild() {
    $told = $false
    while (Get-Process -Name "UnrealBuildTool", "link", "cl" -ErrorAction SilentlyContinue) {
        if (-not $told) { Write-Host "   waiting for a C++ build to finish..."; $told = $true }
        Start-Sleep -Seconds 10
    }
}

Step 1 "terrain (Blender)" {
    & $Blender --background --factory-startup --python "$R/Tools/Blender/kg_build_terrain_v2.py" -- "$R/Art/Packed/KG_Terrain_v2.glb" 2>&1 |
        Select-String "KG_TERRAIN_V2|Error|Traceback" | ForEach-Object { "   " + $_.Line }
}
Step 2 "terrain import (UE commandlet)" {
    WaitBuild
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_import_terrain_v2.py harbour" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_import_terrain_v2.log" | Out-Null
    Grep "$R/Saved/Logs/kg_import_terrain_v2.log" "LogPython: KG_TERRAIN_V2: (terrain meshes|harbour)|Traceback"
    if (Test-Path "$R/Art/Packed/KG_DressTerrace_Clean.json") {
        & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_import_dress_pack.py KG_DressTerrace_Clean" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_import_terrace.log" | Out-Null
        Grep "$R/Saved/Logs/kg_import_terrace.log" "KG_DRESS_PACK|Traceback"
    }
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_make_ocean.py" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_make_ocean.log" | Out-Null
    Grep "$R/Saved/Logs/kg_make_ocean.log" "KG_OCEAN|Traceback"
    # SPRINT-022: landmark / tower / cliff pack, per-house paint, the water family (+ MPC_KG_Weather)
    if (Test-Path "$R/Art/Packed/KG_DressLandmarks_Clean.json") {
        & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_import_dress_pack.py KG_DressLandmarks_Clean" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_import_landmarks.log" | Out-Null
        Grep "$R/Saved/Logs/kg_import_landmarks.log" "KG_DRESS_PACK|Traceback"
    }
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_make_paint_v2.py" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_make_paint_v2.log" | Out-Null
    Grep "$R/Saved/Logs/kg_make_paint_v2.log" "KG_PAINT|Traceback"
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_make_water_v2.py" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_make_water_v2.log" | Out-Null
    Grep "$R/Saved/Logs/kg_make_water_v2.log" "KG_WATER_V2|Traceback"
}
Step 3 "placements (system Python)" {
    python "$R/Tools/Level/prep_v2_placements.py" | ForEach-Object { "   " + $_ }
}
Step 4 "village (UE commandlet)" {
    WaitBuild
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_build_village_v2.py" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_build_village_v2.log" | Out-Null
    Grep "$R/Saved/Logs/kg_build_village_v2.log" "LogPython: KG_V2 (saved|report)|FAILED|Traceback"
}
Step 5 "dressing (UE commandlet)" {
    WaitBuild
    python "$R/Tools/Level/make_slate_texture.py" | Out-Null
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_dress_v2.py $Zones" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_dress_v2.log" | Out-Null
    Grep "$R/Saved/Logs/kg_dress_v2.log" "LogPython: KG_DRESS_V2 (\w+: \{|saved|assets|level fixups|minimap)|FAILED|Traceback"
}
Step 6 "navmesh (ResavePackages -BuildNavigationData)" {
    WaitBuild
    & $Cmd $Proj -run=ResavePackages -Package=/Game/KillGodot/Maps/L_Morrowmere_v2 -BuildNavigationData -IgnoreChangelist `
        "-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False" `
        -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_nav_v2.log" | Out-Null
    Grep "$R/Saved/Logs/kg_nav_v2.log" "Build total execution time|NOT building|Error: [A-Z]"
}
Step 7 "material usage check (UE commandlet)" {
    WaitBuild
    & $Cmd $Proj -run=pythonscript "-script=$R/Tools/Unreal/kg_fix_gltf_materials.py" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_fix_gltf_materials.log" | Out-Null
    Grep "$R/Saved/Logs/kg_fix_gltf_materials.log" "KG_MATFIX (check|reparented|copied|  problem)|Traceback"
}
Step 8 "capture (-game -RenderOffScreen)" {
    WaitBuild
    if ($Shots) { powershell -File "$R/Tools/Unreal/kg_capture_v2.ps1" -Shots $Shots }
    else { powershell -File "$R/Tools/Unreal/kg_capture_v2.ps1" }
}
Step 9 "placement samples (-game -nullrhi, read-only)" {
    WaitBuild
    powershell -File "$R/Tools/Unreal/kg_placement_check_v2.ps1"
}
