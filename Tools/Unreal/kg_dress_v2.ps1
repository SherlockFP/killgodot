<#
  Dress L_Morrowmere_v2 headless (UE commandlet, no window): powershell -File Tools/Unreal/kg_dress_v2.ps1 [-Zones "square harbour"] [-Fast]
  -Fast skips the minimap redraw (quick iterations). Log: Saved/Logs/kg_dress_v2.log. Then rebuild nav + capture:
  powershell -File Tools/Unreal/kg_build_v2_all.ps1 -From 6 -To 7
#>
param([string]$Zones = "", [switch]$Fast)
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$R = $Root -replace '\\', '/'
$Cmd = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
while (Get-Process -Name "UnrealBuildTool", "link", "cl" -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 10 }
if ($Fast) { $env:KG_DRESS_FAST = "1" } else { $env:KG_DRESS_FAST = "" }
& $Cmd (Join-Path $Root "KillGodot.uproject") -run=pythonscript "-script=$R/Tools/Unreal/kg_dress_v2.py $Zones" -unattended -nosplash -nopause -nullrhi "-abslog=$R/Saved/Logs/kg_dress_v2.log" | Out-Null
Select-String -CaseSensitive -Path "$R/Saved/Logs/kg_dress_v2.log" -Pattern "LogPython: KG_DRESS_V2 (\w+: \{|saved|assets|level fixups|minimap|existing)|FAILED|Traceback|Error" |
    Where-Object { $_.Line -notmatch "GameFeature" } | Select-Object -Unique | ForEach-Object { "   " + $_.Line.Substring(0, [Math]::Min(400, $_.Line.Length)) }
