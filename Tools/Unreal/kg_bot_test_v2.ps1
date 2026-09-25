<#
  Headless 20-bot soak of L_Morrowmere_v2 (-game -nullrhi -RenderOffScreen -nosound: no window, no sound).
    powershell -File Tools/Unreal/kg_bot_test_v2.ps1 [-Bots 20] [-RoamS 150] [-GatherS 120] [-ChoreS 240]
  Output: Saved/KG_V2_BotTest.json + the KG_BOT / KG_BOTSTATS lines of Saved/Logs/kg_bot_test_v2.log (summarised here).
  See Tools/Unreal/kg_bot_test_v2.py for the timeline.
#>
param([int]$Bots = 20, [int]$RoamS = 150, [int]$GatherS = 120, [int]$ChoreS = 240, [int]$TimeoutSec = 1200)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Log = Join-Path $Root "Saved\Logs\kg_bot_test_v2.log"
$env:KG_BOTS = "$Bots"; $env:KG_ROAM_S = "$RoamS"; $env:KG_GATHER_S = "$GatherS"; $env:KG_CHORE_S = "$ChoreS"
$Script = Join-Path $env:TEMP "kg_bot_test_v2.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_bot_test_v2.py") $Script -Force
$Script = $Script -replace '\\', '/'
Remove-Item (Join-Path $Root "Saved\KG_V2_BotTest.json") -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $Editor -PassThru -WindowStyle Hidden -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2?MaxPlayers=24", "-game", "-nullrhi", "-RenderOffScreen",
    "-nosound", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"", "-ExecCmds=`"py $Script`"",
    # no automatic match / bot fill: the soak adds its own bots and starts the match itself
    "-ini:Engine:[ConsoleVariables]:kg.AutoStart=0", "-ini:Engine:[ConsoleVariables]:kg.BotFill=0")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_BOTTEST timeout" }
Select-String -Path $Log -Pattern "KG_BOTTEST (roam|gather|meeting|paths|done|error|no game)|KG_BOTSTATS|KG_BOT level" |
    ForEach-Object { $_.Line.Substring(0, [Math]::Min(400, $_.Line.Length)) }
$stuck = @(Select-String -Path $Log -Pattern "KG_BOT stuck").Count
$choreHits = @(Select-String -Path $Log -Pattern "KG_BOT chore .* done (\w+) in ([\d.]+)s" | ForEach-Object { $_.Matches[0].Groups[1].Value })
Write-Host ("KG_BOTTEST stuck lines {0}; chores done {1} ({2} distinct: {3})" -f $stuck, $choreHits.Count,
    @($choreHits | Sort-Object -Unique).Count, (($choreHits | Sort-Object -Unique) -join ","))
