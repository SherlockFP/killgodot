# ITER-013 — Detaylı balık tutma (2026-09-24)

**Kullanıcı:** "gerçek suyla detaylı balık tutma". Editör kapalı (kullanıcı oyunda): her şey headless (kod + build,
commandlet importları, Blender CLI, pencere açmayan -nullrhi / -RenderOffScreen süreçleri).

## Plan -> yapılanlar
| Parça | Nerede | Durum |
|---|---|---|
| Kurallar: tür tablosu, ısırık hızı, dürtme/ısırık takvimi, tür/çöp zarı, ağırlık, fiyat, atış yayı, su sınıfı | `Source/KillGodot/Fishing/KGFishingTypes.*` | ✅ |
| Çekme mini oyunu (60 Hz deterministik, tahmin + uzlaşma) | `FKGReelSim` | ✅ |
| Karakter bileşeni (sunucu yetkili; herkes misina/şamandırayı görür; sahip tahmini, girdi, kamera) | `KGFishingComponent.*` | ✅ |
| Runtime ekleme + Madam Brine tezgâhı + ağ smoke betiği | `KGFishingSubsystem.*`, `KGFishMarket.*` | ✅ |
| Kişisel rekor (yerel kayıt) | `KGFishingJournal.*` | ✅ |
| HUD (güç, ısırık, gerilim çubuğu, av kartı, bildirim, vinyet, debug) | `UI/KGHUDFishing.inl` | ✅ |
| Eşyalar: olta, eski çizme, şişede mesaj, kese; "Fishing" ganimet tablosu = çöp/hazine; ağırlıklı yığın (`FKGItemEntry.Grams`) | `Inventory/` | ✅ |
| FP klipler: rod_idle / rod_windup_in / rod_windup / rod_cast / rod_reel (sol el makara çevirir) / rod_hookset | `Tools/Blender/kg_make_fp_arms2.py --only rod_*` | ✅ |
| TP üst beden klipleri: Fish_Hold / Fish_Cast / Fish_Reel | `Tools/Blender/kg_make_emotes.py` | ✅ |
| Sesler: S_Fish_Cast/Nibble/Bite/Hook/Snap/Fanfare/Escape/Coins, S_FishReel_Loop | `Tools/Audio/kg_synth_sfx.py` | ✅ |
| Tek komutla import (ses + FP + TP) | `Tools/Unreal/kg_import_fishing_assets.py` | ✅ |
| Dev: kg.Fish.Give/Bite/Cast/Hook/Land/Sell/Market/BotCast/Tension/Species + panel "FISHING" | `Dev/` | ✅ |
| Testler: SpeciesTable, BiteRolls, BiteSchedule, ReelMinigame, WeightsAndPrices | `Private/Tests/KGFishingTests.cpp` | ✅ 5/5 |
| Ağ smoke: atış -> zorla ısırık -> vurma -> çekme -> av iki makinede, cepte ağırlıkla | `Tools/Unreal/kg_fish_smoke.ps1` | ✅ PASSED |
| Piksel turu (FP adımları, TP bot, tezgâh) | `Tools/Unreal/kg_fish_capture.ps1` -> `Saved/Screenshots/Fish/` | ✅ 7 kare |

## Kararlar
- Olta tuşu `H` (UI dokümanı `Q`'yu görev parşömenine ayırıyor).
- Görevlere bağlanmadı: balık Hazırlık barını doldurmaz (Sabırsız da balık tutar, görev sahteciliği kurallarını bozmaz);
  kazanç maç içi altın.
- Çekme ayarı Python ile simüle edilip seçildi: uskumru/morina sabit sarmayla gelir, somon ~yarı yarıya kopar, altın
  sazan sabit sarmada her zaman kopar; 0.25 sn gecikmeli "insan" politikası hepsini 10-24 sn'de çeker.
- Host göçünde olta durumu sıfırlanır (bileşen karakter snapshot'ına girmiyor) — backlog'da.

## Kapılar
G1 (editör) ✅, oyun hedefi ✅, G2 38/38 ✅ (5 yeni balık testi), `kg_fish_smoke.ps1` PASSED.

## Görsel doğrulama bekleyenler (piksellerde ilk bakış iyi; oyun içinde elle bakılmalı)
FP olta tutuşu + misinanın görünen uçtan başlaması (FP projeksiyonu), TP olta yönü (hand_r'dan runtime nişan), şamandıra
ölçeği, sıçrama damlaları, tezgâh (masa + tabela + dönen sazan) yerleşimi, av kartı ve gerilim paneli düzeni.
