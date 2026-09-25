# KillGo performans ve minimum sistem planı

**Durum:** taslak, 2026-09-25. Sprint sözleşmesi: `Docs/Iterations/SPRINT-042-Performance.md`.

**Kullanıcının isteği (2026-09-25):** "Haritayı indirilen asset'lerle süsleyebilirsin ama oyun optimize olmalı, takılmamalı, çünkü herkes oynayabilmeli. İleride web'e taşıyabilir miyiz?"

**Bu belgenin amacı:** "herkes oynayabilsin" sözünü sayıya çevirmek. Belge şunları tanımlar:
- donanım kademelerini ve her kademenin kare süresi, CPU, bellek ve draw call bütçelerini;
- ilk ölçümün sonuçlarını (baseline);
- en olası 5 darboğazı;
- bunları kapatacak işleri;
- her sprintten sonra çalışan otomatik performans kapısını.

> **Kısa özet:**
> - **Bugünkü durum:** Geliştirme PC'sinde (RTX 5070 + Ryzen 7 7800X3D) oyun Epic ayarda 1080p'de yaklaşık 100 fps veriyor. Aynı sahne GTX 1050 Ti'de tahminen 11–13 fps verir.
> - **İlk açılış sorunu:** Oyun ilk açılışta motorun varsayılanı olan **Epic** ile açılıyor. Otomatik algılama yalnızca bir düğmede duruyor. Zayıf bir PC'de oyunu açan kişi ilk karede takılmayı görür.
> - **Low ayarda:** GPU süresi 4 kat düşüyor (2,1–2,4 ms). Bu kez darboğaz **oyun thread'i**: 20 karakterle zayıf bir CPU'da 60 fps tutmaz.
> - **İş sırası:** ölçüm kapısı ve ayar presetleri → CPU (tick'ler, karakterler) ve sahne toplama (draw call) paralel → takılmalar (PSO, spawn) → gerçek düşük donanımda doğrulama.

---

## 1. Donanım kademeleri (hedefler)

| Kademe | Örnek GPU | Örnek CPU | RAM / VRAM | Çözünürlük ve hedef |
|---|---|---|---|---|
| **Low ("herkes")** | GTX 1050 Ti 4 GB, RX 560 4 GB, Steam Deck | i5-6500, i3-10100, Ryzen 3 1200, Steam Deck (Zen 2, 4 çekirdek) | 8 GB / 4 GB (Deck: paylaşımlı) | 1080p çıkış, ~%58–67 iç çözünürlük (veya 720p yerel); Deck 1280×800. **60 fps**, p95 ≤ 16,7 ms |
| **Medium** | GTX 1660 Super, RTX 2060, RX 5600 XT, Arc A580 | Ryzen 5 2600, i5-9400 (6 çekirdek) | 16 GB / 6 GB | 1080p, %83 iç çözünürlük. **60 fps** garanti, 90+ hedef |
| **High** | RTX 3060, RX 6600 XT ve üstü | Ryzen 5 3600, i5-10400 | 16 GB / 8 GB+ | 1080p High **100+ fps** (TechResearch'teki 120 fps hedefi Epic için değil High için), 1440p'de 60+ |
| Referans (geliştirme PC'si) | RTX 5070 12 GB | Ryzen 7 7800X3D | 32 GB | Kapı ölçümleri burada alınır (§4) |

Kurallar:
- **"Takılmıyor" tanımı:**
  - 60 sn'lik tur boyunca, ısınma bittikten sonra 50 ms'yi geçen kare yok;
  - p99 ≤ 25 ms (60 fps kademesinde);
  - oyuncu ya da bot katılırken 33 ms'yi geçen en fazla 1 kare.
- **Listen-server'da host da oyuncudur:** host'un ek yükü ≤ 2,5 ms/kare (TechResearch). Low kademesindeki bir PC de host olabilmeli. En zayıf host, 19 uzak oyuncu ve bot ile ölçülür.
- **İlk açılış:** Kademe tahmini otomatik yapılır (bkz. §5.1). Hiç kimse ilk açılışta Epic'le karşılaşmamalı.

### 1.1 Referans PC'den kademeye çevirme katsayıları [TAHMİN]
Low donanım elimizde yok. Bu yüzden kapı referans PC'de ölçer ve bu katsayılarla çevirir. Katsayılar kaba tahmindir: GPU için Time Spy Graphics oranları, CPU için tek çekirdek oyun thread'i oranı. SPRINT-042e'de gerçek bir Low cihazda bir kez doğrulanır ve bu tablo güncellenir.

| Kademe | GPU katsayısı (RTX 5070 = 1) | CPU oyun thread'i katsayısı (7800X3D = 1) |
|---|---|---|
| Low (1050 Ti / RX 560 / Deck) | ×9,5 (RX 560 ×11) | ×2,5 (X3D önbelleği yüzünden daha kötü olabilir) |
| Medium (1660S / 2060) | ×3,5 | ×1,9 |
| High (3060) | ×2,5 | ×1,6 |

**Hedef kademede 60 fps için:** GPU ≤ 14 ms ve oyun thread'i ≤ 12 ms olmalı (%15 pay). Bu, referans PC'de şu bütçelere denk gelir:

| Preset | Referans PC'de GPU bütçesi (ms) | Referans PC'de oyun thread'i bütçesi, 20 karakterle (ms) | Referans PC'de oyun thread'i bütçesi, boş sahne (ms) |
|---|---|---|---|
| Low | **≤ 1,5** | **≤ 4,8** | ≤ 2,5 |
| Medium | ≤ 4,0 | ≤ 6,3 | ≤ 3,0 |
| High | ≤ 4,0 (3060'ta ~100 fps) | ≤ 7,5 | ≤ 3,5 |
| Epic | ≤ 6,5 | ≤ 7,5 | ≤ 3,5 |

---

## 2. Bütçeler

### 2.1 Draw call ve üçgen
`RHI/DrawCalls` ve `RHI/PrimitivesDrawn`, gölgeler dahil tüm geçişlerin toplamıdır. `06_Art_Direction §8`'deki PC bütçesi (≤ 1500 draw call) Epic ve High preset için korunur. Ayrıca şu sınırlar eklenir:

| Preset | Draw call (20 karakter görünürken) | Gölge derinliği draw'ı | Üçgen/kare (tüm geçişler) |
|---|---|---|---|
| Low | ≤ 700 | ≤ 80 | ≤ 1,5 M |
| Medium | ≤ 1100 | ≤ 200 | ≤ 3,5 M |
| High / Epic | ≤ 1500 | ≤ 300 | ≤ 6 M |
| Karakter başına | ≤ 12 draw (tüm geçişler) | | ≤ 10k üçgen LOD0 (sanat bütçesi) |

### 2.2 CPU (20 oyuncu + botlar)
- **Tick'ler:**
  - boş sahnede her karede ≤ 150 aktör/bileşen tick'i;
  - 20 karakterle ≤ 300 tick.
  - Uzak karakterde viewmodel, kamera ve physics handle tick'i yok.
- **Oyun thread'i** (referans PC, 19 bot + host): ≤ 4,8 ms Low hedefi (§1.1).
  - Dağılım hedefi: TickActors ≤ 2,5 ms, animasyon ≤ 0,8 ms, CharacterMovement ≤ 0,5 ms, bot AI ≤ 0,5 ms.
- **Host ağ maliyeti** (19 bağlantıya replikasyon): referans PC'de ≤ 1,0 ms (Low'da ≤ 2,5 ms). `NetServerMaxTickRate=30` kalır.
- **Render thread'i** (referans PC, Low): ≤ 4,8 ms.

### 2.3 Bellek
| Preset | Süreç RAM'i (cook edilmiş build) | VRAM (`GPUMem/LocalUsedMB`) | Texture havuzu |
|---|---|---|---|
| Low | ≤ 3,0 GB (8 GB sistem) | ≤ 1,6 GB (4 GB kart ve Deck için pay) | 400 MB |
| Medium | ≤ 4,0 GB | ≤ 2,5 GB | 700 MB |
| High / Epic | ≤ 4,5 GB | ≤ 3,5 GB | 1000–1500 MB |

### 2.4 Yeni asset kapısı (süsleme bu bütçeyle yapılır)
Kullanıcı yeni asset indirip haritayı süslemeyi istiyor. Her yeni paket şu kurallarla içeri girer:
- `kg_import_dress_pack.py` manifestinde kontrol edilir.
- Kaynak `Tools/Assets/approved_sources.json` politikasına uyar: CC0 ya da benzeri, `Docs/Credits.md`'ye yazılır.
- İndirme ancak kullanıcı somut listeyi onaylayınca yapılır.

Kurallar:
- Üçgen ve materyal sayısı `06_Art_Direction §8`'e uyar. Küçük eşya ≤ 800 üçgen ve 1 materyal (palet) olur.
- Nanite yalnızca büyük ve yoğun mesh'lerde açılır. Küçük eşyada kapalıdır, onun yerine 3 kademe LOD kullanılır.
- Aynı mesh 5'ten fazla kez kullanılıyorsa HISM ile yerleşir.
- Her yerleşim bir çizim mesafesi (cull) alır: küçük eşya 30–60 m, orta boy 80–120 m.
- Bir bölgeye eklenen süsleme o bölgenin draw call değerini **+%10**'dan fazla artıramaz. Bu, performans kapısıyla (§4) ölçülür.

---

## 3. İlk ölçüm (baseline), 2026-09-25

**Nasıl ölçüldü:**
- Tek süreç kullanıldı: `UnrealEditor.exe ... L_Morrowmere_v2 -game -RenderOffScreen -nosound -ResX=1920 -ResY=1080`. Pencere açılmadı, süre 135 sn.
- Oyun modu gerçek `KGGameMode` idi (`kg.AutoStart=0`, `kg.BotFill=0`).
- `KG_CaptureCam` 8 sabit noktada gezdirildi: her noktada 2 sn oturma, ardından 3 sn ölçüm. Veriler CSV profiler'dan alındı.
- Script: `kg_perf_probe.py` (geçici dizinde). SPRINT-042a bunu `Tools/Unreal/kg_perf_tour.py` olarak kalıcılaştıracak.

**Dikkat:** Bu, editör binary'si (Development Editor) ile `-game` ölçümüdür. Cook edilmiş build değildir. GPU'yu kullanıcının açık editörü ve tarayıcısı da paylaşıyordu. Sayılar yön gösterir.

**Ölçüm noktaları** (m, göz → hedef, FOV 80):

| Ad | Göz | Hedef | Ne temsil ediyor |
|---|---|---|---|
| plaza | 12, 16, 6,8 | 0, −10, 9 | Meydan, toplantı yeri |
| belvedere | −3,4, −52,16, 15,7 | 20,23, 46,55, 0 | Tepeden bütün kasaba (gerçekçi en kötü görüş) |
| postcard | 14,22, 21,46, 6,7 | 45, 150, 6 | Liman → deniz, ada |
| jetty_back | 26,2, 72,5, 2,9 | 18, 40, 4,2 | İskeleden amfitiyatroya bakış (bütün kasaba) |
| ropewalk | −21, 34, 6,7 | 2, 14, 6,5 | Dar sokak |
| jp_garden | 53, 23, 9,7 | 44, 15, 8,5 | Sakura bahçesi |
| forest | −100, −28, 16 | −112, −60, 17,6 | Orman, az nesne |
| aerial_stress | 20, 95, 40 | 10, 5, 5 | 40 m havadan (stres testi, oyuncu görüşü değil) |

### 3.1 Epic ayar (kullanıcının şu anki ayarı, tüm gruplar 3), 1080p, bot yok
| Nokta | Kare (ms) | Oyun thread'i | Render thread'i | GPU | Draw call | Üçgen (M) | p99 kare |
|---|---|---|---|---|---|---|---|
| plaza | 9,30 | 3,86 | 9,31 | 8,72 | 778 | 4,42 | 11,45 |
| belvedere | 10,31 | 4,24 | 10,30 | 9,83 | 1213 | 6,71 | 12,70 |
| postcard | 9,78 | 3,92 | 9,79 | 9,34 | 914 | 6,84 | 11,65 |
| jetty_back | 8,85 | 3,77 | 8,85 | 8,36 | 1233 | 5,94 | 10,96 |
| ropewalk | 10,24 | 3,89 | 10,23 | 9,66 | 888 | 4,49 | 12,56 |
| jp_garden | 9,36 | 3,41 | 9,35 | 8,81 | 945 | 5,10 | 10,70 |
| forest | 10,29 | 3,31 | 10,29 | 9,83 | **176** | 1,46 | 12,03 |
| aerial_stress | 9,27 | 3,90 | 9,27 | 8,67 | 1338 | 5,29 | 11,15 |

- **GPU sınırı:** Epic'te darboğaz GPU. Render thread'inin 4,3–7,5 ms'si `EventWait/Visibility`, yani GPU beklemesi. Low'da bu bekleme 0,4–0,7 ms'ye iniyor.
- **Forest noktası:** Yalnızca 176 draw call var ama GPU yine 9,8 ms. Maliyet nesne sayısında değil, tam ekran geçişlerinde (§5 H1).
- **Draw dağılımı** (belvedere):
  - en büyük kalem gölge derinliği: 475 (postcard'da 607);
  - sonra occlusion testleri 263, ışıklar 128, base pass 106 ve custom depth (kontur) 57.

### 3.2 Low ayar (`scalability 0` = %50 iç çözünürlük, güneş gölgesi kapalı), bot yok
| Nokta | Kare | Oyun thread'i | Render thread'i | GPU | Draw call | Üçgen (M) | p99 |
|---|---|---|---|---|---|---|---|
| plaza | 3,84 | 3,73 | 3,49 | 2,11 | 466 | 1,69 | 5,02 |
| belvedere | 4,23 | 3,82 | 4,13 | 2,40 | 680 | 2,15 | 5,44 |
| jetty_back | 4,47 | 3,83 | 4,45 | 2,19 | 832 | 2,27 | 5,64 |
| forest | 3,57 | 3,53 | 2,49 | 2,36 | 102 | 0,61 | 4,68 |

Low'da darboğaz **oyun thread'i**: boş sahnede 3,5–3,8 ms, bunun 2,5 ms'si TickActors.

### 3.3 20 karakter (host + `kg.Bot.Add 19`, `kg.BotGather 1`), Epic
| Nokta | Kare | Oyun thread'i | Render thread'i | GPU | Draw call | Üçgen (M) |
|---|---|---|---|---|---|---|
| forest (botlar yolda, görüş dışında) | 10,24–10,33 | **5,05–5,46** | 10,3 | 9,8 | 176 | 1,46 |
| plaza (19 bot meydanda) | 9,37 | 5,80 | 9,37 | 8,83 | 1275 | 5,97 |
| belvedere | 11,90 | 6,18 | 11,90 | 11,34 | **1826** | **8,61** |
| jetty_back | 9,99 | **6,57** | 9,99 | 9,39 | **1845** | 7,84 |

- **Karakterlerin CPU maliyeti:** 20 karakter, tick sayısını 304'ten 570'e çıkarıyor. Oyun thread'ine +1,7–2,7 ms ekliyor: animasyon 0,6–0,7, CharacterMovement 0,4, TickActors +1,0.
- **Karakter başına tick:** Her karakterde 7 bileşen tick atıyor: Viewmodel, Camera, PhysicsHandle, Mouth, Fishing, Emote, WorldChore. Botlar ve uzak oyuncular dahil.
- **Görünür 20 karakterin draw maliyeti:** yaklaşık +550–610 draw, karakter başına ~27. Dağılım: custom depth 35→152, velocity 19→119, gölge +173, base pass +107.
- **Bütçe aşımı:** 1500 sınırı belvedere ve jetty_back noktalarında aşılıyor.

### 3.4 Takılmalar, bellek, yükleme
- **Takılmalar:**
  - `kg.Bot.Add 19` tek karede 19 karakter yarattı: **315 ms**'lik kare, ardından 50 ve 68 ms'lik iki kare.
  - Ayar değişimi (`scalability 0`) 293 ms + 142 ms sürdü (beklenen bir durum ama menüde "Uygula"ya basınca görülür).
  - Sabit turda 50 ms'yi geçen kare olmadı.
- **PSO:** 67 grafik PSO eksiği + 9 compute PSO eksiği oldu. PSO takılma süresi 75 ms (grafik) + 260 ms (compute). Cook edilmiş, PSO önbelleği olmayan bir build'de her yeni materyal ilk görüldüğünde takılır. Eski QA-002-08 bulgusu (~430 ms takılma) hâlâ açık.
- **Bellek:**
  - VRAM (`GPUMem/LocalUsedMB`): Epic 1,99–2,04 GB, Low 1,75 GB. Low bütçesi 1,6 GB, hafif aşım var.
  - Texture havuzu: Epic 1000 MB, Low 400 MB.
  - Süreç: çalışma kümesi 3,3–3,4 GB, private 5,1–5,4 GB. Bu editör binary'si, cook edilmiş build daha düşük olacak ve 042d'de ölçülecek.
- **Yükleme:** harita 1,1 sn'de yüklendi (sıcak DDC).
- **Sahne:**
  - 7.940 aktör, bunların **7.215'i ayrı StaticMeshActor**;
  - 168 hareketli nokta ışığı (gölgesiz);
  - GPU sahnesinde 37k instance;
  - 95 KGSpinner, 49 buoyancy bileşeni.
  - Seviye World Partition değil, bu yüzden HLOD yok.

### 3.5 Kademe tahmini (§1.1 katsayılarıyla) [TAHMİN]
| | Epic, 1080p | Low, %50 | Oyun thread'i, 20 karakter |
|---|---|---|---|
| GTX 1050 Ti | ~80–95 ms (**11–13 fps**) | ~20–23 ms (**43–50 fps**) | ~12,5–16,5 ms (**60 fps tutmaz**) |
| GTX 1660 Super | ~30–35 ms | ~7,5–8,5 ms | ~9,5–12,5 ms |
| RTX 3060 | ~21–25 ms (40–47 fps) | ~5–6 ms | ~8–10,5 ms |

**Sonuç:** Bugün Low kademe ne GPU'da ne de 20 oyunculu CPU'da 60 fps tutar. Medium, Low preset'le sınırda.

---

## 4. Otomatik performans kapısı (I5)

- **Araçlar:** `Tools/Gauntlet/kg_perf_gate.ps1` ve `Tools/Unreal/kg_perf_tour.py`.
- **Çalışma şekli:**
  - Tek süreç, pencere yok, ≤ 4 dakika.
  - Tur: 7 nokta (aerial_stress yalnızca bilgi amaçlı) × Low / High / Epic, ardından 19 bot ile 3 nokta.
  - Her segmentte avg/p95/p99 kare, oyun thread'i, render thread'i, GPU, draw call, üçgen, tick sayısı, VRAM ve 50 ms'yi geçen kare sayısı alınır.
- **Çıktı:** `Saved/Gauntlet/perf-<stamp>.json` ve CSV.
- **Bütçe dosyası:** `Tools/Gauntlet/perf_budget.json`. Bu dosyada §1.1 ve §2 bütçeleri ve baseline durur.
- **Kurallar:**
  - **Donanımdan bağımsız sayılar kesin kapıdır:** draw call, üçgen, tick sayısı, 50 ms'yi geçen takılma sayısı. Her makinede FAIL verebilirler.
  - **Süreler** (GPU, oyun thread'i, render thread'i) yalnızca `perf_budget.json`'daki referans makinede (GPU adı + CPU adı eşleşince) kapıdır:
    - bütçenin üstü = FAIL;
    - baseline'dan +%15 kötü = WARN;
    - +%30 kötü = FAIL.
  - Başka makinede süreler yalnızca rapor edilir.
- **`run_invariants.ps1`'e I5 olarak eklenir** ve `-Skip I5` ile atlanabilir.
  - GPU'yu ~4 dakika kullanır. Kullanıcı oyun oynarken çalıştırılmaz; bir oyun süreci açıksa ölçüm "UNRELIABLE" olarak işaretlenir.
- **Bir sprint şu durumlarda geri alınır:**
  - kesin kapılardan birini bozarsa;
  - ya da referans makinede herhangi bir segmenti +%30 kötüleştirirse.

---

## 5. En olası 5 darboğaz (sıralı) ve çözüm

### H1. GPU: Epic varsayılanı ve pahalı tam ekran geçişleri; ilk açılışta Epic
- **Kanıt:**
  - Epic'te GPU 8,4–9,8 ms, Low'da 2,1–2,4 ms.
  - 176 draw call'luk forest noktası bile 9,8 ms.
  - `GameUserSettings.ini`'de tüm gruplar 3.
  - `DefaultScalability.ini` yok.
  - Otomatik algılama (`RunHardwareBenchmark`) yalnızca Ayarlar'daki düğmede.
- **Şüpheliler:**
  - Epic TSR (`r.TSR.History.ScreenPercentage=200`);
  - VolumetricCloud;
  - SSR (`r.ReflectionMethod=2`);
  - SSAO;
  - kontur post-process'i (custom depth);
  - saydam okyanus (WPO dalgalar);
  - 3 kaskad CSM (2048);
  - gölge derinliğinde Nanite.
- **Çözüm (042a):**
  - Önce geçiş bazında GPU dökümü alınır (`-csvGpuStats` ve `ProfileGPU`).
  - Sonra `Config/DefaultScalability.ini` KG presetleri yazılır (§5.1).
  - İlk açılışta benchmark çalışır. Sonuç Medium'u geçmez ve başarısız olursa Medium seçilir.
  - VolumetricCloud'un Low için ucuz karşılığı yapılır (statik bulut kubbesi ya da çok düşük örnek sayısı).

### H2. CPU: her zaman açık tick'ler ve karakter başına 7 tick'li bileşen
- **Kanıt:**
  - Boş sahnede 304 tick, TickActors 2,2–2,7 ms. Bu ölçüm en hızlı oyun CPU'larından birinde alındı.
  - 20 karakterle 570 tick ve oyun thread'i 5,0–6,6 ms. Low CPU'da bu ~12,5–16,5 ms eder.
  - Tick yapanlar: KGSpinner ×95, Buoyancy ×49, ChoreSpot ×40, Door ×26, TaskStation ×22, FishSchool ×9.
  - Dönüşümler ucuz (UpdatePrimitiveTransform 0,01–0,04 ms). Yani pahalı olan tick'lerin kendi kodu. Hangi sınıf olduğu `dumpticks` ve Insights (`-trace=cpu`) ile bulunacak.
- **Çözüm (042c):**
  - Kozmetik dönmeler (yel değirmeni, çark, bayrak, fener) materyal WPO'suna ya da yakınlık/görünürlük tabanlı tick'e taşınır.
  - Buoyancy 15 Hz'de ve mesafeye göre çalışır.
  - İstasyon, kapı ve spot'lar yalnızca etkinken tick atar.
  - Uzak karakterde Viewmodel, Camera ve PhysicsHandle tick'i kapatılır. Fishing, Mouth ve Emote yalnızca etkinken çalışır.
  - Uzak karakterlerde animasyon URO (update rate optimization) açılır.
  - Host oyun kuralını bozmamak için hasar ve isabet kontrolü animasyondan bağımsız kalır.

### H3. Draw call ve üçgen: 7.215 ayrı aktör, çizim mesafesi yok, HLOD yok, karakter başına ~27 draw
- **Kanıt:**
  - Epic'te 1826–1845 draw (20 karakterle), bütçe 1500.
  - Gölge derinliği tek başına 607'ye çıkıyor.
  - Üçgen/kare 8,6 M'ye kadar çıkıyor.
  - Builder'ın 6.729 StaticMeshActor'ünde çizim mesafesi yok (yalnızca süsleme HISM'lerinde var).
  - 1.790 iç mekân parçası dışarıdan yalnızca occlusion ile eleniyor (occlusion testi draw'ı 360'a kadar).
  - Seviye WP değil, HLOD yok.
- **Çözüm (042b):**
  - Builder'da toplama geçişi: tekrar eden kit parçaları (duvar, çatı, pencere, korkuluk, merdiven) 32 m hücre × mesh × materyal başına ISM/HISM olur.
  - İç mekânlar ev başına ISM olur ve 40 m cull alır.
  - Sınıf bazlı cull mesafeleri kullanılır.
  - Nanite olmayan mesh'ler için 3 kademe LOD yapılır.
  - Küçük eşyalar ve ağaçlar gölgeyi yakında bırakır (cascade/mesafe).
  - Nokta ışıklarına MaxDrawDistance 60–80 m verilir.
  - Karakter tarafında (042c): modüler kıyafet parçaları tek skeletal mesh'e birleştirilir (ya da bileşen sayısı azaltılır). Kontur (custom depth) yalnızca belli bir mesafe içinde çizilir.
  - WP ve HLOD'a geçiş, yalnızca bunlar yetmezse ayrı bir karar olur.

### H4. Takılmalar: spawn, PSO, ayar değişimi
- **Kanıt:**
  - 19 bot tek karede: 315 ms.
  - Ayar değişimi: 293 ms.
  - 76 PSO eksiği, 335 ms PSO takılma süresi.
  - QA-002-08 (~430 ms) açık.
- **Çözüm (042d):**
  - Oyuncu ve bot spawn'ı karelere yayılır (kare başına 1).
  - Kıyafet ve materyal asset'leri önceden async yüklenir.
  - Maç sırasında senkron yükleme yasaklanır (log'da `FlushAsyncLoading` ve `LoadPackage` kontrolü).
  - Cook edilmiş build'de PSO precaching doğrulanır. Paketlenmiş PSO önbelleği (bundled PSO cache) eklenir.
  - Yükleme ekranında shader/PSO ısıtması yapılır.
  - 430 ms takılmanın kaynağı Insights ile bulunur.

### H5. Ayarlar bizim içeriğimize ulaşmıyor, Low preset de çok kaba
- **Kanıt:**
  - `KGGrassField` ve `KGFoliageField` düz HISM. Cull mesafeleri sabit (çim 50–80 m) ve `sg.FoliageQuality` okunmuyor. Menüdeki "Foliage: grass and plant density" büyük olasılıkla bir şey değiştirmiyor (doğrulanacak).
  - Nokta ışıklarının MaxDrawDistance değeri 0, yani `r.LightMaxDrawDistanceScale` etkisiz.
  - Motorun Low preset'i güneş gölgesini **tamamen** kapatıyor (gölge draw'ı 0). Toon görünüm ve okunabilirlik için 1 kaskad gerekli.
  - Low'da VRAM 1,75 GB, bütçe 1,6 GB.
  - Env mesh'lerinin 60'tan fazlasında Nanite açık. Teknik mimaride "Nanite bağımlılığı yok" kuralı var. Nanite'in Low'daki sabit maliyeti ölçülmedi.
- **Çözüm (042a):**
  - `KGGrassField` ve `KGFoliageField` foliage kalitesine göre yoğunluk ve cull ölçeği alır.
  - Presetler gerçek ayarlara bağlanır (§5.1).
  - Low'da `r.Nanite` 0 ile 1 A/B ölçülür. Kazanan preset'e yazılır.

### 5.1 Presetlerin gerçek ayarlara eşlenmesi (042a'da `DefaultScalability.ini`, ölçümle ince ayar)
| Grup | Low | Medium | High | Epic |
|---|---|---|---|---|
| İç çözünürlük (1080p'de) | %58–67, A/B: TSR q0 ya da FXAA + keskinleştirme | %83, TSR q1 | %100, TSR q2 | %100, TSR q3, history %100 (200 değil) |
| Güneş gölgesi | 1 kaskad, 1024, ~40 m | 2 kaskad, 1024, ~60 m | 3 kaskad, 2048, ~90 m | 3 kaskad, 2048, ~120 m |
| Nokta ışıkları | gölgesiz (zaten), çizim ×0,5 | ×0,75 | ×1 | ×1 |
| Yansıma | SSR kapalı | SSR kapalı | SSR q2 | SSR q3 |
| Post | AO kapalı, bloom düşük, kontur açık | AO düşük | AO | AO + |
| Bulut / gökyüzü | VolumetricCloud yerine statik kubbe (ya da en düşük örnek) | düşük örnek | normal | normal |
| Görüş mesafesi | `r.ViewDistanceScale` 0,6 + sınıf cull'ları | 0,8 | 1,0 | 1,0 |
| Çim ve ağaç (KG) | yoğunluk ×0,35, cull ×0,6, ağaç gölgesi yakında | ×0,6 / ×0,8 | ×1 | ×1 |
| Texture | havuz 400 MB | 700 MB | 1000 MB | 1500 MB |
| Kare sınırı (varsayılan) | 60 | ekran tazeleme hızı | ekran tazeleme hızı | ekran tazeleme hızı |

Ayarlar menüsünde şu anda preset, 7 grup, render ölçeği, fps sınırı, VSync ve "Auto-detect" düğmesi var. Menüye 3 şey eklenir:
- ilk açılışta otomatik algılama;
- Low'da "Steam Deck" alt ayarı (800p, 60 fps);
- ayar değişiminde "Uygulanıyor…" göstergesi, çünkü 293 ms'lik takılma sürpriz olmasın.

---

## 6. İş sırası (SPRINT-042)

| Dilim | İş | Bağımlılık | Kabul özeti |
|---|---|---|---|
| **042a** | Ölçüm kapısı (I5), GPU dökümü, `DefaultScalability.ini`, ilk açılışta otomatik algılama, çim ve ağaç ölçeklemesi | - | Low GPU ≤ 1,5 ms, High ≤ 4,0 ms, Epic ≤ 6,5 ms (referans PC); I5 yeşil |
| **042b** | Sahne toplama: ISM/HISM, cull mesafeleri, LOD, ışık mesafeleri | 042a | Draw call ve üçgen bütçeleri (§2.1); verify ve navcheck 79/79 aynı kalır |
| **042c** | CPU: tick denetimi, uzak karakterler, animasyon URO, karakter draw'ları | 042a (042b ile paralel, dosyalar ayrık) | Oyun thread'i ≤ 2,5 ms boş sahnede, ≤ 4,8 ms 20 karakterle; tick sayısı bütçesi |
| **042d** | Takılmalar: spawn yayma, PSO önbelleği, async yükleme, QA-002-08 | 042a | 50 ms'yi geçen kare yok; PSO eksiği ısınmadan sonra 0; cook edilmiş RAM ve VRAM bütçesi |
| **042e [U]** | Gerçek Low cihazda doğrulama (1050 Ti sınıfı PC ya da Steam Deck) | 042a–d | Katsayı tablosu (§1.1) gerçek sayılarla güncellenir |

**Doğrulama borcu notu:** Doğrulama borcu 13 kalem. 042a ve 042d araç ve altyapı işidir, yeni oyun özelliği eklemez. 042b görünümü değiştirir, bu yüzden bir doğrulama oturumundan sonra başlar.

---

## 7. Web'e taşıma notu (kullanıcının sorusu)

- **Resmî yol yok:** Unreal Engine 5'in resmî bir HTML5/WebGL/WebGPU çıktısı yok. Epic, HTML5 platformunu UE 4.24'te motordan çıkardı ve topluluğa bıraktı.
- **Seçenekler:**
  1. **Pixel Streaming:**
     - Oyun bulutta bir GPU'da çalışır, tarayıcıya video gelir.
     - Tarayıcıdan oynanır ama oyuncu başına sunucu GPU'su gerekir. Bu, saatlik bir maliyet demektir.
     - Gecikme eklenir. 20 kişilik, sesli bir oyunda pahalı olur.
  2. **Üçüncü taraf UE5 → WebGPU çözümleri:**
     - Ticari ve deneysel; bu projede doğrulanmadı.
     - Boyut (indirme), bellek ve ses (EOS) uyumu büyük soru işaretleri.
  3. **Ayrı, hafif bir web istemcisi:** bugünkü kapsamda gerçekçi değil.
- **Öneri:** Web şimdilik "ileride değerlendirilecek" kalır. Web ve mobil denemelerinde işe yarayacak hazırlık bu plandaki Low kademesi bütçeleridir: az draw call, az VRAM, ucuz gökyüzü, CPU'da az tick. Teknik mimarideki "Mobil ES3.1" kuralı da aynı hazırlıktır. Bu yüzden 042 önce gelir.
