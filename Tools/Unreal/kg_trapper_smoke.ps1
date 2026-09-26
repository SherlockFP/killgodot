<#
  SPRINT-041 Trapper network smoke: a listen server (the Trapper) and one client (the victim) as windowless -nullrhi
  game processes, both started with -KGTrapperSmoke so UKGAbilitySubsystem scripts it
  (Source/KillGodot/Abilities/KGAbilitySubsystem.cpp TickSmoke):
    day + 4 frozen bots, host = Trapper, client = Sheriff -> a chest out of sight -> Mimic refused while the client
    stands next to the Trapper (unseen rule) -> armed once alone -> a crate thrown at it: the mimic flinches ->
    the client walks up and presses E (the real ServerInteract path) -> the bite replicates (held, damaged, BiteMarks,
    the scream) -> the chest resets with teeth marks -> a tripwire tells the Trapper who passed -> a snare holds and
    wounds the client (SnareWound).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_trapper_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Morrowmere_v2]
  Logs: Saved/Logs/TrapperSmokeServer.log, TrapperSmokeClient.log. Exit 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Port = 7841,
    [int]$TimeoutSeconds = 420
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGTrapperSmoke " +
          "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=600 " +
          "-ini:Engine:[ConsoleVariables]:kg.BotFill=0 -ini:Engine:[ConsoleVariables]:kg.PhaseScale=10"
$SrvLog = Join-Path $Logs "TrapperSmokeServer.log"
$CliLog = Join-Path $Logs "TrapperSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=TrapperSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bringing up level for play|Phase -> " $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    Start-Sleep -Seconds 5
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=TrapperSmokeClient.log" -PassThru -WindowStyle Hidden
    $okS = Wait-Log $SrvLog "KG_TRAPPER_DONE" $TimeoutSeconds
    $okC = Wait-Log $CliLog "KG_TRAPPER_DONE" 30
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    if (Test-Path $l) {
        Select-String -Path $l -Pattern "KG_TRAPPER_(SMOKE|DONE)|KG_ABILITY |KG_MIMIC|KG_SNARE|KG_TRIPWIRE|KG_ABILITY_NOTE|KG_TRAP (Armed|Fired|Scream|Flinch|Snared|Tripped)" |
            ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' } | Select-Object -First 60
        Select-String -Path $l -Pattern "KG_TRAPPER_SEEN" | Select-Object -Last 4 | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
    }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { if (Test-Path $Path) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) } else { @() } }
if (-not (Lines $SrvLog "KG_TRAPPER_SMOKE Host use Mimic verdict=Seen")) { $fail += "the Mimic was not refused in plain sight (unseen rule)" }
if (-not (Lines $SrvLog "KG_TRAPPER_SMOKE Host use Mimic verdict=None")) { $fail += "the Trapper could not arm the mimic" }
if (-not (Lines $SrvLog "KG_MIMIC flinch|throw-test flinch=1")) { $fail += "the thrown crate did not make the mimic flinch" }
if (-not (Lines $CliLog "KG_MIMIC_SEEN Client flinch")) { $fail += "the flinch did not replicate to the client" }
if (-not (Lines $CliLog "KG_TRAPPER_SMOKE Client opens the chest")) { $fail += "the client never opened the chest" }
if (-not (Lines $SrvLog "KG_MIMIC bite .*wounds=BiteMarks")) { $fail += "the server did not bite / mark the victim" }
if (-not (Lines $CliLog "KG_TRAPPER_SEEN Client look=bite held=1")) { $fail += "the bite (held in the jaws) did not replicate to the client" }
if (-not (Lines $CliLog "KG_MIMIC_CUE Client cue=scream .*audible=1")) { $fail += "the client did not hear the scream" }
if (-not (Lines $CliLog "KG_TRAPPER_SEEN Client look=marks held=0 .*BiteMarks")) { $fail += "the chest did not reset (teeth marks) on the client" }
if (-not (Lines $SrvLog "KG_MIMIC reset .* teethmarks=1")) { $fail += "the chest did not reset on the server" }
if (-not (Lines $SrvLog "KG_TRIPWIRE .* crossed by")) { $fail += "nobody tripped the tripwire" }
if (-not (Lines $SrvLog "KG_ABILITY_NOTE Tripwire: .* passed")) { $fail += "the tripwire did not tell the Trapper" }
if (-not (Lines $SrvLog "KG_SNARE caught .*SnareWound")) { $fail += "the snare did not catch the client" }
if (-not (Lines $CliLog "KG_TRAPPER_SEEN Client .*held=1 .*SnareWound")) { $fail += "the snare hold did not replicate to the client" }
if (Lines $CliLog "KG_TRAPPER_SEEN Client .*holders=[1-9]") { $fail += "LEAK: the client received someone's ability holder" }
if (Lines $CliLog "KG_TRAPPER_SEEN Client .*hostRole=Trapper") { $fail += "LEAK: the client can read the host's role" }
if (-not (Lines $CliLog "KG_TRAPPER_DONE Client ok")) { $fail += "client script did not finish ok" }
if (-not (Lines $SrvLog "KG_TRAPPER_DONE Host ok")) { $fail += "host script did not finish ok" }
foreach ($l in @($SrvLog, $CliLog)) {
    $bad = @(Lines $l "Fatal error|Ensure condition failed|Unhandled Exception|Assertion failed")
    if ($bad.Count) { $fail += "$(Split-Path $l -Leaf): $($bad.Count) fatal/ensure lines, first: $($bad[0])" }
}

if ($fail.Count -gt 0) {
    Write-Host "TRAPPER SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "TRAPPER SMOKE PASSED" -ForegroundColor Green
