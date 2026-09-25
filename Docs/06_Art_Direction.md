# KILL GODOT — Sanat Yönü

## 1. Hedef görünüm
**"Town of Salem'in 3D'si, ama canlı."** Doygun renkler, güçlü silüetler ve az detayla çok karakter. Low-poly ama özenli: kenarlar hafif pahlı, yüzeyler temiz ve düz renk ağırlıklı. Işık sıcak ve masalsı. Referans hissi: Town of Salem, Sea of Thieves'in renk cesareti, R.E.P.O.'nun sevimli tuhaflığı ve Fall Guys'ın okunaklılığı.

## 2. Karakterler: hatırlanabilir ve karizmatik
> **Karar (2026-09-24): Kukla yönü REDDEDİLDİ.** Kullanıcı prosedürel kukla modellerini "AI gibi ve berbat" buldu. Karakterler gerçek, sanatçı elinden çıkmış modellerle yapılıyor: **Quaternius Universal Base Characters + Modular Outfits** (CC0), `M_KG_Character` materyalinde renk tonu ve saturasyon artışıyla canlı renkli. Bkz. `Art/Concept/M2_Villagers.png`, ITER-002. Aşağıdaki bölüm sadece tarihçe olarak duruyor. Rigid-part, çene menteşesi ve kontur gibi teknik fikirlerin hangileri yeni yöne taşınabilir, bu ayrıca değerlendirilecek.

### ~~Önerilen yön: "Boyalı Ahşap Kuklalar"~~ (reddedildi)
Köylüler el oyması, cıvıl cıvıl boyanmış **ahşap kuklalardır**. Fındıkkıran tarzında **menteşeli bir çeneleri** vardır.
- **İkonik:** Among Us fasulyesi ya da Fall Guys fasulyesi gibi tek bakışta tanınan bir marka silüeti olur. Büyük kafa (baş:gövde ≈ 1:3.5), abartılı burun, kaş ve çene.
- **Ses ve ağız:** konuştukça çene **gerçekten menteşeden açılır ve kapanır**. R.E.P.O. tarzı komikliğin en saf hali ve **yüz rig'i gerektirmez**.
- **Teknik avantaj:** her uzuv **tek bir kemiğe %100 bağlı sert parçalar** (rigid skinning) ve bilyeli eklemlerden oluşur.
  - Kozmetikler sadece soketlere takılır.
  - Skinning maliyeti neredeyse sıfırdır, LOD'lar kolaydır, mobilde mükemmel çalışır.
  - İnsansı iskelet olduğu için Mixamo, UE Mannequin ve Game Animation Sample animasyonları retarget edilebilir.
  - Blender'da Python ile prosedürel üretilebilir.
- **Yaş derecesi:** kan yok. Vurulunca **boya pulları ve talaş** saçılır. Asılma, "iplerin kesilmesi" metaforuyla gösterilir.
- **Delil sistemine bağlı:** yaralı kuklanın bıraktığı **boya damlaları kendi rengindedir**. Kırmızı boya izi kırmızı ceketli birine götürür.
- **Mimik:** gözler, kaşlar ve göz kapakları **boya texture'ı** olarak flipbook'la değişir. Göz kırpma, kızgın ve şaşkın ifadeler shader parametresiyle çalışır.
- **Lore bağı:** 333 yıldır yaşlanmayan köylüler… belki de artık tahtadandır.

### Alternatif: **Stilize insanlar**
Klasik cartoon insanlar (büyük kafa, tıknaz gövde). Asset kitlerinde daha çok hazır malzeme var, ama çene kemiği ve blendshape gerekir, kozmetik kıyafetlerin gövdeye uyması (skinning ve clipping) çok daha pahalıdır.

### Başlangıç kadrosu: 12 "Yüz"
Her oyuncu bir **Yüz** seçer (kafa ve imza detay), üstüne kozmetik giyer. Her yüz 20 m'den ve küçük bir resimden bile tanınmalı, tek bir **imza detayı** olmalı.

