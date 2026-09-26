<#
  Headless tabletop network smoke (SPRINT-036b acceptance 3): a listen server and one client as windowless -nullrhi
  game processes, both started with -KGTableSmoke so UKGTabletopSubsystem scripts them (Source/KillGodot/Tabletop):
  the host spawns a board table, seats itself White and the client Black; the client first sends an out-of-turn
  move and an illegal move over the RPC relay (both refused), then plays Black while the host plays Scholar's mate.
  Both machines log the final position (KG_TABLE_FINAL); the FENs and the result must agree.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_table_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Morrowmere_v2]
  Needs an up-to-date editor build (run_gates.ps1). Logs: Saved/Logs/TableSmokeServer.log, TableSmokeClient.log.
  Exit code 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Port = 7794,
    [int]$TimeoutSeconds = 480
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGTableSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=600"
$SrvLog = Join-Path $Logs "TableSmokeServer.log"
$CliLog = Join-Path $Logs "TableSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=TableSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bringing up level for play|Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    Start-Sleep -Seconds 5
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=TableSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_TABLE_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_TABLE_DONE" 60
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    if (Test-Path $l) {
        Select-String -Path $l -Pattern "KG_TABLE" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
    }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { if (Test-Path $Path) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) } else { @() } }
function Fen($Path) {
    $l = @(Lines $Path 'KG_TABLE_FINAL')   # @(): one line unrolls to a string and [0] would be its first char
    if ($l.Count -eq 0) { return $null }
    if ($l[0] -match 'fen="([^"]+)"') { return $Matches[1] } else { return $null }
}
if (-not (Lines $SrvLog "KG_TABLE_SMOKE Host unseated=rejected")) { $fail += "the server accepted a move from an unseated player" }
if (-not (Lines $SrvLog "KG_TABLE_SMOKE Host outofturn=rejected")) { $fail += "the server accepted an out-of-turn move" }
if (-not (Lines $SrvLog "act=reject .*why=not your turn")) { $fail += "the client's out-of-turn move was not refused" }
if (-not (Lines $SrvLog "act=reject .*why=illegal")) { $fail += "the client's illegal move was not refused" }
if (-not (Lines $CliLog "KG_TABLE_NOTICE (illegal|not your turn)")) { $fail += "the client never received a refusal notice" }
if (-not (Lines $SrvLog "act=move by=.* move=g8f6")) { $fail += "the client's legal moves did not arrive on the server" }
if (-not (Lines $SrvLog "KG_TABLE_FINAL Host status=WhiteWins reason=Checkmate")) { $fail += "the host did not end in checkmate for White" }
if (-not (Lines $CliLog "KG_TABLE_FINAL Client status=WhiteWins reason=Checkmate")) { $fail += "the client did not see checkmate for White" }
$fs = Fen $SrvLog; $fc = Fen $CliLog
if (-not $fs -or -not $fc -or $fs -ne $fc) { $fail += "final positions differ: host='$fs' client='$fc'" }
if ($fs -and $fs -ne "r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4") { $fail += "unexpected final position '$fs'" }
if (-not (Lines $CliLog "KG_TABLE_DONE Client ok")) { $fail += "client script did not finish ok" }
if (-not (Lines $SrvLog "KG_TABLE_DONE Host ok")) { $fail += "host script did not finish ok" }

if ($fail.Count -gt 0) {
    Write-Host "TABLE SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "TABLE SMOKE PASSED" -ForegroundColor Green
