<#
  Headless lobby -> role reveal smoke (SPRINT-015 acceptance 1 + 2): a listen server that opens the pre-game lobby
  (?KGLobby) and one client, both windowless -nullrhi game processes on this machine.
    - names: the joiner must appear BY NAME on both machines within 1 s of its login (KG_LOBBY join / KG_LOBBY_NAMES,
      wall-clock ms from the same machine clock),
    - flow: the host starts (-KGLobbySmoke auto-starts once two humans are seated) -> the match goes from the lobby
      STRAIGHT to the RoleReveal phase (no warm-up, no map jump), server-timed (KGReveal::PhaseSeconds),
    - secrecy: the client learns its own role and nobody else's (others_known 0), Impatient teammates come through
      the owner-only reveal component,
    - skip: both players press ready (-KGRevealAutoReady) and the server cuts the phase short,
    - streamer mode on the client (-KGStreamer): other players show under a pseudonym, never the real name.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_lobby_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox]
  Needs an up-to-date editor build. Logs: Saved/Logs/LobbySmokeServer.log, LobbySmokeClient.log. Exit 0 = passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7793,
    [int]$TimeoutSeconds = 240,
    [int]$NameBudgetMs = 1000
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGRevealAutoReady"
$SrvLog = Join-Path $Logs "LobbySmokeServer.log"
$CliLog = Join-Path $Logs "LobbySmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Milliseconds 500
    }
    return $false
}
function Lines($Path, $Pattern) {
    if (-not (Test-Path $Path)) { return @() }
    return @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' })
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`?KGLobby`" $Common -KGLobbySmoke -port=$Port -log=LobbySmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "KG_LOBBY open" $TimeoutSeconds)) { throw "server lobby did not open (see $SrvLog)" }
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -KGStreamer -log=LobbySmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_REVEAL end" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_REVEAL end" 30
    Start-Sleep -Seconds 2
    Write-Host "reveal ended - server: $okS  client: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}

$fail = @()
function Check($Ok, $What) { if ($Ok) { Write-Host "  ok   $What" -ForegroundColor Green } else { Write-Host "  FAIL $What" -ForegroundColor Red; $script:fail += $What } }

# --- 1. Names within 1 s on every machine ------------------------------------------------------------------------
$join = Lines $SrvLog "KG_LOBBY join t=" | Select-Object -Last 1
$joinT = if ($join -match 't=(\d+)') { [int64]$Matches[1] } else { 0 }
$srvNames = Lines $SrvLog "KG_LOBBY_NAMES t=\d+ host n=2" | Select-Object -First 1
$cliNames = Lines $CliLog "KG_LOBBY_NAMES t=\d+ client n=2" | Select-Object -First 1
$srvT = if ($srvNames -match 't=(\d+)') { [int64]$Matches[1] } else { 0 }
$cliT = if ($cliNames -match 't=(\d+)') { [int64]$Matches[1] } else { 0 }
Write-Host "join:        $join"
Write-Host "host sees:   $srvNames"
Write-Host "client sees: $cliNames"
Check ($joinT -gt 0) "server logged the client's login"
Check ($srvT -gt 0 -and ($srvT - $joinT) -le $NameBudgetMs) ("host shows the joiner by name within {0} ms (took {1} ms)" -f $NameBudgetMs, ($srvT - $joinT))
Check ($cliT -gt 0 -and ($cliT - $joinT) -le $NameBudgetMs) ("client shows both names within {0} ms (took {1} ms)" -f $NameBudgetMs, ($cliT - $joinT))
Check ($cliNames -notmatch '\[\s*,|,\s*\]') "no empty names in the client's seat list"

# --- 2. Lobby -> reveal, server-timed ----------------------------------------------------------------------------
$phases = @(Lines $SrvLog "Phase -> " | ForEach-Object { if ($_ -match 'Phase -> (?:EKGPhase::)?(\w+) \(day \d+, (\d+)s\)') { "$($Matches[1])/$($Matches[2])" } })
Write-Host ("server phases: " + ($phases -join " > "))
Check ($phases.Count -ge 1 -and $phases[0] -like "RoleReveal/*") "the lobby start goes straight to RoleReveal (no warm-up first)"
$revealLen = if ($phases.Count -ge 1 -and $phases[0] -match '/(\d+)$') { [int]$Matches[1] } else { 0 }
Check ($revealLen -ge 8 -and $revealLen -le 12) "RoleReveal runs on the match clock for 8-12 s (got $revealLen s)"
Check ((Lines $CliLog "KG_REVEAL begin").Count -ge 1) "client shows the ceremony"
foreach ($stage in "table", "shuffle", "deal", "flip", "role") {
    Check ((Lines $CliLog "KG_REVEAL stage $stage").Count -ge 1) "client ceremony reached stage '$stage'"
}

# --- 3. Owner-only role data -------------------------------------------------------------------------------------
$cliRole = Lines $CliLog "KG_REVEAL role " | Select-Object -First 1
Write-Host "client card: $cliRole"
Check ($cliRole -match 'KG_REVEAL role \w+ alignment (Town|Impatient|Neutral)') "client received its own role"
Check ($cliRole -match 'others_known 0 net client') "client knows no other player's role (COND_OwnerOnly)"
$deals = Lines $SrvLog "KG_REVEAL deal "
$deals | ForEach-Object { Write-Host "  $_" }
Check ($deals.Count -ge 2) "server dealt a reveal to both humans"

# --- 4. Per-player skip -----------------------------------------------------------------------------------------
Check ((Lines $SrvLog "KG_REVEAL ready ").Count -ge 2) "both players pressed ready"
$cut = Lines $SrvLog "KG_REVEAL everyone ready" | Select-Object -First 1
Check ($cut -ne $null) "server cut the reveal short once everyone was ready ($cut)"
Check ($phases.Count -ge 2 -and $phases[1] -like "Day/*") "the village (Day 1) follows the reveal"
$cliEnd = Lines $CliLog "KG_REVEAL end" | Select-Object -First 1
Write-Host "client end: $cliEnd"

# --- 5. Streamer mode (client) -----------------------------------------------------------------------------------
$dump = Lines $CliLog "KG_STREAMER on=1" | Select-Object -First 1
Write-Host "client streamer: $dump"
$pairs = [regex]::Matches([string]$dump, '\[([^\]]+?) -> ([^\]]+?)\]')
$masked = @($pairs | Where-Object { $_.Groups[1].Value -ne $_.Groups[2].Value })
$leaks = @($pairs | Where-Object { $_.Groups[1].Value -ne "Stranger" -and $_.Groups[1].Value -eq $_.Groups[2].Value })
Check ($masked.Count -ge 1) "streamer mode shows other players under pseudonyms"
Check ($leaks.Count -eq 0) "streamer mode never shows another player's real name"

# --- Health ------------------------------------------------------------------------------------------------------
foreach ($l in @($SrvLog, $CliLog)) {
    $bad = @(Select-String -Path $l -Pattern "Fatal error|Ensure condition failed|Unhandled Exception|Assertion failed")
    Check ($bad.Count -eq 0) ("no fatal/ensure in " + (Split-Path $l -Leaf))
}
if ($fail.Count) { Write-Host "LOBBY SMOKE FAILED ($($fail.Count))" -ForegroundColor Red; exit 1 }
Write-Host "PASS lobby -> reveal smoke" -ForegroundColor Green
exit 0
