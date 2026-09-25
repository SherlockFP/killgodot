<#
  Off-screen screenshots of the role reveal ceremony (SPRINT-015 acceptance 2): every stage (table, shuffle, deal,
  flip, role, role+ready) for a Town, an Impatient (with accomplices) and a Neutral role, plus the streamer-mode card
  and the role stage at other screen sizes. One windowless -RenderOffScreen game process on the front-end map
  (kg.UIShot renders the Slate widget into a render target; no window, no focus stealing).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_reveal_shots.ps1 [-Town Sheriff -Impatient Clockmaster -Neutral Executioner]
  Output: Saved/UIShots/reveal-<role>-<stage>_<W>x<H>.png. Needs an up-to-date editor build.
#>
param(
    [string]$Town = "Sheriff",
    [string]$Impatient = "Clockmaster",
    [string]$Neutral = "Executioner",
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Log = Join-Path $Root "Saved\Logs\RevealShots.log"
Remove-Item $Log -ErrorAction SilentlyContinue
$cmds = @(
    "kg.After 6 kg.UIShot revealall:$Town 1920x1080",
    "kg.After 9 kg.UIShot revealall:$Impatient 1920x1080",
    "kg.After 12 kg.UIShot revealall:$Neutral 1920x1080",
    "kg.After 15 kg.UIShot reveal:${Impatient}:role 1280x720 3440x1440 1024x768",
    "kg.After 18 kg.UIShot reveal:${Impatient}:role:ready:streamer 1920x1080",
    "kg.After 22 quit"
) -join ","
$p = Start-Process $Exe -ArgumentList "`"$Proj`" /Game/KillGodot/Maps/L_MainMenu -game -RenderOffScreen -nosound -unattended -nosplash -ResX=1280 -ResY=720 -log=RevealShots.log `"-ExecCmds=$cmds`"" -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "timed out" -ForegroundColor Red }
# Streamer mode in the lobby room: host a real lobby (kg.Session host gives it a join code), turn streamer mode on
# for this session only, shoot the room (join code masked). Output: Saved/UIShots/lobby_1920x1080.png
$LobbyLog = Join-Path $Root "Saved\Logs\RevealShotsLobby.log"
Remove-Item $LobbyLog -ErrorAction SilentlyContinue
$lcmds = @("kg.After 5 kg.Session host Streamer_village", "kg.After 25 kg.Streamer 1", "kg.After 27 kg.UIShot lobby 1920x1080",
           "kg.After 29 kg.Streamer dump", "kg.After 31 quit") -join ","
$lp = Start-Process $Exe -ArgumentList "`"$Proj`" /Game/KillGodot/Maps/L_MainMenu -game -RenderOffScreen -nosound -unattended -nosplash -ResX=1280 -ResY=720 -log=RevealShotsLobby.log `"-ExecCmds=$lcmds`"" -PassThru -WindowStyle Hidden
if (-not $lp.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $lp.Id -Force; Write-Host "lobby shot timed out" -ForegroundColor Red }
Select-String -Path $LobbyLog -Pattern "KG_UISHOT lobby|KG_STREAMER|KG_LOBBY open" | ForEach-Object { Write-Host ($_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '') }

$shots = @(Select-String -Path $Log -Pattern "KG_UISHOT reveal" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' })
$shots | ForEach-Object { Write-Host $_ }
$failed = @($shots | Where-Object { $_ -match 'FAILED' })
if ($shots.Count -lt 18 -or $failed.Count) { Write-Host "REVEAL SHOTS INCOMPLETE ($($shots.Count) shots, $($failed.Count) failed)" -ForegroundColor Red; exit 1 }
Write-Host "PASS reveal shots ($($shots.Count))" -ForegroundColor Green
exit 0
