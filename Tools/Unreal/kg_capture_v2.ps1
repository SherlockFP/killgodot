<#
  Offscreen capture tour of L_Morrowmere_v2 (no window, no sound). Usage:
    powershell -File Tools/Unreal/kg_capture_v2.ps1                 # all shots
    powershell -File Tools/Unreal/kg_capture_v2.ps1 top,S1_postcard  # a subset
  Output: Saved/Screenshots/V2/<name>.png (see Tools/Unreal/kg_capture_v2.py for the shot list)
#>
param([string]$Shots = "", [int]$Warmup = 14, [int]$TimeoutSec = 400)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\V2"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "V2_*.png" -ErrorAction SilentlyContinue | Remove-Item
$env:KG_SHOTS = $Shots
$env:KG_WARMUP = "$Warmup"
$Log = Join-Path $Root "Saved\Logs\kg_capture_v2.log"
# -ExecutePythonScript is editor-only; the "py" console command works in -game (its path must have no spaces).
$Script = Join-Path $env:TEMP "kg_capture_v2.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_capture_v2.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2?game=/Script/Engine.GameModeBase", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_CAPTURE timeout" }
Get-ChildItem $Raw -Filter "V2_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    Move-Item $_.FullName (Join-Path $Out ($_.Name.Substring(3))) -Force
}
Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length
