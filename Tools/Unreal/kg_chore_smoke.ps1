<#
  Headless chore-minigame network smoke: a listen server and one client as windowless -nullrhi game processes (no GPU,
  no focus stealing). The client drives everything through the normal paths (dev verbs go client -> host through
  UKGDevComponent, kg.Dev.AllowClients=1): auto-win on, then
    RingBell (visual chore: the bell effect must replicate to the client),
    DrawWater (2 stages),
    BakeBread interrupted by a meeting at stage 3 (progress kept) and resumed after the meeting.
  Every stage is validated by the server (no TooFast rejects expected: auto-win waits for the timing floor).
  Part 2 (SPRINT-016, world chores): a second server + client pair on Morrowmere v2 plays the WATER RUN end to end the way
  a person does (-KGWorldChoreSmoke=WaterRun on the client, UKGWorldChoreSubsystem::TickSmoke): the host puts the chore on
  the client's list, the client walks (movement input along a navmesh path) to the well, presses E and cranks a full bucket up,
  picks it up with hold-E, carries it to the fountain trough without running and pours it (2 steps, simplified after the
  user's 'keep it simple but fun' feedback). Checked on
  both machines: every step validated by the server, the bucket and its fill replicated, the trough level replicated,
  the chore ticked off the client's list.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_chore_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox] [-SkipWorld]
  Needs an up-to-date editor build. Logs: Saved/Logs/ChoreSmokeServer-<port>.log, ChoreSmokeClient-<port>.log,
  WorldChoreSmokeServer-<port>.log, WorldChoreSmokeClient-<port>.log (random port pair per run). Exit 0 = passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [string]$WorldMap = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Port = 0,
    [int]$TimeoutSeconds = 240,
    [switch]$SkipWorld,
    [switch]$OnlyWorld
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=400 -ini:Engine:[ConsoleVariables]:kg.Dev.AllowClients=1"
# Several agents may run this at once: a random port pair and per-run log names keep the runs apart.
if ($Port -le 0) { $Port = 7700 + 2 * (Get-Random -Minimum 0 -Maximum 140) }
$Tag = "{0}" -f $Port
$SrvLog = Join-Path $Logs "ChoreSmokeServer-$Tag.log"
$CliLog = Join-Path $Logs "ChoreSmokeClient-$Tag.log"
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

$fail = @()
function Has($Path, $Pattern) { [bool](Select-String -Path $Path -Pattern $Pattern -Quiet) }
if (-not $OnlyWorld) {
$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=ChoreSmokeServer-$Tag.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill|LogWorld: Bringing World" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    Start-Sleep -Seconds 3
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common `"-ExecCmds=$Steps`" -log=ChoreSmokeClient-$Tag.log" -PassThru -WindowStyle Hidden
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
}

# ---- part 2: world chore (water run) across two processes ----
if (-not $SkipWorld) {
    $WSrvLog = Join-Path $Logs "WorldChoreSmokeServer-$Tag.log"
    $WCliLog = Join-Path $Logs "WorldChoreSmokeClient-$Tag.log"
    Remove-Item $WSrvLog, $WCliLog -ErrorAction SilentlyContinue
    $WPort = $Port + 1
    $WCommon = "$Common -ini:Engine:[ConsoleVariables]:kg.WorldChore.AnyPhase=1"
    $wsrv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$WorldMap`?listen`" $WCommon -port=$WPort -log=WorldChoreSmokeServer-$Tag.log" -PassThru -WindowStyle Hidden
    $wcli = $null
    try {
        if (-not (Wait-Log $WSrvLog "KG_WORLDCHORE_SETUP .*authority=1" $TimeoutSeconds)) { throw "world server did not start (see $WSrvLog)" }
        Start-Sleep -Seconds 3
        $wcli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$WPort $WCommon -KGWorldChoreSmoke=WaterRun -log=WorldChoreSmokeClient-$Tag.log" -PassThru -WindowStyle Hidden
        $wok = Wait-Log $WCliLog "KG_WC_SMOKE_DONE|KG_WC_SMOKE_FAIL" 330
        Start-Sleep -Seconds 3
        Write-Host "world chore client finished: $wok"
    }
    finally {
        foreach ($p in @($wcli, $wsrv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
    }
    foreach ($l in @($WSrvLog, $WCliLog)) {
        Write-Host "==== $l"
        Select-String -Path $l -Pattern "KG_WORLDCHORE_(GIVE|STEP|DONE|CARRY|CARRYPOS|DROP|SPILL|REVERT|NOTICE|ITEM_SEEN|CLIENT_STEP|SPOT fountain)|KG_WC_SMOKE" |
            ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: (Warning: )?', '' }
    }
    if (-not (Has $WSrvLog "KG_WORLDCHORE_GIVE WaterRun")) { $fail += "world: the host never gave the water run" }
    foreach ($st in 1, 2) {
        if (-not (Has $WSrvLog "KG_WORLDCHORE_STEP WaterRun .*step=$st/2")) { $fail += "world: water run step $st/2 never validated by the server" }
    }
    if (-not (Has $WSrvLog "KG_WORLDCHORE_DONE WaterRun .*fake=0 counted=1")) { $fail += "world: the water run was not completed / counted" }
    if (-not (Has $WSrvLog "KG_WORLDCHORE_CARRY Bucket by")) { $fail += "world: nobody carried the bucket (hold-E)" }
    if (-not (Has $WCliLog "KG_WORLDCHORE_ITEM_SEEN Bucket .*authority=0")) { $fail += "world: the bucket did not replicate to the client" }
    if (-not (Has $WCliLog "KG_WC_SMOKE step 2, item fill=(0\.[4-9]|1\.00)")) { $fail += "world: the client never saw a full bucket (fill replication)" }
    if (-not (Has $WCliLog "KG_WORLDCHORE_CLIENT_STEP WaterRun step=2/2 done=1 authority=0")) { $fail += "world: the client got no 'chore done' tick" }
    if (-not (Has $WCliLog "KG_WC_SMOKE_DONE WaterRun .*level=(0\.[3-9]|1\.00)")) { $fail += "world: the trough level did not replicate / chore not ticked on the client list" }
    if (Has $WCliLog "KG_WC_SMOKE_FAIL") { $fail += "world: client script failed: " + ((Select-String -Path $WCliLog -Pattern "KG_WC_SMOKE_FAIL (.*)" | Select-Object -First 1).Matches[0].Groups[1].Value) }
    foreach ($l in @($WSrvLog, $WCliLog)) {
        $bad = @(Select-String -Path $l -Pattern "Fatal error|Ensure condition failed|Unhandled Exception")
        if ($bad.Count) { $fail += "world: $($bad.Count) fatal/ensure lines in $l, first: $($bad[0].Line)" }
    }
}

if ($fail.Count -gt 0) {
    Write-Host "CHORE SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "CHORE SMOKE PASSED" -ForegroundColor Green
