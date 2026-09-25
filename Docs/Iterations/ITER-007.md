# ITER-007 — Morrowmere köyü ilk hali + boş el (2026-09-24)

**Tetikleyen geri bildirim:** "daha hiçbir şey bitmedi, map yok, içerik yok, elde yanlış duruyor tava gibi bir şey var."
**Karar:** Altyapıyı durdur, görünür içeriğe dön.

## Sonuç
| Hedef | Durum |
|---|---|
| Köy haritası `L_Morrowmere` | ✅ 640×640 m koy arazisi (Blender, vertex renkli: kum/çimen/yol/kaya), turkuaz deniz, 15 adet 2 katlı modüler ev (alt kat taş/sıva, üst kat sıva + ahşap, kiremit çatı, iç merdiven, masa/sandalye/dolap/yatak), meydan (tezgâhlar, arabalar, sandıklar, örs, kazan), ana yol + meşaleler, 48 m iskele, ~800 ağaç/çam/kaya/çalı/çiçek, 3 köylü. Varsayılan harita yapıldı |
| Varlık hattı | ✅ Quaternius Village/Nature/Props kitleri Blender'da tek glb'ye paketlenip (`kg_pack_kit.py`) headless import (`kg_import_env.py`): 337 mesh, ortak materyal/dokular. Arazi: `kg_build_terrain.py` + `kg_import_terrain.py`. Harita: `kg_build_village.py` (deterministik, tekrar çalıştırılabilir) |
| Boş el | ✅ Masum: `relax` açık el, saldırı `jab_R` yumruk; tava kaldırıldı. Bıçak sadece çekilince (knife idle pozunda çözülüp elde) |
| Görüntü | ✅ `kg_capture.py` MCP CaptureViewport ile PIE'siz kamera görüntüleri |

**Öğrenilenler:** `unreal.Rotator(a,b,c)` sırası (roll, pitch, yaw) — ilk kurulumda her şey yatmıştı. Işıklar movable olmalı (baked ışık yok). Klasör adındaki `[Standard]` glob'u bozar. Nanite açık arazide karmaşık çarpışma basitleştirilmiş fallback'i kullanır → arazide Nanite kapalı.

**Sıradaki:** evlere gerçek kapılar (KGDoor + kit kapı mesh'i), meşale/fener ışıkları + gece, iskeleye direkler ve kayık, köylü sayısı ve hareket (NPC), meydana kuyu/çeşme, grass yoğunluğu (ISM), yağmur/rüzgâr.
