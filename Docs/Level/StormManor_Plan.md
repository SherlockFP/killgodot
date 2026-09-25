# Fırtınalı Malikâne (Storm Manor): harita 2 planı

**Durum:** SPRINT-017 tasarımı. Kullanıcı onay verirse SPRINT-018'de inşa edilecek.
**Kaynaklar:** `Tools/Level/stormmanor_layout.json` tek doğru kaynaktır. Onu `Tools/Level/author_stormmanor.py` üretir,
`Tools/Level/validate_stormmanor.py` doğrular (**PASS**), `Tools/Level/render_stormmanor.py` çizer.
**Plan görseli:** `Docs/Level/StormManor.png`. Dört panelin hepsi aynı ölçekte: zemin kat + bahçe, 1. kat, mahzen +
deniz seviyesi, çatı + kule.

Koordinatlar metre cinsinden ve UE eksenleriyle yazılıdır: x doğu, +y güney (UE üst görünüşünde +y aşağıdadır). Her oda
köşesi 2 m'lik kit ızgarasındadır ve katlar arası 3 m'dir (`FLOOR_H = 300`). Bu yüzden v2 inşacısının duvar, zemin ve
merdiven parçaları her odayı kesmeden döşer. Morrowmere'deki "ev = yeniden kaplanabilir birim" fikri korunur: her oda,
v2'deki ev kabuğunun iç mekân hâlidir.

---

## 1. Konsept ve lore kancası

