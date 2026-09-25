<#
  Headless whole-match smoke: one windowless -nullrhi listen server fills with bots, starts a seeded match at high
  clock speed and must reach the Epilogue with a decided winner, without fatal errors or ensures.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/kg_match_smoke.ps1 [-Map /Game/KillGodot/Maps/L_Morrowmere_v2] [-Bots 12]
         [-LogName MatchSmoke.log]   (SPRINT-018: e.g. -Map /Game/KillGodot/Maps/L_StormManor -LogName MatchSmoke_StormManor.log,
                                      so a second map's smoke never overwrites the invariant's log)
  Needs an up-to-date editor build. Log: Saved/Logs/<LogName>. Exit 0 = passed.
#>
param(
    [string]$Map = "/Game/KillGodot/Maps/L_Morrowmere_v2",
    [int]$Bots = 12,
    [int]$Seed = 7,
    [int]$Speed = 30,
    [int]$TimeoutSeconds = 600,
    [string]$LogName = "MatchSmoke.log"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Log = Join-Path $Root "Saved\Logs\$LogName"
Remove-Item $Log -ErrorAction SilentlyContinue
$Exec = "kg.Bot.Fill $Bots,kg.Match.Start $Seed,kg.Match.Speed $Speed"
$args_ = "`"$Proj`" `"$Map`?listen`" -game -nullrhi -RenderOffScreen -nosound -unattended -nosplash -log=$LogName -ExecCmds=`"$Exec`""
$p = Start-Process $Exe -ArgumentList $args_ -PassThru -WindowStyle Hidden
$decided = $false
try {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and -not $p.HasExited) {
        if ((Test-Path $Log) -and (Select-String -Path $Log -Pattern "Match decided:" -Quiet)) { $decided = $true; break }
        Start-Sleep -Seconds 3
    }
    Start-Sleep -Seconds 3
}
finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
$fail = @()
if (-not (Test-Path $Log)) { $fail += "no log written" }
else {
    $phases = @(Select-String -Path $Log -Pattern "Phase -> ([A-Za-z:]+)" | ForEach-Object { $_.Matches[0].Groups[1].Value -replace '.*::', '' })
    Write-Host ("phases: " + ($phases -join " > "))
    foreach ($need in "Day", "Meeting", "Night") { if (-not ($phases -contains $need)) { $fail += "never reached $need" } }
    if (-not $decided) { $fail += "no winner within $TimeoutSeconds s" }
    $bad = @(Select-String -Path $Log -Pattern "Fatal error|Ensure condition failed|Unhandled Exception|Assertion failed")
    if ($bad.Count) { $fail += "$($bad.Count) fatal/ensure lines, first: $($bad[0].Line)" }
    $errs = @(Select-String -Path $Log -Pattern "LogKillGodot: Error")
    if ($errs.Count) { $fail += "$($errs.Count) LogKillGodot errors, first: $($errs[0].Line)" }
}
if ($fail.Count) { $fail | ForEach-Object { Write-Host "FAIL $_" -ForegroundColor Red }; exit 1 }
Write-Host "PASS match smoke ($Bots bots, seed $Seed)" -ForegroundColor Green
exit 0
