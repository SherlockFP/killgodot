<#
  Walkability check of the underground in L_Morrowmere_v2 (Tools/Unreal/kg_underground_walk.py): one windowless
  -game -nullrhi process, read-only (saves nothing). Prints the KG_WALK lines (regions reached, cut-off cells, a plan).
  Usage: powershell -File Tools/Unreal/kg_underground_walk.ps1 [-Map <level>]    Log: Saved/Logs/kg_underground_walk.log
  Exit 1 when a room other than the (locked) Treasure Vault cannot be reached from the well shaft or the crypt stair,
  or when the two sides do not meet through the old mine tunnel.
#>
param([string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2", [int]$TimeoutSec = 400)
$env:KG_WALK_MAP = $Map
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Log = Join-Path $Root "Saved\Logs\kg_underground_walk.log"
Remove-Item $Log -ErrorAction SilentlyContinue
# the "py" console command works in -game; its path must have no spaces
$Script = Join-Path $env:TEMP "kg_underground_walk.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_underground_walk.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "$Map`?game=/Script/Engine.GameModeBase", "-game", "-nullrhi",
    "-nosound", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_WALK timeout" }
$lines = @(Select-String -Path $Log -Pattern "KG_WALK" | ForEach-Object { $_.Line -replace '^.*LogPython: ', '' } | Select-Object -Unique)
$lines | ForEach-Object { Write-Host $_ }
if (-not ($lines -match "KG_WALK verdict PASS")) { Write-Host "UNDERGROUND WALK: FAIL" -ForegroundColor Red; exit 1 }
Write-Host "UNDERGROUND WALK: PASS" -ForegroundColor Green
