<#
  Offscreen fishing captures (no window, no sound): Tools/Unreal/kg_fish_capture.py drives the kg.Fish.* dev verbs on
  L_Morrowmere_v2 (KG game mode) and takes HighResShots of every beat.
    powershell -File Tools/Unreal/kg_fish_capture.ps1
  Output: Saved/Screenshots/Fish/<fish_ready|fish_waiting|fish_bite|fish_fight|fish_catch|fish_tp|fish_market>.png
#>
param([int]$TimeoutSec = 420)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\Fish"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "Fish_*.png" -ErrorAction SilentlyContinue | Remove-Item
$Log = Join-Path $Root "Saved\Logs\kg_fish_capture.log"
# The "py" console command works in -game (its path must have no spaces).
$Script = Join-Path $env:TEMP "kg_fish_capture.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_fish_capture.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_FISH_CAPTURE timeout" }
Get-ChildItem $Raw -Filter "Fish_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    Move-Item $_.FullName (Join-Path $Out ($_.Name.ToLower())) -Force
}
Select-String -Path $Log -Pattern "KG_FISH_CAPTURE|KG_FISH_(CAST|BITE|HOOK|RESULT|MARKET)|KG_DEV" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length
