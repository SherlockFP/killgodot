<#
  SPRINT-041 acceptance 4: seeded whole-match bot runs with a forced Trapper (kg.Roles.Force=Trapper), reporting the
  Town win rate against the charter band (Docs/Design/KillGo_Pillars.md S7.2: bot band 35-65 %).
  Each run is a windowless -nullrhi listen server like kg_match_smoke.ps1; -Parallel runs at a time.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/kg_trapper_balance.ps1 [-Matches 20] [-FirstSeed 101]
         [-Bots 12] [-Speed 20] [-Parallel 2]
  Chunks (the shared UE lock, Tools/Gauntlet/kg_ue_lock.py, wants holds of <= 4 matches):
         ... -Seeds 101,102,103,104        runs only those seeds, writes summary_101-104.json
         ... -Aggregate                     no UE: rebuilds summary.json from every Match_<seed>.log in the folder
  Output: Saved/Logs/TrapperBalance/Match_<seed>.log + Saved/Logs/TrapperBalance/summary.json. Exit 0 = every match
  reached a winner with the Trapper in it, no fatal/ensure/LogKillGodot error lines, and the Town rate is in the band.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Matches = 20,
    [int]$FirstSeed = 101,
    [int[]]$Seeds = @(),
    [switch]$Aggregate,
    [int]$Bots = 12,
    [int]$Speed = 20,
    [int]$Parallel = 2,
    [int]$TimeoutSeconds = 900,
    [double]$BandMin = 0.35,
    [double]$BandMax = 0.65,
    # Control runs: -ForceRole Enforcer -OutName EnforcerControl (same seeds, the default killer instead of the Trapper).
    [string]$ForceRole = "Trapper",
    [string]$OutName = "TrapperBalance",
    [int]$PortBase = 7900
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Dir = Join-Path $Root "Saved\Logs\$OutName"
New-Item -ItemType Directory -Force $Dir | Out-Null

$queue = [System.Collections.Queue]::new()
if ($Seeds.Count) { foreach ($s in $Seeds) { $queue.Enqueue($s) } }
else { for ($i = 0; $i -lt $Matches; $i++) { $queue.Enqueue($FirstSeed + $i) } }
$SummaryFile = if ($Seeds.Count) { "summary_$($Seeds[0])-$($Seeds[-1]).json" } else { "summary.json" }
$running = @{}
$results = @()

function Start-Match($Seed) {
    $name = "TrapperBalance/Match_$Seed.log"
    $log = Join-Path $Dir "Match_$Seed.log"
    Remove-Item $log -ErrorAction SilentlyContinue
    $exec = "kg.Bot.Fill $Bots,kg.Match.Start $Seed,kg.Match.Speed $Speed"
    $a = "`"$Proj`" `"$Map`?listen`" -game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -port=$($PortBase + $Seed % 90) " +
         "-ini:Engine:[ConsoleVariables]:kg.Roles.Force=$ForceRole `"-abslog=$log`" -ExecCmds=`"$exec`""
    $p = Start-Process $Exe -ArgumentList $a -PassThru -WindowStyle Hidden
    return @{ P = $p; Log = $log; Seed = $Seed; Start = Get-Date }
}

function Read-Result($Run, $TimedOut) {
    $log = $Run.Log
    $r = [ordered]@{ seed = $Run.Seed; winner = "none"; trapper = $false; arms = 0; bites = 0; snares = 0; trips = 0; avoided = 0; errors = 0; seconds = [int]((Get-Date) - $Run.Start).TotalSeconds }
    if (Test-Path $log) {
        $d = Select-String -Path $log -Pattern "Match decided: (\w+) win" | Select-Object -First 1
        if ($d) { $r.winner = $d.Matches[0].Groups[1].Value }
        $r.trapper = [bool](Select-String -Path $log -Pattern "KG_ABILITY holder .* role=Trapper" -Quiet)
        $r.arms = @(Select-String -Path $log -Pattern "KG_ABILITY use \w+ by .* -> None").Count
        $r.bites = @(Select-String -Path $log -Pattern "KG_MIMIC bite ").Count
        $r.snares = @(Select-String -Path $log -Pattern "KG_SNARE caught").Count
        $r.trips = @(Select-String -Path $log -Pattern "KG_TRIPWIRE ").Count
        $r.avoided = @(Select-String -Path $log -Pattern "avoids a known mimic").Count
        $r.errors = @(Select-String -Path $log -Pattern "Fatal error|Ensure condition failed|Unhandled Exception|Assertion failed|LogKillGodot: Error").Count
    }
    if ($TimedOut) { $r.winner = "timeout" }
    return [pscustomobject]$r
}

