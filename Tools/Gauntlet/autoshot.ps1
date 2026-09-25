<#
  Launch the game (standalone, windowed), optionally apply a dev scenario, capture one screenshot and quit.
  Usage: powershell -File Tools/Gauntlet/autoshot.ps1 [-Scenario backstab|inspect] [-Name M2_Backstab] [-Map <package>]
  -Map defaults to the default village (Morrowmere v2); the classic map is /Game/KillGodot/Maps/L_Morrowmere.
  Output: Saved/Gauntlet/Shots/<Name>.png
#>
param(
    [string]$Scenario = "",
    [string]$Name = "AutoShot",
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [string]$EngineDir = "D:\Program Files\Epic Games\UE_5.8"
)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $ProjectRoot "KillGodot.uproject"
$ShotDir = Join-Path $ProjectRoot "Saved\Gauntlet\Shots"
New-Item -ItemType Directory -Force $ShotDir | Out-Null
$Raw = Join-Path $ProjectRoot "Saved\Screenshots\WindowsEditor\KG_AutoShot.png"
Remove-Item $Raw -ErrorAction SilentlyContinue
$Log = Join-Path $ShotDir "$Name.log"
$ScenarioArg = if ($Scenario) { "-KGDevScenario=$Scenario" } else { "" }
$Editor = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor.exe"
# The default game map is the front end (L_MainMenu); dev shots always run the village directly.
cmd /c "`"$Editor`" `"$Project`" $Map -game -windowed -ResX=1280 -ResY=720 -KGAutoShot $ScenarioArg -nosplash -unattended `"-abslog=$Log`""
if (Test-Path $Raw) {
    $Out = Join-Path $ShotDir "$Name.png"
    Move-Item $Raw $Out -Force
    Write-Host "KG_SHOT $Out"
} else {
    Write-Host "KG_SHOT_FAILED (see $Log)"
    exit 1
}
