# Orman: Tehditler ve Orman Görevleri (Kurtlar, Sis, Kamp Nöbeti)

> **Durum: Önerildi (kullanıcı onayı gerekiyor), v0.2, 2026-09-25.** Sadece tasarım. Kod, seviye ve mevcut belgeler
> değişmedi. Charter §0.1 (kapsam kuralı) gereği bu yeni bir sistemdir: onaylanırsa `Docs/Backlog.md` "Proposed (needs
> approval)" bölümüne ve oradan sprint kuyruğuna girer. Sprint sözleşmeleri: `Docs/Iterations/SPRINT-033-ForestThreats.md`
> ve `Docs/Iterations/SPRINT-034-ForestChores-Vigil.md` (§14).
>
> **v0.2 revizyonu (3 inceleme turundan sonra, özellik eklenmedi, sadece kesildi ve keskinleştirildi):**
> 1. **PvE öldürücülüğü düştü.** N ≤ 9'da ve oyun sonunda (canlı ≤ max(5, 2·tehdit + 1)) kurt ısırığı 0 hasar, Sis
>    yakalaması Sis Duvarı gibi geri çıkarır. Gece ısırığı 25 → 20, gece Derin kazancı +9 → +7/sn. F1 artık bir **kapı**:
>    PvE ölümleri ≤ %5 (N ≤ 9'da yapı gereği 0) ve Kasaba'nın PvE payı ≤ canlı payı + 10 puan; aşılırsa öldürücülük
>    lobide kapanır.
> 2. **Nöbet ödülü taraf-kör ve küçük.** İmzalayan her başarılı katılımcı sayılır (Sabırsız dahil); B = %2.5, gece tavanı
>    %5, maç tavanı %10; bar, canlı Kasaba'nın kendi görevleriyle ulaşabileceği tavanı aşamaz. Nöbet altını kaldırıldı.
>    N ≤ 7'de en fazla 2 imza. Varış penceresi max(0.55 × Gece, 60 sn), kamp ≤ 110 m yol.
> 3. **Meşale artık kalkan değil.** Gece ×0.5 / gündüz ×0.75 kazanç, ısırıksız yarıçap yok (ilk ısırık o kurdu kovar),
>    Sis dikkatini yarıya indirir ama durdurmaz; yeniden yakmak Reçine + kamp ateşi ister.
> 4. **Telgraf oyuncuya özel damgalanır** (paylaşılan uluma başkasının telgrafı sayılmaz). Aksama sadece koşuyu
>    yavaşlatır, Sis dili doğunca taşınan eşya düşer, AFK kuralı 10/20 sn. Derin bandın her yerinden en fazla 1 ısırık.
> 5. **Radar ve sızıntı kapandı.** Uluma 60 m'nin ötesinde sektörsüz duyulur; uluma ve nöbet imzası Kasaba Panosu'na
>    düşmez. Yem kesildi. Ormandaki kaplara silah parçası konmaz. Tabanca kurda ve Sis'e etki etmez. Tek bir "Islak"
>    durumu var (tabancayı da ıslatır). Cepte şifalı ot ve bandajlı kol izi kaldırıldı.
> 6. **Kurban Hakkı bağı:** grupta bir tehdit oyuncusuyla ≤ 60 sn önce birlikte olan kurbanın PvE ölümü, o takımın
>    Kurban Hakkı'nı harcar (Roadmap 027b geldiğinde; görünmez).
> 7. **Ölü zaman sayımı dürüstleşti:** kendi ürettiğin olaylar ve sana yönelmeyen, 30 m ötesi uluma S1.1'de algı sayılmaz.
>    Ç12 buna göre yeniden cevaplandı.
> 8. **Sprintler bölündü:** önce geometri ölçüm spike'ı (033a); Deep bandın gerçekten var olup olmadığı ölçülmeden kurt
>    ya da Sis kodu yazılmaz.
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
ısırık, o oyuncu için arada en az 20 sn), Derin bantta uzun kalırsa **Sis** (Beklenmeyenler'in sisi) peşine düşer ve
yakalarsa öldürür. İkisi de **asla rastgele değildir**, taraf ayırt etmez ve her aşaması görülür ya da duyulur. **Küçük
lobide (N ≤ 9) ve oyun sonunda orman öldürmez**: ısırık sendeletir, Sis geri çıkarır. Orman, köye ödül getiren dört yeni
**fiziksel görev** (SPRINT-016 çerçevesi) ve riskli **Kamp Nöbeti** ile doludur: 2–4 kişi geceyi kampta ateşi
söndürmeden geçirirse Hazırlık barı küçük, taraf-kör bir parça alır. Çıkarım açısından en önemli şey: kurt ölümü ile
cinayet **ayırt edilebilir** (oyuncuya özel uluma kaydı, ısırık/bıçak yarası, ine doğru pati izli sürükleme) ve Sabırsız
bir cinayeti kurt saldırısı gibi **sahneleyebilir** ya da birini sise **yalnız bırakabilir**. Her şey Olay Defteri'ne
(Roadmap 020b) düşer.

---

## 1. Oyuncu deneyimi

### 1.1 Dört an
1. **Gündüz, 2. gün.** Elif'in listesinde "Kuru dal topla" var. Dal yığınları patikadan 15–25 m içeride, ağaçların
   arasında. Yakından bir uluma geliyor; köyde herkes sadece "uzakta bir kurt" duyuyor. Elif yalnız. Ya patikaya döner
   ya da işi bitirmek için 10 saniye daha kalır. Kalır. Ağaçların arasında iki çift göz beliriyor. Koşar, patikaya
   varır, kurtlar sınırda durur. Toplantıda "Batı Koru'daydım, uluma beni kovaladı" der. Defterlerde o saatte bir uzak
   uluma var: bu **yumuşak bir destek**tir, alibi değil. Uluma birinin ormanda yalnız olduğunu gösterir, kim olduğunu
   değil; aynı ulumayı ormandaki bir katil de sahiplenebilir. Oduncu kampında Elif'i gören biri varsa ancak o zaman
   hikâye sağlamlaşır.
2. **Alacakaranlık.** Toplantı biter. Kamp defterine sabah iki isim yazılmış: Bora ve Kaan. İkisi meşalelerle kuzey
   ormanına yürür. Köyden, tepede kamp ateşinin yandığı görülür.
3. **Gece.** Kampta ateş söndükçe kurtların gözleri yaklaşıyor. Biri oduna gidiyor, diğeri ateşi besliyor. Gece
   ortasında ateş sönüyor. Köyden bakan Zangoç kuzeydeki ışığın söndüğünü görüyor.
4. **Şafak** (N = 14). Bora kampta tek başına. Kaan'ın cesedi kampın 20 m dışında, ısırık yaralarıyla. Toplantıda Bora
   "kurtlar" diyor. Ama o gece kimsenin defterinde **uluma yok** (uzak uluma dahil; uluma 160 m'den duyulur), ceset
   **ine doğru sürüklenmemiş** ve ateşin külleri **ıslak** (söndürülmüş, kendiliğinden bitmemiş). Adli Tabip: "Önce
   bıçak, sonra ısırık." Köy oy verir.

### 1.2 Tasarım ilkeleri (bu belgenin kuralları)
| # | İlke | Somut kural |
|---|---|---|
| P1 | **Önceden görülmeyen ölüm yok** | Bir oyuncu kurt hasarı almadan önce **o oyuncu için** ≥ 20 sn, Sis ölümünden önce ≥ 25 sn telgraf vardır. Telgraf oyuncuya özel damgalanır (§4.2): başkası için atılmış bir uluma senin saatini başlatmaz. Sunucu bu kuralı zorlar ([T], F2). |
| P1b | **Küçük lobide ve oyun sonunda PvE öldürmez** | N ≤ 9'da ve canlı ≤ max(5, 2·tehdit + 1) iken ısırık 0 hasar (sendeleme + stamina), Sis yakalaması Sis Duvarı gibi geri çıkarır (`KG_PVE_LETHAL`). Tek bir PvE ölümü küçük lobide lobinin %11–17'sidir ve pariteyi Sabırsız'a iter (S6.5, S10.5). |
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

Not: GDD §3 "Orman"ı sadece N 15–20'de açıyor. Bu belge Batı ve Kuzey Koru'yu her N'de **öldürmeyen** hâliyle açmayı
önerir (oduncu ve nöbet kampı küçük lobilerde de olsun diye): N ≤ 9'da telgraflar, sendeleme ve Sis geri çıkarması var,
ölüm yok (P1b). Öldüren orman N ≥ 10'da başlar. Onay listesinde (§17, O3).

**Geometri uyarısı (ölçülmeden kod yok):** `nav_bounds` x/y ≈ ±120 m ve z ≤ 45 m'de biter; orman zemini r ≈ 95 m'nin
ötesinde 7 m'den 40–55 m'ye yükselir. Yürünebilir orman bu yüzden ~25 m'lik bir halka olabilir ve Derin bant (> 30 m)
neredeyse hiç var olmayabilir. SPRINT-033a bant raster'ını üretir ve bant/sektör başına yürünebilir m²'yi raporlar.
Sonra kullanıcı karar verir: ya `KG_FOREST_EDGE`/`KG_FOREST_DEEP` eldeki alana ölçeklenir (ör. 8/20 m), ya da
SPRINT-022'nin v2 sahibinden `nav_bounds` genişletilip yeniden inşa istenir (I2).

### 2.4 Sınır: kaybolmayı önlemek
1. **Fiziksel:** dışta dağ halkası (zemin 40–55 m), `nav_bounds` dışı navmesh yok.
2. **Sis Duvarı:** oynanabilir ormanın dış kenarında ve kapalı sektörlerin girişinde duran, yavaş dalgalanan sis
   perdesi. GDD §3'ün "sis duvarı" bölge kapısı ile **aynı nesnedir** (Roadmap 031a bunu kullanır). Duvara giren
   oyuncu **ölmez**: 2 sn beyaz ekran, sonra **en yakın güvenli hücreye**, köye dönük olarak geri çıkar. 60 sn **Islak**
   (damlayan su, görünür ve duyulur; bir iz, §8.3) ve stamina 0. **Islak tektir:** ganimet belgesinin ıslak tabanca
   durumuyla aynı durumdur; sisten dönen oyuncunun tabancası da ıslanır (barut nemlenir) ve bir ateşin 3 m içinde 10 sn'de
   kurur. Böylece duvar bir tabancalının bedelsiz kaçışı değildir. Lore: "Sise giren, geldiği yere geri çıkar."
3. **Işıklı iz:** her iz patikasında 15 m'de bir **yol feneri** (gece yakılır, Yol Fenerleri görevi) ve 20 m'de bir
   ağaç işareti (boya bandı). Patikadan 12 m'ye kadar görünür.
4. **HUD:** Orta/Derin bantta pusulada **"Köy" ve en yakın "Patika"** okları sürekli görünür. Harita orman patikalarını
   ve kampları çizer. İlk girişte bir kez: "Derin Orman. Yalnız kalma, ışıktan ayrılma."
5. **Veri kuralı ([L], F9):** her yürünebilir orman hücresinden en yakın güvenli hücreye ≤ `KG_FOREST_SAFE_MAX` (45 m).
   Bu kural sağlanmazsa veri reddedilir. Sis'in kaçılabilirliği (§5.3) bu sayıya dayanır.
6. **AFK:** Orta/Derin bantta **10 sn** girdi yoksa o oyuncunun kurt ilgisi ve Sis dikkati donar; **20 sn**'de Sis
   Duvarı gibi en yakın güvenli hücreye geri çıkarılır (ölmez). Eski 60 sn kuralı işe yaramıyordu: hareketsiz oyuncu gece
   ~41 sn'de (kurt) ya da ~36 sn'de (Sis) ölürdü. Böylece Sabırsız, AFK birini ormana "park edip" tehditlere yediremez.

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
| `KG_TORCH` | kurt kazancı gündüz ×0.75, gece ×0.5; Sis dikkati dolumu ×0.5 | Elde yanan meşale. **Güvenli yarıçap değildir**: ısırık engellemez; meşale tutanı ısıran ilk kurt `KG_WOLF_REPEL` ile kaçar |
| `KG_LANTERN_SAFE` | 4 m | Yanan yol feneri |
| `KG_HEAR_HOWL` | 160 m; sektör adı sadece ≤ 60 m'de (`KG_HOWL_SECTOR_R`) | Uluma: köy "uzakta bir kurt" duyar, yeri değil |
| `KG_PVE_LETHAL` | N ≥ 10 **ve** canlı > max(5, 2·tehdit + 1) | Bu koşul yoksa ısırık 0 hasar, Sis yakalaması = Sis Duvarı dönüşü |
| `KG_FOREST_AFK` | 10 sn: metreler donar; 20 sn: güvenli hücreye dönüş | Orta/Derin bantta girdi yok |
| `KG_HEAR_GROWL` | 20 m | Hırlama (gözler aşaması) |
| `KG_HEAR_SNARL` | 30 m (= `KG_HEAR_SCREAM`) | Saldırı hırıltısı + kurbanın çığlığı |
| `KG_HEAR_MAUL` | 12 m | Sahneleme (Parçala) sesi |
| `KG_WOLF_HOWL_MIN` | 12 sn | Uluma aşamasından gözler aşamasına en az |
| `KG_WOLF_EYES_MIN` | 8 sn | Gözler aşamasından ilk ısırığa en az |
| `KG_WOLF_TELEGRAPH_MIN` | 20 sn | Bir oyuncuya ilk ısırık, **o oyuncunun telgraf damgasından** en az bu kadar sonra. Damga: oyuncunun ilgisi 35'i ilk geçtiği an, sektörde canlı bir uluma varken ya da yeni uluma atılırken |
| `KG_WOLF_BITE` | 20 hasar (gündüz ve gece); `KG_PVE_LETHAL` yoksa 0; aynı hedefe 4 sn'de en fazla 1; 0.4 sn sendeleme, −15 stamina | Isırık |
| `KG_WOLF_SPEED` | sinsi 2.2, tırıs 4.0, atak 6.4 m/s (en fazla 4 sn), sonra 4.0 | Oyuncu: yürüme 3.2, koşu 5.8 (stamina 100, −18/sn ≈ 5.5 sn ≈ 32 m) |
| `KG_WOLF_REPEL` | vuruş/itme: o kurt 15 sn kaçar, sürünün o oyuncuya ilgisi −40 | Karşı koymak |
| `KG_MIST_NOTICE` | gündüz 40 sn / gece 20 sn (yalnız, Derin); meşaleyle iki katı | Sis dikkati dolma süresi |
| `KG_MIST_SPAWN` | hedefin 40 m arkası, **en yakın güvenli hücrenin ters yönü**, görüş konisi dışı | Sis dili doğduğu yer |
| `KG_MIST_SPEED` | 2.4 m/s başlar, +0.08 m/s², en fazla 4.2 m/s | Yürüyen oyuncu 10 sn sonra geçilir, koşan hiç |
| `KG_MIST_CORE` | 3 m yarıçap; içinde 3 sn = ölüm (`KG_PVE_LETHAL` yoksa Sis Duvarı dönüşü); çıkınca sıfırlanır | Donma |
| `KG_MIST_TELEGRAPH_MIN` | 25 sn | İlk görünür Sis işaretinden (kırağı) ölüme en az |
| `KG_MIST_CREEP` | gece başına 15 m, en fazla 2 adım | Sis Duvarı'nın ilerlemesi (§5.5) |
| `KG_WET` | 60 sn (ateş başında 10 sn) | **Tek** Islak durumu: Sis Duvarı dönüşü, bele kadar su (ganimet belgesi §3.7). Taşınan tabancayı da ıslatır |
| `KG_TORCH_BURN` | 90 sn | Meşale ömrü. Yeniden yakma: 1 Reçine + kamp ateşi |
| `KG_FIRE_FUEL` | 100 birim, −2.5/sn, kütük +50 | Kamp ateşi (dolu ateş 40 sn yanar) |
| `KG_LIMP` | 60 sn: koşu hızı 5.8 → 4.0 m/s; **yürüme değişmez** | Isırık sonrası aksama (Sis'ten yürüyerek kaçış her zaman korunur) |
| `KG_VIGIL` | 2–4 kişi (N ≤ 7'de en fazla 2); varış: Gece başı + max(0.55 × Gece, 60 sn); ateş ≥ %90 yanık; kamp halkası 12 m; kamp meydandan ≤ 110 m navmesh yolu | Kamp Nöbeti |

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
- **Tabanca kurtlara ve Sis'e etki etmez** (ganimet belgesiyle ortak kural). Böylece "kurda ateş ettim" duyulan bir
  atışın ya da boş bir tabancanın doğrulanamaz bahanesi olamaz; iddia uluma ve göz kayıtlarıyla kontrol edilir.
- N ≤ 9'da sürü aynıdır ama öldürmez (P1b). Telgraflar, gözler ve sendeleme birebir aynı: oyuncular kuralları küçük
  lobide de öğrenir.

### 4.2 İlgi metresi (sunucu, oyuncu başına, 0–100)
Sunucu her oyuncu için bir kurt ilgisi tutar. Değer **sadece sunucuda**dır. Oyuncu sayıyı görmez, **aşamayı** görür ve
duyar.

| Durum (4 Hz güncellenir) | Gündüz (2. gün+) | Gece |
|---|---|---|
| Köy, Kıyı, patika, sabit bir ışık yarıçapı (kamp ateşi, yol feneri; elde meşale değil) | −8/sn | −8/sn |
| Orta bant, yalnız | 0 (4. günden sonra +2) | +5 |
| Derin bant, yalnız | +3 | +7 |
| Grupta, 2 kişi | kazanç ×0 | kazanç ×0.5 |
| Grupta, ≥ 3 kişi | kazanç ×0 | kazanç ×0 (sadece çevrede dolaşırlar) |
| Elde yanan meşale | kazanç ×0.75 | kazanç ×0.5 (ısırık engellemez; ilk ısırıkta o kurt kaçar) |
| Çiğ balık/et taşıyor (envanter ya da elde) | ×1.5 | ×1.5 |
| Can < 50 | ×1.25 | ×1.25 |
| Patikaya doğru olmayan koşu (orman içinde) | +3 | +3 |
| Kazanç yokken sönüm | −3/sn | −3/sn |

**Eşikler ve aşamalar** (histerezisli):

| Aşama | Giriş | Ne olur | Çıkış |
|---|---|---|---|
| 0 Sessiz | — | Kuş sesleri, rüzgâr | — |
| 1 **Uluma** | ilgi ≥ 35 | Sektörden uluma (`KG_HEAR_HOWL`, yönlü; sektör adı sadece ≤ 60 m'de). Sürü o oyuncuya doğru tırısa kalkar. Sektör başına 45 sn'de en fazla 1 **duyulan** uluma. İkinci bir oyuncu sektörde zaten canlı olan bir ulumanın içine girerse ona ayrı uluma çalınmaz, ama **telgraf damgası** onun ilgisinin 35'i geçtiği an basılır; bu oyuncu için 12/8/20 sn sayaçları bu damgadan başlar ve hedeflenen oyuncu yakından (≤ 20 m) kısa bir hırlama duyar. | ilgi < 20 |
| 2 **Gözler** | ilgi ≥ 65 **ve** o oyuncunun damgasından ≥ 12 sn | 2 kurt 15–22 m'de görünür, çevresinde döner. Gece gözler parlar (25 m'den görünür). Hırlama `KG_HEAR_GROWL`. | ilgi < 35 |
| 3 **Saldırı** | ilgi = 100 **ve** Gözler'den ≥ 8 sn **ve** damgadan ≥ 20 sn | Kurtlar sırayla atılır (≤ 6 m'den). Isırık: `KG_WOLF_BITE`. Isıran kurt 3 sn geri çekilir (vur-kaç). | ilgi < 65 |

**Zamanlama örnekleri** (yalnız, meşalesiz, ışıksız, `KG_PVE_LETHAL` açık, oyuncu hiç kıpırdamasa; AFK kuralı §2.4/6
normalde 20 sn'de devreye girer, bu tablo "girdi var ama kaçmıyor" durumudur):

| Durum | İlk uluma (damga) | Gözler | İlk ısırık | 100 candan ölüm (en erken, 5 × 20) |
|---|---|---|---|---|
| Derin, gece (+7/sn) | 5.0 sn | 17.0 sn | 25.0 sn | 25 + 4 × 4 → ~41 sn |
| Orta, gece (+5/sn) | 7.0 sn | 19.0 sn | 27.0 sn | ~43 sn |
| Derin, gündüz (+3/sn) | 11.7 sn | 23.7 sn | 33.3 sn | ~49 sn |

Yani ölmek için ≥ 41 sn boyunca uluma, göz ve ısırıkları görmezden gelmek gerekir. Bu P1'in sayısal hâlidir.

**Kaçış garantisi (düzeltilmiş):** Gözler aşamasında (kurtlar 15–22 m'de) koşmaya başlayan oyuncu, Derin bandın herhangi
bir noktasından (en yakın güvenli hücreye ≤ `KG_FOREST_SAFE_MAX` = 45 m) **en fazla 1 ısırık** yer. Kıyıya ≤ 32 m'den
koşan hiç ısırılmaz. Bu iki cümle [T] vakasıdır (SPRINT-033c).

### 4.3 Hedef seçimi ve hareket (deterministik)
- Sürü, ilgisi en yüksek oyuncuyu hedefler. Eşitlikte en yakın, sonra `PlayerKey` sırası. Yazı-tura yok.
- 2 kişilik grupta gece saldırı olursa hedef sırası: **en yakın ışığa ya da güvenli hücreye daha uzak olan**, eşitse
  ilgisi yüksek olan, eşitse `PlayerKey` sırası (kenardan avlanma; "merkezden uzak" tanımı 2 kişide anlamsızdı). Işığa
  yakın duran korunur: "ateşe sırtını ver" tavsiyesi buna dayanır.
- Kurtlar navmesh'te bir **Orman nav alanı** filtresiyle yürür: teras poligonları ve ışık yarıçapları geçilmez
  (nav modifier), gündüz lane yüzeyi geçilmez.
- Kurtların teraslardan ve ışık yarıçaplarından uzak durması **nav modifier ile değil**, sunucu kuralıyla yapılır:
  hedef nokta ve yol düğümleri bant raster'ından ve aktif ışık listesinden süzülür. Sebep: navmesh bugün statiktir
  (çalışma zamanı üretimi ayarı yok); ateş yanıp söndükçe navmesh'i yeniden kurmak hem maliyetli hem de
  `Config/`'e dokunur.
- Koşan oyuncuyu kovalarlar: 6.4 m/s ile en fazla 4 sn, sonra 4.0. Gözleri görünce (15–22 m) koşan oyuncu ≤ 32 m'deki
  patikaya ısırılmadan varır; Derin'in en uç noktasından (45 m) en fazla 1 ısırık yer (§4.2).

### 4.4 Karşı hamleler (hepsi herkese açık)
| Karşı hamle | Etki | Bedeli |
|---|---|---|
| **Patikaya dön** | İlgi −8/sn. Gündüz kurtlar patikaya çıkmaz; gece çıkar ama ısırmaz. | Görev yarım kalır |
| **Grup** | 2 kişi: gündüz saldırı yok, gece yarı hız. ≥ 3 kişi: hiç saldırı yok. | Birlikte gidilen kişi katil olabilir |
| **Meşale** | İlgi kazancı gece ×0.5, gündüz ×0.75; seni ısıran ilk kurt 15 sn kaçar; Sis dikkati yarı hızla dolar. **Kalkan değildir**: sadece zaman kazandırır. | 90 sn yanar. Uzaktan görünürsün (gece 120 m'den bir ışık). Günde kişi başı 1 (kamp meşale kutusu); yeniden yakmak 1 Reçine + kamp ateşi ister (her ateş değil). |
| **Ateş / yol feneri** | Yarıçapı içinde saldırı yok, Sis giremez. | Sabittir, yakıt ister, söndürülebilir |
| **Karşı koy** | Atılan kurda yumruk ya da itme: o kurt 15 sn kaçar, ilgi −40. | İtme 25 stamina. Yanlış anda vurursan ısırık yersin. |
| **Bağır** | SPRINT-023'ün "Yardım!" / "Buraya!" sesli komutu (ses olmadan da çalışır, G1.1): 10 m içindeki kurtlar 3 sn duraksar, ilgi −20. | 30 sn'de 1. Bağırış 40 m'den duyulur: yerini söyler. |
| **Sargı Bezi** | Isırıkların canını geri verir, aksamayı durdurur (ganimet belgesi §3.5 sayıları). | Envanter yeri. İz tektir: ganimet belgesinin **Kanlı Sargı** yer prop'u (90 sn); ayrı "bandajlı kol" izi yok. Sis dili seni hedeflerken sargı başlatılamaz (%50 hızla sisten kaçılmaz). |
| (Tabanca) | **Etkisiz.** Kurtlara ve Sis'e ateş etmek hiçbir şey yapmaz, sadece 60 m'den duyulur. | — |

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
- Katil alanı boştur (PvE). Kazanma hesabında ölüm ölümdür; bu yüzden F1 (PvE payı kapısı) önemlidir.
- **PvE ölümü ve Kurban Hakkı (Roadmap 027b ile):** kurban, öldürücü eşiğe (ilk ölümcül ısırık ya da Sis çekirdeği)
  varmadan önceki 60 sn içinde bir tehdit tarafı oyuncusuyla "Grupta" (`KG_GROUP_R`) bulunduysa bu ölüm o takımın o
  geceki Kurban Hakkı'nı harcar. Hak yoksa bir sonraki gecenin hakkı düşer (ganimet belgesindeki borç kuralı). Kural
  sunucudadır ve **hiçbir şey açığa vurmaz** (sayaç kimseye gösterilmez). Böylece "söndür ve yürü git", "Derin'e götür
  ve bırak" bedava öldürme değildir. 027b'den önce öldürme bütçesi hiç olmadığı için bu kural da yoktur.

---

## 5. Sis (Beklenmeyenler'in sisi)

Lore: ufuktaki Sis hiç kalkmaz. İçinde **Beklenmeyenler** yaşar, sise yürüyenlerin boşluktan silüetleri
(`KillGo_Lore.md` §4). "Fener asla sönmemeli. Sönerse sis içeri girer, sisin içinde yaşayan da." İhtiyar Tuz: "Sis bir
duvar değil evlat, bir ayna." Oyundaki Sis iki parçadır: **Sis Duvarı** (sınır, öldürmez, §2.4) ve **Sis Dili**
(yalnızları kovalar, öldürür).

### 5.1 Sis dikkati (sunucu, oyuncu başına, 0–100)
- Dolar: oyuncu **yalnız** (charter §0.4) **ve** Derin banttaysa **ve** hiçbir ışık yarıçapında değilse. Hız: gündüz
  100/40 sn, gece 100/20 sn. 3. geceden itibaren gece Orta bantta da 100/45 sn. Elde yanan meşale dolum hızını
  yarıya indirir (durdurmaz). Oyuncu `KG_FOREST_AFK` ile donmuşsa dolmaz.
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
  arada ~44 m kalır. Koşan oyuncu (5.8 m/s) hiç yakalanmaz.
- **Yavaş durumlar da kaçar (kural, [T]):** yürüme hızını 3.2 m/s'nin altına düşüren hiçbir durum Sis dili hedefindeyken
  sürmez:
  - Aksama (`KG_LIMP`) sadece koşuyu yavaşlatır, yürümeyi değil.
  - Sis dili doğduğu an hedefin taşıdığı dünya eşyası (Çadır Bezi 1.76 m/s, kova, sandık) **elinden düşer**; olay
    `CarryDropped {sebep: sis}` deftere yazılır. Eşya yerde kalır, sonra alınabilir.
  - Sargı (%50 hız) dil hedefindeyken başlatılamaz; başlamış sargı dil doğunca kesilir.
  Bu yüzden "birini yaralat ya da yük taşıt, sonra bırak git" bir Sis tuzağı değildir. [T] vakası: her yavaş durumdan,
  `KG_FOREST_SAFE_MAX` mesafesinden yürüyerek kaçış.
- Hiç kıpırdamayan ama girdisi olan oyuncu doğuştan ~12.7 sn sonra çekirdekte (37 m), 3 sn sonra ölür (ya da
  `KG_PVE_LETHAL` yoksa geri çıkar). Gece: kırağıdan (dikkat 50, 10. sn) ölüme ~25.7 sn ≥ `KG_MIST_TELEGRAPH_MIN`
  (25 sn). Gündüz: ~35.7 sn. Girdisi olmayan oyuncu 20 sn'de zaten güvenliğe döner (§2.4/6).
- Hedef güvenliğe (Kıyı bandı, ışık, grup) varınca dil durur ve 6 sn'de dağılır. **Hedef değiştirmez.** Kovalanan
  birine yetişen ikinci oyuncu onu kurtarır ("Sis yalnızları sever").
- Tavan: harita genelinde aynı anda en fazla N 6–9: 1, 10–14: 2, 15–20: 3 dil. Tavan doluysa yeni dikkat 99'da bekler
  (ölüm yok, sadece gecikme).

### 5.4 Sis ölümü
- `KG_PVE_LETHAL` yoksa (N ≤ 9, oyun sonu) yakalanan oyuncu ölmez: Sis Duvarı dönüşü (2 sn beyaz, güvenli hücre, Islak).
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
| 2.–3. gece | Orta (+5) ve Derin (+7). | Derin (20 sn). Fener doldurulmadıysa duvar ilerler. | Nöbet açılır. Kasaba'nın gecesi dolar. |
| 4. gün ve sonrası ya da canlı ≤ N/2 | Orta bantta gündüz de (+2/sn). | 3. geceden sonra gece Orta'da da (45 sn). | Geç evre: daha yüksek risk (S10.3). |
| Oyun sonu: canlı ≤ max(5, 2·tehdit + 1) | Telgraflar sürer, **ısırık 0 hasar**. | Yakalama = Sis Duvarı dönüşü. | Pariteyi bir kurt belirlemesin (S10.5: son eleme asma ya da yetenek). |
| Her gün, N ≤ 9 | Telgraflar ve sendeleme, **hasar 0**. | Yakalama = geri çıkarma. | P1b. |
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

**Derin bant kuralı (lint, [L]):** dağıtılan hiçbir görev adımı Derin bantta değildir. Görev adımları Kıyı ve Orta
bantta kalır; bir varyantın Orta'da kesintisiz geçen süresi gündüz ≤ 20 sn'dir (`gen_world_chores.py` ve orman verisi
lint'i). Derin'e sadece **isteğe bağlı** toplamalar (Reçine) götürür. Sebep: Kasaba görev alır, Sabırsız görevini
sahteler ve ormana hiç gitmeyebilir; Derin'de görev, PvE yıpranmasını Kasaba'ya yıkardı.

**Dağıtım gerçeği:** görevler bugün maçta bir kez dağıtılır (`KGGameMode.cpp:764`) ve dünya görevi noktaları şafakta
sıfırlanmaz. Orman görevleri de maçta bir kez dağıtılır. "Her gün" tekrar eden şeyler (fenerlerin sönmesi, odunluk,
ateş) görev değil, **dünya durumu**dur ve şafakta sunucu tarafından sıfırlanır.

### 7.2 MVP görevleri (4 yeni + mevcut Firewood)
| Id | Başlık | Adımlar (fiil: hedef, sn) | Bant | Süre | Hazırlığa sayılır mı | Sahte (Sabırsız) | Sabotaj |
|---|---|---|---|---|---|---|---|
| `ForestDeadwood` | Kuru dal topla | work: 6 dal yığınından varyantın 3'ü, 2.0 sn her biri → `Bundle` çıkar (14 kg, hız 0.85) → bring: kamp odunluğu 1.5 sn (`Stack` +3 kütük) | Orta | 60–80 sn | Evet, 1 | Aynı görünür. Kütükler **gerçekten** odunluğa girer (fiziksel), bar dolmaz | — |
| `PitchCamp` | Kamp kur | take: oduncu barakasında `Canvas` (yeni eşya, 30 kg, hız 0.55, **iki kişilik**) → bring: kamp açıklığı 2 sn → work: 3 kazık (hepsi), 1.5 sn → çadır kurulur (`Stack`) | Kıyı → Orta | 60–90 sn (iki kişiyle ~45 sn) | Evet, 1 | Çadır gerçekten kurulur, bar dolmaz | Sonra: "çadırı yık" |
| `TrailLanterns` | Yol fenerlerini yak | take: oduncu fener kutusunda `Taper` → bring: iz patikasındaki 5 fener (hepsi), 1.0 sn (`Light`) → bring: kutuya geri | Kıyı | 60–80 sn | Evet, 1 | Fenerler gerçekten yanar | `snuff` (mevcut 016 sabotajı): gece fener söner, Sis ve kurt o noktaya yaklaşabilir |
| `ForestHerbs` | Şifalı ot topla | work: 6 ot kümesinden varyantın 3'ü (Kıyı/Orta; **Derin'de küme yok**), 1.5 sn → `Herbs` sepeti → bring: kilisenin şifa masası 1.5 sn | Kıyı/Orta | 60–80 sn | Evet, 1 | Aynı görünür. Otlar sadece masaya gider; **cebe ot ödülü yok** (şifa arzı ganimet belgesinin sargı bütçesinde kalır) | — |
| `Firewood` (mevcut) | Odun kır | work: `chop_block` 2.2 sn × 3 → bring: `inn_woodbox` | Kıyı | mevcut | Evet, 1 | mevcut | — |

Kurallar:
- `PitchCamp` kamp başına maçta bir kez dağıtılır. Çadır maç boyunca kalır. **Nöbetin önkoşuludur** (§7.4).
- Fenerler şafakta söner (dünya durumu). `TrailLanterns` görevi maçta bir kez dağıtılır; fenerleri herkes her gün
  taper ile yakabilir (dünya nesnesi, bara sayılmaz). Yanan fener sayısı gece görünürdür: köyden bakınca kuzeydeki ışık
  dizisi.
- Kamp odunluğu 0–12 kütük tutar, gösterilir (yığın büyür). Nöbetin yakıtıdır.

### 7.3 Kamp ateşi (görev değil, dünya nesnesi)
Ateşi yakmak bir görev değil, **herkesin kullanabileceği bir dünya nesnesidir** (Sabırsız dahil). Aksi hâlde "ateşi
yakamayan Sabırsızdır" diye fizikten rol okunurdu (P2).

| Eylem | Süre | Kim | İz / ses |
|---|---|---|---|
| Yak | 4 sn, `Çıra` ile (kampın çıra kutusu: gece başına 3 çıra, şafakta dolar); kor varsa (sönmeden ≤ 20 sn) 2 sn | Herkes | Alev sesi 20 m. Gece 120 m'den ışık görünür. `FireLit` |
| Meşale yak | 2 sn, 1 Reçine harcar, sadece yanan kamp ateşinde | Herkes | `TorchLit`. Köy ocakları ve fenerler meşale yakmaz |
| Besle | 1 sn, elde bir kütük (odunluktan alınır, 3 kg) | Herkes | `FireFed` (sadece nöbette sayılır) |
| Söndür: su | Dolu kova (016 kovası, dolum ≥ 0.3) ateşe dökülür: anında | Herkes | Tıslama 25 m. **Yaş kül** şafağa kadar |
| Söndür: dağıt | 3 sn tekmeyle | Herkes | 20 m. **Dağılmış kor** şafağa kadar |
| Kendiliğinden söner | Yakıt 0 | — | **Soğuk kül** |

Yakıt: `KG_FIRE_FUEL` (100, −2.5/sn, kütük +50). Yakıt < 20 iken güvenli yarıçap 10 m'den 6 m'ye iner ve alev görünür
şekilde küçülür (telgraf). Ateş yanarken kurtlar 12–18 m'de gözleriyle döner, saldırmaz. Söndükten 8 sn sonra normal
kurallar döner.

### 7.4 Kamp Nöbeti ("bir gece hayatta kal"): riskli, ortak, taraf-kör
**Kim:** herkes. Dağıtılmaz, gönüllüdür. **Nerede:** Avcı Kampı (N1); N ≥ 15'te Doğu Sırtı'ndaki ikinci kamp da.
**Veri kuralı ([L]):** nöbet kampı meydandan ≤ 110 m navmesh yolu (≤ 35 sn yürüyüş) ve en yakın köy lane'inden ≥ 15 m
uzakta olmalıdır. N1'in bugünkü aday bölgesi (x −82…−35, y −118…−82 m) bu kuralı sağlamazsa kamp noktası orman
verisinde yakına çekilir (çalışma zamanı spawn; v2 sahibiyle sıra, §13).

| Kural | Değer |
|---|---|
| Açık olduğu geceler | 2. gece ve sonrası |
| Önkoşul | Çadır kurulu (`PitchCamp`) ve odunlukta ≥ 4 kütük, gündüz bitiminde |
| Kayıt | Kamptaki **nöbet defteri**ne gündüz imza (2 sn, görünür). Defter kampta duran, okumak için yanına gidilmesi gereken bir dünya nesnesidir. **Kasaba Panosu'na düşmez** (S2.5: kendiliğinden duyurulan keşfedilebilir bilgi 0; kimin evden uzak olacağını herkese ilan etmez). |
| Kişi | En az 2, en fazla 4 imza; **N ≤ 7'de en fazla 2**. Gece başında < 2 ise nöbet iptal (imzalayanlara bildirim). |
| Varış | Kamp halkasına (12 m) Gece başı + max(0.55 × Gece, 60 sn) içinde: N = 6: 60 sn, N = 12: 61 sn, N = 20: 74 sn. Kamp ≤ 110 m olduğu için toplantı ışınlamasından sonra ≥ 25 sn pay kalır |
| Kalış | Varıştan şafağa, halka dışında toplam ≤ 15 sn |
| Ateş | Varış süresinin sonundan şafağa kadar sürenin ≥ %90'ında yanık |
| Başarı | Şafakta canlı + kalış + ateş koşulları |
| Ödül (Hazırlık) | **Taraf-kör:** koşulları sağlayan **her** imzalı katılımcı (Sabırsız dahil) B = max(1, round(0.025 × T)) görev birimi getirir. Gece tavanı max(1, round(0.05 × T)), maç tavanı round(0.10 × T). T = maçta dağıtılan Kasaba görev toplamı (bar paydası). |
| Bar tavanı | Nöbet birimleri barı, **canlı** Kasaba'nın kendi kalan görevleriyle ulaşabileceği seviyenin üstüne çıkaramaz: bar ≤ (tamamlanan + canlı Kasaba oyuncularının kalan görevleri) / T. Nöbet barı hızlandırır, ölen kasabalıların açtığı boşluğu doldurmaz. Bu kural G4.2 fener ifşasının yerini alan tasarım gelene kadar geçerlidir. |
| Ödül (altın) | **Yok** (v0.2'de kaldırıldı: ekonomide karşılığı yoktu ve taraf-kör bir çiftlik olurdu). |
| Bedeli | Evde değilsin: ev masasındaki Gece Defteri yeteneği o gece yok (027a'dan sonra), evin boş, oylama sonrası karanlıkta ~35 sn orman yürüyüşü. Kampta tek tehdit kurt değil, **yanındaki kişi**dir. |

**Neden taraf-kör:** eski kuralda sadece Kasaba katılımcıları sayılıyordu. Bar herkese replike olduğu için şafaktaki
sıçrama "imzalayan 2 kişiden kaçı Kasaba" sorusunu cevaplıyordu: +B yerine +2B görmek "biri Sabırsız" demekti. Bu
cinayet dışı bir sert kanıttı (S2.4). Şimdi sıçrama sadece "kaç kişi başardı" der; bu da nöbette kalıp kalmadıklarından
zaten görülebilir.

**Ödül ölçeği** (T ≈ Sabırsız olmayan × 4 görev, `KGGameMode.cpp:763-775`; yuvarlama yarımda yukarı):

| N | T (yaklaşık) | Kişi başı B | Gece tavanı | Maç tavanı | Gece / maç bar payı |
|---|---|---|---|---|---|
| 6 | 5 × 4 = 20 | 1 | 1 | 2 | %5 / %10 |
| 12 | 9 × 4 = 36 | 1 | 2 | 4 | %5.6 / %11 |
| 20 | 15 × 4 = 60 | 2 | 3 | 6 | %5 / %10 |

Nöbet bir oyuncunun tek görevinden biraz fazla değerlidir, bütün gün listesinden azdır. Bar tavanı kuralıyla birlikte
3. geceye kadar fener ifşasını tek başına tetikleyemez.

**Nöbetin dramı (bilerek):** 2 kişilik nöbette biri Sabırsız ise gece kamp en tehlikeli yerdir. 3–4 kişilik nöbet
güvenlidir ama köyü boşaltır (bu yüzden N ≤ 7'de 2 imza). Kiminle nöbet tutacağın bir **güven kararı**dır (S1.4).

### 7.5 Yeni eşyalar (`KGItemCatalog` önerisi)
| Eşya | Nereden | Ne işe yarar |
|---|---|---|
| **Meşale** (Torch) | Kamp meşale kutusu (kişi başı günde 1) ya da 1 Reçine + **kamp ateşi** | 90 sn ışık, §4.4 (kalkan değil). Mevcut mesh: `Torch_Metal`, `SM_KG_Pirate_Torch_*` |
| **Çıra** (Tinder) | Kamp çıra kutusu (gece başına 3) | Ateş yakmak |
| **Reçine** (Resin) | Derin bantta 4 çam kütüğünden (work 2 sn, günde kütük başı 1) | 1 reçine = 1 meşale. Derin riskinin tek isteğe bağlı ödülü |
| **Ot Sepeti** (Herbs) | `ForestHerbs` görevi | Sadece görev taşıma eşyası (kilise masasına). Cepte şifa eşyası değildir |
| **Çadır Bezi** (Canvas) | Oduncu barakası | `PitchCamp` taşıma eşyası (dünya eşyası, envanter değil) |

Sandıklar: Avcı Kampı'ndaki Hunter's Chest ve mağaradaki kaçakçı sandığı zaten var (`dress_wilds.py`). Ganimet
belgesiyle ortak kural: **orman kaplarına silah parçası konmaz** (Ganimet Yönetmeni orman bandındaki kapları parça
yerleşiminden hariç tutar). Sebep: grupta kurt ve Sis'e bağışık olanlar, birbirine güvenen Sabırsız takımıdır; orman
parçası katil takımını besler. Orman riskinin ödülü Reçine ve **Sargı Bezi**dir: Orta/Derin banttaki kaplarda sargı
ağırlığı ×2.

---

## 8. Çıkarım döngüsüne besleme

### 8.1 Olay Defteri olayları (Roadmap 020b'nin `FKGEvent`'i)
Her satır tek satırlık defter metnine döner (G3.5): "Gün 2 · 14:20 · Kuzey Koru: kurt uluması duydun."

| Olay | Alanlar | Tanıklık (020b tanık filtresi) | Kasaba Panosu (024) |
|---|---|---|---|
| `ForestEnter` | oyuncu, sektör, bant | Görenler (`KG_SIGHT_*`), `KG_REGION_DEBOUNCE` | — |
| `WolfHowl` | sektör, sürü, **telgraf damgaları** (sadece sunucu) | Duyanlar: `KG_HEAR_HOWL`; sektör adı sadece ≤ 60 m'deki tanığın satırında, ötesi "uzakta bir kurt" | **Hayır** (pano, "şu an X'te biri yalnız" radarı olurdu) |
| `WolfEyes` | sektör | Görenler | — |
| `WolfAttack` | kurban, sektör, hasar | Görenler + duyanlar (`KG_HEAR_SNARL`) | — |
| `WolfRepelled` | kim, yöntem (meşale-ısırığı/ateş/grup/vuruş/bağırış; **atış yok**) | Görenler | — |
| `CarryDropped` | kim, eşya, sebep = sis | Görenler | — |
| `WolfKill` | kurban, sektör, yara = Isırık, sürükleme vektörü | Görenler + duyanlar | Hayır (ceset bulunur, S2.5) |
| `MistNotice` | oyuncu, sektör | Oyuncunun kendisi + dili görenler | — |
| `MistCaught` | kurban | Görenler | Hayır |
| `MistWallReturn` | oyuncu, çıkış noktası | Varışı görenler | — |
| `FireLit` / `FireOut` | kamp, kim (sunucu gerçeği), neden (yakıt / su / dağıtma) | Görenler; gece 120 m'den ışığın yandığını/söndüğünü görenler (kim olmadan) | Sonra: "kuzeydeki ateş söndü" |
| `VigilSigned` | oyuncu, kamp | İmzayı görenler; sonra kamptaki defteri okuyanlar | **Hayır** (S2.5) |
| `VigilResult` | kamp, başaranlar | Sadece sunucu (duyurulmaz; hayatta kalanlar yürüyüp döner) | Hayır |
| `BodyMauled` | ceset, kim | Görenler + duyanlar (`KG_HEAR_MAUL`) | Hayır |
| `LampSnuffed` (026b) | fener, kim | Görenler | Sabah sönük fener görünür |

(`BaitDropped` ve Yem fiili v0.2'de kesildi: kurtları bir noktaya çekmek PvE'yi uzaktan hedeflemeye çeviriyordu.)

Botlar ve Vaka Dosyası (Roadmap 021) sadece kendi tanık olduklarını okur (G6.1, G7.1).

### 8.2 Kurt ölümü mü, cinayet mi, sahne mi?
| İpucu | Gerçek kurt ölümü | Cinayet (bıçak/künt) | Sahnelenmiş kurt ölümü |
|---|---|---|---|
| O sektörden, ölümden ≥ `KG_WOLF_TELEGRAPH_MIN` önce uluma | **Her zaman** (P1) | Tesadüfen | Katil Derin'de yalnızsa kendisi bir uluma tetikleyebilir; yani bu ipucu tek başına yetmez |
| Yara (herkes, 4 sn diz çöküp inceleme, 021) | Isırık | Bıçak / Künt | Isırık |
| Adli Tabip | "Isırık, kurt" | Silah sınıfı | "**Önce bıçak, sonra ısırık**" |
| Pati izi (yumuşak zemin, 026a) | Var | Yok | **Olabilir**: katile gelen sürü ceset çevresinde iz bırakır |
| Sürükleme | **Her zaman** 3–6 m ine doğru, pati izli | Yok ya da insan ayak izli (025b) | Yok ya da insan ayak izli; kurtlar sahnelenmiş cesedi sürüklemez |
| Duyulan | Hırıltı + çığlık 30 m | Backstab 15 m / çığlık 30 m | Yırtılma 12 m |
| Yer | Orta/Derin, patika ve ışık dışı; gündüz sadece Derin | Her yer | Orta/Derin (fiil kısıtı) |
| Katilde leke (026a) | — | 45–60 sn | 45–60 sn + Parçala'dan 45 sn |
| Kurbanın son durumu | Yalnız (ya da gece 2 kişi, ateş dışında) | Her durum | Her durum |

**Kamu kuralı (tek ve dürüst, oyuncuya ve bota öğretilir):** "Kurt ölümünde o sektörden **en az 20 sn önce bir uluma**
vardır ve ceset **ine doğru, pati izli sürüklenmiştir**. Kurtlar köye girmez, gündüz patikada ısırmaz." Bu iki koşuldan
birini çiğneyen "kurt ölümü" bir çelişkidir (charter §0.4) ve sunucu onu `KG_CONTRA` olarak loglar (S9.5). Uluma ve pati
izi tek başına kanıt değildir: ormanda yalnız kalan bir katil ikisini de gerçekten çekebilir. Sahneyi yakalayan asıl
kamu ipucu **sürüklemenin yönü**, lekeler ve Adli Tabip'tir.

**Sis ölümü:** kırağı, yara yok, sadece Derin (3. geceden sonra gece Orta), kurban yalnızdı, dili görenler olabilir.
Sis ölümü sahnelenemez (fiil yok). Sabırsız'ın sisle oynadığı oyun **yalnız bırakmak**tır (§8.4).

**Kurtadam (rol, `02_Roles.md`):** canavar formunun ısırığı da "Isırık"tır ama köyde, evlerde olur. Kurtlar köye
girmediği için köyde ısırık = Kurtadam (ya da sahne). Orman kurtları Kurtadam'a **ormanda** örtü sağlar, köyde değil.
Adli Tabip ayrımı: "canavar ısırığı" (sonra, rol sprintleriyle).

### 8.3 Yeni izler (charter §3 yaşam döngüsü biçiminde)
| Delil | Kaynak | Ömür | Karşı hamle | Sahte yolu |
|---|---|---|---|---|
| Isırık yarası (ceset) | Kurt saldırısı | Kalıcı | — | **Parçala** (§8.4) |
| Isırık izi (canlı) | Isırılmak: yırtık kol, 60 sn aksama (sadece koşu) | Aksama 60 sn; yırtık kol şafağa kadar | Sargı aksamayı durdurur; sargının izi ganimet belgesindeki **Kanlı Sargı** yer prop'udur (tek iz) | — ("ormanda kurt ısırdı" alibisi kontrol edilebilir) |
| Pati izi | Kurt hareketi, yumuşak orman zemini | 90 sn | — | Dolaylı: Derin'de yalnız kalıp sürüyü çekmek |
| Kurt sürüklemesi | Kurt ölümü | 60 sn | — | İnsan sürüklemesi (farklı iz, ine doğru değil) |
| Kırağı (ceset) | Sis ölümü | Kalıcı | — | Yok |
| Islak (tek durum) | Sis Duvarı'ndan dönüş, bele kadar su | 60 sn (ateş başında 10 sn) | Kuruyana kadar saklan | Kovayla ıslanma (sonra: kuyuda yıkanma zaten ıslatır, 026a) |
| Yaş kül / dağılmış kor / soğuk kül | Ateşin sönme nedeni | Şafağa kadar | Ateşi yeniden yak (kül değişmez, üstüne yeni ateş) | Söndürüp "kendiliğinden söndü" demek: kül yalanı yakalar |
| Sönük yol feneri | `snuff` sabotajı | Yeniden yakılana kadar | `TrailLanterns` | — |
| Nöbet defteri | İmza | Maç boyu | — | Sonra: Sahtekar (Forger) imza silebilir |

Böylece S3.2'nin "her iz türünün ömrü ve karşı hamlesi var, ≥ %50'sinin sahte yolu var" kuralı bu özellik için de
sağlanır (9 türden 5'inin sahte yolu var).

### 8.4 Sabırsız'ın orman fiilleri (S6.1: öldürme dışı fiiller)
| Fiil | Kim | Koşul | Süre / iz | Kasaba'nın cevabı |
|---|---|---|---|---|
| **Parçala** (kurt işi gibi göster) | Sabırsız (her kötü taraf) | Kendi ya da taze ceset (≤ 60 sn), Orta/Derin bant, patika ve ışık dışı | 6 sn çömelme animasyonu, `KG_HEAR_MAUL`, +45 sn leke, `BodyMauled` | Uluma kaydı, pati izi yokluğu, Adli Tabip, leke |
| **Yalnız bırak** | Herkes (Sabırsız için bir silah) | Kurbanı Derin'e götür, sonra ayrıl: kurban yalnız kalınca Sis dikkati dolar | İz: iki kişinin birlikte girip birinin tek çıktığı `ForestEnter` olayları, ayak izleri. Ölüm olursa Kurban Hakkı harcar (§4.6, 027b) | Kiminle ormana girdiğine dikkat et; kovalanana koş (dil hedef değiştirmez) |
| **Ateşi söndür** | Herkes (fiziksel) | Kamp ateşi | Su: tıslama 25 m, yaş kül; dağıt: 3 sn, 20 m, dağılmış kor. Söndürüp giden tehdit oyuncusu, ardından gelen PvE ölümü için Kurban Hakkı öder (§4.6) | Ateşin başında nöbet, kül okuma |
| **Fener sabotajı** | Sabırsız (016/026b "snuff") | Yol feneri ya da fener lambası | 3 sn, görünür, `LampSnuffed`; fener lambası sönerse o gece Sis Duvarı ilerler | Onarım görevi (026b), feneri kimin söndürdüğünü gören |
| **Tabela çevir** (sonra) | Sabırsız | İz patikası tabelası Derin'e döndürülür | 3 sn, gıcırtı 15 m, taze çizik, `SignTurned` | Herkes 2 sn'de düzeltir |

(**Yem** fiili kesildi: kurt ilgisini bir noktaya çekmek PvE'yi uzaktan hedeflemeye çeviriyordu ve Kurban Hakkı
dışında bir öldürme yolu olurdu.)

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
| A3 | Nöbeti gece alibisi olarak kullanmak (Sabırsız) | Serbest ve bedelli: o gece köyde öldüremez. 2 kişilik nöbet riski Kasaba'nın bildiği bir risktir. Nöbet defteri kimin orada olduğunu söyler. Nöbette ateşi söndürüp ortağını kurtlara bırakmak Kurban Hakkı harcar (§4.6). |
| A3b | Nöbet ödülünden taraf okumak (bar sıçraması) | Ödül taraf-kör: her başarılı imzacı sayılır (§7.4). |
| A4 | Bütün köyün her gece kampa gidip geceyi atlaması | En fazla 4 imza (N ≤ 7'de 2; N ≥ 15'te 2 kamp × 4). Evin boş kalması ve Gece Defteri'ni kaçırmak bedeldir. Gece ve maç ödül tavanları. |
| A5 | Toplantıdan kaçmak için ormanda kalmak | Toplantıya uzaktan gelen Geç Kalan olur (021, S4.5b). Orta/Derin bantta HUD "Meydana ~50 sn" gösterir. Bu bölgeler S4.5 ölçümünün dışındadır: bilerek alınan risk. |
| A6 | Meşale istiflemek ya da meşaleyle ormanı "bedava" yapmak | Kişi başı günde 1 kutudan; yeniden yakmak Reçine (Derin) + kamp ateşi ister; 90 sn yanar. Meşale ısırığı engellemez, sadece yavaşlatır (§4.4). |
| A7 | Ateşi söndürerek nöbeti bozmak (trol) | Fiziksel ve izli (kül türü, ses, `FireOut`). Kasabalı trolü de aynı delille yakalanır. |
| A8 | Sis Duvarı'nı ışınlanma kaçışı olarak kullanmak | MVP'de duvar sadece **en yakın güvenli hücreye** geri çıkarır (uzağa değil). Islak 60 sn görünür. İskele versiyonu (sonra) 90 sn Islak ve tanıklı varış ile gelir. |
| A9 | AFK oyuncuyu ormanda tehdide yedirmek | §2.4 madde 6: 10 sn girdi yok → metreler donar; 20 sn → güvenli hücreye dönüş. Her iki eşik de en erken ölümden (≥ 36 sn) önce. |
| A9b | Kurbanı yavaşlatıp (ısırık, yük) Sis'e bırakmak | Aksama yürümeyi yavaşlatmaz; dil doğunca yük düşer; sargı dil hedefindeyken başlamaz (§5.3). |
| A10 | Botu Derin'e götürüp bırakmak | Botlar ormanda patikaya dönme kuralıyla yürür (§10); yalnız kalan bot 5 sn içinde güvenliğe yönelir. |
| A11 | Parçala'yı köyde kullanmak | Fiil sadece Orta/Derin bantta açılır. Köyde ısırık yine de mümkün değil: köyde ısırık gören herkes "Kurtadam ya da yalan" der. |
| A12 | PvE ölümlerinin yıpratma ile Sabırsız'ı kazandırması | P1b (N ≤ 9 ve oyun sonunda öldürmez) + F1 **kapısı** (PvE ölümleri ≤ %5; Kasaba'nın PvE payı ≤ canlı payı + 10 puan). Kapı düşerse öldürücülük lobide varsayılan kapalıya döner; ayar sırası: gece Derin kazancı, sonra ısırık hasarı. Dağıtılan hiçbir görev Derin'de değildir (§7.1). |
| A12b | Tabancayla "kurda ateş ettim" bahanesi | Tabanca kurda ve Sis'e etkisizdir (§4.1); iddia uluma/göz kaydıyla kontrol edilir. |
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
| Nöbet | **Her bot** (Kasaba ve Sabırsız aynı olasılıkla; farklı olsaydı ele verirdi): gündüz imza sayısı ≥ 1, imza tavanı dolmamış ve canlı ≥ 6 ise imzalar. Nöbette yakıt < 40 iken odunluktan kütük taşır. | `BotVigilChance` 0.35, `BotFeedBelow` 40 |
| Meşale | Gece Orta/Derin'e girecekse kutudan meşale alır. | — |
| Sabırsız bot (MVP) | Orman tehditlerinden Kasaba botuyla aynı kurallarla kaçar. Sahneleme yok. | — |
| Sabırsız bot (sonra, Roadmap 023 + SPRINT-033d) | Ormanda tanıksız cinayetten sonra, sektörde son 90 sn içinde uluma **varsa** Parçala kullanır (usta hamle). | `BotMaulChance` 0.5 |
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
| Uluma / hırlama | Yönlü ses + altyazı + hasar göstergesi gibi kenar oku. ≤ 60 m: "[Kurt uluması, Kuzey Koru]"; ötesi: "[Uzakta bir kurt]" (sektörsüz). Sana ait telgrafta yakın hırlama: "[Yakında hırlama]". Sessiz yol (G1.1): işitme engelli oyuncu aynı bilgiyi alır. |
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
| Envanter | `Inventory/KGItemCatalog.cpp`, `KGLoot.*` (tablolar Crate/Barrel/Pot/Chest/Grave/Fishing) | Meşale, Çıra, Reçine, Ot Sepeti; orman kaplarında sargı ağırlığı |
| Kırılabilir | `World/KGBreakable.*` (Health, LootTable, LootSeed) | Orman kasaları (sandık tasarımıyla) |
| Oturak | `World/KGSeat.*` | Kamp kütük oturakları (nöbet atmosferi) |
| Botlar | `AI/KGBotController.cpp` (`UpdateChores`, gezinme) | §10 kaçış kuralları |
| Harita bölgeleri | `World/KGMapInfo.*` (`FKGMapRegion`) | Orman sektör adları defter satırlarında |
| Balıkçılık | `Fishing/` | Çiğ balık taşıyana kurt ilgisi ×1.5 (§4.2) |
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
| **024** Defter (J), Kasaba Panosu | Tek satırlık olaylar, pano | Uluma satırları tanığın defterinde (panoya düşmez) |
| **025b** Ceset taşıma | Taşınma modu | Cesedi sise taşımak (iskele dönüşü, sonra) |
| **026a** Fiziksel izler | İz havuzu (≤ 256), yumuşak zemin | Pati izi, kurt sürüklemesi (aynı havuz ve tavan) |
| **026b** Sabotaj → onarım | `snuff`'ı deftere bağlar | Yol feneri ve fener lambası sabotajı |
| **027a** Gece Defteri | Ev masası | Nöbetin bedeli |
| **027b** Saat Ustası + Tetikçi | Kurban Hakkı (öldürme bütçesi) | PvE ölümünün Kurban Hakkı'na bağlanması (§4.6) |
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
- LoopContract: doğrulama borcu > 10 iken yeni özellik sprintleri durur. `VerificationDebt.md` bugün 12 madde. Bu
  özellik doğrulama oturumundan **sonra** gelir; 033a (sadece araç ve veri, kod yok) istisnadır, borç yaratmaz.
- Kod gerçekleri (okunarak doğrulandı): görevler maçta bir kez dağıtılır (`KGGameMode.cpp:764`); dünya görevi
  noktaları şafakta sıfırlanmaz; navmesh statik (çalışma zamanı üretim ayarı yok); öldürme bütçesi (Kurban Hakkı)
  kodda yok; eşya kullanma yolu yok (`KGInventoryRPCComponent` sadece Transfer/Close/SetLocked/StandUp/Drop).
- Charter §0.1: yeni sistem. Kullanıcı açıkça istedi, ama sırası charter'ın bağımlılık sırasına uyar: defter (020b)
  önce.

---

## 14. MVP ve sonrası (sprint sözleşmeleri, hepsi onay bekler)

Sözleşmeler ayrı dosyalarda, LoopContract biçiminde (sabit kabul, yazılabilir kapsam, sınırlar):
- `Docs/Iterations/SPRINT-033-ForestThreats.md`: **033a** bant ve geometri ölçümü (S, sadece araç + veri),
  **033b** Sis + Sis Duvarı + tek Islak durumu + AFK kuralı (M), **033c** kurtlar (M, kurt modeli indirme onayından
  sonra), **033d** sahneleme ve orman defter olayları (M; 020b, 021, 023, 026a ve 027b'den sonra; eski F3).
- `Docs/Iterations/SPRINT-034-ForestChores-Vigil.md`: **034a** dört orman görevi veri olarak (S, SPRINT-016 bittikten
  sonra), **034b** kamp ateşi + meşale/çıra/reçine + Kamp Nöbeti (M).

### Sıra önerisi
Doğrulama oturumu (borç 12 → ≤ 10) → SPRINT-016 biter → Roadmap 020a (033a buna paralel koşabilir, kod yazmaz) →
Roadmap 020b → SPRINT-035a/035b (sargı ve ortak durum bileşeni; Islak ve aksama bunu kullanır) → **033b** → **034a** →
**033c** → **034b** → doğrulama oturumu → (021, 023, 026a, 027b sonrası) **033d**.
033b, 033c ve 034b 020b'den önce yapılırsa bile maçta **kapalı** kalır (`kg.Forest.Enabled 0` varsayılan; lobi seçeneği
SPRINT-015'in `KGLobbyState` kapsamına öneri olarak gider). Dev komutlarıyla görülür: kullanıcı ekranda yeni içeriği erken
görür ama charter'ın Ç2 kuralı delinmez.

### Sonra (ayrı öneriler)
İşaret Ateşi (sırt mangalı: o sektörde Sis ilerlemesini durdurur; köyün her yerinden görünen doğrulanabilir görev),
Tabela çevirme, sis cesedinin iskelede dönüşü, Sis Duvarı'ndan iskeleye çıkış, Avcı kapanı ve Çoban köpeği,
Kurtadam dolunay korosu, Barut Ustası fişeği, Taş Çember adağı (lore), tuzak kontrolü görevi, kurtları kovalayan ikinci
sürü davranışları, GDD §10 Sis Kuşatması'nın bu Sis ile kurulması (aynı Beklenmeyenler).

---

## 15. Ölçüm

### 15.1 Charter hedefleriyle ilişki
| Kod | Bu özellik neyi değiştirir |
|---|---|
| S1.1, S1.2 (gece, Kasaba) | Bugün Kasaba'nın gecesi tamamen boş (93–135 sn). Nöbet (ateş besleme, odun taşıma, imza kararı) ve gece orman yürüyüşü gece ölü zamanını düşürür. Hedef: nöbet katılımcılarında gece p90 ≤ 60 sn. **Sayım kuralı (v0.2):** bu özellikte S1.1/S1.2 için "algılanan olay" sayılmayanlar: oyuncunun kendi ürettiği olaylar ve ona yönelmemiş, 30 m'den uzak ortam telgrafları (uluma, hırlama). Köyün tamamının duyduğu bir uluma, köydeki herkesin ölü zamanını sıfırlamaz. |
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
| F1 | PvE ölümlerinin tüm ölümler içindeki payı; Kasaba'nın PvE ölüm payı | ≤ %5 (N ≤ 9'da yapı gereği 0, [T]); Kasaba'nın PvE payı ≤ canlı oyuncu-saniyelerindeki Kasaba payı + 10 puan | [B] gece soak'u (bot parametreleri dondurulmuş). **Kapı:** aşılırsa `KG_PVE_LETHAL` lobide varsayılan kapalıya döner, sonra ayar | 200 ölüm |
| F2 | Telgrafsız PvE hasarı | 0 (kurt ≥ 20 sn, Sis ≥ 25 sn) | [T] | — |
| F3 | 1. gün ve 1. gece PvE hasarı | 0 | [T] | — |
| F4 | Ormanda bulunan oyuncu-gün başına orman defter olayı | Medyan ≥ 1 | [B] | 40 oyuncu-gün |
| F5 | Nöbet: Kasaba katılımcılarının başarı oranı; N ≥ 10'da nöbetli gece oranı | %60–85; ≥ %30 | [O]; bot için [U] | 40 katılımcı; 40 gece |
| F6 | Kamu kuralının iki koşulu (≥ `KG_WOLF_TELEGRAPH_MIN` önce o sektörden uluma; ine doğru pati izli sürükleme) | [T]: her gerçek kurt ölümü iki koşulu da sağlar (%100); sahnelenmiş ölüm sürükleme koşulunu hiç sağlamaz (%100). [O]: sahnelemenin ertesi toplantıda yakalanmama oranı %40–70 | [T] [O] | 30 sahne [O] |
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
| Ç5 | Bağlantı | **Evet** | Defter, Vaka Dosyası (yara sınıfı), botlar, roller (Adli Tabip, Kurtadam, Avcı, Çoban, Bekçi), harita (bölge kapısı = Sis Duvarı), Hazırlık barı, fener görevi, balıkçılık (çiğ balık kurt ilgisini artırır), ganimet (orman kaplarında sargı; tek Islak durumu tabancayı da ıslatır). |
| Ç6 | Ölçek | **Evet** | N = 6'da 2 sektör, 1 sürü × 2 kurt, 1 dil, nöbet en fazla 2 kişi, **öldürmeyen** orman (telgraf, sendeleme, geri çıkarma); N ≥ 10'da öldüren orman; N = 20'de 4 sektör, 2 sürü, 2 kamp. Küçük lobide tek PvE ölümünün lobinin %17'si olması riski P1b ile kapandı. |
| Ç7 | Varyans | **Evet** | Metreler ve hedef seçimi deterministik; inler tohumlu ve şafakta duyurulur; ısırık hasarı sabit. |
| Ç8 | Bot | **Evet** | Botlar kendi algılarıyla (uluma, göz, kendi aşaması) kaçar, grup olur, nöbet tutar. Sahneyi algılamak Roadmap 023'e bağlı (alt tabloda Parçala için Hayır). |
| Ç9 | Sessiz yol | **Evet** | Her telgrafın görsel ve altyazılı hâli var; "Yardım!" radyal menüden sessiz çalışır; nöbet defteri yazılı. |
| Ç10 | Okunabilirlik | **Evet** | "Ormanda yalnız kalma: ışık, patika ve arkadaş seni korur." Sahne kuralı: "Kurtlar ulumadan saldırmaz." |
| Ç11 | Frustrasyon | **Evet** | ≥ 20/25 sn telgraf oyuncuya özel damgayla sunucuda zorlanır; 1. gün ve gece hasar yok; N ≤ 9'da ve oyun sonunda PvE öldürmez; Sis her yavaş durumdan yürüyerek kaçılır; duvar öldürmez; AFK 10/20 sn ile korunur. |
| Ç12 | Tempo | **Evet** | Ambient uluma ve kendi ürettiğin olaylar sayılmadan (§15.1 sayım kuralı): Kasaba'nın bugün tamamen boş gecesini **nöbet** doldurur (ateş besleme, odun taşıma, kiminle oturacağın kararı; her biri bir karar fiili ya da o kişiye yönelmiş bir olay). Gündüz orman görevleri sıradan görev kadar ölü zaman azaltır, fazlası değil. Orta ve geç evre tırmanması (§6) karar yoğunluğunu artırır. Risk: uzak görevlerin yürüyüşü; S1.1 gündüz raporda izlenir. |

**Sonuç:** 12/12 (Parçala alt mekaniğinde 11/12). **Koşullu kabul**: Ç2, Roadmap 020b sonunda Evet olmalıdır; olmazsa
orman tehditleri maç dışı (dev) kalır. Ç12 artık sadece nöbet ve karar fiillerine dayanır; nöbet kesilirse Ç12 Hayır
olur (kalan 8'den 7 Evet, yine geçer).

---

## 17. Onay listesi (kullanıcı için)

- [ ] **O1** — Bu tasarımı Backlog "Proposed (needs approval)"a al; sıra §14 (033a → … → 033d).
- [ ] **O2** — Kurt modeli: Quaternius Ultimate Animated Animals (CC0; `AssetResearch.md`). İndirme için izin (ad, kaynak, boyut 033c başında sorulur).
- [ ] **O3** — Batı ve Kuzey Koru her N'de açık ama N ≤ 9'da **öldürmeyen** hâliyle (GDD §3 "Orman sadece 15–20" yerine).
- [ ] **O4** — "Uzun görev" sınıfı (45–90 sn, oyuncu başına ≤ 1), SPRINT-016'nın 30–75 sn kuralına istisna.
- [ ] **O5** — Nöbet ödülü taraf-kör: başarılı her imzacı toplamın %2.5'i, gece tavanı %5, maç tavanı %10; bar canlı Kasaba'nın ulaşabileceği tavanı aşmaz; altın yok.
- [ ] **O6** — MVP'de kurtlar öldürülemez (sadece kovulur); tabanca kurda ve Sis'e etkisiz.
- [ ] **O7** — Sis Duvarı öldürmez (güvenli hücreye geri çıkarır); sadece Sis dili öldürür (ve sadece `KG_PVE_LETHAL` iken).
- [ ] **O8** — Parçala (sahneleme) fiili Sabırsızlara verilsin (033d).
- [ ] **O9** — F1 bir kapı olsun (≤ %5 PvE ölümü): aşılırsa orman öldürücülüğü varsayılan kapalı.
- [ ] **O10** — 033a ölçümüne göre bant eşikleri küçültülsün mü, yoksa `nav_bounds` genişletilip v2 yeniden mi inşa edilsin (SPRINT-022 sahibiyle)?
