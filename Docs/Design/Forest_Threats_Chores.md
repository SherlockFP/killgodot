# Orman: Tehditler ve Orman Görevleri (Kurtlar, Sis, Kamp Nöbeti)

> **Durum: Önerildi (kullanıcı onayı gerekiyor), 2026-09-25.** Sadece tasarım. Kod, seviye ve mevcut belgeler
> değişmedi. Charter §0.1 (kapsam kuralı) gereği bu yeni bir sistemdir: onaylanırsa `Docs/Backlog.md` "Proposed (needs
> approval)" bölümüne ve oradan sprint kuyruğuna girer. Sprint önerileri §14'te, LoopContract biçiminde.
> **Kaynak:** kullanıcının 2026-09-25 isteği: "Ormanda çok derine gidersen kurtlar saldırabilir ya da gizemli bir
> sis peşine düşer, yakalarsa ölürsün. Orman görevleri: odun topla, ateş yak, kamp kur, bir gece hayatta kal."
> Kullanıcı dengeyi ve içeriği bize bıraktı ("yüzeysel söyledim, sen düşün").
> **Neye göre:** `KillGo_Pillars.md` v1.1 (hedef kodları, sabitler §0.3, 12 maddelik liste), `Pillar_Audit.md`,
> `Pillar_Roadmap.md` (sprint kimlikleri), `Docs/Lore/KillGo_Lore.md`, `01_GDD_Core.md`, `SPRINT-016-PhysicalChores.md`.
> **Kapsam dışı:** kırılabilir sandıklar, bandaj ve silah parçaları ile satranç/dama ayrı tasarımlardır (paralel
> ajanlar). Burada sadece bağlandıkları yerler anılır.
> **Numara uyarısı:** Roadmap'teki "SPRINT-022/023" (kazanma çözücüsü, bot zihni) ile `Docs/Iterations/`teki gerçek
> SPRINT-022 (v2 görsel geçiş) ve SPRINT-023 (ses, sosyal) farklı işlerdir. Bu belge roadmap işlerini **"Roadmap 0xx"**,
> dosyadaki sprintleri **"SPRINT-0xx"** diye yazar.

---

## 0. Özet (tek paragraf)

