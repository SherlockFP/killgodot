<#
  SPRINT-033 forest two-process smoke (windowless: a -nullrhi listen server + one client on L_Morrowmere_v2).
  The host runs kg.Forest.Smoke (UKGForestSubsystem::TickSmoke) on the CLIENT's player:
    1. wolf: the client is put in the North deep wood with the Deep-night gain (+7/s); the pack wakes at the den, howls,
       circles with glowing eyes, then bites twice (N = 2 < 10: never lethal -> 0 damage, a stagger). Checked on both
       ends: the stages, >= 20 s from the client's own howl stamp to the first bite, wolves + snarl replicate.
    2. mist: back in the deep wood, the Mist notices the lone client (night rate), a tongue spawns 40 m behind and
       follows; the client walks (kg.Forest.AutoWalk 2: walks, never runs) along its HUD arrow to the path and escapes.
  Owner-only: the client sees exactly its own AKGForestPlayerInfo (the host's never replicates to it).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/kg_forest_smoke.ps1 [-TimeoutSeconds 420]
  Logs: Saved/Logs/ForestSmokeServer-<port>.log, ForestSmokeClient-<port>.log. Exit 0 = passed.
#>
param([int]$Port = 0, [int]$TimeoutSeconds = 420, [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2")
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900 -ini:Engine:[ConsoleVariables]:kg.Forest.Enabled=1"
if ($Port -le 0) { $Port = 7960 + 2 * (Get-Random -Minimum 0 -Maximum 100) }
$SrvLog = Join-Path $Logs "ForestSmokeServer-$Port.log"
$CliLog = Join-Path $Logs "ForestSmokeClient-$Port.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}
function Has($Path, $Pattern) { (Test-Path $Path) -and [bool](Select-String -Path $Path -Pattern $Pattern -Quiet) }

$fail = @()
$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port `"-ExecCmds=kg.Forest.Smoke`" -log=ForestSmokeServer-$Port.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "KG_FOREST setup map=.*active=1" $TimeoutSeconds)) { throw "server did not start the forest (see $SrvLog)" }
    Start-Sleep -Seconds 3
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common `"-ExecCmds=kg.After 3 kg.Forest.AutoWalk 2`" -log=ForestSmokeClient-$Port.log" -PassThru -WindowStyle Hidden
    $ok = Wait-Log $SrvLog "KG_FOREST_SMOKE_DONE" $TimeoutSeconds
    Start-Sleep -Seconds 3
    Write-Host "host script finished: $ok"
}
catch { $fail += $_.Exception.Message }
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    if (Test-Path $l) {
        Select-String -Path $l -Pattern "KG_FOREST(_SMOKE|_CLIENT|_WOLF_SEEN|_MIST_SEEN|_INFO_SEEN| player=| bite| mist| evidence| wolf spawn| wall_return)" |
            Select-Object -First 60 | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
    }
}
# ---- server ----
if (-not (Has $SrvLog "KG_FOREST_SMOKE wolf start")) { $fail += "the host never found the remote player" }
if (-not (Has $SrvLog "KG_FOREST player=.* stage=howl .*bot=0")) { $fail += "no howl stage for the client" }
if (-not (Has $SrvLog "KG_FOREST player=.* stage=eyes .*bot=0")) { $fail += "no eyes stage for the client" }
if (-not (Has $SrvLog "KG_FOREST wolf spawn")) { $fail += "no wolf woke at a den" }
$bites = @(Select-String -Path $SrvLog -Pattern "KG_FOREST bite victim=.* stamp_to_bite=([0-9.]+)" -ErrorAction SilentlyContinue)
if ($bites.Count -lt 1) { $fail += "the wolves never bit the client" }
foreach ($b in $bites) {
    if ([double]$b.Matches[0].Groups[1].Value -lt 20.0) { $fail += "a bite came " + $b.Matches[0].Groups[1].Value + " s after the stamp (< 20 s)" }
}
if (Has $SrvLog "KG_FOREST bite .*lethal=1") { $fail += "a bite was lethal at N = 2 (P1b: never at N <= 9)" }
if (-not (Has $SrvLog "KG_FOREST evidence victim=.* kind=bite")) { $fail += "no bite-wound evidence" }
if (-not (Has $SrvLog "KG_FOREST mist spawn")) { $fail += "no Mist tongue spawned" }
if (-not (Has $SrvLog "KG_FOREST mist escaped")) { $fail += "the client did not escape the Mist by walking" }
if (Has $SrvLog "KG_FOREST mist catch") { $fail += "the Mist caught a walking player" }
if (-not (Has $SrvLog "KG_FOREST_SMOKE_DONE mist=escaped")) { $fail += "host script did not end with an escape" }
# ---- client ----
if (-not (Has $CliLog "KG_FOREST_INFO_SEEN authority=0 own=1")) { $fail += "the client never got its own forest info" }
if (Has $CliLog "KG_FOREST_INFO_SEEN authority=0 own=0") { $fail += "owner-only broken: the client received another player's forest info" }
if (-not (Has $CliLog "KG_FOREST_WOLF_SEEN .*authority=0")) { $fail += "no wolf replicated to the client" }
if (-not (Has $CliLog "KG_FOREST_CLIENT wolf=howl")) { $fail += "client never saw its howl stage" }
if (-not (Has $CliLog "KG_FOREST_CLIENT wolf=eyes")) { $fail += "client never saw its eyes stage" }
if (-not (Has $CliLog "KG_FOREST_CLIENT wolf=bite")) { $fail += "client never saw the bite stage" }
if (-not (Has $CliLog "KG_FOREST_CLIENT wolf snarl")) { $fail += "the lunge snarl did not replicate" }
if (-not (Has $CliLog "KG_FOREST_CLIENT howl heard")) { $fail += "the howl did not reach the client" }
if (-not (Has $CliLog "KG_FOREST_MIST_SEEN .*authority=0")) { $fail += "the Mist tongue did not replicate to the client" }
if (-not (Has $CliLog "KG_FOREST_CLIENT wolf=[a-z]+ mist=2")) { $fail += "the client never saw its own Mist stage" }
foreach ($l in @($SrvLog, $CliLog)) {
    if (Test-Path $l) {
        $bad = @(Select-String -Path $l -Pattern "Fatal error|Ensure condition failed|Unhandled Exception")
        if ($bad.Count) { $fail += "$($bad.Count) fatal/ensure lines in $l, first: $($bad[0].Line)" }
    }
}
if ($fail.Count -gt 0) {
    Write-Host "FOREST SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "FOREST SMOKE PASSED" -ForegroundColor Green
exit 0
