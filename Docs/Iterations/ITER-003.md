# ITER-003 — Sprint 1: stüdyo süreci + QA-001 P1 düzeltmeleri (2026-09-24)

**Hedef:** Stüdyo işletim modelini kurmak (Docs/Studio/), otomatik playtest + bağımsız QA, QA-001'in P1 biletlerini kapatmak.

| Bilet (QA-001) | Durum |
|---|---|
| P1 Inspect eli ekrandan atıyor | ✅ Açılar küçültüldü (4/−8/16°); inspect.png'de tava kadrajda |
| P1 Backstab'da bıçak yok / ekranı dolduruyor | 🟡 Şemsiye-kılıç skini devrede, poz küçüldü, tek el. **Şemsiye ters ve büyük → P2 (gerçek bıçak modeli)** |
| P1 Tava ucuz ve ortada | 🟡 Sağ-alt CS2 kompozisyonu + %80 ölçek ✅; model hâlâ düşük kalite → P2 (daha iyi tava) |
| P1 Nanite usage uyarısı | ✅ Tüm Items meshlerinde Nanite kapatıldı |
| P1 Saç kafadan kopuk | ⏭️ Sprint 2 (Blender compose'da saç rest-pose/ölçek-100 araştırması) |
| P2 Kukla havada | ✅ Spawn Z=92 |

**Kapılar:** G1 ✅ G2 8/8 ✅ · **Performans:** ort. 3.2–3.8 ms, p95 ≤ 4.75 ms (hedef ≤ 8.3 ms).
**Araçlar:** `Tools/Gauntlet/playtest.ps1` (5 senaryo + KG_PERF), `Docs/Studio/Prompts/QA_Review.md`.
**Sonraki sprint önerisi:** Saç düzeltmesi (P1) → FP gerçek eşya modelleri (bıçak/şemsiye/tava) → M3 ağ çekirdeğine başlangıç (localhost 1 host + 3 istemci).
