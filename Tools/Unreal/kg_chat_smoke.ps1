<#
  Headless chat network smoke: a listen server and one client as windowless -nullrhi game processes (no GPU, no
  focus stealing), both started with -KGChatSmoke so UKGChatSubsystem scripts chat traffic (town, nearby, refused
  team/dead lines, reaction, /wave, spam, a bidi-override line) and dumps what each machine received (KG_CHAT lines).
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_chat_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Dev_Greybox]
  Needs an up-to-date editor build (run_gates.ps1). Logs: Saved/Logs/ChatSmokeServer.log, ChatSmokeClient.log.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Dev_Greybox",
    [int]$Port = 7791,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Logs = Join-Path $Root "Saved\Logs"
$Common = "-game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -KGChatSmoke -ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=240"
$SrvLog = Join-Path $Logs "ChatSmokeServer.log"
$CliLog = Join-Path $Logs "ChatSmokeClient.log"
Remove-Item $SrvLog, $CliLog -ErrorAction SilentlyContinue

function Wait-Log($Path, $Pattern, $Seconds) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

$srv = Start-Process $Exe -ArgumentList "`"$Proj`" `"$Map`?listen`" $Common -port=$Port -log=ChatSmokeServer.log" -PassThru -WindowStyle Hidden
$cli = $null
try {
    if (-not (Wait-Log $SrvLog "Bot fill" $TimeoutSeconds)) { throw "server did not start (see $SrvLog)" }
    $cli = Start-Process $Exe -ArgumentList "`"$Proj`" 127.0.0.1:$Port $Common -log=ChatSmokeClient.log" -PassThru -WindowStyle Hidden
    $okC = Wait-Log $CliLog "KG_CHAT_DONE" $TimeoutSeconds
    $okS = Wait-Log $SrvLog "KG_CHAT_DONE" 30
    Write-Host "server done: $okS  client done: $okC"
}
finally {
    foreach ($p in @($cli, $srv)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
foreach ($l in @($SrvLog, $CliLog)) {
    Write-Host "==== $l"
    Select-String -Path $l -Pattern "KG_CHAT|\[Chat\]" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
}
