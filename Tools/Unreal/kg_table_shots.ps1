<#
  Off-screen screenshots of the tabletop panel (kg.TableShot, Source/KillGodot/Tabletop/KGTabletopDev.cpp): a chess
  mid-game with legal-move dots, a checkmate banner, a draughts forced-capture choice and the spectator strip.
  Windowless -game -RenderOffScreen process (no window, no focus stealing), then quits.
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Unreal/kg_table_shots.ps1 [-Sizes "1280x720 1920x1080"]
  Output: Saved/UIShots/table_<name>_<W>x<H>.png   Log: Saved/Logs/TableShots.log
  Needs an up-to-date editor build (run_gates.ps1 or Build.bat KillGodotEditor).
#>
param(
    [string]$Sizes = "1280x720",
    [int]$TimeoutSeconds = 300
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Exe = "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Proj = Join-Path $Root "KillGodot.uproject"
$Log = Join-Path $Root "Saved\Logs\TableShots.log"
Remove-Item $Log -ErrorAction SilentlyContinue
$Cmd = "kg.TableShot $Sizes".Trim()
$ArgList = "`"$Proj`" /Game/KillGodot/Maps/L_MainMenu -game -RenderOffScreen -unattended -nosound -nosplash -nop4 -ResX=1280 -ResY=720 -windowed `"-ExecCmds=$Cmd, quit`" -log=TableShots.log"
$p = Start-Process $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $p.Id -Force
    Write-Host "timed out" -ForegroundColor Red
}
if (Test-Path $Log) {
    Select-String -Path $Log -Pattern "KG_TABLESHOT" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]LogKillGodot: ', '' }
    # The widget renderer writes linear values into the PNG: encode them to sRGB so the images look like the game.
    $files = @(Select-String -Path $Log -Pattern "KG_TABLESHOT .* -> (.*\.png) ok" | ForEach-Object {
        $rel = $_.Matches[0].Groups[1].Value; Join-Path $Root ("Saved\UIShots\" + (Split-Path $rel -Leaf)) })
    if ($files.Count -gt 0) {
        $py = @"
import sys
import numpy as np
from PIL import Image
for path in sys.argv[1:]:
    im = np.asarray(Image.open(path).convert('RGB')).astype(np.float32) / 255.0
    s = np.where(im <= 0.0031308, im * 12.92, 1.055 * np.power(im, 1.0 / 2.4) - 0.055)
    Image.fromarray((np.clip(s, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)).save(path)
"@
        $pyFile = Join-Path $env:TEMP "kg_table_srgb.py"
        Set-Content -Path $pyFile -Value $py -Encoding ascii
        & python $pyFile @files
        Write-Host "sRGB-encoded $($files.Count) image(s)"
    }
    $failed = @(Select-String -Path $Log -Pattern "KG_TABLESHOT .*FAILED").Count
    if ($failed -gt 0 -or $files.Count -eq 0) { exit 1 }
} else {
    Write-Host "no log at $Log" -ForegroundColor Red
    exit 1
}
