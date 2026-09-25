<#
  Headless emote network smoke: a listen server and one client as windowless -nullrhi game processes (no GPU, no
  focus stealing), both started with -KGEmoteSmoke so UKGEmoteSubsystem scripts emote traffic on each machine:
  wave (upper body) -> dance (full body) -> walk forward (must cancel the dance) -> sit -> attack (must cancel) ->
  6x clap (rate limit), dumping what every machine sees (KG_EMOTE_SEEN lines) plus the server's START/STOP/REJECT log
  and each machine's replication log (KG_EMOTE_REP).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_emote_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox]
  Needs an up-to-date editor build (run_gates.ps1). Logs: Saved/Logs/EmoteSmokeServer.log, EmoteSmokeClient.log.
  Exit code 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7792,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGEmoteSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=240"
$SrvLog = Join-Path $Logs "EmoteSmokeServer.log"
$CliLog = Join-Path $Logs "EmoteSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=EmoteSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=EmoteSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_EMOTE_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_EMOTE_DONE" 30
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    Select-String -Path $l -Pattern "KG_EMOTE" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) }
$srvStart = Lines $SrvLog "KG_EMOTE_START"
$srvStop = Lines $SrvLog "KG_EMOTE_STOP"
if (-not ($srvStart -match "emote=dance")) { $fail += "server never started a dance" }
if (-not ($srvStop -match "emote=dance reason=(Moved|Requested)")) { $fail += "dance was not cancelled by moving" }
if (-not ($srvStop -match "emote=sit reason=Attacked")) { $fail += "sit was not cancelled by the attack" }
if (-not (Lines $SrvLog "KG_EMOTE_REJECT.*RateLimited|RateLimited")) {
    # The chat relay's own reaction limiter may swallow the burst before the emote limiter: count clap starts instead.
    $claps = ($srvStart -match "emote=clap").Count
    if ($claps -gt 6) { $fail += "clap spam was not limited ($claps starts)" }
}
$clapStarts = ($srvStart -match "emote=clap").Count
if ($clapStarts -ge 12) { $fail += "rate limit let $clapStarts claps through" }
# Replication: the client sees the host's dance and its own dance; the host sees the client's.
$cliSeen = Lines $CliLog "KG_EMOTE_SEEN.*tag=dance"
if (-not ($cliSeen -match "local=0 replicated=dance shown=dance")) { $fail += "client did not see the host's dance" }
if (-not ($cliSeen -match "local=1 replicated=dance shown=dance")) { $fail += "client did not see its own dance" }
$srvSeen = Lines $SrvLog "KG_EMOTE_SEEN.*tag=dance"
if (-not ($srvSeen -match "local=0 replicated=dance shown=dance")) { $fail += "host did not see the client's dance" }
if (-not ($cliSeen -match "local=1 .*cam=(0\.[5-9]|1\.)")) { $fail += "client's own camera did not pull out for the dance" }
$cliMoved = Lines $CliLog "KG_EMOTE_SEEN.*tag=moved"
if (-not ($cliMoved -match "local=1 replicated=None shown=None")) { $fail += "client's dance still shown after moving" }
if (-not ($cliMoved -match "local=0 replicated=None shown=None")) { $fail += "host's dance still shown on the client after moving" }
if (-not (Lines $CliLog "KG_EMOTE_SEEN.*tag=wave.*local=1 replicated=wave shown=wave.*cam=0\.00")) { $fail += "wave should stay first person (gesture)" }

if ($fail.Count -gt 0) {
    Write-Host "EMOTE SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "EMOTE SMOKE PASSED" -ForegroundColor Green
