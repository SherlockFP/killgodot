<#
  Offscreen shots of a level (no window, no sound): Tools/Unreal/kg_s22_lab_capture.py.
  powershell -File Tools/Unreal/kg_s22_lab_capture.ps1 -Map L_KG_S22_Lab -Shots "kit:0,-12,3,0,0,1.5,70;..." [-Out S22Lab]
  -Map is the level name under /Game/KillGodot/Maps (Test/ for the lab). PNGs -> Saved/Screenshots/<Out>/<name>.png
#>
param([string]$Map = "Test/L_KG_S22_Lab", [string]$Shots = "", [string]$Out = "S22Lab", [string]$Cmds = "",
      [string]$Res = "1600x900", [int]$TimeoutSec = 400, [string]$Mode = "GameModeBase")
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Dst = Join-Path $Root "Saved\Screenshots\$Out"
New-Item -ItemType Directory -Force $Dst | Out-Null
$Prefix = "LAB$([System.Diagnostics.Process]::GetCurrentProcess().Id)_"
$env:KG_LAB_MAP = ($Map -split "/")[-1]
$env:KG_LAB_SHOTS = $Shots
$env:KG_LAB_PREFIX = $Prefix
$env:KG_LAB_CMDS = $Cmds
$env:KG_LAB_RES = $Res
$Log = Join-Path $Root "Saved\Logs\kg_s22_lab_capture.log"
$Script = Join-Path $env:TEMP "kg_s22_lab_capture.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_s22_lab_capture.py") $Script -Force
$Script = $Script.Replace([char]92, [char]47)
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/$Map`?game=/Script/Engine.$Mode", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_LABCAP timeout" }
Get-ChildItem $Raw -Filter "$Prefix*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    Move-Item $_.FullName (Join-Path $Dst ($_.Name.Substring($Prefix.Length))) -Force
}
Select-String -Path $Log -Pattern "KG_LABCAP" | ForEach-Object { $_.Line.Substring(0, [Math]::Min(200, $_.Line.Length)) }
