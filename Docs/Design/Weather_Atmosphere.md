# Hava ve Atmosfer: rüzgâr, yağmur, kar, sis, şimşek + grafik kalite geçişi (v0.1)

> Durum: **Önerildi (kullanıcı onayı gerekiyor)**, 2026-09-25. Backlog'a taşınmadı, kuyruğu değiştirmez.
> Sözleşme: `Docs/Iterations/SPRINT-040-Weather.md` (3 dilim: 040a, 040b, 040c).
> Kaynak: kullanıcının istekleri (Türkçe, özetle): "Görsel tasarımı ve haritaları bir tık iyileştirebilirsin. Rüzgâr
> sistemi, yağmur, kar gibi şeyler ekleyebilirsin; arada şimşek çakabilir." ve "Grafikleri bir tık iyileştir: gölgeler,
> ışık, su da."
> Performans: `Docs/Process/Performance_Plan.md` + SPRINT-042 (kademeler, bütçeler, I5 kapısı, `DefaultScalability.ini`).
> Neye göre: `KillGo_Pillars.md` v1.1 (sabitler §0.3, G4 kontrollü varyans, S10.6 Dünya Olayları, S3 delil yaşam
> döngüsü), `Pillar_Roadmap.md` (020b Olay Defteri, 026a fiziksel izler, R8 Dünya Olayları), `Forest_Threats_Chores.md`
> (Sis, meşale, kamp ateşi), `KillGo_Lore.md` (ufuktaki Sis).
> **Kapsam dışı:** su materyalinin kendisi (SPRINT-022 yapıyor; burada sadece hava kancaları var), ev çeşitliliği,
> yerleşim, kuleler (SPRINT-022). Storm Manor'un odaları (SPRINT-017/018).

---

## 0. Özet (tek paragraf)

Morrowmere'in havası artık bir **oyun durumu**dur, bir süs değil. Sunucu maç başında tohumlu (`FKGRng`) bir **hava planı**
çıkarır. Her gündüz ve gece için bir durum vardır: **Açık, Rüzgârlı, Yağmur, Fırtına (şimşekli), Kar, Sis**. Plan oyuncuya
parça parça ve **önceden** söylenir: Tellal akşam toplantısında yarının gündüzünü, şafakta bu gecenin havasını haber
verir. Hava değişmeden 90 sn önce bulutlar toplanır, rüzgâr döner. Plan maçın temposuyla ağırlaşır: 1. gün hep sakin,
fırtına en erken 3. günde. Her durumun **sayılı** bir oyun etkisi vardır. Yağmur sesleri kısar, izleri çabuk siler (katilin
havası). Kar ayak izlerini tutar, adımları duyurur (dedektifin havası). Şimşek gece 0.35 sn boyunca silüetleri gösterir,
gök gürültüsü mesafeye göre gecikir ve 3 sn boyunca sesleri bastırır. Rüzgâr sesi rüzgâr yönüne taşır, meşaleyi söndürebilir
ve Sis'i dağıtır. Sis görüş menzillerini kısar. Hiçbir hava durumu tek başına bir sonucu belirlemez: öldürmez, rol
göstermez, yazı-tura atmaz. Aynı iş, grafik tarafında "bir tık"lık bir kalite geçişiyle gelir: daha iyi gölgeler (CSM
kaskadları, contact shadow, GTAO), gece ışığının düzeltilmesi (Fener Burnu uçurumu artık simsiyah değil, gece göğü gri pus
değil, lacivert ve yıldızlı) ve faz başına renk grading'i. Hepsi Low/Medium/High kademelerine bağlı, bütçeli ve başsız
ölçülür.

---

## 1. İlkeler (bu belgenin kuralları)