if ($Aggregate) {
    # Read-only: every finished log in the folder (chunked runs), the per-run seconds are unknown here.
    $queue.Clear()
    $SummaryFile = "summary.json"
    foreach ($f in Get-ChildItem $Dir -Filter "Match_*.log" | Sort-Object { [int]($_.BaseName -replace 'Match_', '') }) {
        $seed = [int]($f.BaseName -replace 'Match_', '')
        $results += Read-Result @{ Log = $f.FullName; Seed = $seed; Start = Get-Date } $false
    }
}

while ($queue.Count -gt 0 -or $running.Count -gt 0) {
    while ($queue.Count -gt 0 -and $running.Count -lt $Parallel) {
        $s = $queue.Dequeue()
        $running[$s] = Start-Match $s
    }
    Start-Sleep -Seconds 3
    foreach ($s in @($running.Keys)) {
        $run = $running[$s]
        $decided = (Test-Path $run.Log) -and (Select-String -Path $run.Log -Pattern "Match decided:" -Quiet)
        $timedOut = ((Get-Date) - $run.Start).TotalSeconds -gt $TimeoutSeconds
        if ($decided -or $timedOut -or $run.P.HasExited) {
            if ($decided) { Start-Sleep -Seconds 2 }
            if (-not $run.P.HasExited) { Stop-Process -Id $run.P.Id -Force }
            $res = Read-Result $run ($timedOut -and -not $decided)
            $results += $res
            Write-Host ("seed {0}: {1,-9} trapper={2} arms={3} bites={4} snares={5} trips={6} avoided={7} errors={8} ({9}s)" -f $res.seed, $res.winner, $res.trapper, $res.arms, $res.bites, $res.snares, $res.trips, $res.avoided, $res.errors, $res.seconds)
            $running.Remove($s)
        }
    }
}

$decidedRuns = @($results | Where-Object { $_.winner -eq "Town" -or $_.winner -eq "Impatient" })
$town = @($decidedRuns | Where-Object { $_.winner -eq "Town" }).Count
$rate = if ($decidedRuns.Count) { $town / $decidedRuns.Count } else { 0 }
$sum = [ordered]@{
    matches = $results.Count; decided = $decidedRuns.Count; town = $town; impatient = $decidedRuns.Count - $town
    town_rate = [math]::Round($rate, 3); band = "$BandMin-$BandMax"
    arms = ($results | Measure-Object arms -Sum).Sum; bites = ($results | Measure-Object bites -Sum).Sum
    snares = ($results | Measure-Object snares -Sum).Sum; trips = ($results | Measure-Object trips -Sum).Sum
    avoided = ($results | Measure-Object avoided -Sum).Sum
    runs = $results
}
$sum | ConvertTo-Json -Depth 4 | Out-File -Encoding utf8 (Join-Path $Dir $SummaryFile)
Write-Host ("Town win rate {0:P0} ({1}/{2} decided; band {3:P0}-{4:P0}); trapper actions: arms {5}, bites {6}, snares {7}, trips {8}, avoided {9}" -f $rate, $town, $decidedRuns.Count, $BandMin, $BandMax, $sum.arms, $sum.bites, $sum.snares, $sum.trips, $sum.avoided)

$fail = @()
if ($decidedRuns.Count -ne $results.Count) { $fail += "$($results.Count - $decidedRuns.Count) match(es) without a winner" }
if (@($results | Where-Object { -not $_.trapper }).Count) { $fail += "a match without the forced Trapper" }
if (@($results | Where-Object { $_.errors -gt 0 }).Count) { $fail += "error lines in $(@($results | Where-Object { $_.errors -gt 0 }).Count) match log(s)" }
if ($rate -lt $BandMin -or $rate -gt $BandMax) { $fail += "Town win rate $([math]::Round($rate,2)) outside the band" }
if ($fail.Count) { $fail | ForEach-Object { Write-Host "FAIL $_" -ForegroundColor Red }; exit 1 }
Write-Host "PASS trapper balance" -ForegroundColor Green
exit 0
