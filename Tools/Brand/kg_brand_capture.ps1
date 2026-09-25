<#
  Brand / key-art render tour of L_Morrowmere_v2 (no window, no sound; -game -RenderOffScreen).
    powershell -File Tools/Brand/kg_brand_capture.ps1                          # all brand shots
    powershell -File Tools/Brand/kg_brand_capture.ps1 br_belvedere,ld_heart    # a subset
  Shots and lighting looks: Tools/Brand/kg_brand_capture.py (reuses Tools/Unreal/kg_capture_v2.py).
  Output: Saved/Screenshots/Brand/<name>.png (2560x1440).
#>
param([string]$Shots = "", [int]$Warmup = 20, [int]$TimeoutSec = 600)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\Brand"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "BR_*.png" -ErrorAction SilentlyContinue | Remove-Item
$env:KG_SHOTS = $Shots
$env:KG_WARMUP = "$Warmup"
$env:KG_NAVCHECK = "0"
$Log = Join-Path $Root "Saved\Logs\kg_brand_capture.log"
# The "py" console command needs a path without spaces.
$Script = Join-Path $env:TEMP "kg_brand_capture.py"
Copy-Item (Join-Path $Root "Tools\Brand\kg_brand_capture.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2?game=/Script/Engine.GameModeBase", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_BRAND_CAPTURE timeout" }
Get-ChildItem $Raw -Filter "BR_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    Move-Item $_.FullName (Join-Path $Out ($_.Name.Substring(3))) -Force
}
Select-String -Path $Log -Pattern "KG_CAPTURE_V2" | ForEach-Object { $_.Line }
Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length