Morrowmere'i saran orman halkası üç **banda** (Kıyı, Orta, Derin) ve dört **sektöre** ayrılır. Patikalar, ışık ve
kalabalık güvenlidir. Işıktan uzakta ve **yalnız** kalan oyuncuyu önce **kurtlar** fark eder (uluma → parlayan gözler →
ısırık, arada en az 20 sn), Derin bantta uzun kalırsa **Sis** (Beklenmeyenler'in sisi) peşine düşer ve yakalarsa öldürür.
İkisi de **asla rastgele değildir**, taraf ayırt etmez ve her aşaması görülür ya da duyulur. Orman, köye ödül getiren
dört yeni **fiziksel görev** (SPRINT-016 çerçevesi) ve riskli, yüksek değerli **Kamp Nöbeti** ile doludur: 2–4 kişi
geceyi kampta ateşi söndürmeden geçirirse Hazırlık barı büyük bir parça alır. Çıkarım açısından en önemli şey:
kurt ölümü ile cinayet **ayırt edilebilir** (uluma kaydı, ısırık/bıçak yarası, pati izi, sürükleme) ve Sabırsız bir
cinayeti kurt saldırısı gibi **sahneleyebilir** ya da birini sise **yalnız bırakabilir**. Her şey Olay Defteri'ne
(Roadmap 020b) düşer.

---

## 1. Oyuncu deneyimi

### 1.1 Dört an
1. **Gündüz, 2. gün.** Elif'in listesinde "Kuru dal topla" var. Dal yığınları patikadan 15–25 m içeride, ağaçların
   arasında. Batı Koru'dan tek bir uluma geliyor, bütün köy duyuyor. Elif yalnız. Ya patikaya döner ya da işi
   bitirmek için 10 saniye daha kalır. Kalır. Ağaçların arasında iki çift göz beliriyor. Koşar, patikaya varır, kurtlar
   sınırda durur. Toplantıda "Batı Koru'daydım, uluma beni kovaladı" der. Uluma herkesin defterinde var: alibisi tutar.
2. **Alacakaranlık.** Toplantı biter. Kamp defterine sabah iki isim yazılmış: Bora ve Kaan. İkisi meşalelerle kuzey
   ormanına yürür. Köyden, tepede kamp ateşinin yandığı görülür.
3. **Gece.** Kampta ateş söndükçe kurtların gözleri yaklaşıyor. Biri oduna gidiyor, diğeri ateşi besliyor. Gece
   ortasında ateş sönüyor. Köyden bakan Zangoç kuzeydeki ışığın söndüğünü görüyor.
4. **Şafak.** Bora kampta tek başına. Kaan'ın cesedi kampın 20 m dışında, ısırık yaralarıyla. Toplantıda Bora "kurtlar"
   diyor. Ama Olay Defteri'nde o gece Kuzey Koru'dan **uluma yok**, cesedin çevresinde **pati izi yok** ve ateşin
   külleri **ıslak** (söndürülmüş, kendiliğinden bitmemiş). Adli Tabip: "Önce bıçak, sonra ısırık." Köy oy verir.

### 1.2 Tasarım ilkeleri (bu belgenin kuralları)
| # | İlke | Somut kural |
|---|---|---|
| P1 | **Önceden görülmeyen ölüm yok** | Bir oyuncu kurt hasarı almadan önce **o oyuncu için** ≥ 20 sn, Sis ölümünden önce ≥ 25 sn telgraf vardır. Sunucu bu kuralı zorlar ([T], F2). |
| P2 | **Tehdit taraf-kördür** | Kurtlar ve Sis herkese aynı davranır: Sabırsız, Kurtadam ve nötr dahil. Aksi hâlde tehdit bir rol dedektörü olur (S2.4 ihlali). |
| P3 | **Güvenli olan her zaman görünürdür** | Patika, ışık (ateş, meşale, yol feneri) ve kalabalık. Her yürünebilir orman noktasından en yakın güvenli hücre ≤ 45 m ([L], F9). |
| P4 | **Rastgelelik kurulumda, sonuçta değil** | İn yerleri tohumdan seçilir ve **şafakta** duyurulur (G4). Kurt hedef seçimi, ısırık hasarı ve Sis hızı deterministiktir: yazı-tura yok. |
| P5 | **Risk ödüllendirilir** | Orta/Derin bantta görevler, şifalı ot, reçine, sandıklar ve gözcü noktaları vardır. Nöbet, Hazırlık barının en büyük tek parçasıdır. |
| P6 | **Her şey deftere düşer** | Uluma, göz, saldırı, Sis, ateş, nöbet: hepsi Olay Defteri olayıdır (§8.1). Defter yoksa bu özellik maçta açılmaz (§14). |
| P7 | **Kurtlar köye girmez** | Teras poligonlarına (köy) hiçbir koşulda girmezler. Köyde ısırık = Kurtadam ya da sahne. |

---

## 2. Orman: coğrafya, bantlar, sınır

### 2.1 Mevcut zemin (v2)
- Oynanabilir alan: `nav_bounds` x −120…120, y −120…112 m (`Tools/Level/morrowmere_layout_v2.json`).
- Orman halkası: havza merkezinden r ≈ 95 m'nin ötesi, zemin 7 m'den 40–55 m'ye yükselir; dışta dağ halkası
  (`Docs/Level/v2_iconic.md`). Koruluklar ve dış çam halkası: 2.189 örnek (`v2_Build_Report.md`).
- Orman patikaları: `woods_path` (demirhane → oduncu açıklığı → değirmen gölü → Hollow Way), `hollow_way` (ormandan
  Taç Tepesi batı kapısına gömük yol), `mill_lane`, `orchard_lane`.
- Hazır ilgi noktaları (`Tools/Unreal/dressing/v2/dress_wilds.py`): **Avcı Kampı** (çadır, kamp ateşi ışığı, kütük
  oturaklar, Hunter's Chest; N1 kuzey ve Wrim batı sırtı), **Taş Çember** (7 menhir, adak taşı; N2 ve Erim),
  **Mağara Ağzı** (fener, kaçakçı sandığı; Nrim), **Kömürcü Açıklığı** (N1), **Gözcü Molası** (Erim, N2).
  Yerleri tohumlu arama ile bulunur (`C.stats["pois"]`): **orman verisi konumları koda gömmez, veriden okur** (§13).
- Oduncu: `ChopWood` görevi (−98.4, −2.4), dünya görevi `Firewood` (`chop_block` → `inn_woodbox`).

### 2.2 Bantlar (patikaya uzaklıkla, veri olarak)
Bant, 2 m ızgaralı bir **orman raster'ı**ndan okunur (`Tools/Level/morrowmere_forest_v2.json` önerisi; 120 × 116
hücre, hücre başına 1 bayt bant + 1 bayt sektör, ~28 KB). Güvenli hücreler: köy lane'leri, orman patikaları, yeni
**iz patikaları** (sadece veri: fener direkleri ve ağaç işaretleriyle, mesh yolu yok) ve ışık yarıçapları.

| Bant | Tanım | Gündüz | Gece |
|---|---|---|---|
| **Köy** | Teras poligonları (`zones`) | Güvenli. Kurt ve Sis giremez. | Güvenli (tehditler açısından). |
| **Kıyı** | Güvenli hücreye ≤ `KG_FOREST_EDGE` (12 m) | Güvenli. Kurt patikaya çıkmaz. | Kurtlar patikada yürüyebilir, **ısırmaz**. Sis giremez. |
| **Orta** | 12–30 m | Kurt ilgisi yok (4. günden sonra hafif). | Kurt ilgisi. 3. geceden sonra Sis de. |
| **Derin** | > `KG_FOREST_DEEP` (30 m) ya da iz patikasının dış tarafı | Kurt ve Sis ilgisi (yavaş). | Kurt ve Sis ilgisi (hızlı). |

### 2.3 Sektörler ve oyuncu sayısı
| Sektör | İçerik | Açılma | Risk katmanı (031a) |
|---|---|---|---|
| **Batı Koru** (Brookside ormanı) | Oduncu açıklığı, `woods_path`, değirmen gölü, Hollow Way | Her N | Kıyı: orta; Orta/Derin: tehlikeli |
| **Kuzey Koru** (Taç Tepesi'nin arkası) | Avcı Kampı (N1, **nöbet kampı**), Kömürcü, Taş Çember (N2) | Her N | tehlikeli |
| **Mağara Sırtı** (kuzey dağ eteği) | Mağara Ağzı, kaçakçı sandığı | N ≥ 10 | tehlikeli |
| **Doğu Sırtı** (Meyve Yaylası'nın arkası) | Gözcü Molası, ikinci Taş Çember, ikinci kamp (Wrim yerine) | N ≥ 15 | tehlikeli |

Not: GDD §3 "Orman"ı sadece N 15–20'de açıyor. Bu belge Batı ve Kuzey Koru'yu her N'de açmayı **önerir** (oduncu ve
nöbet kampı küçük lobilerde de olsun diye). Onay listesinde (§17, O3).

### 2.4 Sınır: kaybolmayı önlemek
1. **Fiziksel:** dışta dağ halkası (zemin 40–55 m), `nav_bounds` dışı navmesh yok.
2. **Sis Duvarı:** oynanabilir ormanın dış kenarında ve kapalı sektörlerin girişinde duran, yavaş dalgalanan sis
   perdesi. GDD §3'ün "sis duvarı" bölge kapısı ile **aynı nesnedir** (Roadmap 031a bunu kullanır). Duvara giren
   oyuncu **ölmez**: 2 sn beyaz ekran, sonra **en yakın güvenli hücreye**, köye dönük olarak geri çıkar. 60 sn **Islak**
   (damlayan su, görünür ve duyulur; bir iz, §8.3) ve stamina 0. Lore: "Sise giren, geldiği yere geri çıkar."
3. **Işıklı iz:** her iz patikasında 15 m'de bir **yol feneri** (gece yakılır, Yol Fenerleri görevi) ve 20 m'de bir
   ağaç işareti (boya bandı). Patikadan 12 m'ye kadar görünür.
4. **HUD:** Orta/Derin bantta pusulada **"Köy" ve en yakın "Patika"** okları sürekli görünür. Harita orman patikalarını
   ve kampları çizer. İlk girişte bir kez: "Derin Orman. Yalnız kalma, ışıktan ayrılma."
5. **Veri kuralı ([L], F9):** her yürünebilir orman hücresinden en yakın güvenli hücreye ≤ `KG_FOREST_SAFE_MAX` (45 m).
   Bu kural sağlanmazsa veri reddedilir. Sis'in kaçılabilirliği (§5.3) bu sayıya dayanır.
6. **AFK:** Orta/Derin bantta 60 sn hareketsiz oyuncu Sis Duvarı gibi güvenli hücreye geri çıkarılır (ölmez). Böylece
   Sabırsız, AFK birini ormana "park edip" tehditlere yediremez.

---

## 3. Sabitler (tek kaynak)

Charter §0.3 biçiminde. Kodda ve kabul satırlarında **isimle** anılır.

| Sabit | Değer | Anlamı |
|---|---|---|
| `KG_FOREST_EDGE` | 12 m | Güvenli hücreye bu kadar yakın = Kıyı bandı |
| `KG_FOREST_DEEP` | 30 m | Bundan uzak = Derin bant |
| `KG_FOREST_SAFE_MAX` | 45 m | Herhangi bir yürünebilir orman noktasından en yakın güvenli hücreye en fazla |
| `KG_GROUP_R` | 8 m, LOS (= `KG_IDENT_RANGE`) | "Grupta": bu yarıçapta başka canlı oyuncu var (charter "yalnız" tanımının tersi) |
| `KG_FIRE_SAFE` | 10 m (yakıt < 20 iken 6 m) | Kamp ateşi güvenli yarıçapı: kurt saldırmaz, Sis girmez |
| `KG_TORCH_SAFE` | 6 m | Elde yanan meşale: gece bu yarıçapta kurt ısırmaz, Sis girmez |
| `KG_LANTERN_SAFE` | 4 m | Yanan yol feneri |
| `KG_HEAR_HOWL` | 160 m | Uluma: pratikte bütün köy duyar |
| `KG_HEAR_GROWL` | 20 m | Hırlama (gözler aşaması) |
| `KG_HEAR_SNARL` | 30 m (= `KG_HEAR_SCREAM`) | Saldırı hırıltısı + kurbanın çığlığı |
| `KG_HEAR_MAUL` | 12 m | Sahneleme (Parçala) sesi |
| `KG_WOLF_HOWL_MIN` | 12 sn | Uluma aşamasından gözler aşamasına en az |
| `KG_WOLF_EYES_MIN` | 8 sn | Gözler aşamasından ilk ısırığa en az |
| `KG_WOLF_TELEGRAPH_MIN` | 20 sn | Bir oyuncuya ilk ısırık, **o oyuncu için** atılan ulumadan en az bu kadar sonra |
| `KG_WOLF_BITE` | 20 hasar gündüz / 25 gece; aynı hedefe 4 sn'de en fazla 1; 0.4 sn sendeleme, −15 stamina | Isırık |
| `KG_WOLF_SPEED` | sinsi 2.2, tırıs 4.0, atak 6.4 m/s (en fazla 4 sn), sonra 4.0 | Oyuncu: yürüme 3.2, koşu 5.8 (stamina 100, −18/sn ≈ 5.5 sn ≈ 32 m) |
| `KG_WOLF_REPEL` | vuruş/itme: o kurt 15 sn kaçar, sürünün o oyuncuya ilgisi −40 | Karşı koymak |
| `KG_MIST_NOTICE` | gündüz 40 sn / gece 20 sn (yalnız, Derin) | Sis dikkati dolma süresi |
| `KG_MIST_SPAWN` | hedefin 40 m arkası, **en yakın güvenli hücrenin ters yönü**, görüş konisi dışı | Sis dili doğduğu yer |
| `KG_MIST_SPEED` | 2.4 m/s başlar, +0.08 m/s², en fazla 4.2 m/s | Yürüyen oyuncu 10 sn sonra geçilir, koşan hiç |
| `KG_MIST_CORE` | 3 m yarıçap; içinde 3 sn = ölüm (çıkınca sıfırlanır) | Donma |
| `KG_MIST_TELEGRAPH_MIN` | 25 sn | İlk görünür Sis işaretinden (kırağı) ölüme en az |
| `KG_MIST_CREEP` | gece başına 15 m, en fazla 2 adım | Sis Duvarı'nın ilerlemesi (§5.5) |
| `KG_WET` | 60 sn | Sis Duvarı'ndan dönüşte Islak durumu |
| `KG_TORCH_BURN` | 90 sn | Meşale ömrü |
| `KG_FIRE_FUEL` | 100 birim, −2.5/sn, kütük +50 | Kamp ateşi (dolu ateş 40 sn yanar) |
| `KG_VIGIL` | 2–4 kişi; varış: Gece başı + Gece'nin %55'i; ateş ≥ %90 yanık; kamp halkası 12 m | Kamp Nöbeti |

Faz süreleri (`KG_PHASE_DAY_S`, `KGGameMode.cpp:63`): N = 6'da Gündüz 186 / Gece 93 sn, N = 12'de 222 / 111, N = 20'de
270 / 135.

---

## 4. Kurtlar

### 4.1 Sürüler ve inler
| N | Açık sektör | Sürü × kurt | Aynı anda aktif kurt tavanı | İn adayı → seçilen |
|---|---|---|---|---|
| 6–9 | Batı + Kuzey | 1 × 2 | 2 | 3 → 1 |
| 10–14 | + Mağara Sırtı | 1 × 3 | 3 | 3 → 1 |
| 15–20 | + Doğu Sırtı | 2 × 3 | 6 | 4 → 2 (farklı sektörler) |

- İnler maç tohumundan (`FKGRng`) seçilir ve **1. şafakta Haberci Çocuk söyler**: "Oduncular Kuzey Koru'da kurt izi
  görmüş. Yarın kesin giderler." Böylece oyuncular tehlikenin nerede yoğunlaştığını baştan bilir (G4: kurulum
  rastgeleliği, önceden bildirilir).
- Sürü kendi sektöründe ve komşu sektörde avlanır. Oyuncu yoksa inde uyur (aktör uykuda, maliyet 0, §12).
- MVP'de kurtlar **öldürülemez**, sadece kovulur. PvE savaşı oyunun odağı değildir ve sürü sayısı sabit kalır.

### 4.2 İlgi metresi (sunucu, oyuncu başına, 0–100)
Sunucu her oyuncu için bir kurt ilgisi tutar. Değer **sadece sunucuda**dır. Oyuncu sayıyı görmez, **aşamayı** görür ve
duyar.

| Durum (4 Hz güncellenir) | Gündüz (2. gün+) | Gece |
|---|---|---|
| Köy, Kıyı, patika, herhangi bir ışık yarıçapı | −8/sn | −8/sn |
| Orta bant, yalnız | 0 (4. günden sonra +2) | +5 |
| Derin bant, yalnız | +3 | +9 |
| Grupta, 2 kişi | kazanç ×0 | kazanç ×0.5 |
| Grupta, ≥ 3 kişi | kazanç ×0 | kazanç ×0 (sadece çevrede dolaşırlar) |
| Elde yanan meşale | kazanç ×0.5 | kazanç ×0.25 ve 6 m içinde ısırık yok |
| Çiğ balık/et taşıyor (envanter ya da elde) | ×1.5 | ×1.5 |
| Can < 50 | ×1.25 | ×1.25 |
| Patikaya doğru olmayan koşu (orman içinde) | +3 | +3 |
| Kazanç yokken sönüm | −3/sn | −3/sn |

**Eşikler ve aşamalar** (histerezisli):

| Aşama | Giriş | Ne olur | Çıkış |
|---|---|---|---|
| 0 Sessiz | — | Kuş sesleri, rüzgâr | — |
| 1 **Uluma** | ilgi ≥ 35 | Sektörden uluma (`KG_HEAR_HOWL`, yönlü). Sürü o oyuncuya doğru tırısa kalkar. Sektör başına 45 sn'de en fazla 1 uluma; aynı sektörde ikinci bir oyuncu için ayrı uluma gerekmez ama **o oyuncuya ait telgraf** olarak kaydedilir. | ilgi < 20 |
| 2 **Gözler** | ilgi ≥ 65 **ve** Uluma'dan ≥ 12 sn | 2 kurt 15–22 m'de görünür, çevresinde döner. Gece gözler parlar (25 m'den görünür). Hırlama `KG_HEAR_GROWL`. | ilgi < 35 |
| 3 **Saldırı** | ilgi = 100 **ve** Gözler'den ≥ 8 sn | Kurtlar sırayla atılır (≤ 6 m'den). Isırık: `KG_WOLF_BITE`. Isıran kurt 3 sn geri çekilir (vur-kaç). | ilgi < 65 |

**Zamanlama örnekleri** (yalnız, meşalesiz, ışıksız, oyuncu hiç kıpırdamasa):

| Durum | İlk uluma | Gözler | İlk ısırık | 100 candan ölüm (en erken) |
|---|---|---|---|---|
| Derin, gece | 3.9 sn | 15.9 sn | 23.9 sn | 4 ısırık × 4 sn → ~36 sn |
| Orta, gece | 7 sn | 19 sn | 27 sn | ~39 sn |
| Derin, gündüz | 11.7 sn | 23.7 sn | 33.3 sn | 5 ısırık → ~49 sn |

Yani ölmek için ≥ 36 sn boyunca uluma, göz ve ısırıkları görmezden gelmek gerekir. Bu P1'in sayısal hâlidir.

### 4.3 Hedef seçimi ve hareket (deterministik)
- Sürü, ilgisi en yüksek oyuncuyu hedefler. Eşitlikte en yakın, sonra `PlayerKey` sırası. Yazı-tura yok.
- 2 kişilik grupta gece saldırı olursa, grup merkezinden **uzakta** duranı hedefler (kenardan avlanma). 2 kişi 3 m'den
  yakın durursa ikisi birden "Grupta" sayılır ve ilgi ×0.5 kalır. Birbirine sırt veren iki kişi en güvenlisidir.
- Kurtlar navmesh'te bir **Orman nav alanı** filtresiyle yürür: teras poligonları ve ışık yarıçapları geçilmez
  (nav modifier), gündüz lane yüzeyi geçilmez.
- Koşan oyuncuyu kovalarlar: 6.4 m/s ile en fazla 4 sn, sonra 4.0. İlk gözleri görünce (15–22 m) koşan oyuncu,
  ≤ 32 m'deki patikaya **her zaman** varır. İlk ısırıktan sonra koşan en fazla bir ısırık daha yer.

### 4.4 Karşı hamleler (hepsi herkese açık)
| Karşı hamle | Etki | Bedeli |
|---|---|---|
| **Patikaya dön** | İlgi −8/sn. Gündüz kurtlar patikaya çıkmaz; gece çıkar ama ısırmaz. | Görev yarım kalır |
| **Grup** | 2 kişi: gündüz saldırı yok, gece yarı hız. ≥ 3 kişi: hiç saldırı yok. | Birlikte gidilen kişi katil olabilir |
| **Meşale** | Gece 6 m içinde ısırık yok, ilgi ×0.25. | 90 sn yanar. Uzaktan görünürsün (gece 120 m'den bir ışık). Günde kişi başı 1 (kamp meşale kutusu) ya da her ateşten yeniden yakılır. |
| **Ateş / yol feneri** | Yarıçapı içinde saldırı yok, Sis giremez. | Sabittir, yakıt ister, söndürülebilir |
| **Karşı koy** | Atılan kurda yumruk ya da itme: o kurt 15 sn kaçar, ilgi −40. | İtme 25 stamina. Yanlış anda vurursan ısırık yersin. |
| **Bağır** | SPRINT-023'ün "Yardım!" / "Buraya!" sesli komutu (ses olmadan da çalışır, G1.1): 10 m içindeki kurtlar 3 sn duraksar, ilgi −20. | 30 sn'de 1. Bağırış 40 m'den duyulur: yerini söyler. |
| **Bandaj / şifalı ot** | Isırıkların canını geri verir, aksamayı durdurur. | Envanter yeri. Bandajlı kol görünür (bir iz). |

### 4.5 Kurtların yapamadıkları (kurallar, deftere de yazılır)
- Köy teraslarına girmek (P7).
- Gündüz bir patikada ya da gece bir ışık yarıçapında ısırmak.
- 1. gün ve 1. gece hiç ısırmak (sadece 1. günün son 30 sn'sinde tanıtım uluması).
- Toplantı, Mahkeme ve Epilog'da hareket etmek (S9.1: bu fazlarda hiçbir ölüm yok; kurtlar ine döner).
- Bir cesedi yemek. Kurt ölümü stilize kalır: talaş, yırtık kıyafet.
- Sis dili bir oyuncuyu kovalarken ona saldırmak: kurtlar sisten kaçar ve kaçarken bir kez ulur (ek telgraf).

### 4.6 Kurt öldürdüğünde
- Ceset **3–6 m ine doğru sürüklenir** (her zaman). Yumuşak orman zemininde pati izi ve sürükleme izi kalır (026a).
- Yara sınıfı: **Isırık**. Kurban çığlığı ve saldırı hırıltısı `KG_HEAR_SNARL` içinde duyulur.
- Katil alanı boştur (PvE). Kazanma hesabında ölüm ölümdür; bu yüzden F1 (PvE payı tavanı) önemlidir.

---

## 5. Sis (Beklenmeyenler'in sisi)

Lore: ufuktaki Sis hiç kalkmaz. İçinde **Beklenmeyenler** yaşar, sise yürüyenlerin boşluktan silüetleri
(`KillGo_Lore.md` §4). "Fener asla sönmemeli. Sönerse sis içeri girer, sisin içinde yaşayan da." İhtiyar Tuz: "Sis bir
duvar değil evlat, bir ayna." Oyundaki Sis iki parçadır: **Sis Duvarı** (sınır, öldürmez, §2.4) ve **Sis Dili**
(yalnızları kovalar, öldürür).

### 5.1 Sis dikkati (sunucu, oyuncu başına, 0–100)
- Dolar: oyuncu **yalnız** (charter §0.4) **ve** Derin banttaysa **ve** hiçbir ışık yarıçapında değilse. Hız: gündüz
  100/40 sn, gece 100/20 sn. 3. geceden itibaren gece Orta bantta da 100/45 sn.
- Boşalır: koşullardan biri bozulunca −10/sn. 30 sn güvenli kalınca sıfırlanır.
- Kurt ilgisinden ayrıdır. Sis dili doğunca kurtlar o oyuncudan çekilir (§4.5): bir oyuncuya aynı anda tek tehdit.

### 5.2 Aşamalar (telgraf)
| Dikkat | Oyuncunun gördüğü / duyduğu | Başkasının gördüğü |
|---|---|---|
| ≥ 50 | Ekran kenarında **kırağı**, kuşlar susar, nefes buharı. Pusula iğnesi titrer (saatler durur motifi). | Oyuncunun nefesi buharlaşır (yakından) |
| 100 → **Sis dili doğar** | Fısıltılar (yönlü), pusula döner, ilk kez olan oyuncuya tek satır: "Sis seni fark etti. Işığa ya da patikaya dön." | Yuvarlanan 6 m yüksekliğinde, 10 m genişliğinde sis kütlesi, içinde silüetler. Gündüz 40 m'den, gece 15 m'den görülür. |
| Çekirdeğe girdi | Görüntü beyazlar, hareket ×0.5, 3 sn'lik halka | Kütlenin içinde bir figür |
| 3 sn çekirdekte | Ölüm: "Sise karıştın." | Figür siste kaybolur |

### 5.3 Sis dili hareketi ve kaçış (sayısal kontrol)
- `KG_MIST_SPAWN`: 40 m arkada, **en yakın güvenli hücrenin ters yönünde** doğar. Oyuncuyu güvenliğe doğru güder,
  asla önünü kesmez.
- Hedefin 3 sn önceki konumunu izler. Hız 2.4 m/s'den başlar, 0.08 m/s² artar, 4.2'de durur (22.5. saniye).
- Hemen yürüyerek kaçan oyuncu (3.2 m/s) 45 m'yi (`KG_FOREST_SAFE_MAX`) 14 sn'de alır. Sis bu sürede ~41 m ilerler;
  arada ~44 m kalır. **Yürüyerek kaçış her zaman yeter.** Koşan oyuncu (5.8 m/s) hiç yakalanmaz.
- Hiç kıpırdamayan oyuncu doğuştan ~12.7 sn sonra çekirdekte (37 m), 3 sn sonra ölür. Gece: kırağıdan (dikkat 50,
  10. sn) ölüme ~25.7 sn ≥ `KG_MIST_TELEGRAPH_MIN` (25 sn). Gündüz: ~35.7 sn.
- Hedef güvenliğe (Kıyı bandı, ışık, grup) varınca dil durur ve 6 sn'de dağılır. **Hedef değiştirmez.** Kovalanan
  birine yetişen ikinci oyuncu onu kurtarır ("Sis yalnızları sever").
- Tavan: harita genelinde aynı anda en fazla N 6–9: 1, 10–14: 2, 15–20: 3 dil. Tavan doluysa yeni dikkat 99'da bekler
  (ölüm yok, sadece gecikme).

### 5.4 Sis ölümü
- MVP: ceset yakalandığı yerde kalır, **kırağı** ile kaplı. Yara sınıfı: **Sis** (yara yok, buz gibi, gözler açık).
- Sonra (lore ile): ceset siste kaybolur ve **ertesi şafakta Uzun İskele'de** ıslak hâlde bulunur ("Sis onu geri
  verdi. Biraz ıslak, biraz geç."). 025b geldikten sonra Sabırsız bir cesedi Sis Duvarı'na taşırsa aynı kural işler:
  ceset iskeleye bıçak yarasıyla döner, yani sis cinayeti **saklamaz**, sadece yerini değiştirir.

### 5.5 Sis ilerlemesi ve fener bağı (tırmanma)
- Fener lambası o gün **fiziksel olarak** doldurulmadıysa (`FuelLighthouse` paneli ya da `LighthouseOil` dünya görevi,
  hangi tarafça yapıldığına bakılmaz; SPRINT-016'da dünya etkisi gerçekte ve sahtede aynıdır) Sis Duvarı o gece bütün
  açık sektörlerde 15 m içeri ilerler. En fazla 2 adım. Fener doldurulan her gün bir adım geri çekilir.
- İlerleme Orta bandın iç sınırına ve kamplara asla ulaşmaz (veri kuralı: duvar − kamp ≥ 20 m).
- **Neden taraf-kör:** "Sahte fener doldurma sisi durdurmaz" deseydik fener tek başına bir cinayet dışı sert kanıt
  kaynağı olurdu (S2.4: cinayet dışı kaynak 0). Bunun yerine Sabırsız'ın kolu **sabotajdır**: fener lambasını söndürmek
  (016/026b "snuff"), görülebilen ve deftere düşen bir fiil. Böylece ormanı tehlikeli yapmak bir karar ve bir izdir.

---

## 6. Faz ve tırmanma tablosu

| Dönem | Kurtlar | Sis | Ne için |
|---|---|---|---|
| 1. gün | Isırık yok. Son 30 sn'de tanıtım uluması (inin sektöründen). | Dikkat dolmaz. Duvar temel konumda. | Erken evre: keşif. S10.2 koruma süresiyle uyumlu. |
| 1. gece | Isırık yok, uluma ve gözler var (tanıtım). | Yok. | Kurallar ölmeden öğrenilir. Nöbet yok. |
| 2.–3. gün | Sadece Derin bant (+3/sn). | Sadece Derin (40 sn). | Orta evre: ödül için risk. |
| 2.–3. gece | Orta (+5) ve Derin (+9). | Derin (20 sn). Fener doldurulmadıysa duvar ilerler. | Nöbet açılır. Kasaba'nın gecesi dolar. |
| 4. gün ve sonrası ya da canlı ≤ N/2 | Orta bantta gündüz de (+2/sn). | 3. geceden sonra gece Orta'da da (45 sn). | Geç evre: daha yüksek risk (S10.3). |
| Toplantı, Mahkeme, Epilog | Kurtlar ine döner, hareket yok. | Dil yok, dikkat donar. | S9.1 |

---

## 7. Orman görevleri (SPRINT-016 çerçevesi üstünde)

### 7.1 Çerçeve eşlemesi
Hepsi `Tools/Level/morrowmere_world_chores.json` biçimindedir: fiiller `take` / `work` / `bring` / `panel`, "hepsi"
hedefler, `$değişken` varyantlar, eşya tanımları (`FKGWorldItemDef`), etkiler (`Stack`, `Light`, `Chop`, `Fx`),
`Sabotage` alanı (`snuff`). Mevcut kural: **dünya etkisi gerçek ve sahte görevde aynıdır, sadece Hazırlık sayılmaz**
(`KGWorldChoreComponent.cpp:619, 747`). Orman görevleri bu kuralı bozmaz.

**Süre notu:** SPRINT-016 görev başına 30–75 sn ister (yürüyüş dahil). Meydandan orman kıyısına ~40 sn yürüyüş
var. Orman görevleri 45–90 sn sürer. Öneri: "uzun görev" sınıfı, oyuncu başına dağıtılan listede en fazla 1 tane
(`FKGWorldChoreRules::Deal` kuralı). Onay listesinde (O4).

### 7.2 MVP görevleri (4 yeni + mevcut Firewood)
| Id | Başlık | Adımlar (fiil: hedef, sn) | Bant | Süre | Hazırlığa sayılır mı | Sahte (Sabırsız) | Sabotaj |
|---|---|---|---|---|---|---|---|
| `ForestDeadwood` | Kuru dal topla | work: 6 dal yığınından varyantın 3'ü, 2.0 sn her biri → `Bundle` çıkar (14 kg, hız 0.85) → bring: kamp odunluğu 1.5 sn (`Stack` +3 kütük) | Orta | 60–80 sn | Evet, 1 | Aynı görünür. Kütükler **gerçekten** odunluğa girer (fiziksel), bar dolmaz | — |
| `PitchCamp` | Kamp kur | take: oduncu barakasında `Canvas` (yeni eşya, 30 kg, hız 0.55, **iki kişilik**) → bring: kamp açıklığı 2 sn → work: 3 kazık (hepsi), 1.5 sn → çadır kurulur (`Stack`) | Kıyı → Orta | 60–90 sn (iki kişiyle ~45 sn) | Evet, 1 | Çadır gerçekten kurulur, bar dolmaz | Sonra: "çadırı yık" |
| `TrailLanterns` | Yol fenerlerini yak | take: oduncu fener kutusunda `Taper` → bring: iz patikasındaki 5 fener (hepsi), 1.0 sn (`Light`) → bring: kutuya geri | Kıyı | 60–80 sn | Evet, 1 | Fenerler gerçekten yanar | `snuff` (mevcut 016 sabotajı): gece fener söner, Sis ve kurt o noktaya yaklaşabilir |
| `ForestHerbs` | Şifalı ot topla | work: 6 ot kümesinden varyantın 3'ü (Orta/Derin), 1.5 sn → `Herbs` sepeti → bring: kilisenin şifa masası 1.5 sn | Orta/Derin | 60–90 sn | Evet, 1 | Aynı. Her yapan envanterine +1 **Şifalı Ot** alır (taraf-kör ödül) | — |
| `Firewood` (mevcut) | Odun kır | work: `chop_block` 2.2 sn × 3 → bring: `inn_woodbox` | Kıyı | mevcut | Evet, 1 | mevcut | — |

Kurallar:
- `PitchCamp` kamp başına maçta bir kez dağıtılır. Çadır maç boyunca kalır. **Nöbetin önkoşuludur** (§7.4).
- Fenerler şafakta söner. `TrailLanterns` her gün yeniden dağıtılabilir. Yanan fener sayısı gece görünürdür: köyden
  bakınca kuzeydeki ışık dizisi.
- Kamp odunluğu 0–12 kütük tutar, gösterilir (yığın büyür). Nöbetin yakıtıdır.

### 7.3 Kamp ateşi (görev değil, dünya nesnesi)
Ateşi yakmak bir görev değil, **herkesin kullanabileceği bir dünya nesnesidir** (Sabırsız dahil). Aksi hâlde "ateşi
yakamayan Sabırsızdır" diye fizikten rol okunurdu (P2).

| Eylem | Süre | Kim | İz / ses |
|---|---|---|---|
| Yak | 4 sn, `Çıra` ile (kampın çıra kutusu: gece başına 3 çıra, şafakta dolar); kor varsa (sönmeden ≤ 20 sn) 2 sn | Herkes | Alev sesi 20 m. Gece 120 m'den ışık görünür. `FireLit` |
| Besle | 1 sn, elde bir kütük (odunluktan alınır, 3 kg) | Herkes | `FireFed` (sadece nöbette sayılır) |
| Söndür: su | Dolu kova (016 kovası, dolum ≥ 0.3) ateşe dökülür: anında | Herkes | Tıslama 25 m. **Yaş kül** şafağa kadar |
| Söndür: dağıt | 3 sn tekmeyle | Herkes | 20 m. **Dağılmış kor** şafağa kadar |
| Kendiliğinden söner | Yakıt 0 | — | **Soğuk kül** |

Yakıt: `KG_FIRE_FUEL` (100, −2.5/sn, kütük +50). Yakıt < 20 iken güvenli yarıçap 10 m'den 6 m'ye iner ve alev görünür
şekilde küçülür (telgraf). Ateş yanarken kurtlar 12–18 m'de gözleriyle döner, saldırmaz. Söndükten 8 sn sonra normal
kurallar döner.

### 7.4 Kamp Nöbeti ("bir gece hayatta kal"): riskli, yüksek değerli, ortak
**Kim:** herkes. Dağıtılmaz, gönüllüdür. **Nerede:** Avcı Kampı (N1); N ≥ 15'te Doğu Sırtı'ndaki ikinci kamp da.
**Veri kuralı:** nöbet kampı meydandan ≤ 45 sn yürüyüş (≤ 145 m navmesh yolu) ve en yakın köy lane'inden ≥ 15 m
uzakta olmalıdır. N1'in bugünkü aday bölgesi (x −82…−35, y −118…−82 m) bu kuralı sağlamazsa kamp kurulumunda seçilen
nokta veriyle yakına çekilir (v2 sahibiyle, §13).

| Kural | Değer |
|---|---|
| Açık olduğu geceler | 2. gece ve sonrası |
| Önkoşul | Çadır kurulu (`PitchCamp`) ve odunlukta ≥ 4 kütük, gündüz bitiminde |
| Kayıt | Kamptaki **nöbet defteri**ne gündüz imza (2 sn, görünür). Defter herkesin okuyabileceği bir dünya nesnesidir. 024 geldikten sonra Kasaba Panosu'na da düşer ("X nöbete yazıldı"). |
| Kişi | En az 2, en fazla 4 imza. Gece başında < 2 ise nöbet iptal (imzalayanlara bildirim). |
| Varış | Kamp halkasına (12 m) Gece başı + Gece'nin %55'i içinde: N = 6: 51 sn, N = 12: 61 sn, N = 20: 74 sn |
| Kalış | Varıştan şafağa, halka dışında toplam ≤ 15 sn |
| Ateş | Varış süresinin sonundan şafağa kadar sürenin ≥ %90'ında yanık |
| Başarı | Şafakta canlı + kalış + ateş koşulları |
| Ödül (Hazırlık) | Başarılı her **Kasaba** katılımcısı için B = max(1, round(0.05 × Kasaba görev toplamı)) görev birimi; gece başına tavan max(2, round(0.10 × toplam)). Sabırsız katılımcı 0 (aynı görünür). |
| Ödül (herkes) | Maç içi altın +30 (taraf-kör) |
| Bedeli | Evde değilsin: ev masasındaki Gece Defteri yeteneği o gece yok (027a'dan sonra), evin boş, oylama sonrası karanlıkta ~45 sn orman yürüyüşü. Kampta tek tehdit kurt değil, **yanındaki kişi**dir. |

**Ödül ölçeği** ("Kasaba görev toplamı" = Sabırsız olmayan herkesin görevi, nötrler dahil; oyuncu başına 4 görev,
`KGGameMode.cpp:763-775`):

| N | Sabırsız olmayan × 4 (yaklaşık) | Kişi başı B | Gece tavanı | Tavan bar payı |
|---|---|---|---|---|
| 6 | 5 × 4 = 20 | 1 | 2 | %10 |
| 12 | 9 × 4 = 36 | 2 | 4 | %11 |
| 20 | 15 × 4 = 60 | 3 | 6 | %10 |

Bar payı N'den bağımsız ~%10–11 kalır: nöbet, bir oyuncunun bütün gün listesinden daha değerlidir ama tek başına barı
doldurmaz.

**Nöbetin dramı (bilerek):** 2 kişilik nöbette biri Sabırsız ise gece kamp en tehlikeli yerdir. 3–4 kişilik nöbet
güvenlidir ama köyü boşaltır (N = 6'da 4 kişi köyün 2/3'ü). Kiminle nöbet tutacağın bir **güven kararı**dır (S1.4).

### 7.5 Yeni eşyalar (`KGItemCatalog` önerisi)
| Eşya | Nereden | Ne işe yarar |
|---|---|---|
| **Meşale** (Torch) | Kamp meşale kutusu (kişi başı günde 1) ya da reçine + herhangi bir ateş | 90 sn ışık, §4.4. Mevcut mesh: `Torch_Metal`, `SM_KG_Pirate_Torch_*` |
| **Çıra** (Tinder) | Kamp çıra kutusu (gece başına 3) | Ateş yakmak |
| **Reçine** (Resin) | Derin bantta 4 çam kütüğünden (work 2 sn, günde kütük başı 1) | 1 reçine = 1 meşale. Derin riskinin ödülü |
| **Şifalı Ot** (Herb) | `ForestHerbs` ödülü, Derin bantta nadir kümeler | +15 can, aksamayı durdurur. Bandaj tarifine girebilir (sandık/bandaj tasarımına öneri) |
| **Çadır Bezi** (Canvas) | Oduncu barakası | `PitchCamp` taşıma eşyası (dünya eşyası, envanter değil) |

Sandıklar: Avcı Kampı'ndaki Hunter's Chest ve mağaradaki kaçakçı sandığı zaten var (`dress_wilds.py`). Öneri
(sandık tasarımına): Derin banttaki kasalarda silah parçası ağırlığı ×2. Risk ödüllendirilir, silah ormandan gelir.

---

## 8. Çıkarım döngüsüne besleme

### 8.1 Olay Defteri olayları (Roadmap 020b'nin `FKGEvent`'i)
Her satır tek satırlık defter metnine döner (G3.5): "Gün 2 · 14:20 · Kuzey Koru: kurt uluması duydun."

| Olay | Alanlar | Tanıklık (020b tanık filtresi) | Kasaba Panosu (024) |
|---|---|---|---|
| `ForestEnter` | oyuncu, sektör, bant | Görenler (`KG_SIGHT_*`), `KG_REGION_DEBOUNCE` | — |
| `WolfHowl` | sektör, sürü, **telgraf hedefleri** (sadece sunucu) | Duyanlar: `KG_HEAR_HOWL` → pratikte herkes | Evet: "Kuzey Koru'dan uluma" (herkesin duyduğu bir ses) |
| `WolfEyes` | sektör | Görenler | — |
| `WolfAttack` | kurban, sektör, hasar | Görenler + duyanlar (`KG_HEAR_SNARL`) | — |
| `WolfRepelled` | kim, yöntem (meşale/ateş/grup/vuruş/bağırış) | Görenler | — |
| `WolfKill` | kurban, sektör, yara = Isırık, sürükleme vektörü | Görenler + duyanlar | Hayır (ceset bulunur, S2.5) |
| `MistNotice` | oyuncu, sektör | Oyuncunun kendisi + dili görenler | — |
| `MistCaught` | kurban | Görenler | Hayır |
| `MistWallReturn` | oyuncu, çıkış noktası | Varışı görenler | — |
| `FireLit` / `FireOut` | kamp, kim (sunucu gerçeği), neden (yakıt / su / dağıtma) | Görenler; gece 120 m'den ışığın yandığını/söndüğünü görenler (kim olmadan) | Sonra: "kuzeydeki ateş söndü" |
| `VigilSigned` | oyuncu, kamp | Kamuya açık (defter) | Evet |
| `VigilResult` | kamp, başaranlar | Sadece sunucu (duyurulmaz; hayatta kalanlar yürüyüp döner) | Hayır |
| `BodyMauled` | ceset, kim | Görenler + duyanlar (`KG_HEAR_MAUL`) | Hayır |
| `BaitDropped` (sonra) | kim, eşya | Görenler | — |
| `LampSnuffed` (026b) | fener, kim | Görenler | Sabah sönük fener görünür |

Botlar ve Vaka Dosyası (Roadmap 021) sadece kendi tanık olduklarını okur (G6.1, G7.1).

### 8.2 Kurt ölümü mü, cinayet mi, sahne mi?
| İpucu | Gerçek kurt ölümü | Cinayet (bıçak/künt) | Sahnelenmiş kurt ölümü |
|---|---|---|---|
| O sektörden, ölümden önceki 90 sn'de uluma | **Her zaman** (P1) | Tesadüfen | Sadece katil gerçek bir ulumaya denk getirirse |
| Yara (herkes, 4 sn diz çöküp inceleme, 021) | Isırık | Bıçak / Künt | Isırık |
| Adli Tabip | "Isırık, kurt" | Silah sınıfı | "**Önce bıçak, sonra ısırık**" |
| Pati izi (yumuşak zemin, 026a) | Var | Yok | Yok (pati izi sahtelenemez) |
| Sürükleme | 3–6 m ine doğru, pati izli | Yok ya da insan ayak izli (025b) | Yok ya da insan ayak izli |
| Duyulan | Hırıltı + çığlık 30 m | Backstab 15 m / çığlık 30 m | Yırtılma 12 m |
| Yer | Orta/Derin, patika ve ışık dışı; gündüz sadece Derin | Her yer | Orta/Derin (fiil kısıtı) |
| Katilde leke (026a) | — | 45–60 sn | 45–60 sn + Parçala'dan 45 sn |
| Kurbanın son durumu | Yalnız (ya da gece 2 kişi, ateş dışında) | Her durum | Her durum |

**Tek cümlelik kurallar** (oyuncuya ve bota öğretilir): "Kurtlar ulumadan saldırmaz. Köye girmez. Patikada ısırmaz.
Pati izi bırakır." Bunlardan birini çiğneyen "kurt ölümü" bir çelişkidir (charter §0.4) ve sunucu onu `KG_CONTRA`
olarak loglar (S9.5 ölçümü).

**Sis ölümü:** kırağı, yara yok, sadece Derin (3. geceden sonra gece Orta), kurban yalnızdı, dili görenler olabilir.
Sis ölümü sahnelenemez (fiil yok). Sabırsız'ın sisle oynadığı oyun **yalnız bırakmak**tır (§8.4).

**Kurtadam (rol, `02_Roles.md`):** canavar formunun ısırığı da "Isırık"tır ama köyde, evlerde olur. Kurtlar köye
girmediği için köyde ısırık = Kurtadam (ya da sahne). Orman kurtları Kurtadam'a **ormanda** örtü sağlar, köyde değil.
Adli Tabip ayrımı: "canavar ısırığı" (sonra, rol sprintleriyle).

### 8.3 Yeni izler (charter §3 yaşam döngüsü biçiminde)
| Delil | Kaynak | Ömür | Karşı hamle | Sahte yolu |
|---|---|---|---|---|
| Isırık yarası (ceset) | Kurt saldırısı | Kalıcı | — | **Parçala** (§8.4) |
| Isırık izi (canlı) | Isırılmak: yırtık kol, 60 sn aksama | Aksama 60 sn; yırtık kol şafağa kadar | Bandaj/ot aksamayı durdurur ama **bandaj görünür** | — ("ormanda kurt ısırdı" alibisi kontrol edilebilir) |
| Pati izi | Kurt hareketi, yumuşak orman zemini | 90 sn | — | Yok |
| Kurt sürüklemesi | Kurt ölümü | 60 sn | — | İnsan sürüklemesi (farklı iz) |
| Kırağı (ceset) | Sis ölümü | Kalıcı | — | Yok |
| Islak | Sis Duvarı'ndan dönüş | 60 sn | Kuruyana kadar saklan | Kovayla ıslanma (sonra: kuyuda yıkanma zaten ıslatır, 026a) |
| Yaş kül / dağılmış kor / soğuk kül | Ateşin sönme nedeni | Şafağa kadar | Ateşi yeniden yak (kül değişmez, üstüne yeni ateş) | Söndürüp "kendiliğinden söndü" demek: kül yalanı yakalar |
| Sönük yol feneri | `snuff` sabotajı | Yeniden yakılana kadar | `TrailLanterns` | — |
| Nöbet defteri | İmza | Maç boyu | — | Sonra: Sahtekar (Forger) imza silebilir |

Böylece S3.2'nin "her iz türünün ömrü ve karşı hamlesi var, ≥ %50'sinin sahte yolu var" kuralı bu özellik için de
sağlanır (9 türden 5'inin sahte yolu var).

### 8.4 Sabırsız'ın orman fiilleri (S6.1: öldürme dışı fiiller)
| Fiil | Kim | Koşul | Süre / iz | Kasaba'nın cevabı |
|---|---|---|---|---|
| **Parçala** (kurt işi gibi göster) | Sabırsız (her kötü taraf) | Kendi ya da taze ceset (≤ 60 sn), Orta/Derin bant, patika ve ışık dışı | 6 sn çömelme animasyonu, `KG_HEAR_MAUL`, +45 sn leke, `BodyMauled` | Uluma kaydı, pati izi yokluğu, Adli Tabip, leke |
| **Yalnız bırak** | Herkes (Sabırsız için bir silah) | Kurbanı Derin'e götür, sonra ayrıl: kurban yalnız kalınca Sis dikkati dolar | İz: iki kişinin birlikte girip birinin tek çıktığı `ForestEnter` olayları, ayak izleri | Kiminle ormana girdiğine dikkat et; kovalanana koş (dil hedef değiştirmez) |
| **Ateşi söndür** | Herkes (fiziksel) | Kamp ateşi | Su: tıslama 25 m, yaş kül; dağıt: 3 sn, 20 m, dağılmış kor | Ateşin başında nöbet, kül okuma |
| **Fener sabotajı** | Sabırsız (016/026b "snuff") | Yol feneri ya da fener lambası | 3 sn, görünür, `LampSnuffed`; fener lambası sönerse o gece Sis Duvarı ilerler | Onarım görevi (026b), feneri kimin söndürdüğünü gören |
| **Yem** (sonra) | Herkes | Çiğ balık/et ormana bırakılır: gece 60 m içindeki kurt ilgisini o noktaya çeker | Eşya yerde kalır; kimin tuttuğu balıkçılık kaydında (Fishing, 020b yayıcısı) | "O somonu bugün kim tuttu?" |
| **Tabela çevir** (sonra) | Sabırsız | İz patikası tabelası Derin'e döndürülür | 3 sn, gıcırtı 15 m, taze çizik, `SignTurned` | Herkes 2 sn'de düzeltir |

Bu fiiller Sabırsız'a "öldür ve kaç" dışında bir hikâye kurdurur: **zamanlama** (ulumadan hemen sonra), **yer**
(patika dışı), **bahane** (nöbet, odun).

### 8.5 Rollerle bağlar (roller geldikçe; kodda bugün 0/51 yetenek var)
| Rol | Bağ | Ne zaman |
|---|---|---|
| Adli Tabip (Coroner) | Isırık / sonradan ısırık / sis ayrımı | Rol sprintleri (Roadmap 027a+) |
| İz Sürücü (Tracker) | Pati ve insan izlerini parlak görür | 026a + rol sprinti |
| Avcı (Hunter) | Ayı kapanı kurda da işler: o kurt o gece yok | Sonra |
| Çoban (Shepherd) | Köpek 30 m'deki kurda havlar (erken uyarı) | Sonra |
| Barut Ustası | Gece fişeği bir orman sektörünü 15 sn aydınlatır: ışık yarıçapı gibi davranır | Sonra |
| Kurtadam | §8.2; dolunay gecelerinde (2. ve 4. gece) orman kurtları koro hâlinde ulur: örtü ama duyulur | Sonra |
| Bekçi (Watchman) | Nöbet defterinde imzası olan kamp halkasındayken sokak yasağını çiğnemiş sayılmaz | Roadmap 028 |

---

## 9. Karşı hamleler ve suistimal vakaları

| # | Suistimal | Kural |
|---|---|---|
| A1 | Kurtları köye çekip birini öldürtmek | Kurtlar teras poligonlarına ve ışık yarıçaplarına girmez; gündüz patikaya çıkmaz. |
| A2 | Tehdidi rol dedektörü olarak kullanmak ("kurtlar ona saldırmadı, Sabırsız!") | P2: taraf-kör. Bot ve sunucu kodunda tehdit mantığı `PrivateRoleId` okumaz (G6.1 grep kuralına orman klasörü eklenir). |
| A3 | Nöbeti gece alibisi olarak kullanmak (Sabırsız) | Serbest ve bedelli: o gece köyde öldüremez. 2 kişilik nöbet riski Kasaba'nın bildiği bir risktir. Nöbet defteri kimin orada olduğunu söyler. |
| A4 | Bütün köyün her gece kampa gidip geceyi atlaması | En fazla 4 imza (N ≥ 15'te 2 kamp × 4). Evin boş kalması ve Gece Defteri'ni kaçırmak bedeldir. |
| A5 | Toplantıdan kaçmak için ormanda kalmak | Toplantıya uzaktan gelen Geç Kalan olur (021, S4.5b). Orta/Derin bantta HUD "Meydana ~50 sn" gösterir. Bu bölgeler S4.5 ölçümünün dışındadır: bilerek alınan risk. |
| A6 | Meşale istiflemek | Kişi başı günde 1 kutudan; fazlası reçine (Derin) ister; 90 sn yanar. |
| A7 | Ateşi söndürerek nöbeti bozmak (trol) | Fiziksel ve izli (kül türü, ses, `FireOut`). Kasabalı trolü de aynı delille yakalanır. |
| A8 | Sis Duvarı'nı ışınlanma kaçışı olarak kullanmak | MVP'de duvar sadece **en yakın güvenli hücreye** geri çıkarır (uzağa değil). Islak 60 sn görünür. İskele versiyonu (sonra) 90 sn Islak ve tanıklı varış ile gelir. |
| A9 | AFK oyuncuyu ormanda tehdide yedirmek | §2.4 madde 6: AFK 60 sn → güvenli hücreye dönüş. |
| A10 | Botu Derin'e götürüp bırakmak | Botlar ormanda patikaya dönme kuralıyla yürür (§10); yalnız kalan bot 5 sn içinde güvenliğe yönelir. |
| A11 | Parçala'yı köyde kullanmak | Fiil sadece Orta/Derin bantta açılır. Köyde ısırık yine de mümkün değil: köyde ısırık gören herkes "Kurtadam ya da yalan" der. |
| A12 | PvE ölümlerinin yıpratma ile Sabırsız'ı kazandırması | F1 tavanı (PvE ölümleri tüm ölümlerin ≤ %15'i). Soak'ta aşarsa önce gündüz Derin hızı (+3) ve ısırık hasarı düşürülür. |
| A13 | Sis dilini birinin üstüne "yönlendirmek" | Dil hedef değiştirmez. Tavan dolduğunda yeni dil doğmaz. |
| A14 | Kurtların ısırığıyla kendini yaralayıp "saldırıya uğradım" alibisi | Serbest ve dürüst: gerçekten ormanda yalnızdın, uluma ve saldırı deftere düştü. Sabırsız için pahalı bir alibi. |
| A15 | Yayıncı modu ile isim sızıntısı | Uluma ve ateş satırları isimsizdir; isimli satırlar 015'in takma ad fonksiyonundan geçer (G7.5). |

---

## 10. Botlar

**Adalet (G6.1):** botlar sadece kendi algılarını okur: duydukları ulumalar, gördükleri gözler ve Sis dili, **kendi**
telgraf aşamaları (insanın HUD'da ve seste aldığının aynısı), kendi canı. İlgi metresinin sayısını ve başka oyuncuların
aşamasını okumazlar.

| Durum | Davranış | Parametre (soak öncesi dondurulur, §0.2 kural 2) |
|---|---|---|
| Orman rotası | Navmesh maliyeti: Kıyı ×1, Orta ×3, Derin ×10. Sadece görev adımı için patika dışına çıkar. | `BotForestOffPathMaxS` gündüz 45, gece 25 |
| Uluma duydu (kendi sektörü) ve patika dışında | Adımı bitirmeden en yakın güvenli hücreye **yürür**. | `BotHowlReactS` 1.5 |
| Gözleri gördü | Güvenliğe **koşar**. | — |
| Isırıldı | Koşar. Köşeye sıkışırsa (güvenli hücre > 20 m ve stamina ≥ 25) atılan kurda itme. | `BotShoveWindow` 0.3 sn |
| Sis kırağısı / dil gördü | Hemen güvenliğe; en yakın canlı oyuncu ≤ 15 m ise ona yürür (grup). | — |
| Gece orman görevi | Kasaba botu bir "yol arkadaşı" arar (≤ 30 m'deki botla birlikte gider); bulamazsa görevi sabaha bırakır. | `BotBuddyChance` 0.6 |
| Nöbet | Kasaba botu: gündüz imza sayısı ≥ 1 ve canlı ≥ 6 ise imzalar. Nöbette yakıt < 40 iken odunluktan kütük taşır. | `BotVigilChance` 0.35, `BotFeedBelow` 40 |
| Meşale | Gece Orta/Derin'e girecekse kutudan meşale alır. | — |
| Sabırsız bot (MVP) | Orman tehditlerinden Kasaba botuyla aynı kurallarla kaçar. Sahneleme yok. | — |
| Sabırsız bot (sonra, Roadmap 023 + F3) | Ormanda tanıksız cinayetten sonra, sektörde son 90 sn içinde uluma **varsa** Parçala kullanır (usta hamle). | `BotMaulChance` 0.5 |
| Kasaba bot zihni (Roadmap 023) | "Isırık + o sektörde uluma yok" ya da "patikada ısırık" = çelişki → kurbanla en son görülen kişiye şüphe. "Nöbetten tek dönen" = zayıf şüphe. Oy gerekçesi `reason=<eventId>`. | ağırlıklar dondurulur |

**[U] logları:** `KG_FOREST bot=… stage=howl|eyes|bite|mist action=walk|run|shove|group`, `KG_VIGIL bot=… sign|feed|
result=…`, `KG_MAUL bot=…` (sonra). Botlar görev dağıtımında zaten tırmanma gerektirmeyen görevleri alır
(`FKGWorldChoreRules::Deal`); orman görevleri tırmanma istemez.

---

## 11. UI/UX

Önce dünya, sonra HUD. Metre yok: oyuncu telgrafı gözüyle ve kulağıyla okur.

| Öğe | Nasıl |
|---|---|
| Bant farkındalığı | Orta'ya girince hiçbir şey; Derin'e girince hafif soğuk renk ve gren. İlk girişte bir kez: "Derin Orman. Yalnız kalma, ışıktan ayrılma." |
| Uyarı tabelası (lore) | Orman girişlerinde: TR "Karşılama Komitesi: Ormanda yalnız yürümeyin. Sis yalnızları sever." / EN "Welcome Committee: Do not walk the woods alone. The fog is fond of the lonely." |
| Uluma / hırlama | Yönlü ses + altyazı ("[Uzakta kurt uluması, Kuzey]") + hasar göstergesi gibi kenar oku. Sessiz yol (G1.1): işitme engelli oyuncu aynı bilgiyi alır. |
| Gözler | Gece emissive göz parıltısı, 25 m'den görünür. |
| Sis | Ekran kenarı kırağı (%50), fısıltı, pusula iğnesinin dönmesi, ilk kez tek satır ipucu. Çekirdekte 3 sn'lik beyaz halka. |
| Güvende olmak | Işık yarıçapında stamina çubuğunun yanında küçük fener simgesi. |
| Pusula | Orta/Derin'de "Köy" ve "Patika" okları; haritada orman patikaları, kamplar, yanan fener dizisi (gece). |
| Kamp ateşi | Bakınca yakıt halkası; yakıt < 20'de alev küçülür. |
| Nöbet | Defter istemi "Nöbete yazıl (2/4)". Nöbettekilere tek satır: "Nöbet: 3 kişi · Ateş %60 · Şafağa 48 sn". |
| Ceset inceleme (021) | Kart: yara sınıfı (Bıçak / Künt / Isırık / Sis) + bir ipucu satırı ("Pati izi yok", "Kırağı"). |
| Defter (024) | "Gün 2 · 14:20 · Kuzey Koru: kurt uluması duydun." |
| Dilleri | Bütün metinler String Table'da (EN/TR/RU). Anahtar önerisi `Forest.*`, `Lore.Notice.Forest.*`. |

Lore metinleri (öneri):
- Haberci Çocuk, 1. şafak: TR "Oduncular Kuzey Koru'da kurt izi görmüş. Yarın kesin giderler." / EN "The woodcutters
  saw wolf tracks in the North Wood. They'll be gone tomorrow. Surely."
- Sis ölümü kartı (sonra): TR "Sis onu geri verdi. Biraz ıslak, biraz geç." / EN "The fog gave them back. A little
  wet, a little late."
- Kurtlar: TR "Morrowmere'in kurtları da bekler. Sadece daha az sabırla." / EN "Morrowmere's wolves wait too. With less
  patience."

---

## 12. Performans ve ağ

| Kalem | Tavan / bütçe | Ölçüm |
|---|---|---|
| Aktif kurt | N 6–9: 2, 10–14: 3, 15–20: 6 | [T] |
| Sis dili | N 6–9: 1, 10–14: 2, 15–20: 3 | [T] |
| Sunucu CPU (orman sistemlerinin tamamı: bant okuma, metreler, kurt AI, dil, ateş) | ≤ 0.35 ms/kare p95, N = 20, 6 kurt + 3 dil aktif, bir gün boyunca | [T] başsız (020b'nin 0.25 ms defter ölçümü kalıbı) |
| İstemci GPU (dil + duvar + ateş + fenerler) | ≤ 0.5 ms p95, 1080p, referans makine (RTX 5070 + 7800X3D), 3 dil ve duvar görüşte | [R] ekran dışı render turu |
| Bant raster'ı | 120 × 116 hücre × 2 bayt ≈ 28 KB, O(1) okuma | — |
| Kurt AI | Uykuda 0 tick, görünmez, replike değil (en yakın oyuncu inden > 70 m). Aktifken 10 Hz karar, 1 Hz yeniden yol. `ACharacter` + basit CMC, 6 taneyle sınırlı. | [T] |
| Metre güncellemesi | 4 Hz, oyuncu başına O(1) | — |
| Sis dili | Kinematik (navmesh yok), 2 Hz zemin izi. Görsel: Local Fog Volume + ≤ 300 sprite Niagara. | [R] |
| Sis Duvarı | Sektör başına statik yerel sis hacmi + sis kartları, dinamik ışık yok | [R] |
| Işıklar | Kamp ateşi: 1 Movable point light (gölge kapalı ya da ucuz). Yol fenerleri: `AKGFlickerLight` kalıbı (gölgesiz, yarıçap ≤ 6 m, kamera yakınında titrer), 40 m mesafe kırpma. Ormanda aynı anda görünür ≤ 12 ışık. | [R] |
| Kurt görseli | ≤ 4k üçgen, 2 LOD, URO; 6 kurt animasyonu ≤ 0.15 ms | [R] |
| Ağ | Kurt: NetUpdateFrequency 10, cull 90 m (en kötü ~2.4 KB/sn/istemci). Dil: 5 Hz, herkes görür (tanıklık bilgisi). Metreler ve hedefler **replike edilmez**; oyuncuya sadece kendi aşaması `COND_OwnerOnly` gider (G7). | [T] iki süreçli smoke |
| Host göçü | Orman durumu (inler, metreler, yakıt, odunluk, nöbet imzaları, Sis ilerleme adımı) `UPROPERTY(SaveGame)` + `UKGSnapshotComponent`. Süreler `FKGMatchClock`, kararlar `FKGRng`. World Partition orman aktörlerini asla akıtmaz. | [T] SaveGame arşiv turu |

---

## 13. Bağımlılıklar

### 13.1 Mevcut kod ve veri
| Ne | Dosya | Kullanım |
|---|---|---|
| Dünya görevi çerçevesi | `Source/KillGodot/Chores/WorldChores/` (`KGWorldChoreTypes.h`: fiiller, `FKGWorldItemDef` `bTwoPerson`/`bFlame`, `KGWorldEffect`, `Sabotage`, `FKGWorldChoreRules::Deal`), `Tools/Level/morrowmere_world_chores.json`, `gen_world_chores.py` | §7 görevleri veri olarak eklenir. Yeni etki gerekmez; yeni eşya: `Canvas`, `Herbs`. 016'nın önerilen `EmitChoreStepEvent` kancası defter bağlantısıdır. |
| Görev sayımı | `Core/KGGameMode.cpp:782` `OnTaskCompleted`, `:763` oyuncu başına 4 görev | Nöbet ödülü (B birimi) buraya küçük bir kanca ister. |
| Can ve hasar türü | `Combat/KGHealthComponent.h` `ApplyDamage(Amount, Instigator, FName DamageType)` | "Bite", "Mist" hasar türleri başlık değişikliği istemez. |
| Ölüm | `Character/KGCharacter.cpp:1315` `HandleDeath` | Yara sınıfı, kurt sürüklemesi, kırağı. Islak/aksama durumu (başlık değişikliği: toplu editör kapanışı). |
| Stamina | `Character/KGStamina.h` (100, −18/sn) | Kaçış sayıları §4.3, §5.3 |
| Faz ve saat | `Core/KGGameMode.cpp:63` `GetPhaseDuration`, `KGMatchClock.h`, `KGRng.h` | Nöbet varış süresi, tırmanma, tohumlu inler |
| Envanter | `Inventory/KGItemCatalog.cpp`, `KGLoot.*` (tablolar Crate/Barrel/Pot/Chest/Grave/Fishing) | Meşale, Çıra, Reçine, Şifalı Ot; orman sandıkları |
| Kırılabilir | `World/KGBreakable.*` (Health, LootTable, LootSeed) | Orman kasaları (sandık tasarımıyla) |
| Oturak | `World/KGSeat.*` | Kamp kütük oturakları (nöbet atmosferi) |
| Botlar | `AI/KGBotController.cpp` (`UpdateChores`, gezinme) | §10 kaçış kuralları |
| Harita bölgeleri | `World/KGMapInfo.*` (`FKGMapRegion`) | Orman sektör adları defter satırlarında |
| Balıkçılık | `Fishing/` | Yem (sonra): hangi balığı kim tuttu |
| Yeraltı ışıkları | `Dig/KGFlickerLight.*` | Yol feneri kalıbı |
| Orman süslemesi | `Tools/Unreal/dressing/v2/dress_wilds.py` (Avcı Kampı, Taş Çember, Mağara, Kömürcü, Gözcü) | Kamp ve ilgi noktalarının yerleri (tohumlu). **Değiştirilmez**; orman verisi bu noktaları okur. |
| Layout | `Tools/Level/morrowmere_layout_v2.json` (`lanes`: `woods_path`, `hollow_way`; `zones`; `nav_bounds`) | Bant raster'ının girdisi |
| Varlıklar | Çadır, kamp ateşi, fener, meşale mesh'leri var (`asset_catalog.md`). **Kurt modeli yok.** Aday: Quaternius Ultimate Animated Animals (CC0, `AssetResearch.md` satır 592). **İndirme kullanıcı onayı ister.** Onay yoksa MVP yer tutucu (ör. Çoban köpeği ile aynı sorun) ile durur ve sorar. | §17 O2 |

### 13.2 Yol haritası (Roadmap ID'leri)
| Roadmap | Ne verir | Bu özellik için |
|---|---|---|
| **020a** | Toplantıda ölüm yok, bot sayan faz süresi | Kurtların toplantıda donması aynı kuralı izler |
| **020b** Olay Defteri + tanık filtresi | `UKGEventLedger`, `FKGEvent`, `FKGWitnessFilter` | **Sert bağımlılık.** Ç2 bunsuz Evet olamaz. Özellik maçta ancak 020b'den sonra açılır. |
| **021** Rapor + Vaka Dosyası | `CaseFile.WoundClass`, ceset inceleme | Isırık / Sis yara sınıfları |
| **023** Bot zihni | `FKGBotMind`, `FKGBotView`, `KG_VOTE reason=` | Sahne çelişkisini okuyan Kasaba botları |
| **024** Defter (J), Kasaba Panosu | Tek satırlık olaylar, pano | Uluma satırları, nöbet imzaları |
| **025b** Ceset taşıma | Taşınma modu | Cesedi sise taşımak (iskele dönüşü, sonra) |
| **026a** Fiziksel izler | İz havuzu (≤ 256), yumuşak zemin | Pati izi, kurt sürüklemesi (aynı havuz ve tavan) |
| **026b** Sabotaj → onarım | `snuff`'ı deftere bağlar | Yol feneri ve fener lambası sabotajı |
| **027a** Gece Defteri | Ev masası | Nöbetin bedeli |
| **028** Bekçi, sokak yasağı | Yasak | Nöbet istisnası |
| **031a** Bölge Kapıları | `MinPlayers`, `RiskTier` | Sektör kapıları = Sis Duvarı; orman sektörleri "tehlikeli" |

### 13.3 Dosyadaki sprintler (çakışma yok)
- **SPRINT-016** (fiziksel görevler): orman görevleri 016 **bittikten sonra** onun JSON'una eklenir; 016'nın kapsamına
  dokunulmaz.
- **SPRINT-022** (v2 görsel geçiş) `dressing/v2/*` ve `kg_build_village_v2.py`'nin sahibidir. Orman nesneleri (kurt
  inleri, fenerler, nöbet defteri, meşale kutusu, Sis Duvarı) **çalışma zamanında orman verisinden spawn edilir**
  (Roadmap kuralı: inşa hattına dokunulmaz). Kamp noktasını taşımak gerekirse v2 sahibiyle sıra ayarlanır.
- **SPRINT-023** (ses, sosyal): "Yardım!" sesli komutu kurtları korkutur (§4.4). Kanca, 023'ün komut olayına abone olur.
- **SPRINT-015 / 017**: ilişki yok (017 Storm Manor'un şimşek kuralıyla aynı ilke: telgraf ve tohum).

### 13.4 Süreç engelleri (dürüst)
- LoopContract: doğrulama borcu > 10 iken yeni özellik sprintleri durur. Roadmap bugün 11 madde diyor. Bu özellik
  doğrulama oturumundan **sonra** gelir.
- Charter §0.1: yeni sistem. Kullanıcı açıkça istedi, ama sırası charter'ın bağımlılık sırasına uyar: defter (020b)
  önce.

---

## 14. MVP ve sonrası (sprint önerileri, hepsi onay bekler)

### Sıra önerisi
Doğrulama oturumu → 016 biter → 020a → 020b → **F1** → **F2** → (021, 026a sonrası) **F3**. F1 ve F2, 020b'den önce
yapılırsa bile maçta **kapalı** (lobi seçeneği "Orman tehlikeleri", varsayılan kapalı) kalır; dev komutlarıyla görülür.
Böylece kullanıcı ekranda yeni içeriği erken görür ama charter'ın Ç2 kuralı delinmez.

### SPRINT-F1 — Orman Tehlikesi v1: kurtlar + Sis (L)
**Hedef (görünen):** ormanda yalnız dolaşan oyuncu uluma, göz ve ısırıkla karşılaşır; Derin'de Sis peşine düşer.
Patika, ışık ve grup korur. Sis Duvarı kaybolmayı önler.
**Charter kodları:** S1.4 (risk türü), S4.1, S4.6, S10.3, G4, G5.6. Rapor: F1, F3, F7.
**Kabul (sabit):**
1. **Test** `KillGodot.Forest.Rules` (saf `FKGForestRules`, tohumlu): ≥ 15 tablo vakası: bant/faz/grup/meşale
   kazançları; aşama en az süreleri; aynı hedefe ilk ısırık ulumadan ≥ `KG_WOLF_TELEGRAPH_MIN`; Sis dili güvenliğin
   ters yönünde doğar; 1. gün ve 1. gece hasar 0; toplantıda kurt hareketi 0.
2. **Test (veri lint'i):** orman raster'ında her yürünebilir hücre ≤ `KG_FOREST_SAFE_MAX`; Sis Duvarı'na giren oyuncu
   ≤ 3 sn'de güvenli bir hücrede.
3. **[U] soak** (N 8 ve 20, 2 tohum, gün 1–3): Gözler aşamasına ulaşan her bot `KG_FOREST … action=run` yazar; kurt ve
   dil tavanları hiç aşılmaz; PvE ölüm payı raporda (F1, kapı değil).
4. **Performans:** sunucu ≤ 0.35 ms/kare p95 (N = 20, başsız); [R] GPU ≤ 0.5 ms.
5. Ekran dışı görüntüler `KG_Cap_forest_eyes_night.png` (20 m'de gözler), `KG_Cap_forest_mist.png` (yaklaşan dil);
   2 bakış turu.
6. `run_invariants.ps1` geçer; özellik lobi seçeneğiyle kapalı gelir.
**Yazılabilir kapsam:** yeni `Source/KillGodot/Forest/` (`UKGForestSubsystem` sadece sunucu, `AKGWolf`, `AKGMistTongue`,
`AKGMistWall`, `FKGForestRules`, `KGForestTypes.h`, dev fiilleri `kg.Forest.*`), yeni `Tools/Level/morrowmere_forest_v2.json`
ve üretici `gen_forest_bands.py`, `Character/KGCharacter.cpp` (sadece hasar türü, Islak/aksama durumu: tek toplu
editör kapanışı), `AI/KGBotController.cpp` (sadece orman kaçışı), `UI/` (altyazı, kırağı, pusula okları), testler,
`Tools/Gauntlet/kg_forest_smoke.ps1`, bu belge. **Önkoşul:** kurt modeli için kullanıcı onayı (O2).
**Sınırlar:** 3 deneme / kontrol; 2 bakış turu; plato durdurması.

### SPRINT-F2 — Orman görevleri + Kamp Nöbeti (M)
**Hedef:** dört orman görevi dağıtılır; kamp ateşi yanar, beslenir, söner; 2–4 kişi nöbet tutar ve bar büyür.
**Charter kodları:** S1.1 ve S1.2 gece (Kasaba), S1.4 (güven), S4.1, S4.6. Rapor: F5.
**Kabul (sabit):**
1. 4 görev JSON'da; `kg.WorldChore.Routes` her adımı navmesh'te ulaşılabilir buluyor; süreler 45–90 sn (uzun görev
   sınıfı, oyuncu başına ≤ 1, `Deal` testi).
2. **Test** `KillGodot.Forest.Vigil`: ≥ 10 tablo vakası (en az 2 / en fazla 4, varış süresi N'ye göre, kalış ≤ 15 sn,
   ateş ≥ %90, B ve gece tavanı formülü, Sabırsız 0, 1. gece kapalı).
3. **İki süreçli smoke:** A ateşi yakar → B ateşi görür; A (zorla Sabırsız) ateşi yakar → B için de yanar (taraf-kör);
   kova ile söndürme → yaş kül iki uçta.
4. **[U]** Kasaba botları imzalar, besler, `KG_VIGIL` yazar. Başarı oranı raporda.
5. Görüntüler: köyden gece kuzeydeki kamp ateşi, nöbet HUD'u. `run_invariants.ps1` geçer.
**Yazılabilir kapsam:** `Tools/Level/morrowmere_world_chores.json` (016 bittikten sonra), `Forest/` (`AKGCampfire`,
nöbet), `Inventory/KGItemCatalog.cpp` (4 eşya), `Core/KGGameMode.cpp` (sadece nöbet ödül kancası), `AI/` (nöbet), testler.

### SPRINT-F3 — Sahneleme ve orman delilleri (M; 020b, 021, 026a'dan sonra)
**Kabul (sabit):**
1. **Test:** §8.1'deki her olay türü tanık filtresiyle yazılır (020b tanık testi kalıbı: gördü/duydu/tanımlı).
2. **Test:** Parçala kısıtları (sadece kötü taraf, Orta/Derin, ≤ 60 sn, 6 sn); normal inceleme "Isırık", Adli Tabip
   "önce bıçak"; sektörde uluma yokken ısırıklı ceset `KG_CONTRA` yazar.
3. **Test:** pati izi ve kurt sürüklemesi 026a havuzunu kullanır, ortak 256 tavanı aşılmaz.
4. **[U]** Sabırsız botun Parçala yolu ve Kasaba botunun çelişki şüphesi çalışır, loglanır.
5. `run_invariants.ps1` geçer.

### Sonra (ayrı öneriler)
İşaret Ateşi (sırt mangalı: o sektörde Sis ilerlemesini durdurur; köyün her yerinden görünen doğrulanabilir görev),
Yem, tabela çevirme, sis cesedinin iskelede dönüşü, Sis Duvarı'ndan iskeleye çıkış, Avcı kapanı ve Çoban köpeği,
Kurtadam dolunay korosu, Barut Ustası fişeği, Taş Çember adağı (lore), tuzak kontrolü görevi, kurtları kovalayan ikinci
sürü davranışları, GDD §10 Sis Kuşatması'nın bu Sis ile kurulması (aynı Beklenmeyenler).

---

## 15. Ölçüm

### 15.1 Charter hedefleriyle ilişki
| Kod | Bu özellik neyi değiştirir |
|---|---|
| S1.1, S1.2 (gece, Kasaba) | Bugün Kasaba'nın gecesi tamamen boş (93–135 sn). Nöbet ve gece orman yürüyüşü, uluma/göz algıları ve ateş besleme ile gece ölü zamanını düşürür. Hedef: nöbet katılımcılarında gece p90 ≤ 60 sn. |
| S1.4 | "Risk" türü (gece tehlikeli bölgeye gir) ve "Güven" (nöbet ortağı) karar türlerini çalıştırır. |
| S2.3 | Ormandaki bir vakada uluma, hırlama, `ForestEnter`, ateş olayları → vaka başına ≥ 3 ilgili bilgi parçası. |
| S2.4 | Yeni sert kanıt kaynağı **0** (§5.5'teki taraf-kör fener kuralı bunun için). |
| S3.1, S3.2 | 9 yeni iz türü, hepsinin ömrü ve karşı hamlesi; 5'inin sahte yolu. |
| S4.1 | Dört orman sektörünün her biri ≥ 4 sebep: görev, risk/ödül, karşılaşma, değişen durum (ateş, fener, Sis), sabotaj. |
| S4.6 | Gün başına görünür durum değişikliği: yanan/sönen kamp ateşi, fener dizisi, çadır, Sis ilerlemesi. |
| S6.1 | Parçala, yalnız bırakma, ateş söndürme, fener sabotajı: 4 yeni öldürme dışı fiil. |
| S9.5 | "Isırık ama uluma yok" türü çelişkiler. |
| S10.3 | Tırmanma: 4. günden sonra gündüz Orta tehlikesi, 3. geceden sonra Sis Orta'da, Sis ilerlemesi. |
| G4 | İnler tohumlu ve şafakta duyurulur; sonucu belirleyen yazı-tura 0. |
| G5.6 | Her olumsuz etkinin karşı hamlesi §4.4 ve §5.3'te veride. |

### 15.2 Özelliğin kendi hedefleri (F kodları, charter §0.2 kurallarıyla)
| Kod | Hedef | Değer | Ölçüm | n_min |
|---|---|---|---|---|
| F1 | PvE ölümlerinin tüm ölümler içindeki payı | ≤ %15 (N ≤ 9'da ≤ %10) | [B] gece soak'u, sadece rapor (bot parametreleri dondurulmuş) | 200 ölüm |
| F2 | Telgrafsız PvE hasarı | 0 (kurt ≥ 20 sn, Sis ≥ 25 sn) | [T] | — |
| F3 | 1. gün ve 1. gece PvE hasarı | 0 | [T] | — |
| F4 | Ormanda bulunan oyuncu-gün başına orman defter olayı | Medyan ≥ 1 | [B] | 40 oyuncu-gün |
| F5 | Nöbet: Kasaba katılımcılarının başarı oranı; N ≥ 10'da nöbetli gece oranı | %60–85; ≥ %30 | [O]; bot için [U] | 40 katılımcı; 40 gece |
| F6 | Sahnelenmiş kurt ölümlerinde ≥ 1 kamu çelişkisi (uluma yok ya da pati izi yok) ya da katilin uluma zamanlaması | Yapı gereği %100 kontrol edilebilir [T]; sahnelemenin ertesi toplantıda yakalanmama oranı %40–70 [O] | [T] [O] | 30 sahne [O] |
| F7 | Gündüz canlı oyuncu-saniyelerinde orman payı (bütün sektörler) | %8–20 | [B] | 20 gündüz |
| F8 | Performans ve tavanlar | §12 | [T] [R] | — |
| F9 | Kaybolma | Her yürünebilir orman hücresi ≤ `KG_FOREST_SAFE_MAX`; duvardan dönüş ≤ 3 sn | [L] [T] | — |

---

## 16. Charter kontrol listesi (Ç1–Ç12)

Kural: Ç1–Ç4 zorunlu, kalan 8'den ≥ 6 Evet. Cevaplar sadece Evet/Hayır; her Hayır bir cümleyle.

### 16.1 Alt mekanik bazında
| Mekanik | Ç1 | Ç2 | Ç3 | Ç4 | Ç5 | Ç6 | Ç7 | Ç8 | Ç9 | Ç10 | Ç11 | Ç12 | Sonuç |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Kurtlar | E | E* | E | E | E | E | E | E | E | E | E | E | **Koşullu kabul** (*Ç2: 020b) |
| Sis (dil + duvar) | E | E* | E | E | E | E | E | E | E | E | E | E | **Koşullu kabul** (*Ç2: 020b) |
| Orman görevleri | E | E* | E | E | E | E | E | E | E | E | E | E | **Koşullu kabul** (*Ç2: 016'nın `EmitChoreStepEvent` kancası + 020b) |
| Kamp Nöbeti | E | E* | E | E | E | E | E | E | E | E | E | E | **Koşullu kabul** (*Ç2: 020b) |
| Parçala (sahneleme) | E | E* | E | E | E | E | E | **H** | E | E | E | E | **Koşullu kabul**; Ç8 Hayır: botlar sahneyi ancak Roadmap 023 bot zihniyle algılar (7/8, kural sağlanır) |

### 16.2 Özellik geneli, gerekçeli
| # | Soru | Cevap | Gerekçe |
|---|---|---|---|
| Ç1 | Karar | **Evet** | Ormana gir/girme, yalnız/grup, gündüz/gece, meşale harca/sakla, koş/karşı koy, nöbete yazıl/evde kal, kiminle. Her seçenek farklı risk ve bilgi sonucu taşır. |
| Ç2 | İz | **Evet (koşullu)** | Uluma, göz, saldırı, Sis, ateş, nöbet, Parçala defter olayıdır (§8.1) ve çoğu görülür/duyulur. Ama Olay Defteri bugün kodda yok: 020b gelene kadar özellik maçta kapalı kalır. 020b sonunda Evet değilse özellik dev modunda kalır. |
| Ç3 | Karşı hamle | **Evet** | Tehditlere: patika, ışık, grup, vuruş, bağırış (§4.4, §5.3). Sahnelemeye: uluma kaydı, pati izi, Adli Tabip, leke (§8.2). Nöbet sabotajına: kül türü, ses. |
| Ç4 | Ölçüm | **Evet** | S1.1/S1.2 gece (Kasaba), S1.4, S3.1, S4.6, S6.1 sayıları; F1–F9 (§15). |
| Ç5 | Bağlantı | **Evet** | Defter, Vaka Dosyası (yara sınıfı), botlar, roller (Adli Tabip, Kurtadam, Avcı, Çoban, Bekçi), harita (bölge kapısı = Sis Duvarı), Hazırlık barı, fener görevi, balıkçılık (yem). |
| Ç6 | Ölçek | **Evet** | N = 6'da 2 sektör, 1 sürü × 2 kurt, 1 dil, nöbet 2 kişiyle mümkün; N = 20'de 4 sektör, 2 sürü, 2 kamp. Risk: N = 6'da tek PvE ölümü lobinin %17'si; F1'in N ≤ 9 bandı (%10) bu yüzden daha sıkı. |
| Ç7 | Varyans | **Evet** | Metreler ve hedef seçimi deterministik; inler tohumlu ve şafakta duyurulur; ısırık hasarı sabit. |
| Ç8 | Bot | **Evet** | Botlar kendi algılarıyla (uluma, göz, kendi aşaması) kaçar, grup olur, nöbet tutar. Sahneyi algılamak Roadmap 023'e bağlı (alt tabloda Parçala için Hayır). |
| Ç9 | Sessiz yol | **Evet** | Her telgrafın görsel ve altyazılı hâli var; "Yardım!" radyal menüden sessiz çalışır; nöbet defteri yazılı. |
| Ç10 | Okunabilirlik | **Evet** | "Ormanda yalnız kalma: ışık, patika ve arkadaş seni korur." Sahne kuralı: "Kurtlar ulumadan saldırmaz." |
| Ç11 | Frustrasyon | **Evet** | ≥ 20/25 sn telgraf sunucuda zorlanır; 1. gün ve gece hasar yok; Sis yürüyerek bile kaçılır; duvar öldürmez; AFK korunur. |
| Ç12 | Tempo | **Evet** | Orta ve geç evreye hizmet eder (tırmanma §6); Kasaba'nın boş gecesini doldurur. Risk: uzak görevlerin yürüyüşü; uluma ve bant olayları algı sayıldığı için ölü zaman tanımını tetiklemez, yine de S1.1 gündüz raporda izlenir. |

**Sonuç:** 12/12 (Parçala alt mekaniğinde 11/12). **Koşullu kabul**: Ç2, Roadmap 020b sonunda Evet olmalıdır; olmazsa
orman tehditleri maç dışı (dev) kalır.

---

## 17. Onay listesi (kullanıcı için)

- [ ] **O1** — Bu tasarımı Backlog "Proposed (needs approval)"a al; sıra: doğrulama oturumu → 016 → 020a → 020b → F1 → F2 → F3.
- [ ] **O2** — Kurt modeli: Quaternius Ultimate Animated Animals (CC0; `AssetResearch.md`). İndirme için izin (ad, kaynak, boyut F1 başında sorulur).
- [ ] **O3** — Batı ve Kuzey Koru her N'de açık (GDD §3 "Orman sadece 15–20" yerine).
- [ ] **O4** — "Uzun görev" sınıfı (45–90 sn, oyuncu başına ≤ 1), SPRINT-016'nın 30–75 sn kuralına istisna.
- [ ] **O5** — Nöbet ödülü: başarılı Kasaba katılımcısı başına toplamın %5'i, gece tavanı %10.
- [ ] **O6** — MVP'de kurtlar öldürülemez (sadece kovulur).
- [ ] **O7** — Sis Duvarı öldürmez (güvenli hücreye geri çıkarır); sadece Sis dili öldürür.
- [ ] **O8** — Parçala (sahneleme) fiili Sabırsızlara verilsin (F3).
