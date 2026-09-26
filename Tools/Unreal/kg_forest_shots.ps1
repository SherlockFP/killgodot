<#
  SPRINT-033/034 offscreen renders (no window, no sound) on L_Morrowmere_v2, KG game mode, standalone, night rules:
    KG_Cap_forest_eyes_night.png   the player's view: wolves circling at 15-22 m, glowing eyes (eyes stage)
    KG_Cap_forest_attack_1..3.png  side view of the player while the pack's lunger bites (attack stage)
    KG_Cap_forest_mist.png         the player's view back at the Mist tongue rolling in
    KG_Cap_forest_mist_side.png    the tongue from the side
    KG_Cap_forest_frost_ui.png     the HUD: frost at the screen edge, the line and the arrow back to the path
    KG_Cap_forest_camp_fire.png    the lit camp fire at the vigil camp
  Driven by kg.After + kg.Forest.Test / kg.Forest.Cam (Forest/KGForestCommands.cpp). Output: Saved/Screenshots/Forest/.
  Usage: powershell -File Tools/Unreal/kg_forest_shots.ps1
#>
param([int]$TimeoutSec = 240)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\Forest"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "Forest_*" -ErrorAction SilentlyContinue | Remove-Item
$Log = Join-Path $Root "Saved\Logs\kg_forest_shots.log"
$W = 1600; $H = 900
$Steps = @(
    "kg.After 14 kg.Forest.Night 1",
    "kg.After 14.5 kg.World.Look Night",
    "kg.After 16 kg.Forest.Test wolf me",
    "kg.After 37 kg.Forest.Cam pov:wolf",
    "kg.After 38 HighResShot ${W}x${H} filename=Forest_eyes_night",
    "kg.After 39.5 kg.Forest.Cam me 4.5 1.4 75",
    "kg.After 40.5 HighResShot ${W}x${H} filename=Forest_attack_1",
    "kg.After 41.3 kg.Forest.Cam me 4.5 1.4 75",
    "kg.After 41.6 HighResShot ${W}x${H} filename=Forest_attack_2",
    "kg.After 44.8 kg.Forest.Cam me 4.5 1.4 75",
    "kg.After 45.2 HighResShot ${W}x${H} filename=Forest_attack_3",
    "kg.After 48 kg.Forest.Test stop me",
    "kg.After 48.5 kg.Forest.Cam off",
    "kg.After 50 kg.Forest.Test mist me",
    "kg.After 75 kg.Forest.Cam pov:mist",
    "kg.After 76 HighResShot ${W}x${H} filename=Forest_mist",
    "kg.After 76.8 kg.Forest.Cam mist 16 5 50",
    "kg.After 77.5 HighResShot ${W}x${H} filename=Forest_mist_side",
    "kg.After 78.3 kg.Forest.Cam off",
    "kg.After 79 shot showui filename=Forest_frost_ui",
    "kg.After 88 kg.Forest.Goto camp",
    "kg.After 89 kg.Forest.Test fire me stay",
    "kg.After 91 kg.Forest.Cam camp 7 2.2 30",
    "kg.After 92.5 HighResShot ${W}x${H} filename=Forest_camp_fire",
    "kg.After 94 kg.Forest.Status",
    "kg.After 96 quit"
) -join ","
$p = Start-Process -FilePath $Editor -PassThru -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=$W", "-ResY=$H", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900", "-ini:Engine:[ConsoleVariables]:kg.Forest.Enabled=1",
    "`"-ExecCmds=$Steps`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_FOREST_SHOTS timeout" }
Get-ChildItem $Raw -Filter "Forest_*" -ErrorAction SilentlyContinue | ForEach-Object {
    $n = ($_.BaseName -replace '\d{5}$', '').Substring(7).ToLower()
    Move-Item $_.FullName (Join-Path $Out ("KG_Cap_forest_" + $n + ".png")) -Force
}
Select-String -Path $Log -Pattern "KG_FOREST(_TEST|_STATUS|_WOLF_RIG| cam| player=| bite| mist| wolf spawn| lanterns| world)" |
    ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' } | Select-Object -First 40
Get-ChildItem $Out -Filter "KG_Cap_forest_*.png" | Select-Object Name, Length, LastWriteTime
