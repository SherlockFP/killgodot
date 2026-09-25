<#
  SPRINT-022 water perf A/B (headless -game -RenderOffScreen 1920x1080, no window, no sound):
  Tools/Unreal/kg_s22_perf.py -> Saved/KG_V2_WaterPerf.json (old vs new water materials, same views, same session).
  Run it with the editor closed (a running editor shares the GPU and makes the numbers noisy).
#>
param([int]$TimeoutSec = 400, [string]$Map = "L_Morrowmere_v2")
$env:KG_V2_MAPNAME = ($Map -split "/")[-1]
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Log = Join-Path $Root "Saved\Logs\kg_s22_perf.log"
$Script = Join-Path $env:TEMP "kg_s22_perf.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_s22_perf.py") $Script -Force
$Script = $Script.Replace([char]92, [char]47)
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/$Map`?game=/Script/Engine.GameModeBase", "-game",
    "-RenderOffScreen", "-nosound", "-ResX=1920", "-ResY=1080", "-unattended", "-nosplash", "-NoLoadingScreen",
    "`"-abslog=$Log`"", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_PERF timeout" }
Select-String -Path $Log -Pattern "KG_PERF" | ForEach-Object { $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
