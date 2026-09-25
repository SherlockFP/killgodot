# ITER-002 — M2 FP karakter + karakter yönü değişikliği (2026-09-24)

**Karar:** Prosedürel kukla karakterler kullanıcı tarafından reddedildi ("AI gibi, berbat"). Kullanıcı önce "daha sevimli stil" seçti, ardından Quaternius modellerinin iyi olduğunu söyledi → **Quaternius Universal Base Characters + Modular Outfits** köylüler (CC0) canlı renk ayarıyla kullanılıyor. Kukla içerikleri projeden kaldırıldı.

| Adım | Sonuç |
|---|---|
| Köylüler (Blender) | `Tools/Blender/kg_build_villagers.py`: UBC kafa + Peasant kıyafeti + saç/kaş tek iskelette; erkek/kadın; FP kol mesh'leri (sadece omuz-kol-el) |
| Animasyonlar | UAL1'in 42 klibi tek tek FBX (Blender 5 slotted action/NLA tek stack'e birleştiriyordu); UE'de `import_uniform_scale=100` (iskelet metre/cm farkı) |
| UE import | `kg_import_villagers.py` (yan varlıklar `save_directory` ile kaydediliyor), `kg_setup_villager_materials.py` (M_KG_Character: Tint + Desaturation −0.3 = canlı renk) |
| C++ M2 | Stamina (tükenme), çömelme, can + ölüm animasyonu, E etkileşim / fizik tutma, LMB saldırı / fırlatma, RMB itme, F inspect, B dev-bıçak, kapı (aç/kilit/kır), backstab (TF2 poz), gövde animasyon durum makinesi, FP kollar + tava (kamera uzayında otomatik yerleşim) |
| Testler | G1 ✅, G2 **8/8** ✅ (Stamina, Health, DoorSwing eklendi) |
| Görsel doğrulama | `Tools/Gauntlet/autoshot.ps1` + `-KGDevScenario=backstab|inspect` + KG_DIAG logları; `Art/Concept/M2_*.png` |

**Teknik borç:**
- İskelet kemiklerinde ölçek 100 (glTF→FBX zinciri); eklentiler `SetUsingAbsoluteScale` ile düzeltildi. Kalıcı çözüm: Blender'da ölçeği uygulayıp tek birimle dışa aktarmak.
- Gövde animasyonu `PlayAnimation` ile geçişsiz; M5'te AnimBP + blendspace + çene (voice) kemiği.
- Kaşlar beyaz (hair tint kaşa uygulanmıyor); köylü yüzlerine çene kemiği henüz eklenmedi.
- FP kollar tabanca nişan pozunda; gerçek FP animasyon seti (çekme/inspect/vuruş) M2.5.
- CommonUI viewport client uyarısı (M10).
