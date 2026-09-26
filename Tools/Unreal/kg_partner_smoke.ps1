<#
  Headless partner-emote network smoke (SPRINT-023): a listen server and one client as windowless -nullrhi game
  processes started with -KGPartnerSmoke (UKGEmoteSubsystem::TickPartnerSmoke). The host stands the client next to
  itself, then: high five (offer -> E accept -> both play synced clips), rock-paper-scissors (server-resolved outcome
  must be identical on both machines), handshake cancelled by the host attacking, dance-off (full body on both).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_partner_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox]
  Logs: Saved/Logs/PartnerSmokeServer.log, PartnerSmokeClient.log. Exit code 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7798,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGPartnerSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=240"
$SrvLog = Join-Path $Logs "PartnerSmokeServer.log"
$CliLog = Join-Path $Logs "PartnerSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=PartnerSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=PartnerSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_PARTNER_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_PARTNER_DONE" 30
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    Select-String -Path $l -Pattern "KG_PARTNER" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) }
# Stage numbers: 0 None, 1 Offering, 2 Playing, 3 Result.
foreach ($m in @(@{ Log = $SrvLog; Name = "Host" }, @{ Log = $CliLog; Name = "Client" })) {
    $hf = Lines $m.Log "KG_PARTNER_SEEN $($m.Name) tag=highfive"
    if (($hf -match "local=1 stage=2 kind=highfive").Count -lt 1) { $fail += "$($m.Name): own body not playing the high five" }
    if (($hf -match "local=0 stage=2 kind=highfive").Count -lt 1) { $fail += "$($m.Name): the other body not playing the high five" }
    if (($hf -match "stage=2 .*shown=cheer").Count -lt 2) { $fail += "$($m.Name): high five clips not shown on both bodies" }
    $rps = Lines $m.Log "KG_PARTNER_SEEN $($m.Name) tag=rps"
    if (($rps -match "stage=3 kind=rps").Count -lt 2) { $fail += "$($m.Name): RPS result stage not shown on both bodies" }
    $cancel = Lines $m.Log "KG_PARTNER_SEEN $($m.Name) tag=cancelled"
    if (($cancel -match "stage=0 kind=none").Count -lt 2) { $fail += "$($m.Name): handshake not cancelled on both bodies after the attack" }
    $dance = Lines $m.Log "KG_PARTNER_SEEN $($m.Name) tag=danceoff"
    if (($dance -match "stage=2 kind=danceoff .*shown=dance").Count -lt 2) { $fail += "$($m.Name): dance-off not playing on both bodies" }
}
# The RPS outcome is one server byte: both machines must log the same one.
function Result($Path, $Machine) {
    $l = @(Lines $Path "KG_PARTNER_RESULT $Machine kind=rps")
    if ($l.Count -eq 0) { return $null }
    return [regex]::Match($l[-1], 'serial=(\d+) result=(\d+)').Value
}
$rh = Result $SrvLog "Host"
$rc = Result $CliLog "Client"
if (-not $rh -or -not $rc) { $fail += "RPS result missing (host='$rh' client='$rc')" }
elseif ($rh -ne $rc) { $fail += "RPS outcome differs: host $rh vs client $rc" }
if (-not (Lines $SrvLog "KG_PARTNER_STOP .*kind=handshake .*reason=Attacked")) { $fail += "attack did not cancel the handshake on the server" }
if (-not (Lines $SrvLog "KG_PARTNER_START .*kind=highfive")) { $fail += "high five never started on the server" }
$dist = @(Lines $SrvLog "KG_PARTNER_START .*kind=highfive .*dist=(\d+)")   # @(): a single line unrolls to a string, [-1] would be its last char
if ($dist -and -not ($dist[-1] -match "dist=(9[0-9]|1[0-9][0-9])\b")) { $fail += "high five bodies not aligned ~95 cm apart: $($dist[-1])" }

if ($fail.Count -gt 0) {
    Write-Host "PARTNER SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "PARTNER SMOKE PASSED" -ForegroundColor Green
exit 0
