# ITER-011 — Gerçek su, ses, yaşam (2026-09-24)

**Geri bildirim:** gerçek su/yüzme/balık · Sea of Thieves gibi su · kutu kırma · ruhsuz ve boş · kulelere çıkılmıyor · ağaç yoğunluğu · CS tarzı yükleme.

- ✅ `FKGWaves` (C++) = M_KG_Ocean WPO (aynı 3 dalga). Çarpıtılmış yoğun deniz mesh'i (58k köşe, limanda 0.6 m). Saydam okyanus: derinliğe göre turkuaz→lacivert, tepe parlaması, kıyı + tepe köpüğü, mesafeyle sönen dalgacıklar, iki taraflı.
- ✅ Yüzme: `UKGCharacterMovement` (dalga yüzeyine göre suda/daldırma) + kapsülün fizik hacmi elle "deniz" hacmi ↔ varsayılan (UE yalnızca su hacminde yüzer). Baktığın yöne yüz/dal, Space ile çık, dalgayla sallan. Su altı: bulanık turkuaz sis + dalgalanma (M_KG_Underwater), çift taraflı yüzey.
- ✅ `UKGBuoyancyComponent`: omurgadan örnekleme, fiziksel sandık/fıçı/kayık dalgada yüzer. Kayıklar su hattı zemin altında (içi kuru).
- ✅ `AKGFishSchool`: 5 sürü (uskumru, morina, somon, altın sazan), zamanla deterministik, yüzücüden kaçar; M_KG_Fish kuyruk kıvrımı (vertex alfa).
- ✅ `AKGBreakable`: yumruk/bıçakla kırılan sandık-fıçı, tahta kıymıkları, ses. 22 adet (balık pazarı, iskele, meydan).
- ✅ `AKGLadder` + tırmanma (W/S, tepede platforma adım, Space ile bırak). Çan kulesi ve fener içinde merdiven, tepede seyir katı; "Çanı çal" ve "Feneri yak" görevleri tepede.
- ✅ Ses: `Tools/Audio/kg_synth_sfx.py` (numpy/scipy sentezi, 29 ses) — rüzgâr, deniz, iskele suyu, kuş, martı, ateş, çan, kapı zili, ayak sesi (çimen/ahşap/taş), sıçrama, sandık, yumruk, bıçak, kapı, görev melodisi, gong. 18 ortam kaynağı; C++'ta ayak sesi, vuruş, sıçrama, kapı, kırılma, toplantı gongu + çan, görev melodisi (`Audio/KGAudio.h`).
- ✅ Rüzgârda sallanan yapraklar/çiçekler (M_KG_Foliage, kit yaprak materyalleri yeniden bağlandı). Ağaç 230 + çam 380.
- ✅ CS tarzı yükleme kartı (harita adı, mod, ipucu, çubuk). Asset ajanı: `kg_make_water_props.py` (balıklar, kayık, kürek, olta, şamandıra, kürek, kazı tümseği/çukuru, kapı zili).
- Öğrenilenler: UE yalnızca su PhysicsVolume'unda yüzer; `bReplicates` korumalı (SetReplicates); unity build → ortak yardımcılar başlıkta olmalı; sRGB vertex renkleri materyalde karelenmeli.
