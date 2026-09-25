# ITER-012 — Çoklu ajan, ikonik mekânlar, viewmodel, taşıma (2026-09-24)

**Kullanıcı:** "multi ajan", viewmodel kocaman/kötü/çok yukarıda, eller kırık, bıçak elden fırlıyor, inspect CS2 gibi, can/stamina barı, E basılı taşıma (GMod/HL2), online oyun menüleri/ayarlar/kozmetik mağaza, ev içleri kötü, Japon mekânlar, ikonik lokasyonlar, Sea of Thieves iskele, bulutlar.

## Ajanlar (paralel)
| Ajan | Kapsam | Durum |
|---|---|---|
| Su props | balıklar, kayık, kürek, olta, şamandıra, kürek, kazı tümseği/çukuru, kapı zili | ✅ |
| Japon props | torii, pagoda, taş fener, bahçe lambası, sakura×2, akçaağaç, bonsai, bambu, koi göleti, köprüler, koi, yatai | ✅ |
| Menüler/ayarlar/online | ana menü, host/join, ayarlar (grafik/oyun/ses/kontrol/dil), duraklatma, UKGGameUserSettings, AKGPlayerController | çalışıyor |
| Envanter/ekonomi | eşya kataloğu, envanter, pickup, loot, sandık (Minecraft), oturma, kozmetik katalog + mağaza | çalışıyor |
| Ev içleri | oda oda döşeme, gerçek iç merdiven + korkuluk | çalışıyor |
| HUD | her zaman görünür can/stamina, hasar efekti, görev halkası, stil | çalışıyor |
| FP kollar v2 | yeni stilize kol rig'i, weapon_r soketi, CS2 dengesinde animasyonlar | çalışıyor |

## Director
- ✅ Viewmodel: FOV 72, daha aşağı/ileri, boş el gizli (sadece eylemde), bıçak 0.45, prosedürel CS2 slash (iki yön), saplama; rol dağıtılınca Sabırsız olmayanın bıçağı kınına.
- ✅ (derleme bekliyor) GMod/HL2 taşıma: E basılı tut = taşı, bırak = düşür, R = 45° çevir, sol tık = fırlat, yumuşak fizik tutacağı.
- ✅ (derleme bekliyor) CS2 inspect: 3.2 sn anahtar kare (kaldır → yassı yüz → öbür yüz → parmaklarda 360° çevirme → ağız kontrolü → indir).
- ✅ Korsan iskelesi (9 parça, halat korkuluk, bayrak, meşale, fıçı/sandık), ada (ayrı mesh) + denizde torii + pagoda + sakura/fener, köyde Japon bahçesi (yükseltilmiş koi göleti, kemerli köprü, koi sürüsü, lambalar), yatai tezgâhları, sakura ağaçları.
- ✅ Bulutlar (MI_KG_Clouds: alçak kalın kümülüs, rüzgârla), atmosfer puslu mavi; martı sürüleri (kanat çırpan M_KG_Bird).
- Öğrenilenler: glTF materyalsiz primitif → "Bad MeshDescription"; dokulu/PSD materyaller → kaydedilemeyen MID; çözüm `kg_sanitize_glb.py` (üçgenle + tek düz materyal) ve renkleri vertex'e pişirmek. Başarısız import editörün Interchange durumunu bozabiliyor → editör yeniden başlatma şart. Ajanlar editörü kilitlememek için "game" hedefini derler.