| Yüz | İmza | Kişilik |
|---|---|---|
| **İhtiyar Tuz** | Ak sakal, pipo, kırmızı burun | Her şeyi görmüş kaptan |
| **Tilly Çil** | Çiller, kızıl kısa saç, **eksik ön diş** | Yaramaz balıkçı kız |
| **Peder Bellweather** | Keşiş tıraşı, yuvarlak gözlük, tombul yanaklar | Fazla iyi niyetli |
| **Dul Marrow** | Sivri çene, siyah dantel, tek kaş hep kalkık | Her şeyden şüphelenir |
| **Bay Pozzo** | Geniş bıyık, melon şapka, çift çene | Gösterişli ve kibirli |
| **Lucky** | Sıska, upuzun boyun, dağınık beyaz saç | Hüzünlü bilge |
| **Demirci Hodge** | Kare çene, kurum lekeleri, çapa dövmesi | Az konuşur, çok vurur |
| **Dumpling** | Yuvarlak, un lekeli yanaklar, hep gülümser | Fırıncı neşesi |
| **Nell Fener** | Alnında dalgıç gözlüğü, rüzgârda savrulmuş saç | Kararlı kaşif |
| **Tick** | Monokl, saat çarkı küpe, dar yüz | Şüpheli derecede dakik |
| **Madam Vellum** | Kocaman topuz, boyalı dudak, yelpaze | Dedikodunun kraliçesi |
| **Kıvırcık Bo** | Afro bukleler, kulak arkasında kalem, gap-toothed gülümseme | Herkesin kankası |

Sezonlarla yeni yüzler gelir. Beckett'tan esinli **Vladimir** ve **Estragon** melon şapkalı ikili yüzleri özel sezon ödülü olur.

