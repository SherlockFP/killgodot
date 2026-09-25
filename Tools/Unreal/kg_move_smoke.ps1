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
    [int]$PingMs = 100   # simulated one-way latency via the engine's built-in PktLag (contract: "at 100 ms ping")
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $Logs | Out-Null
# PktLag simulates one-way latency (ms); both ends set it so the round trip is ~2xPktLag =~ PingMs.
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGMoveSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=240 -PktLag=$([int]($PingMs/2))"
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

$PngOut = Join-Path $Root "Docs\Level\SPRINT-026_speed_curve.png"
python (Join-Path $PSScriptRoot "kg_move_curve.py") $CliLog $PngOut
