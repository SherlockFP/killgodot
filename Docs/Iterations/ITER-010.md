# ITER-010 — Köy yeniden tasarımı, görevler, çayır (2026-09-24)

**Geri bildirim:** "Among Us gibi görev yerleri lazım, town design çok kötü" · "Ghost of Tsushima gibi grass".

- ✅ `Tools/Level/morrowmere_layout.json`: tek kaynak (Blender arazi + UE kurucu). Kıvrımlı sokaklar (liman caddesi, batı sokağı, doğu çarşısı, kilise yolu, çiftlik yolu, ara sokaklar), bölgeler (meydan, balık pazarı, kilise avlusu, demirci, çiftlik, oduncu, fener), 16 ev + belediye, han, fırın, kayıkhane, ahır, kilise, çan kulesi, deniz feneri (burun), mezarlık, kuyu, darağacı, ilan panosu.
- ✅ Arazi: sokak boyama, bina zeminleri, kilise tepeciği, fener burnu.
- ✅ `AKGTaskStation` + görev dağıtımı (kişi başı 4), sarı işaret (sadece görevi olana), E ile yapılır (uzaklaşırsan iptal), hazırlık barı, dolunca fener ifşası (rastgele Sabırsız 10 sn kırmızı). 18 görev. Botlar NavMesh ile görevlerine yürür (60 sn'de %65 hazırlık).
- ✅ `AKGGrassField` (HISM) + prosedürel çim öbekleri + M_KG_Grass (iki taraflı yaprak, kökten uca renk, rüzgâr dalgaları) ~22k öbek.
- Öğrenilenler: bina içindeki görev noktası kapalı kapıda erişilemez → kapı önüne taşındı; NavMesh hacmi tek başına tile üretmez → `RebuildNavigation`; `Layers` AActor'da zaten var (UHT gölgeleme hatası).
