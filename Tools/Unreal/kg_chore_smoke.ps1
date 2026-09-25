<#
  Headless chore-minigame network smoke: a listen server and one client as windowless -nullrhi game processes (no GPU,
  no focus stealing). The client drives everything through the normal paths (dev verbs go client -> host through
  UKGDevComponent, kg.Dev.AllowClients=1): auto-win on, then
    RingBell (visual chore: the bell effect must replicate to the client),
    DrawWater (2 stages),
    BakeBread interrupted by a meeting at stage 3 (progress kept) and resumed after the meeting.
  Every stage is validated by the server (no TooFast rejects expected: auto-win waits for the timing floor).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_chore_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox]
  Needs an up-to-date editor build. Logs: Saved/Logs/ChoreSmokeServer.log, ChoreSmokeClient.log. Exit 0 = passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7793,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=400 -ini:Engine:[ConsoleVariables]:kg.Dev.AllowClients=1"
$SrvLog = Join-Path $Logs "ChoreSmokeServer.log"
$CliLog = Join-Path $Logs "ChoreSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

# Client script (seconds after the client starts; it needs ~10 s to connect and spawn).
$Steps = @(
    "kg.After 14 kg.Chore.AutoWin 1",
    "kg.After 15 kg.Chore.Play RingBell",
    "kg.After 25 kg.Chore.Play DrawWater",
    "kg.After 45 kg.Chore.Play BakeBread",
    "kg.After 55 kg.Match.Phase Meeting",
    "kg.After 61 kg.Match.Phase Day",
    "kg.After 64 kg.Chore.Play BakeBread",
    "kg.After 80 kg.Debug.Dump"
) -join ","

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=ChoreSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill|LogWorld: Bringing World" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    Start-Sleep -Seconds 3
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common `"-ExecCmds=$Steps`" -log=ChoreSmokeClient.log" -PassThru -WindowStyle Hidden
    $ok = Wait-Log $CliLog "KG_DUMP" $TimeoutSeconds
    Start-Sleep -Seconds 2
    Write-Host "client finished script: $ok"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    Select-String -Path $l -Pattern "KG_CHORE" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
}

# ---- checks ----
$fail = @()
function Has($Path, $Pattern) { [bool](Select-String -Path $Path -Pattern $Pattern -Quiet) }
if (-not (Has $SrvLog "KG_CHORE_OPEN RingBell")) { $fail += "server never opened RingBell for the client" }
if (-not (Has $CliLog "KG_CHORE_PANEL open RingBell")) { $fail += "client never showed the RingBell panel" }
if (-not (Has $SrvLog "KG_CHORE_DONE RingBell .*visual=1")) { $fail += "RingBell was not completed as a visual chore" }
if (-not (Has $CliLog "KG_CHORE_FX_SEEN RingBell .*authority=0")) { $fail += "the bell effect did not replicate to the client" }
if (-not (Has $SrvLog "KG_CHORE_STAGE DrawWater .*stage=2/2")) { $fail += "DrawWater stage 2 never validated" }
if (-not (Has $SrvLog "KG_CHORE_DONE DrawWater")) { $fail += "DrawWater never completed" }
if (-not (Has $SrvLog "KG_CHORE_CLOSE BakeBread .*reason=Phase")) { $fail += "the meeting did not interrupt BakeBread" }
if (-not (Has $CliLog "KG_CHORE_PANEL closed BakeBread reason=Phase")) { $fail += "the client panel did not close for the meeting" }
if (-not (Has $SrvLog "KG_CHORE_OPEN BakeBread .*stage=[12]/3")) { $fail += "BakeBread did not resume at its saved stage" }
if (Has $SrvLog "KG_CHORE_REJECT .*verdict=TooFast") { $fail += "auto-win tripped the timing floor (TooFast)" }

if ($fail.Count -gt 0) {
    Write-Host "CHORE SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "CHORE SMOKE PASSED" -ForegroundColor Green