Morrowmere koyunun ağzında, köyden ~900 m açıkta **Kuzgun Kayası** durur. Üstünde **Pozzo'nun Malikânesi** vardır
(lore'daki yan kadrodan Pozzo ve Lucky). Pozzo, Godot'yu "herkesten önce görmek" için evini sise en yakın kayaya
kurdurmuştur. Kule onun gözcü yeridir, bütün saatleri de Anselm yapmıştır.

**Kanca:** Fırtınalı akşamlarda Haberci Çocuk'un küreksiz kayığı köye yanaşamaz. Rivayete göre mektubu malikânenin
kayıkhanesine bırakır. Bu yüzden Karşılama Komitesi fırtına gecelerini malikânede bekleyerek geçirir. Sofralar kurulur,
çocuk odası hazır tutulur, kulede sinyal feneri yakılır. Kapılar fırtınada kilitlenir ve kimse kayadan ayrılamaz. Maç
böyle bir gecede başlar: **kapalı, Among Us tarzı bir harita**. Dışarısı fırtına, içerisi birbirine bağlı 24 oda.

Motif nesneleri: **boş sandalye** (uzun masanın başında, Godot'nun yeri), **hep hazır çocuk odası** (mavi şapka askıda),
**Anselm'in portresi** (gözleri gözetleme deliği), denizden vuran **"G." kolileri**, hep aynı dakikada duran saatler.

Tema ve ton: köyün canlı paletinin koyu, fırtınalı hâli. İçeride sıcak turuncu ışık (şömine, mum, kıvılcım lambaları),
pencerelerde soğuk mavi fırtına, şimşekte bembeyaz bir an. TF2'deki sıcak/soğuk ayrımı burada iç/dış ayrımıdır.

---

## 2. Harita özeti

| Ölçü | Değer |
|---|---|
| Adlandırılmış oda | **24** (zemin 9, 1. kat 9, çatı katı 3, mahzen 3) |
| Diğer alanlar | 13: bahçe (Ön Avlu, Hizmet Avlusu, Sera, Aile Mezarlığı, Kayıkhane), açık hava (Fırtına Terası, Çatı Yolu, Kule Tepesi), koridorlar (Uşak Koridoru, Misafir Koridoru, Doğu Koridoru, Kaçakçı Tüneli, Kayalık Patika) |
| Kat | 5 seviye: mahzen z −3, zemin 0, 1. kat +3, çatı +6, kule tepesi +9 |
| Kapı / merdiven / gizli geçit | 53 / 11 / 6 |
| Pencere | 145 dış pencere (29'u takırdayan) + 1 iç pencere (çocuk odasından salona) |
| Görev | 19 fiziksel görev (28 varyant), 56 görev noktası |
| Yürünebilir alan | 8.724 m² (N = 20'de oyuncu başına 436 m²) |
| Toplanma (Büyük Salon) | p90 **9.9 sn**, en fazla **14.0 sn** (sınır 30 sn, `KG_GATHER_WINDOW` 35 sn) |

Evin ana gövdesi 68 × 36 m'dir. Bahçeyle birlikte kaya 104 × 76 m tutar. Mesafeler kısa ama katlar çok: harita
"geniş" değil, **derin**. 20 kişide bile her katta saklanacak köşe vardır, toplanma da yine 15 saniyenin altında kalır.

---

## 3. Odalar

"Sn": odanın en uzak noktasından Büyük Salon'a koşarak varış süresi (validator, 5.8 m/sn, merdiven ×1.18).
"N": bölge kapısı (§8), yani odanın açıldığı en küçük oyuncu sayısı. Risk katmanı: G = güvenli, O = orta, T = tehlikeli.
Etkileşim sütunu: görev (numaralar §6'daki görevler), saklanma yeri, delil yüzeyi, gizli geçit (S).

### 3.1 Zemin kat (z 0)
| # | Oda | Boyut | Amaç | Döşeme notları | Etkileşim | Sn | N | Risk |
|---|---|---|---|---|---|---|---|---|
| 1 | **Büyük Salon** | 20×20, çift kat | Toplantı odası, doğuş halkası | 20 sandalyeli uzun meşe masa, başında boş sandalye; kuzey duvarında şömine; saat odasından iple inen avize; iki yanda büyük merdiven; Pozzo'nun sancakları | Görev 1, 3, 4; kürsü perdesi (saklanma); şömine külü ayakkabıda | 2.5 | 6 | G |
| 2 | **Giriş Holü** | 20×10 | Ön kapı, posta masası, vestiyer | Damalı taş zemin, damlayan şemsiyelik, güvercin gözlü posta masası, vestiyer dolabı, fırtına fenerleri | Görev 8, 17; vestiyer dolabı; ıslak paltolar dışarı kimin çıktığını gösterir | 3.9 | 6 | G |
| 3 | **Yemek Salonu** | 12×14 | Komitenin resmî sofrası | 13 kişilik ziyafet masası, şamdanlar, büfe, portreler | Görev 19; büfe dolabı; yeri değişen isim kartları, şarap lekesi | 4.4 | 6 | O |
| 4 | **Mutfak** | 12×16 | Yemek, uşak merdiveni, yemek asansörü | Ocak, kazan, asılı tavalar, uzun tezgâh, ot askıları | Görev 2, 7; masa altı; bıçaklıktaki boşluk görünür; S2 | 6.7 | 6 | O |
| 5 | **Kiler** | 12×12 | Erzak, mahzen merdiveni | Çuvallar, fıçılar, raflarda kavanozlar, porselen dolabı, yerde un | Görev 19; çuval yığını; **un zemin: ayak izi 120 sn** | 7.2 | 6 | O |
| 6 | **Çamaşırhane** | 12×10 | Leğenler, mengene; kan yıkama noktası | Leğenler, sıkma mengenesi, ipte çarşaflar, buhar | Görev 13; çamaşır sepeti; yıkanınca leğen 45 sn kızarır | 5.5 | 6 | O |
| 7 | **Şapel** | 12×10 | Çan ipi, mumlar, kripta merdiveni | Sıralar, sunak, çan ipi, mum rafları, günah çıkarma hücresi | Görev 16, 18; günah çıkarma hücresi; mum sayısı, sallanan ip; S1 | 4.3 | 6 | O |
| 8 | **Kütüphane** | 12×14 | Okuma, şömine, Pozzo'nun döner merdiveni | Tavana kadar raflar, kayar merdivenler, şömine, küre, dönen kitaplık | Görev 3, 15; pencere perdesi; yarı çekili kitaplar; S1 | 6.9 | 6 | O |
| 9 | **Balo Salonu** | 24×16 | En büyük açık oda, orkestriyon, avluya Fransız kapılar | Cilalı zemin, aynalar, orkestriyon, örtülü sandalyeler, iki avize, uzun pencereler | Görev 5, 12; toz örtüleri, sahne perdesi; cilalı zeminde ıslak iz | 6.7 | 6 | O |

### 3.2 Mahzen (z −3)
| # | Oda | Boyut | Amaç | Döşeme notları | Etkileşim | Sn | N | Risk |
|---|---|---|---|---|---|---|---|---|
| 10 | **Şarap Mahzeni** | 16×16 | Şarap rafları, kiler merdiveni, tünelin ağzı | Tonoz tuğla, raflar, fıçılar, mum nişleri, örümcek ağı, sahte raf | Görev 1; boş fıçı; **toz zemin: iz 120 sn**; S4 | 9.8 | 8 | T |
| 11 | **Kıvılcım Odası** | 12×12 | Anselm'in kıvılcım panosu: Leyden kavanozları evin lambalarını besler | Bakır bobinler, raflarda cam kavanozlar, büyük bıçak şalter, göstergeler, kıvılcımlar | Görev 11; kavanoz rafının arkası; **şalteri çekenin elinde 45 sn is** | 10.4 | 6 | O |
| 12 | **Sarnıç** | 20×16 | Salonun altındaki yağmur sarnıcı, çıkrık, kripta merdiveni | Kara suda sütunlar, tahta iskeleler, çıkrık, damlalar, yukarıda avizenin ip deliği | Görev 2; sütun arası (diz boyu suda); ıslak paça 60 sn; S5 | 8.7 | 6 | T |

### 3.3 Birinci kat (z +3)
| # | Oda | Boyut | Amaç | Döşeme notları | Etkileşim | Sn | N | Risk |
|---|---|---|---|---|---|---|---|---|
| 13 | **Portre Galerisi** | U, 176 m² | Salon boşluğunu saran galeri, toplantı masasına bakar | Salona korkuluk, Madam Vellum'un portreleri, Anselm'in portresi (gözleri gözetleme deliği), büstler | Görev 4, 14; büst kaidesi; gözetleme deliği; S3 | 5.8 | 6 | O |
| 14 | **Mavi Misafir Odası** | 12×12 | Misafir yatak odası | Mavi cibinlikli yatak, gardırop, lavabo, sandık | Görev 5, 8; gardırop; kapı altına atılan mektuplar | 8.5 | 8 | O |
| 15 | **Hamam** | 12×12 | Bakır küvet, kazan; yıkanma noktası; buhar (görüş 5 m) | Bakır küvetler, kazan, havlular, çinili zemin, kurutma askısı | Görev 2, 13; buhar paravanı; ıslak iz | 7.0 | 8 | O |
| 16 | **Kırmızı Misafir Odası** | 12×12 | Misafir yatak odası | Kırmızı yatak, gardırop, yazı masası, şömine | Görev 5, 8; gardırop; açık kepenk | 8.5 | 8 | O |
| 17 | **Portre Atölyesi** | 12×12 | Madam Vellum'un atölyesi | Şövaleler, örtülü tuvaller, boya kapları, model sandalyesi, tavan penceresi | Görev 5, 14; tuval örtüsü; **boya zemin: renkli iz 90 sn** | 7.0 | 8 | O |
| 18 | **Çocuk Odası** | 20×8 | Haberci Çocuk için hep hazır oda; salona bakan iç pencere | Küçük yatak, askıda mavi şapka, sallanan at, müzik kutusu, oyuncak kayıklar, gardırop | Görev 5, 8, 12; gardırop; sallanan at biri geçince 10 sn sallanır; S6 | 6.5 | 6 | O |
| 19 | **Çalışma Odası** | 12×12 | Pozzo'nun masası, kasası, kıyı haritası | Masa, kasa, harita masası, teleskop, av trofeleri | Görev 7, 8; harita dolabı; kasa kaydı (kim açtı) | 7.0 | 6 | O |
| 20 | **Efendi Yatak Odası** | 12×12 | Pozzo'nun odası; döner merdiven kütüphaneye iner, kuleye çıkar | Dört direkli yatak, gardırop, makyaj masası, şömine, Lucky'nin ip kancası | Görev 5, 7, 8; yatak altı; döner merdiven gıcırtısı (12 m); S6 | 8.4 | 8 | O |
| 21 | **Bilardo Odası** | 12×14 | Bilardo ve kâğıt oyunu; terasa kapı | Bilardo masası, isteka rafı, kâğıt masası, içki dolabı | Görev 5, 8, 15; içki dolabı; top dizilimi değişir, eksik isteka = silah | 7.4 | 6 | O |

### 3.4 Çatı katı ve kule (z +6 / +9)
| # | Oda | Boyut | Amaç | Döşeme notları | Etkileşim | Sn | N | Risk |
|---|---|---|---|---|---|---|---|---|
| 22 | **Tavan Arası** (ve hizmetkâr yatakları) | 24×20 | Kirişler, Lucky'nin portatif yatağı, çamaşır ipleri, lamba yağı deposu | Kirişler, sandıklar, toz örtüleri, portatif yataklar, ipler, yağ bidonları, yemek asansörünün tepesi | Görev 6, 13; sandık, örtü yığını; **toz zemin: iz 120 sn**; S2 | 9.0 | 10 | T |
| 23 | **Saat Odası** | 20×10 | Salonun üstünde Anselm'in büyük saati; avize çıkrığı | Dev çarklar, sarkaç, kadranın içten görünüşü, avize çıkrığı, kıvılcım bobini | Görev 4, 16; saat kasası (içindeyken saat durur); S3 | 8.3 | 10 | O |
| 24 | **Fırtına Kulesi** + Kule Tepesi | 8×8 ×2 kat | Paratonerden şimşekle dolan kavanozlar; tepede sinyal feneri | Paratoner kablosunda kavanoz rafı, döner merdiven, el merdiveni kapağı, dört yanda pencere; tepede fener ve korkuluk | Görev 6, 11; merdiven altı; boş kavanoz yuvası kimin aldığını gösterir | 10.1 / 12.2 | 10 | T |

### 3.5 Bahçe, açık hava ve dolaşım
| Alan | Kat | Amaç ve döşeme | Etkileşim | Sn | N |
|---|---|---|---|---|---|
| **Ön Avlu** | F0 | Vladimir-Estragon heykelli çeşme, limon saksıları, demir lambalar, rıhtım merdiveni kapısı. Çamur (iz). | Görev 10; heykel kaidesi | 10.3 | 6 |
| **Hizmet Avlusu** | F0 | Odunluk, kütük, rüzgârda çırpan boş ipler, kümes, el arabaları, su birikintileri | Görev 3; odunluk arkası; kütükteki balta = silah | 10.9 | 8 |
| **Sera** | F0 | Cam ev: yağmur üstünde davul gibi çalar (ses maskesi), çatlak camlar, palmiyeler, su fıçısı, eski kuyu | Görev 2, 10; palmiye arkası; S5 | 10.6 | 8 |
| **Aile Mezarlığı** | F0 | Mezar taşları, demir çit, anıt mezar, kuru ağaç, 4 fener | Görev 18; açık mezar; taze toprak; S4 | 12.2 | 12 |
| **Kayıkhane** | C | Deniz seviyesi: kızakta kayık, pompa, ağlar, "G." kolileri, içeri vuran dalgalar | Görev 9, 17; ters kayığın altı; ıslak iz | 11.4 | 8 |
| **Fırtına Terası** | F1 | Balo salonunun çatısında açık teras: devrik sandalyeler, çırpan tente, paratoner direği | Şimşekte herkes görünür | 8.8 | 10 |
| **Çatı Yolu** | F2 | Saat odası ile kule arasında tahta iskele, ip korkuluk, bacalar | Darboğaz, açık | 10.3 | 10 |
| Uşak Koridoru | F0 | 44 m'lik hizmet omurgası, ışık sadece iki uçta, zil panosu | Darboğaz | 4.8 | 6 |
| Misafir / Doğu Koridoru | F1 | Halı yolluk, aplikler, Anselm'in duran saatleri | — | 6.8 / 7.7 | 6 |
| Kaçakçı Tüneli | C | Şarap mahzeninden kayıkhaneye 50 m kaya tüneli: raylar, eski fenerler | Darboğaz | 14.0 | 8 |
| Kayalık Patika | F0 | Batı yarında açık yol: mutfağın arka kapısından mezarlığa, köpük ve rüzgâr | Darboğaz | 10.4 | 12 |

---

## 4. Dolaşım

### 4.1 Döngüler
Validator her bölgeye iki bağımsız yol olduğunu doğrular. Tek köprü **Kule → Kule Tepesi**'dir: `dead_end_ok`, bilerek
bırakılmış bir çıkmaz.

| Döngü | Yol |
|---|---|
| A: Hizmet halkası (zemin) | Salon → Yemek Salonu → Mutfak → Uşak Koridoru → Salon |
| B: Tören halkası (zemin) | Salon → Balo Salonu → Kütüphane → Uşak Koridoru → Şapel → Salon |
| C: Ön halka | Giriş Holü → Ön Avlu → Balo Salonu (Fransız kapı) → Salon → Giriş Holü |
| D: Batı dış halkası | Mutfak → Kayalık Patika → Mezarlık → Hizmet Avlusu → Kiler → Mutfak |
| E: Mahzen halkası | Kiler ↓ Şarap Mahzeni → Kıvılcım Odası → Sarnıç ↑ Şapel → Salon → Çamaşırhane → Kiler |
| F: Deniz halkası | Şarap Mahzeni → Kaçakçı Tüneli → Kayıkhane ↑ Rıhtım Merdiveni → Ön Avlu → Hizmet Avlusu → Kiler ↓ |
| G: Misafir halkası (1. kat) | Galeri → Misafir Koridoru → Mavi Oda → Hamam → Galeri; Kırmızı Oda → Atölye → Çocuk Odası → Galeri |
| H: Efendi halkası | Galeri → Çalışma Odası → Efendi Yatak Odası → Doğu Koridoru → Galeri; Bilardo ↔ Teras ↔ Doğu Koridoru |
| I: Çatı halkası | Misafir Koridoru ↑ Tavan Arası → Saat Odası ↓ Galeri; Saat Odası → Çatı Yolu → Kule ↓ Efendi Yatak Odası |
| J: Dikey kestirme | Kütüphane ⟳ Efendi Yatak Odası ⟳ Kule (Pozzo'nun özel merdiveni) |

### 4.2 Merdivenler (11)
| Merdiven | Bağlantı | Tür |
|---|---|---|
| Büyük Merdiven batı / doğu | Salon ↔ Galeri | düz, 2 m (galeriden herkes görür) |
| Uşak Merdiveni alt / üst | Mutfak ↔ Misafir Koridoru ↔ Tavan Arası | düz, dar |
| Kiler Merdiveni | Kiler ↔ Şarap Mahzeni | düz |
| Kripta Merdiveni | Şapel ↔ Sarnıç | düz |
| Pozzo'nun Döner Merdiveni | Kütüphane ↔ Efendi Yatak Odası | döner |
| Saat Merdiveni | Galeri ↔ Saat Odası | düz |
| Kule Merdiveni + el merdiveni | Efendi Yatak Odası ↔ Kule ↔ Kule Tepesi | döner + el merdiveni |
| Rıhtım Merdiveni | Ön Avlu ↔ Kayıkhane | açık hava, 4 m |

### 4.3 Gizli geçitler (6, havalandırma benzeri)
| No | Ad | Uçlar | Nasıl |
|---|---|---|---|
| S1 | Dönen Kitaplık | Kütüphane ↔ Şapel (günah çıkarma hücresi) | Kırmızı kitabı çek, raf döner |
| S2 | Yemek Asansörü | Mutfak ↔ Tavan Arası (2 kat) | Asansöre gir, ipi çek |
| S3 | Anselm'in Portresi | Galeri ↔ Saat Odası | Portre açılır, kadranın arkasına merdiven. **Gözleri toplantı masasına bakan gözetleme deliğidir.** |
| S4 | Anıt Mezar Merdiveni | Mezarlık ↔ Şarap Mahzeni | Pozzo'nun babasının mezar kapağını kaydır, sahte şarap rafından çık |
| S5 | Eski Kuyu | Sera ↔ Sarnıç | Demir basamaklar |
| S6 | Duvar Arası | Çocuk Odası gardırobu ↔ Efendi Yatak Odası | Duvarların arasında sürünme |

**Kurallar** (JSON `secret_rules`):
- **Kim:** Sabırsızlar hepsini maç başından haritada görür. Diğerleri keşfetmelidir: şimşek anında, 4 m içinde ve görüş
  hattındaysan geçidin çatlağı 0.35 sn parlar. Birini açan, o anda görenlerin haritasına da işler.
- **Maliyet:** geçiş 3 sn sürer, iki uçta 12 m'den duyulan gıcırtı çıkar. Panel 20 sn aralık kalır ve iki uçta toz
  bulutu asılı durur (iz).
- **Karşı hamle:** herkes bir geçidi 4 sn'de o günlük takozlayabilir (takoz görünür). Çilingir kilitleyip açabilir.
- Geçitler bağlantı hesabına hiç katılmaz. Harita gizli geçitsiz de tam bağlıdır.

### 4.4 Görüş hatları
- **Galeri → toplantı masası:** 1. kattan salon boşluğuna tam görüş. Toplantıda galeride duran, herkesi görür.
- **Çocuk Odası iç penceresi → salon:** salonun güneyinde, üstten bakan cam.
- **Portrenin gözleri (S3)** saat odasından masaya bakar. Birinin arkasında durduğu, gözlerin kıpırdamasından anlaşılır.
- **Uşak Koridoru:** 44 m düz çizgi, ışık sadece iki uçta. Ortası 5 m görüşlü karanlık.
- **Kule Tepesi:** avluyu, rıhtım merdivenini ve terası görür. Gözcü noktasıdır ama tek girişli bir tuzaktır.
- **Balo Salonu Fransız kapıları ↔ Ön Avlu:** şimşekte avludaki herkes siluet olur.

### 4.5 Darboğazlar
Planda kırmızı eşkenar dörtgenle işaretlidir:
- Uşak Koridoru (4 m × 44 m)
- Kaçakçı Tüneli (4 m × 50 m)
- Rıhtım Merdiveni
- Kayalık Patika
- Çatı Yolu
- Büyük Merdivenler (2 m)
- Kule el merdiveni

Hepsi "tehlikeli" risk katmanındadır. Pillars S4.3'ün cinayet dağılımı (cinayetlerin ≥ %60'ı orta/tehlikeli bölgede)
buralardan gelmelidir.

### 4.6 Toplantı, doğuş, seyahat süreleri
- **Toplantı odası = Büyük Salon.** Uzun masa, 20 sandalye, başında boş sandalye. Acil toplantı çanı buradaki şömine
  saatidir.
- **Doğuş:** masanın çevresinde 6 m yarıçaplı, 20 kişilik halka, yüzler merkeze. v2'deki çeşme halkasının eşidir.
- **Seyahat** (validator, koşu 5.8 m/sn):
  - En uzak nokta: Kaçakçı Tüneli'nin ortası, **14.0 sn**.
  - Mezarlık ve Kule Tepesi 12.2 sn, Kayıkhane 11.4 sn. Diğer her oda ≤ 11 sn.
  - p90 9.9 sn. S4.5 (p90 ≤ 30 sn, en fazla ≤ 35 sn) rahatça geçer.
  - Kat değiştirmek ~2 sn ekler. Kilitli bölge kapıları yolu uzatmaz, çünkü her N'de açık odalar tek parçadır
    (validator).

---

## 5. Fırtına oynanışı

| Mekanik | Kural (JSON `storm`) | Neden ilginç |
|---|---|---|
| **Şimşek** | 20–45 sn'de bir, `FKGRng` ile tohumlu. Günün fırtına şiddeti (1–3) şafakta duyurulur. Flaş 0.35 sn sürer. Dış penceresi olan odalarda ve açık havada görüş o an `KG_SIGHT_UNLIT` 5 m'den `KG_SIGHT_DAY` 30 m'ye çıkar. | Karanlıkta yapılan cinayet, bir flaşta tanık kazanabilir ("Şimşek Tanığı"). Katil penceresiz odaları (mahzen, koridor, saat odası) seçer. |
| **Gök gürültüsü** | Flaştan 1.5 sn sonra, 2 sn boyunca duyma yarıçapları ×0.5. | Katil zamanlama yapar: flaşı görür, gürültüde vurur. Kasaba bunu bilir ve gürültü anında kimin yanında durduğuna bakar. Öngörülebilir, yani yazı-tura değildir (Ç7). |
| **Işık Kesintisi** (sabotaj) | Sabırsız, kıvılcım panosunda ya da bir lamba kavşağında sabotaj fiili yapar. Günde bir kez, günün ilk 90 sn'sinde yapılamaz. Evin bütün lambaları söner. Görüş 5 m'ye iner; şömine, mum ve şimşek hariç. Tamir için iki yol var: Kıvılcım Odası şalteri + Saat Odası bobini **aynı anda** 3 sn tutulur, ya da dolu bir kavanoz (Görev 11) panoya takılır. Tamir edilmezse 60 sn'de kendiliğinden biter. | Among Us "ışıklar"ının dikey hâli. Tamir iki kişiyi iki uzak kata (−3 ve +6) ayırır. **İz:** şalteri çekenin elinde 45 sn is kalır. |
| **Takırdayan pencereler** | Tohumlu 29 pencere fırtınada açılır ve çarpar. Ses 25 m'den duyulur, 6 m çevresinde ayak seslerini maskeler. Altına yağmur girer, ıslak zemin 60 sn iz tutar. Herkes E ile 2 sn'de kapatır. Sabırsız birini kasten açabilir. | Ses örtüsü ile iz arasında bir takas. Görev 5 (Kepenkler) bu pencereleri kapatır. |
| **Rüzgâr darbeleri** | Ön kapı ve Fransız kapılar darbede çarpar (ses 25 m, zararsız). | Sahte alarm. Duyulan "kapı kırma" sesinin fırtına mı yoksa biri mi olduğu tartışılır. |
| **Deniz** | Kayıkhane kızağı 20 sn'de bir dalga yer. Orada duranlar 60 sn ıslak kalır. | Kayıkhaneye kimin indiği ıslak paçadan belli olur. |
| **Sinyal feneri** (Görev 6) | Yakılınca ışığı gün boyu 8 sn'de bir avluyu ve rıhtım merdivenini tarar. | Kasaba için görünür bir dünya durumu değişikliği (S4.6). |

Görünür dünya durumu değişiklikleri (S4.6, gün başına ≥ 2): açık ya da kapalı pencereler, ışık kesintisi, avizenin yanık
ya da sönük olması, sinyal feneri, asılı çarşaflar (görüşü keser), çalan orkestriyon, zehirli sürahi, takozlu geçitler.

---

## 6. Görevler (19 görev, SPRINT-016 çerçevesi)

SPRINT-016 kurallarına uyar: 1–3 basit adım, her adımda tek açık fiil (`take` al / `bring` götür / `work` çalış), 25–50
sn, her adımda ödül (ışıltı, ses, kalıcı sonuç). Sahtelenebilir: Sabırsız da taşır ama sonuç sayılmaz. Süre =
salondan yürüyerek (3.2 m/sn) + taşıma (eşya hız katsayısıyla) + iş, en kötü varyant (validator).

| No | Görev | Adımlar | Süre | N | Oyunlaştırma |
|---|---|---|---|---|---|
| 1 | **Salona Şarap** | Mahzenden şarap sepeti al → salondaki sürahiyi doldur | 36 | 8 | Koşarsan şişeler şıngırdar (duyulur). Sabırsız sürahiyi zehirleyebilir: mor halka, sonraki kişi önce boşaltır. |
| 2 | **Sarnıçtan Su** (mutfak / hamam / sera) | Çıkrıktan dolu kova çek → kazana ya da fıçıya dök (yürü!) | 36–38 | 6 | v2 su taşımanın eşi. Koşarsan su dökülür, dolum kovada görünür. Hedef zehirlenebilir. |
| 3 | **Şömine Odunu** (salon / kütüphane) | Odunluktan demet al → şömineye at | 33–43 | 8 | 3 m'den fırlatmak da sayılır ("Güzel atış!"). Şömine harlanır. |
| 4 | **Avizeyi Yak** (saat odası / galeri) | Çıkrığı çevir, avize iner → salonda mumları yak | 26–34 | 6 | Avize herkesin gözü önünde iner: salondaki herkes yakanı görür (alibi). |
| 5 | **Kepenkleri Kapat** (batı / ön / doğu) | Çarpan 3 kepengi kapat, sıra serbest | 30–38 | 6 | Sabırsız birini yeniden açabilir: ses örtüsü ama ıslak iz. |
| 6 | **Sinyal Feneri** | Tavan arasından yağ bidonu al → çatı yolundan geçip kuleye tırman, feneri doldur | 43 | 10 | Çatı yolunda şimşeğe açıksın. Fener gün boyu avluyu tarar. |
| 7 | **Pozzo'nun Akşam Yemeği** (çalışma / yatak odası) | Tezgâhtan tepsiyi al → Pozzo'ya götür | 31–34 | 6 | Uşak merdiveni kestirmedir. Koşarsan tabak kırılır. |
| 8 | **Misafir Mektupları** (aile / misafirler) | Posta masasından mektupları al → 3 doğru kapının altından at | 34–47 | 6 | v2 mektuplarının eşi. Kapı altındaki mektup, o odaya kimin uğradığının izidir. |
| 9 | **Kayığı Boşalt** | Sintine pompasını 3 kez bas → halatı bağla | 30 | 8 | Dalga vurursa ıslanırsın (60 sn iz). |
| 10 | **Saksıları İçeri Al** | Devrilen limon saksısını kaldır → sera tezgâhına koy | 29 | 8 | Ağır (×0.7): yavaş yürürsün, avluda görünürsün. |
| 11 | **Kıvılcım Kavanozu** | Kulede şimşekle dolan kavanozu al → mahzendeki panoya tak | 49 | 10 | 4 kat iner (kule → mahzen). Koşarsan boşalır. Takılan kavanoz Işık Kesintisi'ni anında bitirir. |
| 12 | **Müzik Kutusu** | Çocuk odasından silindiri al → orkestriyona tak | 28 | 6 | Orkestriyon 40 sn çalar: 30 m'den duyulur, balo salonunda ayak ve cinayet seslerini maskeler. Karar: müzik sen görevini yaparken katile de yarar. |
| 13 | **Çamaşırları As** (tavan arası / hamam) | Islak çarşaf sepetini al → ipe as | 29–34 | 8 | Asılı çarşaflar görüşü keser (gün boyu). |
| 14 | **Portreyi As** | Atölyeden portreyi al → galerideki boş çerçeveye as | 22 | 8 | Portrede rastgele bir canlının yüzü vardır. Sabırsız, çerçeveletilmiş bir yüzle değiştirebilir (Framer tadında). |
| 15 | **Kayıp Kitap** | Bilardo odasından kitabı al → kütüphanedeki boş rafa koy | 26 | 6 | Rafa konunca kitaplık bir an titrer: S1'e ipucu. |
| 16 | **Saat ve Çan** | Büyük saati kur → şapelde çan ipini çek | 35 | 10 | Çan bütün malikânede duyulur (v2 "çan ve saat"in eşi). |
| 17 | **G. Kolisi** | Kayıkhanede koliyi kaldır → giriş holündeki G. köşesine taşı | 49 | 8 | İki kişilik taşıma: tek başına ×0.55, iki kişiyle tam hız. Açmak yasak, herkes açar (kozmetik ganimet). |
| 18 | **Mezar Fenerleri** | Şapelden yanan fitil al → 4 mezar fenerini yak (sıra serbest) | 47 | 12 | Kayalık patikada koşarsan fitil söner. Mezarlık gece boyu parlar. |
| 19 | **Sofrayı Kur** | Kilerden porselen al → ziyafet masasına diz | 22 | 6 | Kısa ve güvenli; yeni başlayanlar için. |

Bölge kapılarına göre açık görev sayısı: N = 6'da 8, N = 8'de 15, N = 10'da 18, N ≥ 12'de 19. Maç listesindeki %70 dünya
/ %30 panel oranı (SPRINT-016) aynen geçerlidir. Panel mini oyunları mekâna uyarlanır: kilit açma çalışma odasının
kasasında, sigorta kıvılcım odasında, ağ örme kayıkhanede.

**Veri:** anchor ve chore'lar, `morrowmere_world_chores.json` şemasıyla aynı fiillerle yazıldı (`take/bring/work`,
`$var` varyantları, `any_order`, `repeat`, `fill`, `two_person`, `sabotage`). SPRINT-018, `gen_world_chores.py`'nin bir
kopyasıyla `KGWorldChoreData`'ya dönüştürecek.

---

## 7. Kanıt ve ayak izi zeminleri

Pillars §3'e göre ayak izi "yumuşak zemin"de kalır. Layout JSON'da her odanın `surface` alanı vardır:

| Zemin | Odalar | İz ömrü |
|---|---|---|
| Toz | Şarap Mahzeni, Tavan Arası | 120 sn |
| Un | Kiler | 120 sn |
| Boya | Portre Atölyesi | 90 sn |
| Islak | Hamam, Sarnıç, Kayıkhane, Çamaşırhane, Tünel, teras, çatı yolu, açık pencerelerin önü | 60 sn |
| Çamur | Avlular, Sera, Mezarlık, Kayalık Patika | 60–120 sn |

Taş, ahşap ve halı iz tutmaz. Yıkanma noktaları: Çamaşırhane leğeni, Hamam küveti, Sarnıç çıkrığı. Yıkanma görünür ve
`KG_HEAR_WASH` ile duyulur.

---

## 8. Bölge kapıları (G2.2) ve risk katmanları

| N | Açılan | Açık alan / oda / görev | m² / oyuncu |
|---|---|---|---|
| 6 | Zemin kat evi, Ön Avlu, Sarnıç + Kıvılcım Odası, Galeri, Çocuk Odası, Çalışma Odası, Bilardo Odası, iki koridor | 19 / 15 / 8 | 760 |
| 8 | + Şarap Mahzeni, Tünel, Kayıkhane, Sera, Hizmet Avlusu, Mavi Oda, Kırmızı Oda, Hamam, Atölye, Efendi Yatak Odası | 29 / 21 / 15 | 890 |
| 10 | + Tavan Arası, Saat Odası, Çatı Yolu, Kule, Fırtına Terası | 35 / 24 / 18 | 816 |
| 12+ | + Mezarlık, Kayalık Patika | 37 / 24 / 19 | 727 → 436 (N = 20) |

Kapalı bölgenin kapıları "fırtına kepengi" ile kilitlidir ve kapalı bölgeye görev atanmaz. Validator her N'de açık
odaların tek parça olduğunu doğrular.

Oyuncu başına alan N = 8–12'de yüksektir, çünkü alan katlara dağılır. Karşılaşma sıklığı (S4.4, G2.3) SPRINT-018
soak'unda ölçülür. Tutmazsa N = 8 kapısı ikiye bölünür: misafir odaları 10'a geçer.

Risk katmanı her odada JSON'da tutulur:
- **Güvenli:** Salon, Giriş Holü
- **Orta:** oda ve bahçelerin çoğu
- **Tehlikeli:** Uşak Koridoru, Mahzen, Sarnıç, Tünel, Tavan Arası, Çatı Yolu, Kule, Teras, Mezarlık, Patika

---

## 9. Kit ve prop ihtiyacı

### 9.1 Mevcut kit (indirme yok)
- **Duvarlar:** dışta Quaternius köy kiti `Wall_UnevenBrick_*` (fırtınada koyu taş, soğuk MI tonu), içte
  `Wall_Plaster_*` ve `Wall_Plaster_WoodGrid`. Kapılar için `*_Door_Round` ve `*_Door_Flat`, çift kanatlı kapılar için
  `Door_2/4/8`. Pencereler için `*_Window_Wide_Round` / `_Thin_Round`, dış pencerelere `Window_*`. Yıkık mahzen
  kemerleri için `Wall_Arch`, köşelere `Corner_*`.
- **Zemin:** Salon, Giriş ve Şapel `Floor_Brick` / `Floor_RedBrick`; odalar `Floor_WoodDark` / `Floor_WoodLight`;
  mahzen `Floor_UnevenBrick`. Galeri kenarlarına `Floor_WoodDark_Half*`.
- **Merdiven:** `Stair_Interior_Solid` (3.0 m / 3.85 m; bütün düz merdivenler buna göre ≥ 3.85 m çalışır, validator
  kontrol eder), `Stair_Interior_Rails`, `SM_KG_StairRail`. Galeri korkuluğu için `Balcony_Cross_Straight/Corner`.
- **Çatı:** `Roof_RoundTiles_*`, kuleye `Roof_Tower_RoundTiles`, `Roof_Dormer_RoundTile`, `Prop_Chimney*`.
- **Mobilya:** `Kitchen_*` (ocak, dolaplar, uzun masa, raflar), `Bedroom_*` (çift ve tek yataklar, gardırop, komodin,
  sandıklar), `SM_KG_Hearth`, `SM_KG_Rug_*`, `SM_KG_Candlestick`, `SM_KG_Chest_Wood`, `BookGroup_*`, `Chair_1`, `Bench`,
  `Lantern_Wall`. Bütün doldurma `kg_interiors.py`'nin oda fonksiyonlarını (`hearth`, `dining`, `bedroom`, `storage`,
  `desk`, `corner_set`) oda türüne göre yeniden kullanır.
- **Dış mekân:** Pirate paketi (`Barrel`, `crates`, `pier`, `Torch`, `Rowboat`: kayıkhane ve tünel), `Prop_MetalFence_*`
  (mezarlık), `SM_KG_GardenLamp`, `SM_KG_Firewood`, `SM_KG_Oar`, doğa kiti kayaları.

### 9.2 Yeni Blender propları (`KG_DressManor` paketi, palet dokusu, `kg_import_dress_pack.py` ile)
| Prop | Üçgen | Not |
|---|---|---|
| Avize (iple inip kalkan, mumlu) | 1.5k | Görev 4; çıkrık ve makara ayrı |
| Uzun toplantı masası (20 kişilik) + oymalı sandalye | 1k + 400 | Salon |
| Orkestriyon | 2k | Balo salonu, Görev 12 |
| Kıvılcım panosu + Leyden kavanozu (taşınabilir) + bıçak şalter | 1.5k + 200 | Görev 11, Işık Kesintisi |
| Büyük saat mekanizması (çark, sarkaç, kadranın içi) | 3k | Saat Odası |
| Çan ipi ve küçük çan | 300 | Şapel |
| Kilise sırası, sunak, günah çıkarma hücresi | 400 / 600 / 800 | Şapel, S1 |
| Kitaplık (duvar modülü 2 m) + dönen kitaplık | 600 | Kütüphane |
| Bilardo masası, isteka rafı | 800 | Bilardo |
| Bakır küvet + kazan | 900 | Hamam |
| Şövale, tuval, portre çerçevesi (boş ve dolu) | 200–400 | Atölye, Galeri, S3 |
| Sallanan at, oyuncak kayık, müzik kutusu | 300–500 | Çocuk Odası |
| Kepenkli pencere (animasyonlu kanat) | 300 | 29 takırdayan pencere |
| Sarnıç sütunu + tahta iskele + çıkrık | 400 | Sarnıç |
| Mezar taşları (3), anıt mezar, mezar feneri | 200–1.2k | Mezarlık, S4 |
| Sera cam modülü (2 m) | 150 | Sera |
| Paratoner + sinyal feneri | 600 | Kule |
| "G." kolisi | 150 | Görev 17 |

### 9.3 Yeni yapı parçaları
- **Çift kat salon kabuğu:** v2 duvarları iki kat üst üste konur. Galeri döşemesi `Floor_WoodDark` ile boşluk
  bırakarak döşenir.
- **Kaya adası ve deniz:** v2 arazi hattı `kg_build_terrain_v2.py` küçük bir kaya ağıyla kullanılır. Su için mevcut
  `KG_Water` ve normal dokuları.

---

## 10. Performans bütçesi

Referans makine RTX 5070 + Ryzen 7 7800X3D, 1080p. Hedef ≥ 90 fps (en az 60). Sahne hedefi `06_Art_Direction §8`'e
uyar: ekranda ≤ 2.5M üçgen, **≤ 1500 draw call**.

| Kalem | Bütçe | Nasıl |
|---|---|---|
| Kit parçası | ~6.000 örnek (duvar, zemin, çatı) | Mesh başına tek bir HISM; tek palet materyali → birkaç yüz draw call |
| Mobilya ve prop | Oda başına 15–25k üçgen, ~4.000 örnek | Paylaşılan mesh'ler HISM; küçük eşyada 15 m cull mesafesi |
| Görünürlük | Kat ve oda kapatma | Duvarlar kalın. `Precomputed Visibility` yok (her şey Movable) → HLOD ve ön oklüzyon: kat başına `Cull Distance Volume`, mahzen ve çatı ayrı. Ekranda aynı anda ≤ 3 oda hedeflenir. |
| Işık | Görünür gölgeli ≤ 4 (şömineler); gölgesiz nokta ışık ≤ 24 görünür, toplam ≤ 160, yarıçap ≤ 8 m | Kural: bütün ışıklar Movable. Mumlar emissive + ortak ışık. |
| Şimşek | Tek Directional Light yoğunluk darbesi + pencerelere emissive, post-process pozlama | Pencere başına ışık yok |
| Yağmur | Sadece dış mekânda GPU Niagara, ≤ 30k parçacık; pencerelerde decal ve damla | İçeride yok |
| Deniz | Mevcut su materyali, ≤ 2 düzlem | — |
| Ses | Fırtına ambiyansı 2D; takırdayan pencereler 3D, ≤ 16 aynı anda | Gök gürültüsü global |
| Ağ | Yeni replike aktör: 29 pencere + 6 geçit + ~56 görev noktası + 1 fırtına yöneticisi | Pencere ve geçit durumu FastArray'de, push-model |
| Ölçüm | SPRINT-018 kabulü: `kg.World.Stat unit`, salonda 20 bot ile ekran dışı render turu [R] | — |

---

## 11. Mekanik kontrol listesi (Pillars, Ç1–Ç12)

Haritanın yeni mekanikleri tek tek puanlandı. Ç1–Ç4 zorunludur. Kalan 8 sorudan en az 6'sı Evet olmalıdır.

| # | Soru | Harita (bütün) | Işık Kesintisi | Şimşek + gök gürültüsü | Gizli geçitler | Takırdayan pencere |
|---|---|---|---|---|---|---|
| Ç1 | Karar | Evet: 2+ yol her yerde (döngüler, darboğaz mı güvenli mi), penceresiz oda mı pencereli mi | Evet: tamire koş / fırsatı kolla / salonda bekle | Evet: flaşı bekle ya da gürültüde vur | Evet: geçit hızlı ama gürültülü ve iz bırakıyor | Evet: kapat ya da örtü olarak kullan |
| Ç2 | İz | Evet: oda = defter bölgesi, 5 zemin türü iz tutuyor | Evet: elde is (45 sn), defter "sabotaj" olayı | Evet: flaş tanığı deftere düşer | Evet: gıcırtı 12 m, aralık panel, toz | Evet: ses 25 m, ıslak zemin izi |
| Ç3 | Karşı hamle | Evet: gözetleme noktaları (galeri, portre, kule) | Evet: iki şalter ya da kavanoz; 60 sn tavan | Evet: penceresiz oda riski; tahmin edilebilir | Evet: takoz ve Çilingir | Evet: herkes 2 sn'de kapatır |
| Ç4 | Ölçüm | S4.1 (her bölgede ≥ 3 sebep), S4.3 (risk eğimi), S4.5 (toplanma p90 9.9 sn) | S4.6 (gün başı değişiklik), S6.1 (Sabırsızın öldürme dışı fiili) | S2.1 (%50–80 tanık bandı), S2.2 | S6.1, S4.3 | S3.1 (her kötü fiilin izi), S4.6 |
| Ç5 | Bağlantı | Evet: defter, bot navmesh, bölge kapıları, görevler | Evet: defter, rol (Çilingir), görev 11, bot | Evet: algı modeli, defter, toplantı | Evet: defter, bot, harita, rol | Evet: algı, iz, görev 5 |
| Ç6 | Ölçek | Evet: bölge kapıları 6→20 | Evet | Evet | Evet | Evet |
| Ç7 | Varyans | Evet: tohum şafakta | Evet: sabotajı oyuncu seçer | Evet: tohumlu ve duyurulu şiddet | Evet | Evet: tohumlu |
| Ç8 | Bot | Evet: navmesh + görev rotaları | Evet: tamir hedefi iki nokta | Hayır: botların flaş zamanlaması sonra gelecek (G6) | Evet: geçit navlink'i | Evet: kapatma hedefi |
| Ç9 | Sessiz yol | Evet | Evet: HUD uyarısı | Evet: görsel | Evet | Evet |
| Ç10 | Okunabilirlik | Evet: oda adları, harita | Evet: "Işıklar söndü: panoya ve bobine" | Evet | Evet: "Gizli geçit: 3 sn, gürültülü" | Evet |
| Ç11 | Frustrasyon | Evet: kilitli kapı görünür | Evet: HUD sayacı, 60 sn üst sınır | Evet: flaş önce, gürültü sonra | Evet: toz izi | Evet |
| Ç12 | Tempo | Evet: erken keşif, geç dönemde darboğaz | Evet: orta ve geç evre | Evet: bütün evreler | Evet: orta evre | Evet: erken |
| **Sonuç** | | **12/12** | **12/12** | **Ç1–4 Evet, 7/8** (Ç8 Hayır: G6 bot algısı gelince Evet) | **12/12** | **12/12** |

---

## 12. Doğrulama (tekrarlanabilir)

```powershell
python Tools/Level/author_stormmanor.py     # JSON üretir
python Tools/Level/validate_stormmanor.py   # PASS
python Tools/Level/render_stormmanor.py     # Docs/Level/StormManor.png
```

Validator'ın sert kuralları:
- Bölümler tam, 2 m ızgara
- Kat içinde çakışma yok; çift kat boşluğunun üstünde oda yok
- Her kapı iki odanın ortak duvarında ve tam genişlikte
- Merdivenler ardışık katları bağlıyor ve kite sığıyor (koşu ≥ 1.28 × yükseklik)
- Bağlantı (gizli geçitsiz) tam, 6 bölge kapısının her birinde de
- Çıkmazlar işaretli, köprüler sadece işaretli çıkmazları kesiyor
- Toplantı her noktadan ≤ 30 sn
- Her görev adımı erişilebilir ve ≤ 75 sn
- Her adlandırılmış odada bir görev noktası ya da gizli geçit ucu, bir saklanma yeri ya da delil yüzeyi var
- 18–24 oda, 3 kat + mahzen, ≥ 3 geçit, ≥ 15 görev

**Sonuç: PASS.** 24 oda, toplanma p90 9.9 sn / en fazla 14.0 sn, görevler 22–49 sn, tek köprü Kule → Kule Tepesi
(işaretli). Tek uyarı yoktur.

---

## 13. SPRINT-018 inşa sırası (her adım ekranda yeni içerik gösterir)
1. Kaya adası + deniz + fırtına gökyüzü (boş kaya, ilk görsel).
2. `kg_build_manor.py`: JSON'dan odaları kit parçalarıyla kurar. Kat kat ilerler (zemin → 1. kat → mahzen → çatı ve
   kule), sonra kapılar (`KGDoor`), merdivenler, pencereler.
3. Döşeme: oda türüne göre `kg_interiors` fonksiyonları, sonra `dress_manor_*` bölge modülleri.
4. Işık: şömineler, mumlar, kıvılcım lambaları.
5. Görevler: `stormmanor_world_chores.json` + route check (`kg.WorldChore.Routes`).
6. Fırtına sistemi: şimşek, gök gürültüsü, pencereler, Işık Kesintisi; sonra gizli geçitler.
7. Doğrulama: navmesh route, 20 bot maç smoke'u, render turu. Invariant'lar I1–I4 bozulmamalı.

## 14. Riskler ve açık sorular
- **Alan / oyuncu:** N = 8'de 890 m²/oyuncu yüksek olabilir. Karşılaşma soak'u karar verir, kapılar veriyle ayarlanır.
- **Şimşek ile S2.2 bandı:** flaş tanıklığı tanımlı tanık oranını %20'nin üstüne itebilir. Gerekirse flaşta isim
  etiketi gösterilmez (sadece siluet).
- **Yeni sistemler:** kodda henüz yok. Işık Kesintisi, pencereler, gizli geçitler ve fırtına yöneticisi SPRINT-018'de
  (ya da ayrı bir sistem sprintinde) yazılır. Bu belge sadece veriyi ve kuralları sabitler.
- **Lore anakronizmi:** kıvılcım lambaları "Anselm'in icadı" olarak sunulur (1690'lar). Köyün saat-büyü tonuna uyar.
