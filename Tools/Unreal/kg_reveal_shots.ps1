<#
  Off-screen screenshots of the role reveal MOMENT (SPRINT-037; the SPRINT-015 card table is gone): every beat
  (spin, tease, flip = the flash, slam = the name landing, role = settled) for a Town, an Impatient (with two
  accomplices) and a Neutral role at 1920x1080, the settled role at 1280x720 for all three, plus the details view
  (Tab held), the ready + streamer-mode variant and two odd screen sizes. One windowless -RenderOffScreen game
  process on the front-end map (kg.UIShot renders the Slate widget into a render target; no window, no focus stealing).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_reveal_shots.ps1 [-Town Sheriff -Impatient Clockmaster -Neutral Executioner] [-Lobby]
  Output: Saved/UIShots/reveal-<role>-<stage>_<W>x<H>.png. Needs an editor build of the current code (UnrealEditor.exe
  -game loads UnrealEditor-KillGodot.dll): run it from a checkout whose Binaries are up to date.
  -Lobby also shoots the lobby room in streamer mode (hosts a real session; slower).
#>
param(
    [string]$Town = "Sheriff",
    [string]$Impatient = "Clockmaster",
    [string]$Neutral = "Executioner",
    [switch]$Lobby,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Log = Join-Path $Root "Saved\Logs\RevealShots.log"
Remove-Item $Log -ErrorAction SilentlyContinue
$cmds = @()
$t = 6
foreach ($role in $Town, $Impatient, $Neutral) {
    foreach ($stage in "spin", "tease", "flip", "slam") {
        $cmds += "kg.After $t kg.UIShot reveal:${role}:$stage 1920x1080"; $t += 1
    }
    $cmds += "kg.After $t kg.UIShot reveal:${role}:role 1920x1080 1280x720"; $t += 1
}
$cmds += "kg.After $t kg.UIShot reveal:${Impatient}:details 1920x1080 1280x720"; $t += 1
$cmds += "kg.After $t kg.UIShot reveal:${Impatient}:role:ready:streamer 1920x1080"; $t += 1
$cmds += "kg.After $t kg.UIShot reveal:${Impatient}:role 3440x1440 1024x768"; $t += 2
$cmds += "kg.After $t quit"
$cmdline = $cmds -join ","
$p = Start-Process $Exe -ArgumentList "`"$Proj`" /Game/KillGodot/Maps/L_MainMenu -game -RenderOffScreen -nosound -unattended -nosplash -ResX=1280 -ResY=720 -log=RevealShots.log `"-ExecCmds=$cmdline`"" -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "timed out" -ForegroundColor Red }

if ($Lobby) {
    # Streamer mode in the lobby room: host a real lobby (kg.Session host gives it a join code), turn streamer mode on
    # for this session only, shoot the room (join code masked). Output: Saved/UIShots/lobby_1920x1080.png
    $LobbyLog = Join-Path $Root "Saved\Logs\RevealShotsLobby.log"
    Remove-Item $LobbyLog -ErrorAction SilentlyContinue
    $lcmds = @("kg.After 5 kg.Session host Streamer_village", "kg.After 25 kg.Streamer 1", "kg.After 27 kg.UIShot lobby 1920x1080",
               "kg.After 29 kg.Streamer dump", "kg.After 31 quit") -join ","
    $lp = Start-Process $Exe -ArgumentList "`"$Proj`" /Game/KillGodot/Maps/L_MainMenu -game -RenderOffScreen -nosound -unattended -nosplash -ResX=1280 -ResY=720 -log=RevealShotsLobby.log `"-ExecCmds=$lcmds`"" -PassThru -WindowStyle Hidden
    if (-not $lp.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $lp.Id -Force; Write-Host "lobby shot timed out" -ForegroundColor Red }
    Select-String -Path $LobbyLog -Pattern "KG_UISHOT lobby|KG_STREAMER|KG_LOBBY open" | ForEach-Object { Write-Host ($_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '') }
}

$shots = @(Select-String -Path $Log -Pattern "KG_UISHOT reveal" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' })
$shots | ForEach-Object { Write-Host $_ }
$failed = @($shots | Where-Object { $_ -match 'FAILED' })
if ($shots.Count -lt 23 -or $failed.Count) { Write-Host "REVEAL SHOTS INCOMPLETE ($($shots.Count) shots, $($failed.Count) failed)" -ForegroundColor Red; exit 1 }
Write-Host "PASS reveal shots ($($shots.Count))" -ForegroundColor Green
exit 0
