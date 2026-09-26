<#
  Headless proximity-voice network smoke (SPRINT-023): a listen server and one client as windowless -nullrhi game
  processes, both started with -KGVoiceSmoke so UKGVoiceSubsystem sends a synthetic 220 Hz tone through the real
  capture -> Opus -> ServerVoice -> per-receiver routing -> ClientVoice path (no microphone headless: the injected
  stream takes the microphone's place, everything after it is the shipping code). The host walks the client through
    near (3 m) -> mid (16.5 m) -> far (40 m) -> muted (3 m, client /mutes the host) -> meeting (40 m, phase Meeting)
    -> ghostspk (host dies: the living client must hear nothing) -> ghostboth (client dies too: ghosts hear ghosts)
  and every machine logs KG_VOICE_HEARD tag=<step> packets=<per second> maxgain=<server gain> played= dropped=.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_voice_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox]
  Needs an up-to-date editor build (run_gates.ps1). Logs: Saved/Logs/VoiceSmokeServer.log, VoiceSmokeClient.log.
  Exit code 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7797,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGVoiceSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=240"
$SrvLog = Join-Path $Logs "VoiceSmokeServer.log"
$CliLog = Join-Path $Logs "VoiceSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=VoiceSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=VoiceSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_VOICE_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_VOICE_DONE" 30
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    Select-String -Path $l -Pattern "KG_VOICE_(CODEC|STEP|HEARD|SMOKE|DONE|REJECT)" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) }
function Steady($Path, $Machine, $Tag) {
    # The last per-second window of a step = its steady state (the first window straddles the teleport / phase change).
    $lines = Lines $Path "KG_VOICE_HEARD $Machine tag=$Tag "
    if ($lines.Count -lt 2) { return $null }
    $l = $lines[-1]
    $m = [regex]::Match($l, 'packets=(\d+) maxgain=([\d.]+) played=(\d+) dropped=(\d+)')
    return @{ packets = [int]$m.Groups[1].Value; gain = [double]$m.Groups[2].Value; played = [int]$m.Groups[3].Value; dropped = [int]$m.Groups[4].Value }
}
if (-not (Lines $CliLog "KG_VOICE_CODEC")) { $fail += "client never opened the voice codec" }
if (-not (Lines $SrvLog "KG_VOICE_CODEC")) { $fail += "host never opened the voice codec" }
$near = Steady $CliLog "Client" "near"
if (-not $near -or $near.packets -lt 5 -or $near.gain -lt 0.95) { $fail += "near (3 m): client should hear the host at full gain, got $($near | ConvertTo-Json -Compress)" }
$mid = Steady $CliLog "Client" "mid"
if (-not $mid -or $mid.packets -lt 5 -or $mid.gain -lt 0.3 -or $mid.gain -gt 0.7) { $fail += "mid (16.5 m): expected ~0.5 gain, got $($mid | ConvertTo-Json -Compress)" }
$far = Steady $CliLog "Client" "far"
if (-not $far -or $far.packets -ne 0) { $fail += "far (40 m): client must receive nothing, got $($far | ConvertTo-Json -Compress)" }
$muted = Steady $CliLog "Client" "muted"
if (-not $muted -or $muted.packets -lt 5 -or $muted.played -ne 0 -or $muted.dropped -lt 1) { $fail += "muted: packets arrive but none may play, got $($muted | ConvertTo-Json -Compress)" }
$meeting = Steady $CliLog "Client" "meeting"
if (-not $meeting -or $meeting.packets -lt 5 -or $meeting.gain -lt 0.95) { $fail += "meeting (40 m): the square hears everyone, got $($meeting | ConvertTo-Json -Compress)" }
$ghostspk = Steady $CliLog "Client" "ghostspk"
if (-not $ghostspk -or $ghostspk.packets -ne 0) { $fail += "ghost speaker: the living client must hear nothing, got $($ghostspk | ConvertTo-Json -Compress)" }
$ghostboth = Steady $CliLog "Client" "ghostboth"
if (-not $ghostboth -or $ghostboth.packets -lt 5 -or $ghostboth.gain -lt 0.95) { $fail += "ghost -> ghost: must hear at full gain, got $($ghostboth | ConvertTo-Json -Compress)" }
$hostNear = Steady $SrvLog "Host" "near"
if (-not $hostNear -or $hostNear.packets -lt 5) { $fail += "client -> host direction: host heard nothing at 3 m, got $($hostNear | ConvertTo-Json -Compress)" }
if (-not (Lines $SrvLog "KG_VOICE_ROUTE .*gain=0\.00 .*tag=far")) { $fail += "server never logged a zero-gain route at 40 m" }
$stepsFailed = Lines $SrvLog "KG_VOICE_STEP dev .*FAILED"
if ($stepsFailed) { $fail += "dev steps failed: $($stepsFailed -join ' | ')" }

if ($fail.Count -gt 0) {
    Write-Host "VOICE SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "VOICE SMOKE PASSED" -ForegroundColor Green
exit 0
