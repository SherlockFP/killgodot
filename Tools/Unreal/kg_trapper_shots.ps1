<#
  SPRINT-041 offscreen Trapper shots (no window, no sound): Tools/Unreal/kg_trapper_shots.py stages each scene with
  kg.Trapper.Stage in a windowless -game session and takes HighResShots: rest, bite, marks, flinch, snare, ui.
    powershell -File Tools/Unreal/kg_trapper_shots.ps1 [-Size 1600x900] [-Tag trapper]
  Output: Saved/Screenshots/Trapper/<Tag>_<name>.png   Log: Saved/Logs/kg_trapper_shots.log
#>
param([string]$Size = "1600x900", [string]$Tag = "trapper", [int]$TimeoutSec = 240)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\Trapper"
New-Item -ItemType Directory -Force $Out | Out-Null
$Script = Join-Path $env:TEMP "kg_trapper_shots.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_trapper_shots.py") $Script -Force
$Script = $Script -replace '\\', '/'
$w, $h = $Size.Split("x")
$env:KG_TR_W = $w
$env:KG_TR_H = $h
$env:KG_TR_TAG = $Tag
Get-ChildItem $Raw -Filter "${Tag}_*.png" -ErrorAction SilentlyContinue | Remove-Item
$Log = Join-Path $Root "Saved\Logs\kg_trapper_shots.log"
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=$w", "-ResY=$h", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900", "-ini:Engine:[ConsoleVariables]:kg.BotFill=0",
    "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_TRAPPER_SHOTS timeout" }
Get-ChildItem $Raw -Filter "${Tag}_*.png" -ErrorAction SilentlyContinue | ForEach-Object { Move-Item $_.FullName (Join-Path $Out $_.Name) -Force }
Select-String -Path $Log -Pattern "KG_TRAPPER_SHOTS|KG_DEV|KG_MIMIC|KG_SNARE|Error" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' } | Select-Object -Last 40
Get-ChildItem $Out -Filter "${Tag}_*.png" | Select-Object Name, Length
