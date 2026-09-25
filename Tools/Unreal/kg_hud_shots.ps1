<#
  SPRINT-025 offscreen HUD proof (no window, no sound): Tools/Unreal/kg_hud_shots.py runs inside a windowless -game
  session on L_Morrowmere_v2 (KG game mode, warmup held) and takes HighResShots of the chore markers at 3 distances +
  the behind-the-camera case, the held big map, the tapped full map and two HUD demo states, at every requested size.
    powershell -File Tools/Unreal/kg_hud_shots.ps1 [-Sizes "1280x720 1920x1080"] [-Tag before]
  Output: Saved/Screenshots/HUD/<Tag>_<W>x<H>_<name>.png   Log: Saved/Logs/kg_hud_shots_<W>x<H>.log
#>
param([string]$Sizes = "1280x720 1920x1080", [string]$Tag = "hud", [int]$TimeoutSec = 240)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\HUD"
New-Item -ItemType Directory -Force $Out | Out-Null
$Script = Join-Path $env:TEMP "kg_hud_shots.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_hud_shots.py") $Script -Force
$Script = $Script -replace '\\', '/'
foreach ($size in $Sizes.Split(" ")) {
    $w, $h = $size.Split("x")
    $env:KG_HUD_W = $w
    $env:KG_HUD_H = $h
    $env:KG_HUD_TAG = "${Tag}_${w}x${h}"
    Get-ChildItem $Raw -Filter "${Tag}_${w}x${h}_*.png" -ErrorAction SilentlyContinue | Remove-Item
    $Log = Join-Path $Root "Saved\Logs\kg_hud_shots_${w}x${h}.log"
    $p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
        "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2", "-game", "-RenderOffScreen", "-nosound",
        "-ResX=$w", "-ResY=$h", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
        "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900", "-ExecCmds=`"py $Script`"")
    if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_HUD_SHOTS timeout $size" }
    Get-ChildItem $Raw -Filter "${Tag}_${w}x${h}_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
        Move-Item $_.FullName (Join-Path $Out $_.Name) -Force
    }
    Select-String -Path $Log -Pattern "KG_HUD_SHOTS|KG_DEV" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' } | Select-Object -Last 40
}
Get-ChildItem $Out -Filter "${Tag}_*.png" | Select-Object Name, Length
