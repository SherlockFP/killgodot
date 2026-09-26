<#
  SPRINT-023 offscreen renders (no window, no sound): Tools/Unreal/kg_voice_shots.py drives kg.Mouth.Force,
  kg.Bark.Bots, kg.Voice.Tone and kg.Partner(.Bots) on L_Morrowmere_v2 (KG game mode) and takes HighResShots / UI shots.
    powershell -File Tools/Unreal/kg_voice_shots.ps1
  Output: Saved/Screenshots/Voice/
    mouth_closed.png, mouth_open.png        a bot's face with the mouth pinned (acceptance 2)
    bark_1..3.png                           three frames of a real bark (mouth chewing the syllables)
    bark_ui.png, talking_ui.png             speaking marker + bubble + NEAR line; own mic pill (acceptance 1/3)
    partner_highfive/rps/danceoff.png       two bots aligned, synced clips (acceptance 4)
    partner_rps_result_ui.png, partner_offer_ui.png
  Log lines: KG_VOICE_SHOTS, KG_BARK, KG_PARTNER_START
#>
param([int]$TimeoutSec = 600)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\Voice"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "Voice_*.png" -ErrorAction SilentlyContinue | Remove-Item
$Log = Join-Path $Root "Saved\Logs\kg_voice_shots.log"
$Script = Join-Path $env:TEMP "kg_voice_shots.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_voice_shots.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_VOICE_SHOTS timeout" }
Get-ChildItem $Raw -Filter "Voice_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    # "shot" appends a 5-digit counter even with nosuffix: strip it.
    Move-Item $_.FullName (Join-Path $Out (($_.Name.Substring(6) -replace '\d{5}\.png$', '.png').ToLower())) -Force
}
Select-String -Path $Log -Pattern "KG_VOICE_SHOTS|KG_BARK |KG_PARTNER_START|KG_PARTNER_REJECT|Mouth:" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' } | Select-Object -First 40
Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length
