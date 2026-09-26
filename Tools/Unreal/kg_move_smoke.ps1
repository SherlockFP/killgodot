<#
  SPRINT-026 headless movement smoke: a listen server and one client as windowless -nullrhi game processes (no GPU,
  no focus stealing), both started with -KGMoveSmoke so AKGCharacter::TickMoveSmoke scripts a bad-strafe-then-good-
  strafe run on the connecting client and logs KG_MOVE_CSV speed samples plus a KG_MOVE_DONE summary (final speed +
  UKGCharacterMovement::CorrectionCount, i.e. how many times the server had to correct the client's predicted move -
  the contract's "no corrections at 100 ms ping" check). Also renders the speed-curve PNG via kg_move_curve.py.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_move_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox] [-PingMs 100]
  Needs an up-to-date editor build (run_gates.ps1). Logs: Saved/Logs/MoveSmokeServer.log, MoveSmokeClient.log.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7793,
    [int]$TimeoutSeconds = 240,
    [int]$PingMs = 100,  # simulated one-way latency via the engine's built-in PktLag (contract: "at 100 ms ping")
    # Frame cap for both processes. Uncapped, a -nullrhi client runs ~1200 fps and (pressing Jump every frame, so no
    # two moves can combine) overflows the engine's 96-entry saved-move buffer every ~80 ms at 100 ms ping - the
    # buffer is flushed and corrections can no longer be replayed (46 "Hit limit of 96 saved moves" warnings in the
    # 2026-09-26 run). 120 fps is a realistic player frame rate. Pass 0 for uncapped. Set via -ExecCmds: an ini
    # [ConsoleVariables] t.MaxFPS is overridden by the game user settings' frame-rate limit.
    [int]$MaxFps = 120,
    # Listen-server bot fill (kg.BotFill). Default 0: bots are server-authoritative bodies that the client only sees
    # ~100 ms late, so running into one is a pawn-collision correction, not a movement-prediction one (the 2026-09-26
    # run at 120 fps had 27 corrections in the straight-line "bad" phase from exactly that). Pass 6 to include them.
    [int]$Bots = 0
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $Logs | Out-Null
# PktLag simulates one-way latency (ms); both ends set it so the round trip is ~2xPktLag =~ PingMs.
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGMoveSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=240 -ini:Engine:[ConsoleVariables]:kg.BotFill=$Bots -PktLag=$([int]($PingMs/2)) `"-ExecCmds=p.NetShowCorrections 1, t.MaxFPS $MaxFps`""
$SrvLog = Join-Path $Logs "MoveSmokeServer.log"
$CliLog = Join-Path $Logs "MoveSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=MoveSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=MoveSmokeClient.log" -PassThru -WindowStyle Hidden
    $ok = Wait-Log $CliLog "KG_MOVE_DONE" $TimeoutSeconds
    Write-Host "client scripted run done: $ok"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}

Write-Host "==== $CliLog (KG_MOVE_DONE / correction count)"
Select-String -Path $CliLog -Pattern "KG_MOVE_DONE" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }

$Samples = @(Select-String -Path $CliLog -Pattern "KG_MOVE_CSV").Count
Write-Host "KG_MOVE_CSV samples logged: $Samples"

# Corrections per phase: each KG_MOVE_CORRECTION line is attributed to the phase of the last KG_MOVE_CSV sample.
$Phase = "settle"; $PerPhase = [ordered]@{ settle = 0; bad_strafe = 0; good_strafe = 0 }
foreach ($m in Select-String -Path $CliLog -Pattern "KG_MOVE_CSV,[^,]+,([a-z_]+),|KG_MOVE_CORRECTION") {
    if ($m.Line -match "KG_MOVE_CSV,[^,]+,([a-z_]+),") { $Phase = $Matches[1] }
    elseif ($m.Line -match "KG_MOVE_CORRECTION") { $PerPhase[$Phase] = $PerPhase[$Phase] + 1 }
}
Write-Host ("corrections by phase: settle={0} bad_strafe={1} good_strafe={2}" -f $PerPhase.settle, $PerPhase.bad_strafe, $PerPhase.good_strafe)
$Overflow = @(Select-String -Path $CliLog -Pattern "Hit limit of").Count
$ServerErrors = @(Select-String -Path $SrvLog -Pattern "\*\*\* Server: Error for").Count
Write-Host "saved-move buffer overflows (client): $Overflow; server-side position errors: $ServerErrors"

$PngOut = Join-Path $Root "Docs\Level\SPRINT-026_speed_curve.png"
python (Join-Path $PSScriptRoot "kg_move_curve.py") $CliLog $PngOut
