<#
  Storm Manor offscreen session (no window, no sound): the render tour or the probe (Tools/Unreal/kg_capture_stormmanor.py).
    powershell -File Tools/Unreal/kg_capture_stormmanor.ps1                       # all shots -> Saved/Screenshots/SM/
    powershell -File Tools/Unreal/kg_capture_stormmanor.ps1 -Shots great_hall,top
    powershell -File Tools/Unreal/kg_capture_stormmanor.ps1 -Mode probe           # -> Saved/KG_SM_Probe.json (+ routes)
#>
param([string]$Shots = "", [string]$Mode = "shots", [int]$Warmup = 16, [int]$TimeoutSec = 500)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\SM"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "SM_*.png" -ErrorAction SilentlyContinue | Remove-Item
$env:KG_SHOTS = $Shots
$env:KG_WARMUP = "$Warmup"
$env:KG_SM_MODE = $Mode
$Log = Join-Path $Root "Saved\Logs\kg_sm_$Mode.log"
$Script = Join-Path $env:TEMP "kg_capture_stormmanor.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_capture_stormmanor.py") $Script -Force
$Script = $Script.Replace([char]92, [char]47)
$render = if ($Mode -eq "probe") { "-nullrhi" } else { "-RenderOffScreen" }
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_StormManor`?game=/Script/Engine.GameModeBase", "-game", $render,
    "-nosound", "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_SM_CAPTURE timeout" }
Get-ChildItem $Raw -Filter "SM_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    Move-Item $_.FullName (Join-Path $Out ($_.Name.Substring(3))) -Force
}
Select-String -Path $Log -Pattern "KG_SM_CAPTURE (probe|done|error|no )|KG_WORLDCHORE_ROUTES|Traceback" | ForEach-Object { "   " + $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
if ($Mode -ne "probe") { Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length | Format-Table -AutoSize | Out-String | Write-Host }
