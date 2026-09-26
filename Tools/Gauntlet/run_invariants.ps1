<#
  Kill Godot - INVARIANTS: everything that already works must still work. Run before and after every sprint;
  a sprint is only done when this passes (Docs/Process/LoopContract.md). Headless: no editor, no windows.
    I1  build + automation tests            Tools/Gauntlet/run_gates.ps1
    I2  v2 map layout/build/clearances      Tools/Level/verify_v2_build.py
    I3  network smokes (2 windowless procs) chat, emote, fish, chore, dig (dig = digging + the well passage, needs the
        underground in L_Morrowmere_v2: Tools/Unreal/kg_dig_smoke.ps1), voice (proximity voice routing, ghost rules,
        mute: kg_voice_smoke.ps1), partner (partner emotes offer/accept/RPS/cancel: kg_partner_smoke.ps1)
    I4  whole match with bots reaches a winner  Tools/Gauntlet/kg_match_smoke.ps1
  Usage: powershell -ExecutionPolicy Bypass -File Tools/Gauntlet/run_invariants.ps1 [-Skip I3,I4]
  Writes Saved/Gauntlet/invariants-<stamp>.json. Exit 0 = all green.
#>
param([string[]]$Skip = @())
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $Root
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Out = [ordered]@{}
function Run($Id, $Name, [scriptblock]$Body) {
    if ($Skip -contains $Id) { $Out[$Name] = "SKIPPED"; Write-Host "[$Id] $Name SKIPPED" -ForegroundColor Yellow; return }
    $t0 = Get-Date
    & $Body *> "Saved\Gauntlet\inv-$Id-$Stamp.log"
    $ok = ($LASTEXITCODE -eq 0)
    $Out[$Name] = if ($ok) { "PASS" } else { "FAIL" }
    $c = if ($ok) { "Green" } else { "Red" }
    Write-Host ("[{0}] {1} {2} ({3:N0}s, log Saved\Gauntlet\inv-{0}-{4}.log)" -f $Id, $Name, $Out[$Name], ((Get-Date) - $t0).TotalSeconds, $Stamp) -ForegroundColor $c
}
New-Item -ItemType Directory -Force "Saved\Gauntlet" | Out-Null
Run "I1" "build+tests" { powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Gauntlet/run_gates.ps1 }
Run "I2" "v2 verify" { python Tools/Level/verify_v2_build.py }
foreach ($s in "chat", "emote", "fish", "chore", "dig", "voice", "partner") {   # dig: KG_DIG (Docs/Level/Underground.md); voice/partner: SPRINT-023
    Run "I3" "smoke:$s" { powershell -NoProfile -ExecutionPolicy Bypass -File "Tools/Unreal/kg_${s}_smoke.ps1" }
}
Run "I4" "match smoke" { powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Gauntlet/kg_match_smoke.ps1 }
$Out | ConvertTo-Json | Set-Content "Saved\Gauntlet\invariants-$Stamp.json"
if ($Out.Values -contains "FAIL") { Write-Host "INVARIANTS BROKEN" -ForegroundColor Red; exit 1 }
Write-Host "ALL INVARIANTS HOLD" -ForegroundColor Green
exit 0