## 3. Kozmetik slotları ("bir sürü" olacak)
| Bölge | Slotlar |
|---|---|
| **Kafa** | Şapka (üç köşeli, melon, fötr, korsan, aşçı, taç…), Saç/Peruk, Kaş ve Göz boyası, Bıyık/Sakal, Gözlük/Monokl, Küpe, **Diş stili** (altın diş, eksik diş, vampir dişi) |
| **Boyun** | Atkı, dantel yaka, kolye, papyon |
| **Gövde** | Üst (gömlek, yelek, ceket, redingot), **Pelerin/Cüppe** (kemik zinciri ile ucuz fizik dalgalanması), Kemer ve Çanta |
| **Eller** | **Eldiven**: viewmodel'de de görünür, yani first-person'da kendi eldivenini görürsün |
| **Bacak** | Pantolon, etek, şalvar |
| **Ayak** | **Ayakkabı/Çizme**: her birinin kendine özgü **taban izi** var (Casus'u yakalatır!) |
| **Sırt** | Sepet, lavta, olta, kürek, heybe |
| **Gövde malzemesi** | Kukla yönünde: meşe, kiraz, abanoz, **yaldızlı**, porselen, mermer, deniz kabuğu |
| **Oyun içi** | Sabırsız maskesi (kill cam, epilog ve kendi ekranında görünür), **yakın dövüş skinleri** (şemsiye kılıç, çekiç, tava, kılıçbalığı, baget, kürek, şamdan, **yelkovan bıçağı**, tırpan, kauçuk tavuk, dev tüy kalem…), **Araf kılıçları**, silah skinleri, fener skini, köpek skini, kayık skini |
| **İfade** | Emote, dans, fotoğraf pozu, **ölüm efekti** (konfeti, tüy fırtınası, balık yağmuru, talaş kasırgası), hayalet izi, hortlak kefeni, ses efekti paketi |
| **Kimlik** | Unvan, kapı tabelası, rol kartı arkası, mühür (sticker), ev teması, yükleme ekranı, menü sahnesi |

## 4. Renk paleti
Tek bir **palet atlas texture'ı** (256×256, 8×8 renk kutusu ve her kutuda dikey açık-koyu gradyan). Ortamın %90'ı bu tek texture ve tek master materyal ile boyanır. Sonuç: devasa batching, çok az draw call.

| Ad | Hex | Kullanım |
|---|---|---|
| Turkuaz Deniz | `#1FB5C4` | Sığ su, vurgu |
| Derin Deniz | `#0E6E8C` | Açık deniz, gölge tonu |
| Çayır Yeşili | `#7ACC3D` | Çimen |
| Orman | `#2E8B57` | Ağaçlar |
| Kiremit | `#E0413A` | Çatılar |
| Turuncu | `#F28C28` | Çatılar, tabelalar |
| Hardal | `#F2C230` | Kapılar, detay |
| Krem Duvar | `#FFF1D6` | Duvarlar |
| Ahşap | `#A0612B` | Kereste |
| Koyu Ahşap | `#5A3A22` | Kirişler |
| Erik | `#7B3F9E` | Kumaş, büyü |
| Alacakaranlık Pembesi | `#E0529C` | Gökyüzü (akşam) |
| Gece Çividi | `#26235C` | Gökyüzü (gece) |
| Fener Işığı | `#FFB347` | Pencereler, mumlar |
| Sabırsız Kırmızısı | `#C8102E` | Katil UI'ı, maske |
| Hayalet Camgöbeği | `#9FF3FF` | Hayaletler |
| Sis Leylağı | `#C9C3E6` | Sis |

## 5. Işık ve fazlar
| Faz | His |
|---|---|
| **Gündüz** | Sıcak sarı güneş, turkuaz gökyüzü, kısa yumuşak gölgeler, canlı doygunluk |
| **Toplantı (akşamüstü)** | Turuncudan pembeye gökyüzü, uzun gölgeler, çan kulesinde tepe ışığı |
| **Gece** | Çivit ve mor, ay ışığı soğuk mavi. Pencerelerden **turuncu** sıcaklık. Fener ışını her 8 sn'de köyü tarar. |
| **Sis** | Leylak hacimsel sis (PC). Mobilde ucuz yükseklik sisi ve kartlar. |

**Işık bütçesi:** gece sahnesinde gölgeli dinamik ışık ≤ 4 (oyuncunun feneri ve yakın mum). Diğer ışıklar gölgesiz ya da emissive kartlarla sahte ışık. Pencere parlaması materyalle yapılır.

## 5.1 Atmosfer: gökyüzü, bulutlar, rüzgâr
**Bulutlar:**
- **PC:** UE **Volumetric Cloud**. Stilize ayarla yumuşak, pamuksu, kenarları ışık alan kümülüs bulutları. Gün batımında pembe-turuncu kenarlar.
- Bulut kalitesi ölçeklenebilirliğe bağlıdır: Yüksek'te volumetric, Orta'da daha düşük örnekleme.
- **Mobil ve Düşük ayar:** elle boyanmış bulut kubbesi (panoramik texture ve 2 katman kayan bulut kartı).
- Bulutlar **rüzgâr yönünde akar**. Fırtına olayında kararır ve alçalır.
- Sis bulutları denizde duvar gibi durur. Sis olayında köye doğru süzülür.

**Rüzgâr mekaniği** (`UKGWindSubsystem`, tek replike değer: yön ve güç):
| Etkilediği şey | Nasıl |
|---|---|
| Ağaçlar, çimen, çalılar | Materyal WPO sallanması, rüzgâr gücüne bağlı genlik (maliyetsiz) |
| Bayraklar, çamaşır ipleri, tabelalar, pelerinler | Kemik zinciri ve AnimDynamics ya da vertex animasyonu |
| Yel değirmeni, rüzgâr gülü, gemi yelkenleri | Dönme hızı ve yön |
| Bacalardan duman, mum alevleri, şömine | Niagara rüzgâr kuvveti |
| Yapraklar, tozlar, kıvılcımlar | Niagara parçacık akıntısı |
| **Ses** | Rüzgâr uğultusu, güçlü rüzgârda **proximity voice rüzgâr yönünde biraz daha uzağa taşınır**, ters yönde kısalır (hafif, oynanışı bozmaz) |
| Fırlatılan hafif nesneler | Küçük sürükleme kuvveti (şapkan uçabilir! Şapka uçunca kovalanır, kozmetik esprisi) |
| Kayıklar | Yelkenli kayık rüzgârla hızlanır |
| Deniz | Dalga yüksekliği rüzgârla artar |

**Hava durumları** (rastgele olaylarla bağlantılı): Açık, Parçalı Bulutlu, Sisli, Yağmurlu, **Fırtına** (şimşek, gök gürültüsü proximity voice'u bastırır), Dolunay gecesi (parlak ay, kurt uluması).
- Su birikintileri ıslak yüzey materyal parametresiyle, yağmur damlaları kameraya yakın Niagara'yla yapılır. Mobilde yağmur çizgi kartlarıdır.

**Atmosfer bütçesi:** volumetric cloud + height fog + sky atmosphere PC'de ≤ 1.2 ms (RTX 3060, 1080p). Mobilde bulut kubbesi ve üstel yükseklik sisi ≤ 0.3 ms.

## 6.0 TF2 dokunuşu (kullanıcı referansı)
Team Fortress 2'nin "illüstratif" render'ından şunları alıyoruz:
- Sıcaktan soğuğa kayan gölge rampası (half-Lambert). Gölgeler hiçbir zaman ölü siyah değil.
- Güçlü **rim ışığı** ve karakterlerin arka plandan her zaman ayrışması.
- Hafif **boyanmış** yüzey hissi: palet texture'ındaki gradyan satırları ve çok hafif fırça dokusu.
- Abartılı ama okunaklı silüet, her rolün tek bakışta okunması.

Sonuç: **Among Us 3D (kontur + düz renk) × TF2 (sıcak/soğuk boyalı ışık, rim)**. Among Us'tan temiz konturu ve oyuncak renklerini, TF2'den ışığın boyalı sıcaklığını alıyoruz.

## 6. Shading: **Among Us 3D tarzı toon** (kullanıcı referansı)
Referans: Among Us 3D ekran görüntüsü. Düz, doygun renkler, **2–3 bantlı cel shading**, **kalın siyah konturlar**, net gölge sınırları, oyuncak gibi temiz yüzeyler.
- **Toon ışık modeli:** özel shading yerine, stilize master materyalde `N·L` bir **ramp texture** ile 3 banda (ışık / yarı ton / gölge) basamaklanır. Gölge tonu siyah değildir, rengin **doygun ve koyu** hali olur (Among Us kırmızısının bordo gölgesi gibi).
- Ramp gökyüzü rengine göre ısınır ya da soğur. Gece gölgeleri mor-lacivert olur.
- **Kontur (çekirdek görünüm, opsiyonel değil):**
  - **Karakterler ve eldeki eşyalar:** *inverted hull* yöntemiyle kalın, sabit ekran kalınlığında siyah kontur. Hem viewmodel'de hem mobilde çalışır ve ucuzdur (tek ek draw, sadece karakterlerde).
  - **Ortam:** PC'de derinlik ve normal tabanlı **post-process kontur** (Sobel), mesafeyle incelir. Mobilde kapalı, yerine binaların ana kenarlarında modele gömülü ince kontur şeritleri kullanılır.
- **Rim ışığı:** karakter silüetini gece bile okunur yapar.
- Albedo paletten gelir. Pürüzlülük yüksektir (mat oyuncak hissi). Speküler sadece metal ve cam eşyada, keskin tek bir parlama lekesi olarak.
- Vertex color ile **boyanmış AO** (Blender'da bake).
- Su: stilize su (Gerstner dalga, **toon köpük çizgileri**, bantlı derinlik rengi). PC'de SSR, mobilde yansıma yok.

## 7. First-person ve viewmodel sanatı
- **Kollar:** kukla kolları ve bilyeli bilek. Eldiven kozmetiği burada görünür. 4–6k üçgen.
- **Silahlar ve eşyalar:** kameraya çok yakın oldukları için **daha detaylı**: 3–8k üçgen, 512–1024 px ayrı texture. Skin materyal sistemi: desen, **aşınma (0.00–1.00)**, **desen tohumu (0–999)**, mühür çıkartmaları.
- **Animasyon hedefi CS2 akıcılığı:** çekme (draw), bekleme, **inspect** (her silahın kendi gösterişli inspect'i), saldırı, dolum. Üstüne yürüme sallanması (bob) ve fare sürüklenmesi (sway). Detaylar `05_Tech_Architecture.md` içinde.

## 8. Bütçeler (hedef, ölçülerek güncellenecek)
| Varlık | Üçgen (LOD0) | Texture | Materyal |
|---|---|---|---|
| Karakter (kozmetiklerle) | 6–10k | palet + 512 yüz atlası | ≤ 3 |
| Ev dış kabuğu | 3–6k | palet | 1 |
| Ev içi (tüm mobilya) | 15–25k | palet | 1–2 |
| Küçük eşya | 50–800 | palet | 1 |
| Viewmodel silah | 3–8k | 512–1024 | 1 |
| Ağaç | 800–2.5k | palet + yaprak kartı | 2 |
| **Sahne (PC, 20 oyuncu)** | ≤ 2.5M ekranda | | **≤ 1500 draw call** |
| **Sahne (mobil)** | ≤ 400k | | **≤ 350 draw call** |

## 8.1 Fikir üretimi: "Çok üret → en iyisini seç" döngüsü
1. **Prosedürel varyasyon sayfaları (Blender, şu an çalışıyor):** `Tools/Blender/kg_variant_sheet.py` parametre uzayından (yüz, saç, şapka, burun, kaş, renk paleti) **tohumlu** rastgele kombinasyonlar üretir ve kontakt sayfası olarak render eder (`Art/Concept/Variants/`). Beğenilen kartın **tohum numarası** söylenir, o tasarım "Yüz" olarak sabitlenir.
2. **Harici AI görsel ve 3D araçları (opsiyonel):** Midjourney, SD, Meshy, Tripo, Rodin gibi araçlar için hazır promptlar `Docs/Prompts/ArtPrompts.md` içinde. Bu oturumda görsel üretim modeli bağlı değil. Bir görsel ya da 3D üretim bağlantısı (MCP) eklenirse döngü otomatik yürür: üret → kontakt sayfası → seç → Blender'da temizle ve palete oturt → UE.
3. **Seçim kriterleri:** 20 m'den okunuyor mu, 64 px ikonda tanınıyor mu, çene menteşesi görünüyor mu, palet içinde kalıyor mu, poligon bütçesinde mi?

## 9. Asset pipeline (Blender → UE)
1. **Kaynak:** ücretsiz paketler (bkz. `Research/AssetResearch.md`) ya da prosedürel Blender scriptleri.
2. **`Tools/Blender/kg_process_asset.py`** (headless) şunları yapar:
   - içe aktarma (FBX/GLB/OBJ)
   - ölçek normalizasyonu (1 birim = 1 m)
   - orijini tabana alma
   - renkleri palete eşleme (en yakın palet rengi ile UV'yi o renk kutusuna yığar)
   - materyal birleştirme
   - basit **LOD** üretme (decimate)
   - **UCX_** çarpışma kutusu
   - UE için FBX dışa aktarma (`SM_`, `SK_` ön ekleri)
3. **UE içe aktarma:** `Tools/Unreal/kg_import.py` (editör Python) klasör yapısını, LOD'ları, çarpışmayı ve materyal atamasını otomatik yapar.
4. **Adlandırma:** `SM_KG_House_Fisher_A`, `SK_KG_Puppet`, `M_KG_Palette`, `MI_KG_Palette_Night`, `T_KG_Palette`, `BP_KG_Door`, `GC_KG_Crate` (kırılabilir).
5. **Lisans takibi:** her içe aktarılan paket `Docs/Credits.md` içine yazılır (kaynak, yazar, lisans, atıf metni).
