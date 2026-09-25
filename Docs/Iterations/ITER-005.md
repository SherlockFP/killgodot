# ITER-005 — FP kollar + canlı editör akışı (2026-09-24)

**Tetikleyen geri bildirim:** "el modeli çok kötü duruyor, eller kırık gibi; şemsiye uygun durmuyor" + "Unreal MCP ile editör açıkken yap".

**Hedef ve kabul kriterleri**
1. **P1 FP kollar:** Köylüden türetilmiş kırık görünen kollar yerine özel FP kol rig'i (Drillimpact PSX First Person Arms, CC0). Sağ yumruk sağ altta (CS2 kadrajı), sol kol kadraj dışında.
2. **P1 Tutuş:** Tava (masum aleti) ve bıçak (Impatient/Spy) yumrukta, sap avuçta, doğru yönde; animasyonda elle birlikte hareket eder.
3. **P1 Canlı akış:** Editör kapanmadan Live Coding + MCP PIE + ekran görüntüsü döngüsü tek komutla.

## Sonuç (retro)
| Hedef | Durum |
|---|---|
| FP kollar | ✅ `arms_rig` + 18 klip (knife idle/hit/draw, push, grab...). Rig'in `camera` kemiği göze hizalandı, sonra sola/yukarı kaydırıldı (yumruk kamerada ≈ ileri 37, sağ 10, aşağı 7 cm) |
| Tutuş | ✅ Tutuş idle pozda çözülüp `hand_R` kemiğine bağlanıyor → vuruşta bıçak elde kalıyor. Bıçak sapı avuçta (`bBladeGripAtMaxZ=true`, ölçek 0.6), tava dik ve sağda (ölçek 0.6) |
| Ten rengi | ✅ Soluk PSX dokusu × sıcak ton (`BaseColorFactor` 1.0/0.62/0.40) — köylülerle uyumlu |
| Canlı akış | ✅ `Tools/Unreal/kg_live.py` (StopPIE → Live Coding → StartPIE → HighResShot), `kg_pie_probe.py`, `kg_pie_blade.py` |

**Kök nedenler**
- Interchange glTF, varlıkları `FPArms/arms_rig/SkeletalMeshes/` altına koydu → constructor yolu tutmadı, kol mesh'i `None`.
- Live++ eski `static` yerelleri korur: `static FObjectFinder` yolu düzeltilse bile eski başarısız sonucu tuttu → non-static yapıldı.
- Eski probe betiği mesh'i kendisi atadığı için sorunu gizledi (düzeltildi: probe artık hiçbir şeyi değiştirmiyor).

**Açık kalanlar:** vuruşun tepe karesinde ön kol ekranın çoğunu kaplıyor (klip/FOV ayarı) · kol çıplak — kıyafet kolu/eldiven kozmetiği (M-kozmetik) · tam derleme + G2 kapıları editör bir sonraki kapandığında.
**Öğrenilen:** Probe betikleri salt-okunur olmalı; durumu değiştiren test yardımcıları ayrı dosyada.