| # | Kural | Neden / ölçüm |
|---|---|---|
| H1 | **Hava bir Dünya Olayıdır.** Açık ve Rüzgârlı "temel hava"dır. Yağmur, Fırtına, Kar ve Sis o fazın Dünya Olayı sayılır: faz başına ≤ 1, ≥ 20 sn önceden bildirilir, sonucu belirleyen 0 (S10.6). Aynı fazda ikinci bir Dünya Olayı (ör. R8'deki Gemi Enkazı) çıkmaz. | Charter sözlüğü "Dünya Olayı" tanımında zaten "Sis, Fırtına" var. |
| H2 | **Rastgelelik kurulumda.** Plan maç başında tohumdan çıkar. Maç sırasında hiçbir hava etkisi zar atmaz: şimşek zamanları, rüzgâr esintileri, bulut toplanması plandan türetilir (G4, Ç7). | [T] determinizm testi (§9). |
| H3 | **Önce gösterilir, sonra etkiler.** Her hava geçişinin ≥ 90 sn görsel habercisi, her şimşeğin 1.5 sn'lik habercisi (bulut içi titreşme + uzak gürleme), her sert esintinin 3 sn'lik habercisi (uğultu + çimde ilerleyen dalga) vardır. | Ç11: etkiyi yiyen oyuncu önceden görür. |
| H4 | **İki tarafa da yarar.** Yağmur ve gök gürültüsü katile örtü verir. Kar, şimşek ve rüzgâr yönü tanığa yarar. Hiçbir hava "Kasaba kazanır" ya da "Sabırsız kazanır" demek değildir. | S6.5 / S7.2 bantları hava başına **sadece rapor** olarak yazılır. |
| H5 | **Hava öldürmez.** Şimşek kimseye çarpmaz, soğuk can yakmaz, fırtına kimseyi denize atmaz. Tehlike dolaylıdır: az duyulmak, az görmek, sönen meşale. | Ölüm kaynakları charter ve orman belgesinde kalır. |
| H6 | **Sunucu sahibidir, herkes aynı havayı görür.** Hava durumu sunucuda tutulur ve replike edilir. Delil kuralları sunucudaki hava durumuyla hesaplanır. İstemcinin gördüğü görsel, sunucunun kullandığı sayıyla aynıdır. | Adil delil. §7 iki süreçli smoke. |
| H7 | **Grafik ayarı oyun bilgisini değiştirmez.** Sis yoğunluğu, gece karanlığı, gece dolgu ışığı ve şimşek aydınlığı **her kademede aynıdır**. Low sadece parçacık sayısını, gölge çözünürlüğünü ve ekstraları kısar. Low'a geçen oyuncu siste daha uzağı görmez. | [T] kademe testi (§8.4). |
| H8 | **İçerisi kurudur.** Evlerin içine, yeraltına ve Storm Manor'un içine yağmur ve kar yağmaz. Pencereden dışarısı görünür. | [R] "iç mekân kuru" render kontrolü. |
| H9 | **Botlar aynı sayıları okur.** Görüş ve duyma çarpanları tek bir saf fonksiyondan gelir. Tanık filtresi de, bot görüşü de onu kullanır. Botlar gelecek planı okuyamaz, sadece ilan edilen tahmini bilir (G6.1). | [T] + G6.1 grep kontrolü. |

---

## 2. Hava durumları

### 2.1 Durum tablosu

"Şiddet" planın tırmanma puanıdır (§3). Rüzgâr gücü `s` 0–1 arasıdır.

| Durum | Şiddet | Rüzgâr `s` | Ne görünür | Ne duyulur | Oyundaki anlamı (tek cümle, HUD kartı) |
|---|---|---|---|---|---|
| **Açık** (Clear) | 0 | 0.2 | Bugünkü görünüm, pofuduk kümülüs | Martılar, dalga | "Güzel bir gün. Godot yine gelmedi." |
| **Rüzgârlı** (Windy) | 1 | 0.6 | Çimen ve ağaçlar belirgin sallanır, bayraklar dalgalanır, bulutlar hızlı akar, deniz beyaz köpüklü | Uğultu, esinti | "Ses rüzgârla gider. Meşalene dikkat." |
| **Yağmur** (Rain) | 2 | 0.4 | Yağmur çizgileri, ıslak parlak taş, su birikintisi ve halkalar, gri-mavi grading | Yağmur, çatıda tıpırtı | "Sesler kısılır, izler çabuk silinir." |
| **Fırtına** (Storm) | 3 | 1.0 | Sağanak, kara bulut, köpüren deniz (dalga ×1.6), şimşek | Gök gürültüsü, fırtına uğultusu | "Şimşek çakınca herkes görünür. Gürlerken kimse duymaz." |
| **Kar** (Snow) | 2 | 0.3 | Süzülen kar taneleri, çatılarda ve toprakta biriken kar, soğuk mavi ışık | Boğuk sessizlik, adımlarda gıcırtı | "Her adım iz bırakır ve duyulur." |
| **Sis** (Fog) | 2 | 0.05 | Denizden gelen yoğun sis, fenerlerin etrafında hâle | Uzak sis düdüğü, damla | "Görüş kısa. Yalnız kalma." |

**Kış mevsimi:** Kar ancak mevsim **Kış** iken çıkar. Lobi seçeneği: `Mevsim: Otomatik / Sonbahar / Kış`. Otomatik'te
tohum maçların %20'sinde Kış seçer (kurulum rastgeleliği, lobi ekranında yazılı). Kış'ta planın Yağmur'u Kar'a,
Fırtına'sı **Kar Fırtınası**'na (şimşeksiz, `s` = 1.0, görüş Fırtına'daki gibi, iz kuralları Kar'daki gibi) döner. Çatı
ve toprak karı maç boyunca birikir ve eriyebilir (§5.4).

### 2.2 Geçiş ve şiddet eğrisi
- Hava **sadece faz sınırlarında** değişir. Yeni durum `KG_WX_BLEND` (60 oyun saniyesi) içinde karışır. Karışım
  Toplantı, Mahkeme ve Şafak fazlarına denk getirilir: bu fazlarda saldırı yok (S9.1), geçiş kimseyi hazırlıksız yakalamaz.
- Bir fazın içinde durum aynı kalır, **şiddet** (`I`, 0–1) plandaki eğriyi izler: Yağmur 0.4 → 1.0 → 0.6; Fırtına ilk
  %30'da 0 → 1 yükselir, sonra 0.8–1.0 arasında dalgalanır (tohumlu eğri); Sis ve Kar sabit 1.0.
- Tüm süreler `FKGMatchClock`'tan okunur (CLAUDE.md kuralı). `TimerManager` yok.

---

## 3. Maç hava planı: tohumlu, önceden bildirilen, tempoyla tırmanan

### 3.1 Tohum
- Plan, maç tohumundan **türetilmiş ayrı bir akıştan** çekilir: `FKGRng(MatchSeed ^ Hash("Weather"))`. Böylece hava
  eklemek mevcut tohumların rol listesini, evlerini ve görevlerini **değiştirmez** (eski tohumlar aynı maçı verir).
- Plan maç başında bir kez, `N` gün için (S10.1'den en fazla 7 gün + 1 yedek) çıkar ve **sadece sunucuda** kalır
  (`UPROPERTY(SaveGame)`). İstemcilere sadece şimdiki durum ve **ilan edilmiş** tahmin gider (§7.2). Gelecek havayı
  veriden okumak mümkün değildir.

### 3.2 Faz başına olasılık tablosu (öneri, [L] ile lint edilir)

| Faz | Açık | Rüzgârlı | Yağmur | Sis | Fırtına |
|---|---|---|---|---|---|
| 1. gündüz | %70 | %30 | — | — | — |
| 1. gece | %50 | %30 | — | %20 | — |
| 2. gündüz | %35 | %25 | %20 | %20 | — |
| 2. gece | %25 | %20 | %25 | %30 | — |
| 3. gün ve sonrası, gündüz | %20 | %20 | %25 | %20 | %15 |
| 3. gece ve sonrası | %15 | %15 | %25 | %20 | %25 |

**Kısıtlar (plan üreticisi zorlar, [T]):**
1. Fırtına maçta en fazla 1 kez (plan ≥ 6 gün ise 2). Çekilirse ikinci Fırtına Yağmur'a iner.
2. Şiddet ≥ 2 olan durum art arda en fazla 2 faz sürer. Üçüncüsü Rüzgârlı olur (nefes payı).
3. Fırtına gecesinden önceki gündüz Açık olamaz, en az Rüzgârlı olur (bulut ve rüzgâr gelişi görünür).
4. Kış'ta Yağmur → Kar, Fırtına → Kar Fırtınası.
5. **Tırmanma (S10.3):** planın 3. gün ve sonrası ortalama şiddeti, 1–2. günlerin ortalamasından yüksek olur. Tutmazsa
   plan bir sonraki alt akışla yeniden çekilir (en fazla 4 deneme, sonra en ağır iki fazı yer değiştirir). Hepsi
   deterministiktir.
6. **Oyun sonu:** canlı ≤ max(5, 2·tehdit + 1) iken (orman belgesinin eşiği) yeni bir Fırtına başlamaz. Başlamış olan
   sürer. Pariteyi gürültü örtüsü belirlemesin (S10.5).

### 3.3 Tahmin: kim, ne zaman, nasıl söyler
| Ne | Ne zaman söylenir | Önceden | Kanal |
|---|---|---|---|
| 1. gündüz + 1. gece | Rol açılışı kartının altında | ≥ 20 sn (açılış + şafak) | Kart satırı |
| Bu gecenin havası | Şafakta Tellal | Gündüz boyu (≥ 150 sn) | Tellal satırı + Kasaba Panosu (024) + HUD saat simgesi |
| Yarının gündüzü | Akşam toplantısı açılışında Tellal | Toplantı + gece (≥ 120 sn) | Aynı |
| Geçişin kendisi | Görsel haberci, son 90 sn | 90 sn | Bulut örtüsü artar, rüzgâr döner, ufuktan sis bankası gelir, uzak gürleme |

- Tahmin **her zaman doğrudur**. "%60 yağmur" gibi bir belirsizlik yok. Belirsizlik oyuncularda olsun, gökte değil.
- **Tahmin kartı (okunabilirlik):** kullanıcı kartların okunmasının sıkıcı olduğunu söyledi. Hava kartı bu yüzden okumaya
  değil **bakmaya** göre tasarlanır: büyük bir hava ikonu (yağmur bulutu, kar tanesi, şimşek), ikonun altında en fazla
  **6 kelimelik** etki satırı ("Sesler kısılır, izler silinir") ve altında en fazla 14 kelimelik Tellal şakası. Kart 4 sn
  kalır, sonra saat simgesinin yanına küçük ikon olarak iner. Renk: durum başına bir şerit (Yağmur çelik mavisi, Fırtına
  mor, Kar buz beyazı, Sis süt grisi). G3.1 bütçesinin içinde kalır.
- **Tellal satırları** (ST_Lore'a gider; EN/TR, RU sonra):

| Anahtar | TR | EN |
|---|---|---|
| Lore.Crier.Wx.Clear | Duyduk duymadık demeyin! Bu gece gök açık. Godot'nun bir bahanesi eksildi. | Hear ye! Clear skies tonight. Godot is running out of excuses. |
| Lore.Crier.Wx.Windy | Rüzgâr çıkacak! Şapkalarınızı ve sırlarınızı sıkı tutun. | Wind tonight! Hold on to your hats and your secrets. |
| Lore.Crier.Wx.Rain | Yağmur geliyor. Ayak izleri akıp gidecek, günahlar da. | Rain is coming. Footprints will wash away. So will sins. |
| Lore.Crier.Wx.Storm | Fırtına! Şimşek çakınca etrafınıza bakın. Gürlerken kulak vermeyin. | Storm! When lightning strikes, look around. When it thunders, don't bother listening. |
| Lore.Crier.Wx.Snow | Kar yağacak. Nereye gittiyseniz kar söyleyecek. | Snow tonight. Wherever you go, the snow will tell. |
| Lore.Crier.Wx.Fog | Sis ufuktan iniyor. Feneri doldurun, birbirinizi kaybetmeyin. | The fog is coming in. Fill the lighthouse, and don't lose each other. |
| Lore.Crier.Wx.Loop | Takvim yine aynı gün. Bari hava değişsin dedik, değişti. | The calendar says the same day again. We asked for different weather. Granted. |

### 3.4 Lore bağı: Sis ve fener
Ufuktaki Sis hiç kalkmaz; "fener sönerse sis içeri girer" (`KillGo_Lore.md` §3.9, Lore.Load.10). Hava planındaki **Sis**
bu sisin köye inmesidir. Bir karar kancası (Ç1) olarak:
- Gece Sis'i tahmin edildiyse ve fener lambası o gün **fiziksel olarak** doldurulduysa (`FuelLighthouse` paneli ya da
  `LighthouseOil` dünya görevi, taraf-kör: orman belgesi §5.5 ile aynı kural), gece **Hafif Sis** olur: görüş çarpanları
  §4.2'deki Sis satırı yerine Hafif Sis satırıyla uygulanır. Doldurulmadıysa tam Sis.
- Tahmin satırı bunu açıkça söyler: "Sis geliyor. Fener dolarsa kıyıda kalır." Sabırsız'ın karşı kolu, orman belgesiyle
  aynı: lambayı söndürmek (016/026b `snuff`), deftere düşen bir fiil.
- Bu kanca orman belgesinin fener kuralıyla aynı dünya durumunu okur. İki belgenin sahibi aynı fener durumunu kullanır,
  ikinci bir "fener doldu mu" değişkeni yazılmaz. (Onay listesi W3.)

---

## 4. Oyun kancaları (sayılarla)

Tüm çarpanlar tek bir saf fonksiyondan gelir: `FKGWeatherRules` (UObject'siz, `FKGRoleListGenerator` kalıbı). Charter'ın
§0.3 sabitleri **değişmez**; hava onların üstüne çarpan uygular. Bu belgenin kendi sabitleri `KG_WX_*` adıyla §4.8'de
toplanır ve kodda/kabulde isimle anılır.

### 4.1 Duyma (charter `KG_HEAR_*` olay sesleri)

Etkin yarıçap = `KG_HEAR_x` × `M_hava` × `M_rüzgâr(θ)` × (duvar arkasındaysa `KG_WALL_FACTOR`) × (gök gürültüsü
maskesi varsa `KG_WX_THUNDER_MASK`), sonra [`KG_WX_HEAR_MIN` × taban, `KG_WX_HEAR_MAX` × taban] aralığına kırpılır.

| Durum | `M_hava` | Not |
|---|---|---|
| Açık | 1.00 | |
| Rüzgârlı | 1.00 | Etkisi yön çarpanından gelir |
| Yağmur | 0.80 (şiddet 1'de 0.75) | `M = 1 − 0.25·I` |
| Fırtına | 0.65 (şiddet 1'de 0.60) | `M = 1 − 0.40·I` |
| Kar | 0.90 | Kar sesi yutar, ama adımlar ayrıca yükselir (§4.3) |
| Sis | 1.00 | Görmediğini duyarsın: gerilim buradan gelir |

**Rüzgâr sesi taşır:** `M_rüzgâr(θ) = 1 + 0.25 · s · cos θ`. θ, rüzgâr vektörü ile kaynaktan dinleyiciye giden yön
arasındaki açıdır. Fırtınada (`s` = 1) rüzgâr altındaki dinleyici ×1.25, rüzgâr üstündeki ×0.75. Rüzgârlıda
(`s` = 0.6) ×1.15 / ×0.85.

**Kırpma:** `KG_WX_HEAR_MIN` = 0.30, `KG_WX_HEAR_MAX` = 1.30. Örnek: fırtınada, rüzgâr üstünde, gök gürlerken bir bıçak
cinayeti (`KG_HEAR_KILL` 15 m): 15 × 0.60 × 0.75 × 0.5 = 3.4 m → kırpılır: **4.5 m**. Aynı cinayet açık havada 15 m.

**Gök gürültüsü maskesi:** gürültü bir dinleyiciye ulaştığı andan itibaren `KG_WX_THUNDER_MASK_S` (3 sn) boyunca o
dinleyici için duyma ×0.5 (`KG_WX_THUNDER_MASK`). Ulaşma anı dinleyiciye göre hesaplanır (§4.4).

**Ses sohbeti etkilenmez.** Yakınlık sesinin menzilleri G1.2'de sabittir. Hava sadece oyun olaylarının duyulma
yarıçapını değiştirir. (Oyuncu fırtınada bağırmak zorunda kalmaz, ama bir çığlığı duymayabilir.)

**Kurt uluması ve Sis fısıltıları:** aynı çarpanla ölçeklenir ama tabanı `KG_HOWL_SECTOR_R` (60 m) altına inmez.
Orman telgrafları oyuncuya özel sunucu damgasıyla çalışır (orman belgesi P1), duyma yarıçapına bağlı değildir. Hava
telgraf garantisini bozmaz.

**Botlar:** `FKGBotView` duyma testini aynı fonksiyonla yapar. Bot, maske penceresini de "duyduğu" gök gürültüsünden
bilir (algı olayı), plandan değil.

### 4.2 Görüş (charter `KG_SIGHT_*`)

| Durum | `KG_SIGHT_DAY` 30 m | `KG_SIGHT_NIGHT_LIT` 12 m | `KG_SIGHT_UNLIT` 5 m | `KG_IDENT_RANGE` 8 m |
|---|---|---|---|---|
| Açık / Rüzgârlı | 30 | 12 | 5 | 8 |
| Yağmur | 24 | 10 | 4 | min(8, görüş) |
| Fırtına | 20 | 9 | 4 (şimşekte bkz. §4.4) | min(8, görüş) |
| Kar (yağarken) | 22 | 11 | 5 | 8 |
| Kar örtüsü (yerde ≥ %50, gece) | — | 12 | **7** (kar ay ışığını yansıtır) | 8 |
| Sis | **15** | **8** | **3.5** | min(8, görüş) → gece ışıksızda 3.5 |
| Hafif Sis (fener doldu, §3.4) | 22 | 10 | 4.5 | min(8, görüş) |

- Görüş konisi `KG_SIGHT_CONE` ve LOS şartı değişmez.
- **Görsel = kural:** siste 15 m'deki bir köylü silüeti ekranda da o mesafede kaybolur. Sis yoğunluğu, 040b'deki silüet
  kontrast ölçümüyle bu sayılara kalibre edilir ([R], §9): hedef silüet `KG_SIGHT` × 0.9'da Weber kontrastı ≥ 0.10,
  `KG_SIGHT` × 1.3'te ≤ 0.04.
- **İsim etiketi** `KG_IDENT_RANGE` ile aynı kurala uyar: görüş 8 m'nin altına inerse etiket de o mesafede biter.
- **Botlar:** tanık filtresi ve bot görüşü aynı tabloyu okur. Sis'te Kasaba botları gruplaşma ağırlığını artırır,
  Sabırsız botları Sis ve gürültü pencerelerini "örtü" sayar (§4.7, parametreler dondurulur).

### 4.3 Adımlar
Bugünkü kod: bir adım sesi ~1.7 m'de bir, `KGAudio::Near` sönümüyle ≈ 20 m'den duyulur (2.5 m iç yarıçap + 18 m sönüm).
Bu sayı `KG_WX_STEP_BASE` (20 m) olarak adlandırılır.

| Durum | Adım yarıçapı çarpanı | Sonuç (yürüme) | Ses |
|---|---|---|---|
| Açık / Rüzgârlı | 1.0 (+ rüzgâr yönü) | 20 m | Bugünkü |
| Yağmur | 0.6 | 12 m | Islak şapırtı |
| Fırtına | 0.45 | 9 m | Islak şapırtı, yağmurda kaybolur |
| Kar (yerde ≥ %30) | **1.25** | 25 m | Gıcırtı (yeni `S_Wx_Step_Snow_0..2`) |
| Sis | 1.0 | 20 m | Bugünkü |

Adım sesi bugün bir defter olayı değildir (düz hareket karar fiili değil). Bu çarpan sadece ses yarıçapını değiştirir.
Botlar adım sesini bugün algılamıyor; 023 bot zihni adım algısı eklerse aynı çarpanı kullanır.

### 4.4 Şimşek ve gök gürültüsü
| Sabit | Değer | Anlamı |
|---|---|---|
| `KG_WX_STRIKE_GAP` | 20–45 sn (tohumlu), ilk şimşek şiddet ≥ 0.6 olunca | Fırtına fazında iki şimşek dizisi arası. N = 12'de 111 sn'lik gece: 2–5 şimşek |
| `KG_WX_PREFLICKER` | 1.5 sn | Haberci: bulut içi zayıf titreşme + uzak gürleme. Oyun etkisi yok |
| `KG_WX_FLASH` | 2–3 darbe, toplam ≤ 0.4 sn | Parlama dizisi |
| `KG_WX_FLASH_WINDOW` | 0.35 sn (ilk darbeden itibaren) | Oyun penceresi |
| `KG_WX_FLASH_SIGHT` | 25 m | Pencere içinde, **gece açık havada** (ya da pencereden dışarıya LOS'u olan) tanığın görüşü: silüet (`bSaw`) |
| Tanımlama | `KG_IDENT_RANGE` (8 m) | Pencere içinde ışıksız alandaki tanık 8 m'ye kadar tanımlar (normalde 5 m). Maske yine gizler |
| `KG_WX_BOLT_DIST` | köy merkezinden 250–1200 m (tohumlu), denizde ya da orman ötesinde | Yıldırım asla oyuncuya, oynanabilir alana ya da bir oyun nesnesine düşmez |
| Gecikme | mesafe / 343 m/sn → 0.7–3.5 sn | Gök gürültüsü her dinleyiciye kendi mesafesiyle ulaşır |
| `KG_WX_THUNDER_MASK` | ×0.5 duyma, `KG_WX_THUNDER_MASK_S` 3 sn | Gürültü ulaşınca |

- **Neden adil (G4):** şimşeğin **anı** tohumdan gelir ama haberci 1.5 sn önce herkese görünür ve duyulur. Katil
  "şimşekten sonra, gürültü gelmişken" vurmayı seçebilir; tanık "şimşekte etrafına bakmayı" seçebilir. Şimşek bir yazı-tura
  değil, okunabilir bir ritimdir (017 Storm Manor ile aynı ilke).
- **Sert kanıt bütçesi (S2.4):** parlama tanımlamayı sadece `KG_IDENT_RANGE` içinde ve sadece ışıksız alandaki tanık için
  5 m'den 8 m'ye çıkarır; bu, zaten aydınlık bir alandaki tanıklıkla aynıdır. 8–25 m arası sadece **silüet**tir
  (yumuşak ipucu: "fırtınada meydanın doğusunda biri eğiliyordu"). S2.2 soak'ta hava durumuna göre ayrı raporlanır.
- **Defter:** her şimşek bir `Lightning` olayı yazar (konum, pencere); pencere içindeki her algı `bByFlash` bayrağıyla
  tanık listesine düşer. Gündüz şimşeği silüet vermez (zaten gündüz görüşü geçerli), sadece gürültü maskesi verir.
- **Botlar:** bot, haberciyi algı olayı olarak alır (insan gibi). Sabırsız bot "gürültü penceresinde saldır" ağırlığını,
  Kasaba botu "parlamada etrafa bak" davranışını kullanır; ikisi de `KG_WX_BOT reason=flash|thunder` loglar ([U]).

### 4.5 Rüzgâr: sesi taşır, fenerleri sallar, meşaleyi söndürür, Sis'i dağıtır
- **Tek rüzgâr vektörü:** maç başında tohumdan bir yön seçilir, denizden karaya doğru ±60° sektörde (kıyı köyü). Faz
  değişiminde en fazla 30° döner. Güç `s` durum tablosundan (§2.1).
- **Esintiler (`KG_WX_GUST`):** Rüzgârlı'da 60–120 sn'de bir, Fırtına'da 25–50 sn'de bir (tohumlu). Esinti 3 sn önceden
  duyulur ve rüzgâr üstünden gelen bir çimen dalgası olarak görünür, sonra 4 sn boyunca `s` +0.4 olur.
- **Fenerler sallanır:** asılı fenerler, tabelalar, bayraklar, çamaşır ipleri ve yelkenler rüzgârla sallanır (WPO, §5.1).
  Yol fenerleri ve sokak lambaları **kapalı camlıdır, rüzgârla sönmez**. (Orman belgesi P3: güvenli yer her zaman
  görünür. Rüzgâr güvenli ışığı söndüremez.)
- **Meşale (orman belgesi `KG_TORCH`, `KG_TORCH_BURN` 90 sn):** Yağmur'da yanma süresi ×0.75 (68 sn), Fırtına'da ×0.67
  (60 sn). **Fırtına esintisi** meşaleyi söndürür, **ancak** tutan kişi açıktaysa (rüzgâr üstüne göğüs hizasında 3 m'lik
  izde bir engel yok) **ve** koşuyorsa. Yürüyen ya da bir duvarın rüzgâr altında duran oyuncunun meşalesi sadece titrer.
  Karar ve karşı hamle: esinti 3 sn önceden duyulur; yavaşla ya da duvara sığın. Rüzgârlı'da meşale hiç sönmez.
  Sönen meşale `TorchOut` olayı yazar (tanıklar görür: ışık kaybolur).
- **Açık havadaki mumlar:** mezarlıktaki ve sundurmalardaki açık mumlar Fırtına esintisinde söner (kilisenin içindeki
  `LightCandles` mumları etkilenmez). Dünya durumudur, şafakta sıfırlanır, `CandlesOut` herkese açık olayı yazar (S4.6).
- **Kamp ateşi (orman belgesi `KG_FIRE_FUEL`):** yakıt tüketimi Yağmur'da ×1.3, Fırtına'da ×1.6, Kar'da ×1.1.
  `KG_FIRE_SAFE` yarıçapı değişmez. **Nöbet dengesini etkiler; orman belgesi sahibinin onayı gerekir (W4).**
- **Sis ve rüzgâr:** Rüzgârlı ve Fırtına'da Sis dikkati (`KG_MIST_NOTICE`) ×0.8 hızla dolar (rüzgâr sisi dağıtır);
  Sis havasında ×1.25 hızla (gündüz 40 → 32 sn, gece 20 → 16 sn). `KG_MIST_SPEED`, `KG_MIST_CORE` ve
  `KG_MIST_TELEGRAPH_MIN` **değişmez**: orman belgesinin kaçış garantisi (§5.3) havadan bağımsızdır. Sis dili siste de
  kendi parlak kenarıyla orman belgesindeki mesafelerden (gündüz 40 m, gece 15 m) görünür kalır. Rüzgâr, Sis
  Duvarı'nın görselini rüzgâr yönünde 5 m geri iter ve sis perdelerini o yöne akıtır (sadece görsel).
- **Deniz:** dalga yüksekliği hava ile ölçeklenir (§5.1, `SwellScale`): Açık 0.8, Rüzgârlı 1.1, Yağmur 1.0, Fırtına 1.6,
  Kar 0.9, Sis 0.6. Yüzme, yüzdürme ve olta şamandırası `FKGWaves` üzerinden aynı değeri kullanır.
- **Botlar:** meşaleli bot esinti habercisini duyunca Fırtına'da yavaşlar ya da rüzgâr altına geçer
  (`BotWxTorchLee`); duyma çarpanı rüzgâr yönüyle aynı fonksiyondan.

### 4.6 İzler: yağmur siler, kar tutar (charter §3 delil tablosu üzerine)

Ömür = charter ömrü × çarpan. Çarpan, izin **açık gökyüzü altında** olup olmamasına bakar (sunucu: sığınak hacmi ya da
yukarı 25 m iz, §6.3). Kapalı yerdeki iz her havada charter ömrünü korur. Hava bir izin ömrünü **oluştuğu andan sonra**
değişirse, kalan ömür yeni çarpanla yeniden ölçeklenir (kalan süre × yeni/eski).

| İz (charter ömrü) | Yağmur | Fırtına | Kar yağarken | Kar örtüsü (yağış durdu) |
|---|---|---|---|---|
| Ayak izi (60–120 sn) | ×0.5 → 30–60 sn | ×0.35 → 21–42 sn | ×1.5 → 90–180 sn | ×3 → 180–360 sn |
| Boya/talaş lekesi, yerde (45–60 sn) | ×0.5 → 23–30 sn | ×0.35 → 16–21 sn | ×1.5 → 68–90 sn (kırmızı karda daha görünür) | ×2 → 90–120 sn |
| Leke, katilin üstünde (45–60 sn) | ×0.5 (yağmur yıkar ama yavaş; kuyuda yıkanma 7 sn yine en hızlısı) | ×0.35 | ×1.0 | ×1.0 |
| Sürükleme izi (60 sn) | ×0.5 → 30 sn | ×0.35 → 21 sn | ×1.5 → 90 sn | ×3 → 180 sn |
| Açık mezar, kırık kapı, ceset | ×1.0 | ×1.0 | ×1.0 | ×1.0 |

**Yumuşak zemin genişler:**
- Yağmurda çimen ve toprak yollar da çamur sayılır: ayak izi **daha çok yerde** çıkar ama **daha çabuk** silinir. Taş sokak
  yine iz tutmaz (katilin kaçış yolu olarak kalır).
- Kar örtüsü ≥ %50 iken **açık havadaki her yürünebilir zemin**, taş sokak dahil, yumuşak zemindir. Kar, taş sokak kaçışını
  kapatır; bu yüzden en güçlü delil havasıdır ve planda en fazla 2 faz sürer.

**Bütçe:** iz havuzu tavanı charter S3.5'te 256'dır ve değişmez. Kar'da ayak izi her ikinci adımda bir çıkar (≈ 3.4 m'de bir)
ki havuz yürüyüşle dolmasın; havuz dolunca en eski **ayak izi** önce silinir (leke ve sürükleme izleri korunur).
S3.5 GPU bütçesi (256 iz, ≤ 0.2 ms) karda da ölçülür.

**Islak durumu:** yağmur, ganimet/orman belgelerindeki tek **Islak** (`KG_WET`) durumunu **vermez**. Islak, Sis Duvarı'ndan
ve bele kadar sudan dönüşün izidir; yağmurda herkes ıslansa o iz anlamsızlaşırdı.

**Botlar:** iz takibi (026a, `KG_TRAIL`) aynı ömür fonksiyonunu okur. Kar örtüsünde Kasaba botlarının taze iz takip
ağırlığı artar (`BotWxSnowTrack`), yağmurda azalır.

### 4.7 Bot parametreleri (dondurulur, soak başlığına yazılır, §0.2 kural 2)
| Parametre | Varsayılan | Etki |
|---|---|---|
| `BotWxCoverWeight` | 0.3 | Sabırsız botun saldırı puanına gürültü maskesi / Sis / Fırtına katkısı |
| `BotWxFogGroup` | +0.4 | Kasaba botunun Sis'te grup takip ağırlığı |
| `BotWxFlashLook` | açık | Haberciyi algılayan Kasaba botu, parlama penceresinde görüş konisini en yakın silüete çevirir (deterministik: en yakın hedef, zar yok) |
| `BotWxTorchLee` | açık | Fırtına esintisi habercisinde meşaleli bot yavaşlar / sığınır |
| `BotWxSnowTrack` | +0.5 | Kar örtüsünde taze iz takip ağırlığı |

Hepsi [U] olarak loglanır (`KG_WX_BOT bot=… reason=thunder|fog|flash|lee|track`). Yüzde bandı yok.

### 4.8 Sabitler (bu belgenin tek kaynağı)
| Sabit | Değer |
|---|---|
| `KG_WX_BLEND` | 60 sn durum karışımı |
| `KG_WX_PRECURSOR` | 90 sn görsel haberci |
| `KG_WX_HEAR_MIN` / `MAX` | 0.30 / 1.30 × taban |
| `KG_WX_WIND_CARRY` | 0.25 (`1 + 0.25·s·cos θ`) |
| `KG_WX_THUNDER_MASK` / `_S` | ×0.5 / 3 sn |
| `KG_WX_STRIKE_GAP` | 20–45 sn |
| `KG_WX_PREFLICKER` | 1.5 sn |
| `KG_WX_FLASH_WINDOW` | 0.35 sn |
| `KG_WX_FLASH_SIGHT` | 25 m |
| `KG_WX_BOLT_DIST` | 250–1200 m |
| `KG_WX_GUST` | Rüzgârlı 60–120 sn, Fırtına 25–50 sn; 3 sn haberci, 4 sn +0.4 |
| `KG_WX_STEP_BASE` | 20 m |
| `KG_WX_WET_UP` / `_DRY` | Islaklık 0 → 1: 90 sn yağmur; 1 → 0: 180 sn kuru hava |
| `KG_WX_SNOW_UP` / `_MELT` | Kar örtüsü 0 → 1: 240 sn kar; 1 → 0: 300 sn (Kış'ta Açık/Rüzgârlı gece erimez) |
| Görüş, duyma, iz tabloları | §4.1–§4.6 |

### 4.9 Kontrol listesi (charter Ç1–Ç12)
| # | Cevap | Gerekçe |
|---|---|---|
| Ç1 Karar | Evet | Nereye (taş sokak / çamur), ne zaman (gürültü penceresi), meşaleyle nasıl yürüneceği, fener doldurulsun mu |
| Ç2 İz | Evet | `WeatherChange`, `Lightning` (+ `bByFlash` tanıklar), `TorchOut`, `CandlesOut` deftere düşer; izlerin ömrü havayla değişir |
| Ç3 Karşı hamle | Evet | Her etki önceden bildirilir; iki taraf da kullanır (H4) |
| Ç4 Ölçüm | Evet | S4.6 (günlük görünür dünya değişikliği), S10.3 (tırmanma), S10.6 (Dünya Olayı kuralı), S3.2 (ömür tablosu) |
| Ç5 Bağlantı | Evet | Defter, tanık filtresi, izler, botlar, orman (Sis, meşale, ateş), ses |
| Ç6 Ölçek | Evet | N'den bağımsız çarpanlar; N = 6'da da okunur |
| Ç7 Varyans | Evet | Tohumlu plan, önceden bildirilir, sonuç zarı yok |
| Ç8 Bot | Evet | Aynı fonksiyonlar, dondurulmuş parametreler |
| Ç9 Sessiz yol | Evet | Tahmin kartı ve Pano yazılı |
| Ç10 Okunabilirlik | Evet | Durum başına tek satır (§2.1) |
| Ç11 Frustrasyon | Evet | Haberciler (H3) |
| Ç12 Tempo | Evet | Orta ve geç evre (§3.2) |

---

## 5. Görseller

### 5.1 Rüzgâr: tek vektör, her şey aynı yöne
Bugün rüzgâr yönü materyallere gömülü: `M_KG_Foliage` SWAY kodu `float2(0.8, 0.45)` ve materyal başına `WindStrength`
kullanıyor, `M_KG_Grass` ve `M_KG_JapanFoliage` aynı yönü kopyalıyor, bulutlar ve deniz kendi başına.
- Yeni **`MPC_KG_Weather`** (Material Parameter Collection) tek kaynak olur:
  `WindDir (xy), WindStrength, Gust, Wetness, RainIntensity, SnowIntensity, SnowCover, FogAmount, LightningFlash,
  NightAmount, PrecursorAmount`.
- Yeni materyal fonksiyonu **`MF_KG_Wind`**: bugünkü SWAY formülü, yönü ve genliği MPC'den okuyarak. Genlik
  `0.4 + 1.6·(WindStrength + Gust)`: Açık'ta bugünküne yakın, Fırtına'da ~3 kat.
  Kullananlar: yapraklar, çiçekler, çimen, Japon bahçesi bitkileri, bayraklar/çamaşır/yelkenler (yeni `MF_KG_ClothWind`:
  tutturma kenarından uzaklıkla artan dalga), asılı fenerler ve tabelalar (`MF_KG_HangSway`: üst tutturma noktası etrafında
  sarkaç, genlik ≤ 8°).
- **Deniz:** `MPC_KG_Water`'a `SwellScale` eklenir; `FKGWaves::HeightAt` aynı değeri `FKGWaves::SetSwellScale` ile alır.
  Değer sunucunun replike ettiği hava durumundan ve maç saatinden hesaplanır, istemci ile sunucu aynı yüzeyi görür.
  Rüzgâr ayrıca dalgaların ana yönünü en fazla 20° çevirebilir (sadece ilk dalga; `KGWaves.h` ↔ `kg_make_ocean.py` senkron
  kuralı korunur). Köpük miktarı (`WhitecapAmount`) SPRINT-022'nin su materyaline bir **parametre** olarak verilir; su
  materyalini 022 yazar, bu iş sadece parametreyi sürer.
- **Bulutlar:** `MI_KG_Clouds`'un kapsama, yoğunluk, albedo ve akış ofseti MPC'den sürülür. Akış = rüzgâr yönü × güç.
  Fırtına: kapsama 0.95, albedo 0.35 (kara bulut), taban irtifası 2.2 → 1.4 km (alçalır). Haberci süresinde (90 sn)
  kapsama önce ufukta, sonra tepede artar.
- **Yetki:** hepsi başsız yazılabilir (`kg_make_ocean.py` kalıbı: Python ile materyal ifadesi + MPC). Yayında olan varlıklar
  yerinde üzerine yazılmaz (LoopContract): yeni ana materyaller `M_KG_FoliageWx`, `M_KG_GrassWx`, `M_KG_JapanFoliageWx`
  olarak yazılır ve instance'lar bir betikle bunlara bağlanır; betiğin `--revert` modu eskisine döndürür.

### 5.2 Yağmur
- **Kamera-yerel yağmur hacmi, mesh tabanlı (Niagara yok):** Blender'da başsız üretilen bir "yağmur kutusu" mesh'i: 20×20×12 m
  kutuda binlerce ince çift-quad; her quad'ın vertex rengi/UV2'si bir tohum taşır. `M_KG_RainStreaks` (translucent,
  unlit, eklemeli) WPO ile her çizgiyi `frac(Time·hız + tohum)` ile aşağı indirir ve kutu yüksekliğinde sarar; kutu
  kameranın konumuna **dünya ızgarasında** (kutu boyutu modülünde) kilitlenir, yani yürürken yağmur "kameraya yapışık"
  görünmez. Rüzgâr çizgileri `WindDir × WindStrength` ile eğer (Fırtına'da ~25°). İki katman: yakın (10 m, yoğun, uzun
  çizgi) ve uzak (30 m, seyrek). Tek draw call / katman.
- **Uzak perde:** 30 m ötesi, sis ile birleşen bir post-process yağmur perdesi (ekran-uzayı dikey gürültü, derinliğe göre).
  Çok ucuz, uzak dağların arkasında "yağmur sağanağı" hissi verir.
- **Sıçrama ve halkalar:** yüzey materyalinde (5.3) yatay yüzeylerde animasyonlu halka normal'i. Niagara ile derinlik
  tamponu çarpışmalı sıçrama parçacıkları sadece High'da ve sadece bir **spike** başarılı olursa (1 deneme: bir motor
  şablon sistemini kopyalayıp kullanıcı parametrelerini Python'dan ayarlamak). Başarısızsa halkalar yeter.
- **Kamera:** birinci şahısta ekranda damla efekti **yok** (okunabilirlik ve G3). Sadece çok hafif bir kontrast düşüşü.

### 5.3 Islak yüzeyler ve su birikintileri (`MF_KG_Wetness`)
- ~8 ana materyalin (palet, bina, zemin, arazi, sahne eşyası) sonuna eklenen tek bir fonksiyon, `MPC_KG_Weather.Wetness`
  ile sürülür:
  - albedo × (1 − 0.30·w) (gözenekli yüzey koyulaşır),
  - roughness → lerp(r, 0.18, w·düzlük),
  - **birikinti:** normal.z > 0.95 ve dünya-hizalı gürültü maskesi eşiği aşan yerlerde roughness 0.05, albedo koyu,
    yağarken halka normal'i. Birikinti oranı Wetness 1'de yatay yüzeyin ~%18'i.
- **Kapalı alan maskesi:** yağmur tıkanma haritası (§5.7) High/Epic'te sundurma altlarını ve saçak diplerini kuru tutar;
  Low/Medium'da sadece iç mekân hacimleri kurudur.
- Islaklık `KG_WX_WET_UP` / `_DRY` ile yavaşça gelir ve kurur; yağmur durunca taşlar 3 dk boyunca parlar (güzel bir
  "yağmurdan sonra" anı, özellikle altın saatte).

### 5.4 Kar
- **Parçacık:** yağmurla aynı kamera-yerel mesh yaklaşımı, `M_KG_Snowfall`: yavaş düşüş (1.2 m/sn), sinüs salınımı, kameraya
  dönük quad (WPO billboard), rüzgârla sürüklenme.
- **Birikme (`MF_KG_Snow`):** dünya-hizalı karışım: `maske = saturate((N.z − 0.55)·6) · SnowCover · tıkanma`. Çatılar,
  toprak, çit üstleri, ağaç taçlarının üstü (yaprak materyallerinde üst yüz beyazlaşır). Kar rengi paletten (buz beyazı,
  mavi gölge), roughness 0.75. Taş sokak kenarlardan içe doğru beyazlar (gürültülü kenar).
- **Ayak izleri** 026a'nın decal havuzundan gelir; karda decal materyali "basık kar" varyantını kullanır (koyu mavi gölgeli
  çukur). Leke karda kırmızı-kahve kalır (G3: okunur, ama stilize: talaş/boya, kan değil).

### 5.5 Şimşek
Otomatik pozlama kapalı (`r.DefaultFeature.AutoExposure=False`), bu yüzden parlama gerçekten parlar; sınırlar §8.5'te.
- **Gök parlaması:** `LightningFlash` MPC değeri bulut materyalinde iç ışımayı (emissive) ve gökyüzü sky luminance'ını
  yükseltir.
- **Işık darbesi:** yeni bir ışık eklenmez (ikinci bir gölgeli yönlü ışık tam bir CSM geçişi daha demektir). Ay ışığı
  (mevcut yönlü ışık) 0.9 lux'tan en fazla 30 lux'a darbe yapar ve rengi mavi-beyaza döner; SkyLight yoğunluğu da aynı
  darbede ×4. Böylece şimşek mevcut gölgelerle gelir ve evlerin pencerelerinden içeri de düşer (Storm Manor için önemli).
- **Yıldırım mesh'i:** Blender'da başsız üretilen 6 dallanmış şerit mesh (L-sistemi), unlit emissive `M_KG_Bolt`,
  0.25 sn görünür, sonra 0.4 sn'de söner. Konum `KG_WX_BOLT_DIST`'ten.
- **Ses:** `S_Wx_Thunder_Near` (keskin çatırtı + gürleme), `_Mid`, `_Far` (alçak geçiren gürleme). Dinleyiciye
  mesafe/343 sn gecikmeyle, mesafeye göre katman seçilir.

### 5.6 Bulut ve sis geçişleri
- **Yükseklik sisi:** bugün sabit (`fog_density` 0.012, inscattering (0.55, 0.68, 0.85)). Yeni: yoğunluk ve renk faz
  profilinden (§8.2) ve havadan gelir. Sis havasında yoğunluk silüet kalibrasyonuyla bulunur (§4.2); başlangıç değeri
  ~0.06, `fog_height_falloff` 0.25 (sis yerde çöker, kule tepeleri sisin üstünde kalır: güzel bir görüntü).
- **Hacimsel sis (volumetric fog):** sadece High'da, fener ve lamba hâleleri için. Gameplay görüşünü **değiştirmez**
  (H7): görüşü taşıyan şey her kademede aynı olan exponential height fog'dur.
- **Haberciler:** Sis'ten önce ufukta bir sis bankası (büyük, yavaş yaklaşan sis kartları; Sis Duvarı ile aynı materyal
  ailesi), Fırtına'dan önce kara bulut duvarı ve uzak gürleme, Kar'dan önce bulutların beyaz-grileşmesi ve sessizleşen
  ortam sesi.

### 5.7 İçerisi kuru kalır
Üç katman, her biri ayrı bir işe:
1. **`AKGShelterVolume`** (oyun + ses + kamera): layout JSON'daki `"interior": true` evler, yeraltı (ayrı Data Layer),
   kapalı pazar çatıları ve (018'de) Storm Manor'un iç hacmi. Hacimler **çalışma zamanında** layout JSON'dan spawn edilir
   (yol haritası kuralı: v2 inşa hattına dokunulmaz). Kamera bir hacmin içindeyse yağmur/kar kutusu gizlenir, yağmur
   sesi "çatıda yağmur" katmanına (alçak geçiren 800 Hz, −12 dB; G1.2 duvar kuralıyla uyumlu) geçer ve iç mekân PP'si
   devreye girer (§8.2).
2. **Yağmur tıkanma haritası** (görsel, High/Epic): kameranın etrafında 64×64 m'lik, yukarıdan ortografik bir
   `SceneCaptureComponent2D` sadece derinliği yakalar: High 256² (4 karede bir), Epic 512² (2 karede bir). Yağmur/kar
   materyali her çizginin dünya Z'sini bu yükseklikle karşılaştırır: çatının altına düşen çizgi çizilmez. Aynı harita ıslaklık ve kar maskesini de kısar. Böylece kapı eşiğinden dışarı bakınca yağmur görünür, saçağın
   altı kurudur.
3. **Sunucu kuralı (oyun):** bir iz ya da oyuncu "açık gökyüzü altında" mı? Önce sığınak hacmi, yoksa yukarı doğru 25 m'lik
   tek bir iz (trace). Sonuç iz başına oluşunda bir kez hesaplanır ve saklanır.

### 5.8 Ses (hepsi prosedürel, `Tools/Audio/kg_synth_sfx.py`)
Yeni sesler `S_Wx_` önekiyle: `Rain_Light_Loop`, `Rain_Heavy_Loop`, `Rain_Roof_Loop`, `Wind_Strong_Loop`, `Gust_0..2`,
`Thunder_Near/Mid/Far_0..1`, `Step_Snow_0..2`, `Torch_Out`, `Fog_Horn` (uzak sis düdüğü, ufuktaki Sis'in sesi).
**Önemli:** betiğin tek bir paylaşılan `rng`'si var (tohum 1848) ve sesleri sırayla üretiyor. Yeni sesler betiğin **sonuna**
eklenir ya da kendi `rng`'lerini kullanır; yoksa mevcut bütün sesler değişir. Sentez: yağmur = filtrelenmiş gürültü +
rastgele damla tıkları; gök gürültüsü = alçak geçirilmiş gürültü patlamaları, üstel sönüm ve yavaş genlik modülasyonu,
yakın olanda yüksek frekanslı çatırtı geçişi; kar adımı = kısa bant geçiren sıkışma gürültüsü.

---

## 6. Teknik yapı

### 6.1 Sınıflar (öneri; yeni klasör `Source/KillGodot/Weather/`)
| Sınıf | Nerede çalışır | Görev |
|---|---|---|
| `FKGWeatherRules` (saf) | Her yerde | Plan üreticisi, görüş/duyma/adım/iz çarpanları, şimşek ve esinti takvimi, erişilebilirlik sınırlayıcısı. Test edilen tek yer. |
| `AKGWeatherManager` | Sunucu yetkili, replike, `bAlwaysRelevant`, **spatially loaded değil**, `UKGSnapshotComponent` | Planı tutar (SaveGame), şimdiki durumu ve ilan edilmiş tahmini replike eder, ıslaklık/kar örtüsünü entegre eder, olayları deftere yazar |
| `AKGAtmosphereDirector` | Sadece istemci (ve listen server'ın yerel görünümü), replike değil | Güneş/ay, SkyLight, yükseklik sisi, bulutlar, PP, yıldız kubbesi, `MPC_KG_Weather`, `MPC_KG_Water.SwellScale`; faz görünümü + hava katmanı |
| `AKGWeatherFX` | Sadece yerel oyuncu | Kamera-yerel yağmur/kar kutuları, tıkanma haritası yakalayıcısı, ortam sesi katmanları |
| `AKGShelterVolume` | Her yerde (sunucu kuralı + istemci görseli) | İç mekân / yeraltı / çatı altı |
| `UKGAtmosphereProfile` (DataAsset ya da C++ tablo) | Her yerde | Faz görünümleri (bugün `KGGameState.cpp` `GetLook` içinde) + hava katmanları + kademe haritası. **Tek kaynak:** render turu da bunu okur |

`AKGGameState::UpdatePhaseLighting` bugün güneşi ve SkyLight'ı kendisi karıştırıyor. Değişiklik: bu fonksiyon
`AKGAtmosphereDirector`'a devredilir (tek satırlık yönlendirme); `GetLook` tablosu profile taşınır. `kg_capture_v2.py`'deki
`NIGHT` sabiti (bugün `GetLook`'un elle kopyası) da profilden okunur.

### 6.2 Niagara mı, mesh mi, materyal mi? (başsız üretim gerçeği)
| Yaklaşım | Başsız yazılabilir mi | Ne için | Karar |
|---|---|---|---|
| **Mesh + WPO materyal** (kamera-yerel kutu) | **Evet.** Mesh Blender'da betikle, materyal Python'la (`kg_make_ocean.py` kalıbı) | Yağmur, kar, yıldırım, sis kartları | **Birincil** |
| **Salt materyal** (yüzey fonksiyonu, PP) | Evet | Islaklık, birikinti, halka, kar örtüsü, uzak yağmur perdesi, gece grading'i | **Birincil** |
| **Niagara** | Kısmen. Python bir Niagara sisteminin modül yığınını güvenilir biçimde kuramaz; sadece var olan bir sistemi kopyalayıp kullanıcı parametrelerini ayarlayabilir | Derinlik çarpışmalı sıçrama (High) | **Opsiyonel spike**, 1 deneme; başarısızsa atlanır |
| **Instanced Static Mesh** (C++'ta dinamik) | Evet | Uzak sis kartları, yıldız kubbesi yerine | Gerekirse |

Mesh yaklaşımının artısı: tek draw call, CPU'da parçacık simülasyonu yok, determinizm kolay, editör kapalıyken üretilir.
Eksisi: çarpışma yok (tıkanma haritası ile çözülür).

### 6.3 Ağ: sunucu sahibidir
- **Replike edilen (`FKGWeatherNet`, push-model, Iris-hazır):** `Kind`, `PrevKind`, `BlendStartClock`, `BlendSeconds`,
  `IntensityCurveId`, `WindYaw` (uint16), `WindStrength` (uint8), `Wetness` (uint8, 1 Hz), `SnowCover` (uint8, 1 Hz),
  `bLightFog`, `Forecast[2]` (ilan edilen iki faz), `Season`. Değişiklik sadece geçişlerde ve 1 Hz entegrasyonda: saniyede
  birkaç bayt.
- **Şimşek ve esinti "habercide" gider:** gelecek şimşek zamanları istemciye önceden **gönderilmez** (tohum da gönderilmez).
  Sunucu, haberci anında (1.5 sn önce) `NextStrike {Serial, ClockTime, Pos, VariantId}` alanını replike eder; istemci
  parlamayı `ClockTime`'a göre oynatır. Ağ gecikmesi (30 Hz, ~100 ms) 1.5 sn habercinin içinde kalır. Esinti aynı şekilde
  3 sn önceden (`NextGust`). Böylece hile yapan istemci de bir sonraki şimşeği insandan önce bilemez (G7).
- **Kurallar sunucuda:** tanık filtresi, iz ömürleri, meşale sönmesi, duyma yarıçapları sunucudaki durumla hesaplanır.
  İstemcinin görseli bu durumun aynısından türediği için oyuncunun gördüğü ile kuralın kullandığı aynıdır.
- **Host migration:** plan, şimdiki durum, ıslaklık, kar örtüsü, şimşek/esinti seri numaraları `UPROPERTY(SaveGame)`;
  yöneticinin `UKGSnapshotComponent`'i var. Yeni host planı kaldığı yerden sürdürür.
- **Deniz:** `SwellScale` replike durum + maç saatinden türetilir, sunucu ve istemci aynı `FKGWaves` yüzeyini hesaplar.

### 6.4 Performans bütçeleri (1080p)
Bu bölüm `Docs/Process/Performance_Plan.md`'ye (SPRINT-042, paralel yazıldı) **uyar**, ona rakip bir bütçe açmaz:
- Kademeler, referans makine (RTX 5070 + Ryzen 7 7800X3D, charter S3.5 ile aynı) ve çevirme katsayıları oradan gelir:
  High (RTX 3060) GPU ×2.5, Medium (1660S/2060) ×3.5, Low (1050 Ti / Deck) ×9.5.
- **Mutlak kapı değişmez:** her preset, referans makinede Performance_Plan §1.1'in GPU bütçesini **Fırtına'da da** tutar
  (Low ≤ 1.5 ms, Medium ≤ 4.0, High ≤ 4.0, Epic ≤ 6.5). Hava bir bahane değildir.
- Ölçüm aracı SPRINT-042a'nın I5 kapısıdır (`kg_perf_gate.ps1` + `kg_perf_tour.py`). Bu iş tura **hava segmentleri**
  ekler (aynı noktalar, `kg.Weather.Set Storm 1` ve `Snow 1` ile), kendi ölçüm aracını yazmaz.

**Hava sistemleri (en ağır durum: Fırtına ya da Kar, Açık'a göre fark, aynı preset, referans makine, p95 GPU):**
| Parça | Low | Medium | High | Epic |
|---|---|---|---|---|
| Yağmur kutusu (katman, çizgi) | 1 katman, 800 kalın çizgi (%58–67 iç çözünürlükte ince çizgi titrer) | 1 katman, 2.500 | 2 katman, 5.000 + uzak perde | 2 katman, 7.000 + uzak perde |
| Tıkanma haritası | yok (sığınak hacmi) | yok (sığınak hacmi) | 256², 4 karede 1 | 512², 2 karede 1 |
| Islaklık / birikinti / halka | koyulaşma + roughness | + birikinti | + halka normal'i | + halka |
| Sıçrama | yok | yok | halka | halka (+ Niagara spike başarılıysa) |
| Kar tanesi | 600 | 2.000 | 4.000 | 6.000 |
| Bulut geçişi | statik kubbe materyalinin parametreleri (042a'nın Low bulutu) | VolumetricCloud, düşük örnek | normal | normal |
| Hacimsel sis | kapalı | kapalı | kapalı | açık |
| Şimşek, sis yoğunluğu, gece ışığı | **aynı** (H7) | **aynı** | **aynı** | **aynı** |
| **Kapı: fark (referans)** | **≤ +0.10 ms** | **≤ +0.20 ms** | **≤ +0.30 ms** | **≤ +0.50 ms** |
| Hedef kademede karşılığı | ~1.0 ms (1050 Ti) | ~0.7 ms (2060) | ~0.75 ms (3060) | — |

Draw call: hava en fazla +6 (Performance_Plan §2.1 bütçeleri geçerli). CPU: tek bir yönetici aktörü 10 Hz tick
(Performance_Plan §2.2 tick bütçesine +1), sunucuda ≤ 0.05 ms; yerel FX aktörü sadece şiddet > 0 iken tick atar.
VRAM: hava materyalleri ve mesh'ler ≤ 24 MB (Low bütçesi 1.6 GB).

### 6.5 Mevcut grafik ayarlarına bağlama
Ayarlar menüsü (`SKGSettingsMenu.cpp`) bugün motorun 7 kalite grubunu Low/Medium/High/Epic olarak gösteriyor:
ViewDistance, Shadow, Texture, AntiAliasing, Foliage, VisualEffect (Effects), PostProcess. Yeni bir kalite satırı eklenmez.
`Config/DefaultScalability.ini` dosyasını **SPRINT-042a yazar** (Performance_Plan §5.1). Bu iş o dosyaya, 042a bittikten
sonra, sadece kendi satırlarını ekler:
- **sg.EffectsQuality** → yağmur/kar sayıları, tıkanma haritası, sıçrama, hacimsel sis (`kg.Wx.Quality` cvar'ı).
- **sg.ShadowQuality** → 042a'nın kaskad tablosunun üstüne contact shadow ve PCSS (§8.1).
- **sg.PostProcessQuality** → AO yöntemi (SSAO/GTAO) ve kalitesi (§8.1).
- **sg.FoliageQuality** → rüzgâr WPO'nun kapanma mesafesi (042a'nın yoğunluk/cull ölçeğine ek).
- Oyuncunun preset seçmesi hiçbir zaman sis yoğunluğunu, gece dolgu ışığını, local exposure'ı ya da şimşek aydınlığını
  değiştirmez (H7). Bunlar kalite gruplarının dışında, profil tarafından her kademede aynı değerle yazılır.

---

## 7. Erişilebilirlik: şimşek parlaması

- **Her zaman geçerli sınırlar (Tam modda bile):**
  - saniyede en fazla **3** parlama darbesi, bir dizi ≤ 0.4 sn, iki dizi arası ≥ 20 sn (`KG_WX_STRIKE_GAP` tabanı);
  - ekranın ortalama parlaklığı bir darbede en fazla **×3** (≈ +1.6 EV) artar; darbe tek karelik değil, ≥ 60 ms yükselir;
  - parlama rengi mavi-beyazdır, **doygun kırmızı parlama yoktur**.
  Bunlar WCAG 2.3.1 "üç parlama" eşiğinin mantığını izler. Sınırlayıcı `FKGWeatherRules` içinde saf bir fonksiyondur ve
  test edilir.
- **Ayar:** Ayarlar → Erişilebilirlik → **Şimşek parlaması: Tam / Azaltılmış / Kapalı** (`UKGGameUserSettings`'te saklanır).
  - **Azaltılmış:** darbe gücü %25, tek darbe, 200 ms yumuşak yükselme ve iniş.
  - **Kapalı:** ekran parlaması yok; gökte sadece yumuşak bulut içi ışıma.
- **Adalet:** parlama bir oyun bilgisi taşıdığı için (silüet), Azaltılmış ve Kapalı modda pencere süresince
  `KG_WX_FLASH_SIGHT` içindeki karakterlere mevcut kontur (`M_KG_PP_Outline`) ile ince bir silüet çizgisi verilir. Bilgi
  aynı, parlama yok.
- İlk açılışta bir fotosensitivite uyarı kartı (tek satır + ayara kısayol) gösterilir.
- Gök gürültüsünün ses şiddeti Efektler ses kanalına bağlıdır; yakın gürlemede tepe değer −3 dBFS ile sınırlanır.

---

## 8. Grafik kalite geçişi: gölgeler, ışık, pozlama ("bir tık")

Bugünkü taban (`Config/DefaultEngine.ini`, `05_Tech_Architecture.md` §8): deferred, **Lumen kapalı**
(`r.DynamicGlobalIlluminationMethod=0`), **VSM kapalı** (CSM), SSR (`r.ReflectionMethod=2`), **mesh distance field kapalı**
(`r.GenerateMeshDistanceFields=False`: DFAO ve distance field gölgeler yok), TSR, otomatik pozlama kapalı, sabit pozlama
+0.4, doygunluk 1.12, tek güneş/ay + gerçek zamanlı yakalanan SkyLight. Su materyali SPRINT-022'nin işi.

**Bütçe gerçeği:** Performance_Plan §3'e göre bugün Epic referans makinede 8.4–9.8 ms GPU harcıyor, bütçe 6.5 ms. Yani
kalite geçişi "daha çok efekt" olamaz. Sıra şu: önce SPRINT-042a presetleri gerçek ayarlara bağlar ve pahalı geçişleri
keser; bu iş ondan **sonra** gelir ve kazanılan payın küçük bir kısmını, en çok görünen yere harcar: gölgenin yere
oturması (contact shadow, AO), gece ışığı ve grading. Pahalı olan (PCSS, 4. kaskad, hacimsel sis) sadece Epic'e gider.

### 8.1 Gölgeler
**VSM mi CSM mi?** CSM'de kalınır. Gerekçe: VSM Nanite olmadan pahalıdır ve bu köy WPO dolu (rüzgârlı yapraklar, çimen,
deniz, şimdi bayraklar ve fenerler); WPO her karede VSM sayfalarını geçersiz kılar, maliyet sallanan her yaprakla büyür.
Performance_Plan H5, küçük mesh'lerde Nanite'i kapatmayı öneriyor; VSM bu yönün tersidir. VSM ileride Epic için bir
**deney** olabilir, bu işte değil.

Kaskad sayısı, çözünürlük ve mesafe **Performance_Plan §5.1'in** (042a) değerleridir; bu iş onları değiştirmez, üstüne ucuz
ekler koyar:
| Ayar | Low | Medium | High | Epic |
|---|---|---|---|---|
| CSM (042a, §5.1): kaskad / çözünürlük / mesafe | 1 / 1024 / ~40 m | 2 / 1024 / ~60 m | 3 / 2048 / ~90 m | 3 / 2048 / ~120 m |
| Bu işin eki: kaskad dağılım üssü (yakın kaskad keskin) | 2.5 | 2.8 | 3.0 | 3.0 |
| Güneş kaynak açısı (yumuşak kenar, ücretsiz) | 0.5° | 0.8° | 1.0° | 1.0° |
| Filtre (`r.ShadowQuality`) | 2 | 3 | 4 | 5 |
| **Contact shadow** (güneş ve ay, uzunluk 0.03; ayak altı, saçak, eşik) | kapalı | kapalı | açık | açık |
| 4. kaskad, 150 m | — | — | — | açık |
| PCSS (`r.Shadow.FilterMethod=1`) | — | — | — | spike: ≤ 0.15 ms ise açık |
| Gölgeli yerel ışık | 0 | 0 | 0 | 2 (fener odası spot'u, en yakın ocak) |
| **Tahmini fark (referans makine)** | ~0 | ~+0.02 ms | ~+0.10 ms | ~+0.35 ms |

**AO:** DFAO yok (distance field kapalı; açmak içerik ve bellek maliyeti, bu işte değil). Performance_Plan §5.1 Low'da AO
kapalı, Medium'da düşük, High'da açık diyor; bu iş yöntemi seçer:
| | Low | Medium | High | Epic |
|---|---|---|---|---|
| Yöntem | kapalı | SSAO, 1 seviye, yarım çözünürlük | **GTAO** (`r.AmbientOcclusion.Method=1`), yarım çözünürlük (042a'nın GPU dökümü SSAO'dan pahalı çıkarırsa SSAO 2 seviye) | GTAO tam çözünürlük |
| Yoğunluk / yarıçap | — | 0.5 / 80 cm | 0.6 / 120 cm | 0.6 / 120 cm |
| **Tahmini fark (referans makine)** | 0 | ~+0.05 ms | ~+0.15 ms | ~+0.25 ms |

AO, stilize görünümde evlerin köşelerini, saçak altlarını ve sokak dipini "oturtur"; bugün her şey biraz havada duruyor.

### 8.2 Işık
- **Güneş / gök dengesi (faz profili, §9 tablosu):** güneş gündüz 9 lux'ta kalır, SkyLight gündüz 1.3 → 1.1 (AO ve contact
  shadow gelince gölgeler biraz daha okunur olsun). Güneşin atmosfer diski ve yükseklik sisinin yönlü inscattering'i açılır
  (güneşe bakınca sıcak hâle).
- **Sekme ışığı (GI) bütçe içinde:** Lumen kapalı kalır (3060'ta 1080p'de 2–4 ms ve distance field ister). Onun yerine
  ucuz sahte sekme:
  1. SkyLight **alt yarıküre rengi**: zeminin palet rengine yakın sıcak toprak rengi, böylece duvar dipleri mavi değil
     sıcak dolar;
  2. güneşin tersinden gelen, **gölgesiz** bir "sekme" yönlü ışığı: güneşin %6'sı, zemin rengi (~0.05 ms);
  3. gece için aynı şekilde gölgesiz, soğuk mavi bir "ay dolgusu" (0.12 lux) ayın ters yönünden (§9.2).
  Bu ışıklar **her kademede aynıdır** (H7: gece görünürlüğü oyun bilgisidir).
- **Yerel ışıklar:** fenerler, lambalar, ocaklar. Kurallar: gölgesiz varsayılan, `AttenuationRadius` ≤ 8 m (sokak lambası)
  / ≤ 5 m (ev içi), `MaxDrawDistance` 60–80 m (Performance_Plan §5.1 ve 042b ile aynı); görünür gölgesiz yerel ışık
  tavanı Low 6, Medium 12, High ve Epic 24 (ötesi emissive + sahte hâle kartı). Işık sayısı değil, gece görüşünü taşıyan **ışıklı alan** kademe ile değişmez: `KG_SIGHT_NIGHT_LIT`
  ışık yarıçapından oyun kuralıyla hesaplanır, çizilen ışıktan değil.
- **İç mekân ışığı:** her `"interior": true` ev, gece bir sıcak ocak/mum ışığı alır (gölgesiz, kamera 25 m içindeyse açık).
  SkyLight distance field olmadan içeri sızar (içerisi gündüz mavi-gri görünür): `AKGShelterVolume` aynı zamanda bir iç
  mekân PP hacmidir: pozlama +0.3, beyaz dengesi sıcak (−600 K), doygunluk 1.05, AO yoğunluğu +0.2. Böylece kapıdan girince
  "içerisi" hissi gelir.

### 8.3 Pozlama ve tonemapping
- **Pozlama sabit kalır** (düz stilize renkler için doğru karar). Ama faz başına bir **pozlama sapması** gelir (§9).
- **Local exposure** (UE 5.x, PP ayarı): gölge kontrastı 0.8, parlak kontrastı 0.9. Fener Burnu uçurumu gibi büyük karanlık
  yüzeyleri, genel görüntüyü soldurmadan kaldırır. Ucuz (~0.1 ms). Otomatik pozlama kapalıyken çalıştığı 040a'nın ilk
  adımında doğrulanır; çalışmıyorsa yerine faz başına "shadows gain" grading'i kullanılır. Her kademede açık (H7).
- **Film tonemapper** (UE varsayılanı ACES tabanlı): slope 0.88, toe 0.55, shoulder 0.26, white clip 0.04. Canlı paletin
  doygunluğunu parlak alanlarda korur, siyahı ezmez.
- **Gece grading'i** PP renk grading'i ile (ayrı LUT dosyası yok; faz profilinde sayılar): §9.

**Grafik geçişinin toplam kapısı** (açık gündüz, 042a sonrası yeni baseline'a göre fark, referans makine, p95 GPU, I5
turunun 7 noktası): Low ≤ +0.05 ms, Medium ≤ +0.15 ms, High ≤ +0.30 ms, Epic ≤ +0.60 ms. **Ve** her preset
Performance_Plan §1.1'in mutlak GPU bütçesinin içinde kalır. (Hedef kademede: Low ~0.5 ms, Medium ~0.5 ms, High ~0.75 ms.)
Bütçe tutmazsa sıra şudur: önce Epic ekleri, sonra High contact shadow, en son GTAO → SSAO. Gece ışığı (dolgu, local
exposure, grading) kesilmez: ucuzdur ve H7 gereği oyun bilgisidir.

### 8.4 Kademe adaleti testi
[T] `KillGodot.Weather.TierFairness`: her kademe (0–3) için profil uygulandıktan sonra yükseklik sisi yoğunluğu, gece
dolgu ışıkları, local exposure ayarları, şimşek darbe gücü ve `KG_SIGHT_*` çarpanları **aynı**dır.

---

## 9. Harita görünümü: faz grading'i, gece ışığı, gökyüzü ("bir tık daha iyi")

### 9.1 Faz başına görünüm (profil; bugünkü `GetLook` değerleri korunarak genişletilir)
| Faz | Güneş/ay (pitch, lux, renk) | SkyLight | Pozlama sapması | Beyaz dengesi | Doygunluk / kontrast | Sis rengi (inscattering) | Not |
|---|---|---|---|---|---|---|---|
| Şafak | −9°, 5 lux, pembe-turuncu | 0.8 | +0.3 | 5800 K | 1.10 / 1.00 | açık pembe (0.55, 0.45, 0.50) | Denizden kalkan hafif pus |
| Gündüz | −42°, 9 lux, sıcak beyaz | 1.1 | +0.4 (bugünkü) | 6500 K | 1.12 / 1.02 | açık mavi (0.55, 0.68, 0.85) | Bugünkü görünüm + AO + contact shadow |
| Toplantı / akşam | −16°, 7.5 lux, altın | 1.0 | +0.45 | 5200 K | 1.18 / 1.05 | sıcak altın (0.70, 0.55, 0.40) | Uzun gölgeler, altın saat |
| Gece | −22°, 0.9 lux, ay mavisi | **0.6** (bugün 0.35) | **+1.1** | 9000 K | 0.85 / 1.05, gölgelerde +0.02 mavi kaldırma | **lacivert (0.015, 0.02, 0.045)** | Yıldızlar, ay dolgusu, local exposure |

Hava katmanları bu tablonun üstüne **fark** olarak gelir: Yağmur (doygunluk ×0.85, kontrast ×0.95, −300 K), Fırtına
(doygunluk ×0.75, gündüz pozlama −0.5, bulut albedo 0.35), Sis (doygunluk ×0.9, sis rengi faz renginin açık tonu), Kar
(+0.2 pozlama, −500 K, gölgeler mavi).

### 9.2 Gece: Fener Burnu uçurumu ve gri pus (lore ajanının bulgusu, `Backlog.md` önerisi)
`Saved/Screenshots/V2/night_lighthouse.png` bugün iki sorun gösteriyor: uçurumun dikey yüzü tek parça simsiyah bir levha,
gökyüzü ise lacivert değil gri-beyaz bir pus.
**Nedenler (koddan):**
1. Yükseklik sisinin inscattering rengi sabit ve gündüz değerinde: gece de açık mavi-gri saçıyor → gri pus.
2. Gece SkyLight 0.35 ve gerçek zamanlı yakalanan gece göğü çok karanlık; aya bakmayan dikey yüzeylere neredeyse hiç ışık
   düşmüyor; AO da yok, doku da okunmuyor → simsiyah levha.
3. Hacimsel bulutlar ay ışığında açık gri, parlak kalıyor; atmosfer ay ışığını gündüz gibi saçıyor.
**Düzeltmeler:**
- Sis rengi ve yoğunluğu faz profilinden (gece lacivert, yoğunluk 0.012 → 0.008).
- SkyAtmosphere `SkyLuminanceFactor` gece (0.25, 0.30, 0.50): atmosfer koyulaşır ama mavi kalır.
- **Yıldız kubbesi:** büyük, içe dönük bir küre mesh'i, `M_KG_NightSky` (emissive, yıldız noktaları, yavaş parıltı, Samanyolu
  bandı stilize), `NightAmount` ile açılır, bulutların arkasında kalır. Başsız üretilir.
- **Ay:** güneş diski gece ay diski olur (daha küçük, soğuk, hâlesi az), ay denize parlak bir yol çizer (SPRINT-022 su
  materyali speküleri; burada sadece ay yönü ve rengi).
- **Ay dolgusu** (gölgesiz, 0.12 lux, ayın ters yönü) + SkyLight gece 0.6 + local exposure: uçurum yüzü koyu ama dokusu ve
  kaya katmanları okunur.
- Bulutlar gece albedo ×0.4, ay tarafında gümüş kenar.
- Fener hüzmesi uçurumun üst kenarını tarar (mevcut fener ışığının yönü; yeni ışık yok).
**Ölçülebilir kabul ([R], 040a):** `night_lighthouse` render'ında
- uçurum bölgesinin (ROI, sabit piksel dikdörtgeni) ortalama sRGB parlaklığı 0.06–0.20 ve standart sapması ≥ 0.015
  (düz levha değil, doku okunuyor);
- gökyüzü bölgesinin (üst %20) ortalama parlaklığı ≤ 0.12 ve mavi kanalı kırmızı kanaldan ≥ %25 yüksek (gri değil
  lacivert);
- açık gece gökyüzü ROI'sinde ≥ 30 yıldız noktası (yerel tepe sayımı).
Aynı ölçüm `night_square`, `night_harbour`, `night_town` için rapora yazılır.

### 9.3 Gökyüzü (gündüz)
- Bulut katmanı rüzgârla akar (bugün duruyor gibi); Açık'ta kümülüs, Rüzgârlı'da daha parçalı ve hızlı.
- Şafak ve akşamda güneşe doğru sıcak Mie hâlesi (`mie_scattering_scale` faz profilinden: şafak/akşam 0.010, gündüz 0.006).
- Martı sürüsü (`V2/Sky/Gulls`) Fırtına ve Sis'te gizlenir, Yağmur'dan sonra geri döner (küçük ama "canlı köy" hissi).

---

## 10. Storm Manor (017/018) ile ilişki
- Storm Manor kalıcı bir **Fırtına profili** kullanır: plan üreticisi o harita için "Fırtına, şiddet 0.6–1.0" sabit planı ve
  aynı şimşek ritmini (`KG_WX_STRIKE_GAP`, haberci, erişilebilirlik) çıkarır. Şimşek pencereden içeri düşen ay ışığı darbesiyle
  gelir, iç mekân kurudur (`AKGShelterVolume` tüm iç hacim). Elektrik kesintisi sabotajı 026b modelinde kalır.
- Yağmur sesi katmanları (açık / çatı / iç) Storm Manor'un pencere ve koridorlarında aynıdır.

---

## 11. Riskler ve kapsam dışı
| Risk | Önlem |
|---|---|
| Fırtına + gürültü maskesi cinayet tanığını çok düşürür (S2.1) | Kırpma tabanı 0.30, Fırtına maçta ≤ 1, soak'ta hava başına rapor. Gerekirse `KG_WX_THUNDER_MASK` 0.5 → 0.7 (tek sayı) |
| Kar delili çok güçlü (taş sokak kaçışı kapanır) | Kar ≤ 2 ardışık faz, sadece Kış (%20), iz havuzu 256, kar örtüsü erir |
| Tıkanma haritası maliyeti | Sadece High/Epic; High 256², 4 karede bir; bütçe kapısında ayrı satır |
| Local exposure sabit pozlamayla çalışmazsa | Grading "shadows gain" yedeği (§8.3) |
| Performans: Epic bugün bütçenin üstünde (Performance_Plan §3) | Bu iş 042a'dan sonra gelir; mutlak preset bütçeleri Fırtına'da da geçerli; kesme sırası §8.3 |
| Materyal değişiklikleri SPRINT-022 ile çakışır | Su materyali 022'nin; bu iş sadece `MPC_KG_Water.SwellScale` ve `WhitecapAmount` parametresini sürer, 022 bittikten sonra |
| Niagara başsız yazılamaz | Mesh + materyal birincil; Niagara 1 denemelik spike |

**Kapsam dışı (öneri olarak Backlog'a):** oyuncu şemsiyesi, soğuk/ısınma durumu, yıldırımın ağaca düşüp yangın çıkarması,
gökkuşağı, mevsime göre farklı köy dekoru, hava durumunu değiştiren rol yetenekleri (ör. "Havacı" rolü), balık tutmada hava
etkisi.

---

## 12. Onay listesi (kullanıcı için)
- [ ] **W1** Hava planı ve olasılık tablosu (§3.2), 1. gün hep sakin, Fırtına ≤ 1.
- [ ] **W2** Kış mevsimi lobi seçeneği ve Otomatik'te %20 Kış.
- [ ] **W3** Sis ↔ fener kancası (§3.4) ve orman belgesiyle ortak fener durumu.
- [ ] **W4** Kamp ateşi yakıtı ve meşale ömrü hava çarpanları (§4.5), orman belgesi sahibiyle.
- [ ] **W5** Şimşek silüeti ve tanımlama kuralı (§4.4); S2.2 hava başına rapor.
- [ ] **W6** Kar'da taş sokağın da iz tutması (§4.6).
- [ ] **W7** Grafik geçişi: CSM'de kalma, GTAO, sahte sekme, faz grading'i (§8–§9).
- [ ] **W8** Sprint sırası: 040a (görünüm) → 040b (hava çekirdeği) → 040c (şimşek, kar, delil).
- [ ] **W9** Hava bir Dünya Olayıdır (H1); R8'in cevabı "hava önce, diğer Dünya Olayları M12'de" olarak yazılır.
