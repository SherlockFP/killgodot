<#
  Placement samples of L_Morrowmere_v2 for the geometric validator (SPRINT-022): a -game -nullrhi session (no window,
  no sound, nothing saved) runs Tools/Unreal/kg_placement_check_v2.py -> Saved/KG_V2_PlacementSamples.json.
  Then: python Tools/Level/verify_v2_build.py   (the "placement" section evaluates the samples)
#>
param([int]$TimeoutSec = 400, [string]$Map = "L_Morrowmere_v2")
$env:KG_V2_MAPNAME = ($Map -split "/")[-1]
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Log = Join-Path $Root "Saved\Logs\kg_placecheck_v2.log"
$Script = Join-Path $env:TEMP "kg_placement_check_v2.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_placement_check_v2.py") $Script -Force
$Script = $Script.Replace([char]92, [char]47)
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/$Map`?game=/Script/Engine.GameModeBase", "-game", "-nullrhi",
    "-RenderOffScreen", "-nosound", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_PLACECHECK timeout" }
Select-String -Path $Log -Pattern "KG_PLACECHECK" | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
