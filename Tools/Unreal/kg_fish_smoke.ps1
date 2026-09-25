<#
  Headless fishing network smoke: a listen server and one client as windowless -nullrhi game processes (no GPU, no
  focus stealing), both started with -KGFishSmoke so UKGFishingSubsystem scripts it (Source/KillGodot/Fishing):
  the host puts both villagers on the jetty head and gives the client a rod -> the client takes the rod out and casts
  into the basin (server-validated arc + water check) -> the host forces a bite (FKGRng roll, like kg.Fish.Bite) ->
  the client strikes inside the hook window and plays the reel minigame with the expert input (its prediction,
  reconciled with the server) -> the server lands the fish into the client's pockets with its weight -> the catch
  replicates to both machines. Every machine logs what it sees (KG_FISH_SEEN), the client its pockets (KG_FISH_POCKETS).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_fish_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Morrowmere_v2]
  Needs an up-to-date editor build (run_gates.ps1). Logs: Saved/Logs/FishSmokeServer.log, FishSmokeClient.log.
  Exit code 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Port = 7793,
    [int]$TimeoutSeconds = 480
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGFishSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=600"
$SrvLog = Join-Path $Logs "FishSmokeServer.log"
$CliLog = Join-Path $Logs "FishSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=FishSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bringing up level for play|Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    Start-Sleep -Seconds 5
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=FishSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_FISH_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_FISH_DONE" 60
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    if (Test-Path $l) {
        Select-String -Path $l -Pattern "KG_FISH" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
    }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { if (Test-Path $Path) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) } else { @() } }
if (-not (Lines $SrvLog "KG_FISH_CAST .*water=(Basin|Sea)")) { $fail += "server never accepted a cast into the sea/basin" }
if (-not (Lines $SrvLog "KG_FISH_BITE")) { $fail += "no bite on the server" }
if (-not (Lines $SrvLog "KG_FISH_HOOK")) { $fail += "the client's strike did not hook on the server" }
if (-not (Lines $SrvLog "KG_FISH_RESULT .*result=Landed")) { $fail += "the server did not land the fish" }
if (-not (Lines $SrvLog "KG_FISH_SEEN Host tag=line .*phase=Waiting .*water=(Basin|Sea)")) { $fail += "the host never saw the client's bobber on the water" }
if (-not (Lines $SrvLog "KG_FISH_SEEN Host tag=catch .*result=Landed")) { $fail += "the host did not see the client's catch" }
if (-not (Lines $CliLog "KG_FISH_SEEN Client tag=bite")) { $fail += "the client never saw the bite" }
if (-not (Lines $CliLog "KG_FISH_SEEN Client tag=line .*local=1 .*phase=Fight")) { $fail += "the client never saw its own fight" }
$catch = Lines $CliLog "KG_FISH_SEEN Client tag=catch .*result=Landed item=Fish_\w+ grams=[1-9]"
if (-not $catch) { $fail += "the catch (with a weight) did not replicate to the client" }
$pockets = Lines $CliLog "KG_FISH_POCKETS Client item=Fish_\w+ count=[1-9]\d* grams=[1-9]"
if (-not $pockets) { $fail += "the fish with its weight is not in the client's pockets" }
if (-not (Lines $CliLog "KG_FISH_DONE Client ok")) { $fail += "client script did not finish ok" }
if (-not (Lines $SrvLog "KG_FISH_DONE Host ok")) { $fail += "host script did not finish ok" }

if ($fail.Count -gt 0) {
    Write-Host "FISH SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "FISH SMOKE PASSED" -ForegroundColor Green
