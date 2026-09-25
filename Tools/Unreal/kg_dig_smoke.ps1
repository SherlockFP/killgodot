<#
  Headless digging + underground network smoke: a listen server and one client as windowless -nullrhi game processes,
  both started with -KGDigSmoke so UKGDigSubsystem scripts it (Source/KillGodot/Dig/KGDigSubsystem.cpp):
  the host gives the client a shovel and puts a fresh mound in front of it -> the client takes the shovel out (Q path),
  aims and holds the dig (server-validated view, hold clock, 3 stages) -> the stages replicate (the host sees the hole
  deepen, the client sees its own spot) and the loot lands in the client's pockets -> the host stands the client at the
  Old Well -> the client presses E on the rim (AKGPassage) -> both machines see it below ground, in the Well Cellar.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_dig_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Morrowmere_v2]
  Needs an up-to-date editor build and the underground built into the map (Tools/Unreal/kg_build_underground.py).
  Logs: Saved/Logs/DigSmokeServer.log, DigSmokeClient.log. Exit code 0 = every check passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Port = 7795,
    [int]$TimeoutSeconds = 480
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGDigSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=600"
$SrvLog = Join-Path $Logs "DigSmokeServer.log"
$CliLog = Join-Path $Logs "DigSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=DigSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bringing up level for play|Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    Start-Sleep -Seconds 5
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=DigSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_DIG_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_DIG_DONE" 60
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    if (Test-Path $l) {
        Select-String -Path $l -Pattern "KG_DIG_(SMOKE|STAGE|REGION|POCKETS|DONE|GEN)|KG_PASSAGE|KG_DIG_SEEN (Host|Client) tag=(result|dugout)" |
            ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
    }
}

# ---- checks ----
$fail = @()
function Lines($Path, $Pattern) { if (Test-Path $Path) { @(Select-String -Path $Path -Pattern $Pattern | ForEach-Object { $_.Line }) } else { @() } }
if (-not (Lines $SrvLog "KG_DIG_GEN seed=\d+ spots=[1-9]")) { $fail += "the server generated no dig spots" }
if ((Lines $SrvLog "KG_DIG_STAGE spot=\d+ kind=Mound stage=\d/3").Count -lt 3) { $fail += "the server did not apply 3 mound stages" }
if (-not (Lines $SrvLog "KG_DIG_SEEN Host tag=dugout")) { $fail += "the host did not see the mound dug out" }
if (-not (Lines $SrvLog "KG_DIG_SEEN Host tag=state .*digging=1")) { $fail += "the host never saw the client digging (replicated stroke)" }
if (-not (Lines $CliLog "KG_DIG_SEEN Client tag=state .*shovel=1")) { $fail += "the client never had its shovel out" }
if (-not (Lines $CliLog "KG_DIG_SEEN Client tag=state .*stage=[1-3]/3")) { $fail += "the hole's stages did not replicate to the client" }
if (-not (Lines $CliLog "KG_DIG_SEEN Client tag=result .*stage=3/3")) { $fail += "the final result did not reach the client" }
if (-not (Lines $CliLog "KG_DIG_POCKETS Client items=\[.*Shovel")) { $fail += "the client's pockets did not replicate" }
if (-not (Lines $SrvLog "KG_PASSAGE WellTop -> WellShaft")) { $fail += "nobody went down the well" }
if (Lines $SrvLog "KG_DIG_SMOKE Host fallback") { $fail += "E on the well rim did not work (the host had to send the client down)" }
if (-not (Lines $SrvLog "KG_DIG_REGION Host below=1 .*region=Well Cellar")) { $fail += "the host did not see the client in the Well Cellar" }
if (-not (Lines $CliLog "KG_DIG_REGION Client below=1 region=Well Cellar")) { $fail += "the client did not see itself in the Well Cellar" }
if (-not (Lines $CliLog "KG_DIG_DONE Client ok")) { $fail += "client script did not finish ok" }
if (-not (Lines $SrvLog "KG_DIG_DONE Host ok")) { $fail += "host script did not finish ok" }

if ($fail.Count -gt 0) {
    Write-Host "DIG SMOKE FAILED:" -ForegroundColor Red
    $fail | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
Write-Host "DIG SMOKE PASSED" -ForegroundColor Green
