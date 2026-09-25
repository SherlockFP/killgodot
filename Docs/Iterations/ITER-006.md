# ITER-006 — Karakter görünümü + ağ çekirdeği dilimi (2026-09-24)

**Yetki:** Kullanıcı "bana sormadan devam et geliştir" dedi → sprint otonom.

**Hedef ve kabul kriterleri**
1. **P1 QA-002-03 Köylü görünümü:** yüz gölgede çamur değil, karakter arka plandan ayrışıyor (TF2 kontur), kontur duvar arkasından GÖRÜNMEZ.
2. **P1 Kalıcı derleme:** Live Coding değişiklikleri tam derlemede + G2 testlerde geçsin.
3. **M3a Ağ dilimi:** listen-server + 1 istemci PIE'de istemcinin vuruşu, backstab'ı, bıçak durumu, itmesi ve etkileşimi sunucu otoriteli çalışsın; ölüm istemcilere yansısın.

## Sonuç (retro)
| Hedef | Durum |
|---|---|
| Köylü görünümü | ✅ `M_KG_Character`: rim (fresnel × sıcak renk × taban rengi) + gölge kaldırma (emissive %14). `M_KG_PP_Outline`: custom-depth mürekkep kontur, 1080p'de 1.6 px, 25–50 m'de söner, sahne derinliği kontrolü ile duvar arkasından çizilmez. `KG_PP_Look` sınırsız PP volume. Karakter gövdesi `SetRenderCustomDepth(true)`; FP kol/eşya konturlanmaz |
| Kalıcı derleme | ✅ G1 + G2 8/8 (iki kez: görünüm sonrası ve ağ sonrası) |
| Ağ dilimi | ✅ İstemci vuruşu sunucuda 100→80 can, istemcide 80 görünür · istemci bıçağı `ServerSetAssassinBlade` ile sunucuda True · istemci backstab'ı sunucuda öldürdü, ölüm `OnRep_Health` ile istemciye yansıdı · ters yön: host (kullanıcı PIE'de oynarken) istemciyi ve kuklayı öldürdü. 🟡 Kapı etkileşimi otomatik testte doğrulanamadı (istemci o sırada öldürülmüştü) |

**Mimari:** Girdi → yerel kozmetik (viewmodel tepmesi, kol klibi) anında → `Server*` RPC (bekleme süresi 0.15 s tolerans, kamera orijini 120 cm doğrulaması, stamina sunucuda) → `MulticastSwing` diğer istemcilerde gövde animasyonu. Fizik tutuş sunucuda; `bHoldingObject` sadece sahibine replike. Eylemler `BlueprintCallable` (botlar aynı yolu kullanacak).

**Kök nedenler / öğrenilenler**
- `MaterialEditingLibrary.delete_all_material_expressions` canlı editörde `!IsRooted` assert'i ile editörü ÇÖKERTTİ → materyaller artık headless commandlet'te yeniden yaratılıyor (`kg_toon_look.py materials`), canlıda sadece volume.
- Editör Python'undan çağrılan fonksiyonlar `GAllowActorScriptExecutionInEditor` altında çalışır → tüm RPC'ler yerel yürür. Test için `kg.Act attack|shove|interact|blade` konsol komutu eylemi bir sonraki tick'e kuyruklar.
- PIE dünyalarında aktör adları farklıdır (istemcide kendi piyonu `_1`) → eşleme PlayerState adıyla.
- LevelEditorPlaySettings Python'a kapalı → MCP `ConfigSettingsToolset` (Editor/LevelEditor/PlayIn: playNetMode, playNumberOfClients).

**Açık kalanlar:** kapı + fizik tutuşun ağ testi · fizik proplarının replikasyonu (bReplicateMovement) · sprint/stamina tahmini CMC SavedMove'a (şu an RPC + iki tarafta aynı simülasyon) · 3 bantlı toon ramp (P3).
