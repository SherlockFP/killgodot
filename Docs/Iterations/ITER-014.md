# ITER-014 — Görev mini oyunları (Among Us tarzı) + görsel görevler (2026-09-24)

**Kullanıcı:** Among Us tarzı görevler, "çok sayıda, yeni nesil de olsun": her göreve mini oyun, bulanık dünya
üstünde küçük pencere, kesilebilir (ayrıl / vurul / toplantı), çok aşamalıda ilerleme saklanır, sunucu doğrular (anlık
bitirme yok), görsel görevler (çan köyde çalar, fener parlar, baca tüter, ilan panoda belirir), Sabırsız sahte yapabilir,
`kg.Chore.Play/AutoWin`, testler, ekran dışı görüntülerle en az 2 tur gözden geçirme. Editör kapalı, başsız çalışma.

## Yapılanlar
| Parça | Dosya | Durum |
|---|---|---|
| Katalog + kurallar (22 görev, aşamalar, sunucu alt sınırları, görsel etki türü) | `Chores/KGChoreTypes.*` | ✅ |
| Sunucu oturumu, doğrulama, kesintiler, kayıtlı aşama, sahte yol, sahibin paneli | `Chores/KGChoreComponent.*` (AKGCharacter alt nesnesi) | ✅ |
| Görsel görev etkileri (replike, her zaman ilgili) | `Chores/KGChoreFx.*` + `M_KG_ChoreGlow/Smoke` | ✅ (yerleşim PIE'de doğrulanacak) |
| Panel (bulanık/karartılmış dünya, başlık, talimat, aşama noktaları, kutlama, ret/kesinti şeridi) | `Chores/UI/SKGChorePanel.*` | ✅ |
| Mini oyun çatısı + ressam (640×400 mantıksal tuval) | `Chores/UI/KGMinigame.*` | ✅ |
| 22 mini oyun | `KGMinigames_Core` (4), `_Village` (6), `_Craft` (6), `_Coast` (6) | ✅ (3 paralel ajan + ben) |
| İstasyon yönlendirmesi (oyuncu → mini oyun, bot/mini oyunsuz → tut-E) | `World/KGTaskStation.cpp` | ✅ |
| Dev: `kg.Chore.Play`, `kg.Chore.AutoWin`, panel düğmeleri; `kg.ChoreShot` | `Dev/`, `Chores/KGChoreDebug.cpp` | ✅ |
| Sesler (25 prosedürel `S_Chore_*`, `S_UI_*`) + commandlet içe aktarma | `Tools/Audio/kg_synth_sfx.py`, `Tools/Unreal/kg_import_chore_assets.py` | ✅ |
| Testler `KillGodot.Chores.Catalog/Rules/MinigameLogic/ServerValidation/FakeTask` | `Private/Tests/KGChoreTests.cpp` | ✅ 5/5 |
| Ekran dışı görüntüler (40 aşama) + 2 tur gözden geçirme | `Tools/Unreal/kg_chore_shots.ps1` → `Saved/UIShots/chore_*.png` | ✅ |
| Başsız ağ testi (istemci → host, görsel etkinin istemciye gelmesi, toplantı kesintisi + devam) | `Tools/Unreal/kg_chore_smoke.ps1` | ✅ geçti |

## Gözden geçirme turları
1. Başlıkta aşama noktaları talimat satırıyla çakışıyordu, etiketler üst üste biniyordu, uzun talimatlar taşıyordu →
   noktalar başlık satırına, talimat otomatik küçülür, sahte/pratik etiketleri alt çubuğa. ChopWood baltası ters yöne
   bakıyordu, kütük bölünmüş görünüyordu; DrawWater taşımada "kol" pembe sütun gibiydi → düzeltildi. Yakalama
   PNG'leri doğrusal renk yazıyordu (soluk/yanlış) → betik sRGB'ye çevirir (oyundaki renkler).
2. LightCandles "YOUR TURN" etiketi mumu örtüyordu → sunak örtüsüne; HarvestCarrots çürük saplar toprakla aynı
   kahverengiydi → gri-mor; Craft HUD panelleri yarı saydamdı → neredeyse opak. UnloadFish'te `Pts.Add(Pts[0])`
   dizi takma adı çökmesi (görüntü betiği yakaladı) → düzeltildi.

## Öğrenilenler
- Bir .cpp'de global `using namespace KGMg;` (W, H, A, C gibi kısa adlar) motor şablonlarında C4459 hatası verir:
  mini oyunlar `namespace KGMg { }` içinde yazılır.
- `SWidget::Tag` üyesi var: widget içinde `Tag` adlı lambda C4458.
- `FWidgetRenderer` + `ExportRenderTarget2DAsPNG`: gama kapalı hedef doğrusal değer yazar → sRGB kodla.
- `-nullrhi` süreçlerde Slate görünüm alanı widget'larını tick'lemez: bileşen paneli `ManualTick` ile yürütür (sadece `!FApp::CanEverRender()`).
- Paralel ajanlar aynı DLL'i kilitler (başsız oyun süreçleri): derleme `LNK1104` → süreç bitince tekrar.

## Açık (PIE / editörde doğrulanacak)
Görsel etkilerin haritadaki yeri/ölçeği/parlaklığı, panelin oyundaki hissi ve bulanıklığı, ses seviyeleri, gerçek oyuncu
süreleri (tasarımlar ~15–30 sn; hedef 30–60 → sayılar artırılabilir).
