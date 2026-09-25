<#
  SPRINT-027a contact sheets, offscreen (no window, no sound): Tools/Unreal/kg_villager_sheet.py drives kg.Look.Lineup
  on L_Morrowmere_v2 (KG game mode) over the harbour basin and takes HighResShots.
    powershell -File Tools/Unreal/kg_villager_sheet.ps1 [-Part all|lineup|roles]
  Output: Saved/Screenshots/Villagers/
    lobby20_idle.png                 20 seeded players, 2 rows (acceptance 1 evidence, uniqueness in the log)
    archetypes_<n>_<pose>.png        every archetype, 10 per sheet, idle / sit / dance
    fp_cuff.png                      the owner-only alignment cuff on the first-person arms
    sash_reveal.png                  a revealed bot wearing the public role sash
  Log lines: KG_LOOK_LINEUP spawned/unique, KG_LOOK <i> <archetype ...>
#>
param([int]$TimeoutSec = 600, [string]$Part = "all")   # all | lineup | roles
$env:KG_SHEET_PART = $Part
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Editor = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Raw = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$Out = Join-Path $Root "Saved\Screenshots\Villagers"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Raw -Filter "Villagers_*.png" -ErrorAction SilentlyContinue | Remove-Item
$Log = Join-Path $Root "Saved\Logs\kg_villager_sheet.log"
# The "py" console command works in -game (its path must have no spaces).
$Script = Join-Path $env:TEMP "kg_villager_sheet.py"
Copy-Item (Join-Path $Root "Tools\Unreal\kg_villager_sheet.py") $Script -Force
$Script = $Script -replace '\\', '/'
$p = Start-Process -FilePath $Editor -PassThru -ArgumentList @(
    "`"$Root\KillGodot.uproject`"", "/Game/KillGodot/Maps/L_Morrowmere_v2", "-game", "-RenderOffScreen", "-nosound",
    "-ResX=1600", "-ResY=900", "-unattended", "-nosplash", "-NoLoadingScreen", "`"-abslog=$Log`"",
    "-ini:Engine:[ConsoleVariables]:kg.WarmupSeconds=900", "-ExecCmds=`"py $Script`"")
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host "KG_VILLAGER_SHEET timeout" }
Get-ChildItem $Raw -Filter "Villagers_*.png" -ErrorAction SilentlyContinue | ForEach-Object {
    Move-Item $_.FullName (Join-Path $Out ($_.Name.Substring(10).ToLower())) -Force
}
Select-String -Path $Log -Pattern "KG_VILLAGER_SHEET|KG_LOOK_LINEUP|Appearance:" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' } | Select-Object -First 40
Get-ChildItem $Out -Filter "*.png" | Select-Object Name, Length
