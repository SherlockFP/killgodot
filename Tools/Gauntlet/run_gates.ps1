<#
  Kill Godot - Gauntlet quality gates (Docs/09_Roadmap_Gauntlet.md).
  G1 build (KillGodotEditor Win64 Development)  G2 automation tests (KillGodot.*)
  G3 network smoke and G4 perf are added in milestones M3 / M7.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/run_gates.ps1 [-SkipBuild] [-Filter KillGodot]
#>
param(
    [switch]$SkipBuild,
    [string]$Filter = "KillGodot",
    [string]$EngineDir = "D:\Program Files\Epic Games\UE_5.8"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $ProjectRoot "KillGodot.uproject"
$LogDir = Join-Path $ProjectRoot "Saved\Gauntlet"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Results = [ordered]@{}

function Write-Gate($Name, $Ok, $Detail) {
    $Results[$Name] = if ($Ok) { "PASS" } else { "FAIL" }
    $color = if ($Ok) { "Green" } else { "Red" }
    Write-Host ("[{0}] {1} {2}" -f $Name, $Results[$Name], $Detail) -ForegroundColor $color
}

# G1 - build
if (-not $SkipBuild) {
    $BuildBat = Join-Path $EngineDir "Engine\Build\BatchFiles\Build.bat"
    $BuildLog = Join-Path $LogDir "G1-build-$Stamp.log"
    # cmd.exe redirection keeps the log UTF-8/ANSI (PowerShell 5.1 '*>' writes UTF-16).
    # -NoHotReloadFromIDE: an editor open on another project shares UnrealEditor.exe and would otherwise trip
    # UBT's live-coding guard even though our module DLLs are not loaded by it.
    cmd /c "`"$BuildBat`" KillGodotEditor Win64 Development `"-Project=$Project`" -WaitMutex -NoHotReload -NoHotReloadFromIDE > `"$BuildLog`" 2>&1"
    $ok = ($LASTEXITCODE -eq 0)
    Write-Gate "G1-Build" $ok "(log: $BuildLog)"
    if (-not $ok) { Get-Content $BuildLog -Tail 40; exit 1 }
}

# G2 - automation tests
$EditorCmd = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$TestLog = Join-Path $LogDir "G2-tests-$Stamp.log"
$ReportDir = Join-Path $LogDir "G2-report-$Stamp"
cmd /c "`"$EditorCmd`" `"$Project`" `"-ExecCmds=Automation RunTests $Filter; Quit`" -unattended -nopause -NullRHI -nosplash `"-ReportExportPath=$ReportDir`" -testexit=`"Automation Test Queue Empty`" `"-abslog=$TestLog`" > NUL 2>&1"
$failed = Select-String -Path $TestLog -Pattern "Test Completed\. Result=\{Fail" -SimpleMatch:$false
$passed = Select-String -Path $TestLog -Pattern "Test Completed\. Result=\{Success"
$ok = ($LASTEXITCODE -eq 0) -and (-not $failed) -and ($passed.Count -gt 0)
Write-Gate "G2-Tests" $ok ("({0} passed, {1} failed; log: {2})" -f $passed.Count, @($failed).Count, $TestLog)

$Summary = Join-Path $LogDir "gates-$Stamp.json"
$Results | ConvertTo-Json | Out-File -Encoding utf8 $Summary
if ($Results.Values -contains "FAIL") { exit 1 }
Write-Host "All gates passed. Summary: $Summary" -ForegroundColor Green
