<#
  Studio playtest: runs every dev scenario, gathers screenshots, logs and perf into Saved/Gauntlet/Playtest/<stamp>.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/playtest.ps1
#>
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Out = Join-Path $ProjectRoot "Saved\Gauntlet\Playtest\$Stamp"
New-Item -ItemType Directory -Force $Out | Out-Null
$Scenarios = @("", "backstab", "inspect", "dummy_front", "door")
$Perf = @()
foreach ($s in $Scenarios) {
    $name = if ($s) { $s } else { "default" }
    $args2 = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $PSScriptRoot "autoshot.ps1"), "-Name", "PT_$name")
    if ($s) { $args2 += @("-Scenario", $s) }
    & powershell @args2 | Out-Null
    $shots = Join-Path $ProjectRoot "Saved\Gauntlet\Shots"
    Copy-Item (Join-Path $shots "PT_$name.png") (Join-Path $Out "$name.png") -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $shots "PT_$name.log") (Join-Path $Out "$name.log") -ErrorAction SilentlyContinue
    $line = Select-String -Path (Join-Path $Out "$name.log") -Pattern "KG_PERF" -ErrorAction SilentlyContinue | Select-Object -Last 1
    $Perf += "{0,-12} {1}" -f $name, $(if ($line) { $line.Line.Substring($line.Line.IndexOf("KG_PERF")) } else { "no perf" })
}
$Perf | Out-File -Encoding utf8 (Join-Path $Out "perf.txt")
$Perf | ForEach-Object { Write-Host $_ }
Write-Host "KG_PLAYTEST $Out"
