<#
  SPRINT-016 world chores, headless proof on Morrowmere v2 (no window, no sound, no match flow):
    1. kg.WorldChore.Routes  every chore x variant: navmesh legs between its spots (+ ladder climbs, work time) at walking
                             / carrying speed -> Saved/KG_WorldChoreRoutes.json (acceptance: reachable, 30-75 s)
    2. kg.WorldChore.Pose    each chore's key moment staged (spot states, items, villagers), shot offscreen
                             -> Saved/Screenshots/WorldChores/WC_<Chore>.png (+ Twist_Poison, Twist_TwoCarry)
  Driven by Tools/Unreal/kg_worldchore_capture.py (the "py" console command works in -game).
  Usage: powershell -File Tools/Unreal/kg_worldchore_capture.ps1 [-Shots "WaterRun,Nets"] [-NoShots]
  Log: Saved/Logs/kg_worldchore_capture.log
#>
param([string]$Shots = "all", [switch]$NoShots, [int]$TimeoutSec = 420)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\WorldChores"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "WC_*.png" -ErrorAction SilentlyContinue | Remove-Item
$Log = Join-Path $Root "Saved\Logs\kg_worldchore_capture.log"
$env:KG_WC_SHOTS = $Shots
$env:KG_WC_NOSHOTS = if ($NoShots) { "1" } else { "0" }
# The "py" console command needs a path without spaces.
$Script = Join-Path $env:TEMP "kg_worldchore_capture.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_worldchore_capture.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2?game=/Script/Engine.GameModeBase", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_WORLDCHORE_CAPTURE timeout" }
Get-ChildItem $Raw -Filter "WC_*.png" -ErrorAction SilentlyContinue | ForEach-Object { Move-Item $_.FullName (Join-Path $Out $_.Name) -Force }
Select-String -Path $Log -Pattern "KG_WORLDCHORE_ROUTE|KG_WORLDCHORE_POSE|KG_WORLDCHORE_SETUP|KG_WC_CAPTURE" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: (Warning: )?', '' }
Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length
