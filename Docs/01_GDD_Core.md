# KILL GODOT — Çekirdek Oyun Tasarımı (GDD)

## 1. Maç akışı (Klasik mod)

```
LOBİ (Meyhane) → ISINMA → ROL KARTI → [ ŞAFAK → GÜNDÜZ → TOPLANTI → MAHKEME → GECE ] × n → SON EKRAN
```

| Faz | Süre (N = oyuncu) | Ne olur |
|---|---|---|
| **Rol Kartı** | 12 sn | Tarot tarzı kart açılır: rol, hedef, meslek, ev adresi. Sabırsızlar birbirini görür. |
| **Şafak** (Dawn) | 12 sn | Haberci Çocuk (NPC) iskelede duyuru yapar: gece kimler öldü, bulunan vasiyetler, olaylar. *"Bay Godot bu akşam gelemeyecek, ama yarın kesin gelecek."* |
| **Gündüz** (Work) | 150 + 6·N sn (6→186, 20→270) | Meslek, görev, keşif, loot, rol yapma. Öldürmek mümkün ama çok riskli. Ceset bulunursa **çan** çalınır. |
| **Toplantı** (Meeting) | 45 + 3·N sn | Belediye binasında konuşma. Suçlama yapılır, oylamayla biri kürsüye çıkar. |
| **Mahkeme** (Trial) | savunma 20 + karar 15 + son söz 8 sn | Sanık darağacı sahnesinde savunma yapar. Herkes *Suçlu/Masum/Çekimser* der. Suçluysa asılır. |
| **Gece** (Night) | 75 + 3·N sn | Sokağa çıkma yasağı. Herkes evine gider. Roller yeteneklerini kullanır, Sabırsızlar avlanır. |

- Gündüz ilk turda toplantı yoktur. Birinci gün tanışma ve görev günüdür.
- **Toplanma:** Çan çalınınca 20 sn içinde herkes belediyeye yürür. Süre bitince yetişemeyenler karartmayla ışınlanır ve "Geç Kalan" damgası alır, o turda oy veremezler. Bu 20 saniye kanıt saklamak veya son dakika cinayeti için gerilim penceresidir.
- **Maç sonu:** kazanan taraf, tüm rollerin açıklanması, zaman çizelgesi (kim kimi öldürdü), MVP'ler, XP/altın ödülleri.

## 2. Taraflar ve kazanma koşulları
| Taraf | Kim | Kazanma |
|---|---|---|
| **Bekleyenler** (Town) | Kasaba rolleri | Tüm Sabırsızlar ve düşman nötrler ölünce. |
| **Saat Kırıcılar** (Clockbreakers) | Sabırsız takımı (mafya) | Kalan herkesi sayıca geçince ya da öldürünce. |
| **Yalnız Katiller** | Seri Katil, Kurtadam, Kundakçı… | Son ayakta kalan olunca. |
| **Vampir Soyu / Veba** | Dönüştürücü takımlar | Kendi takımlarından başka kimse kalmayınca. |
| **Gezginler** (Nötr) | Soytarı, Cellat, Korsan… | Kendi özel hedefleri (bkz. `02_Roles.md`). |

**İkinci kazanma yolu, Büyük Hazırlık:** Kasaba görevlerle **Godot'nun Karşılaması** barını doldurursa ertesi şafak **Fener Aydınlanması** olur. Fenerin ışığı rastgele bir Sabırsızı 10 saniyeliğine kırmızı yakar. Kesin zafer değildir ama güçlü bir ipucudur. Katiller bu yüzden sabotaj yapmak zorunda kalır, görevler anlamlı hale gelir.

## 3. Ölçekleme (6–20 oyuncu)

### Rol dağılımı
| N | Kasaba | Sabırsız | Nötr | Not |
|---|---|---|---|---|
| 6 | 4 | 1 | 1 | Sabırsız = Saat Ustası |
| 7 | 5 | 1 | 1 | |
| 8 | 5 | 2 | 1 | |
| 9 | 6 | 2 | 1 | |
| 10 | 6 | 3 | 1 | 2 Saat Kırıcı + 1 yalnız katil |
| 11 | 7 | 3 | 1 | |
| 12 | 7 | 3 | 2 | |
| 13 | 8 | 3 | 2 | |
| 14 | 8 | 4 | 2 | |
| 15 | 9 | 4 | 2 | Town of Salem klasiği gibi |
| 16 | 9 | 4 | 3 | |
| 17 | 10 | 4 | 3 | |
| 18 | 10 | 5 | 3 | 3 Saat Kırıcı + 2 yalnız katil |
| 19 | 11 | 5 | 3 | |
| 20 | 12 | 5 | 3 | |

**Güç bütçesi:** Her rolün bir `PowerScore` değeri var (bkz. `02_Roles.md`). Üretici, `Σ Kasaba − Σ Sabırsız − Σ Nötr Tehdit` farkının N'ye göre hedef bantta kalmasını sağlar. Aynı N'de fazla güçlü kasaba dizilişi gelirse rastgele bir kasaba rolü daha zayıfıyla değiştirilir. Hedef kazanma oranları: Kasaba **%48–52**, Sabırsız **%35–40**, geri kalanı nötr. Telemetri her maçı kaydeder ve katsayılar yamayla ayarlanır.

### Harita ölçekleme (Bölge Kapıları)
| N | Açık bölgeler |
|---|---|
| 6–9 | Liman, Meydan, Çarşı, 1 mahalle kümesi, Fener Burnu, yeraltının %40'ı |
| 10–14 | + Çayırlar ve Çiftlik, Mezarlık ve Kilise, 2. mahalle kümesi, yeraltının %70'i |
| 15–20 | Hepsi: + Orman, Dağ ve Eski Maden, 3–4. mahalle kümeleri, tüm yeraltı |

Kapalı bölgeler lore'a uygun şekilde kapanır: "Karantina – Veba" tabelası, çökmüş köprü, sis duvarı. Görev sayısı `8 + N` olur ve sadece açık bölgelerde üretilir. Evler oyunculara açık bölgelerden atanır.

## 4. Kamera, hareket, dövüş (First-person)
- **FP kamera + viewmodel:** kollar ve eldeki eşya ayrı render edilir, duvara girmez (CS2 hissi). Diğer oyuncular tam gövdeyi görür. Detaylar `05_Tech_Architecture.md` içinde.
- **Hareket:** yürüme 3.2 m/s, koşu 5.8 m/s (stamina), çömelme, zıplama, tırmanma (merdiven/kasa), yüzme (liman), kürek çekme (sandal).
- **Etkileşim:** `E` kullan, `LMB` saldır/at, `RMB` blok/nişan. Nesne tutma **fizik tutuşu** ile olur (R.E.P.O./Gmod hissi): tut, döndür, fırlat, istifle.
- **Can:** 100. Yenilenmez; Doktor, Şifacı veya ekmek/çorba ile iyileşir.
- **Yakın dövüş:** herkesin elinde meslek aleti ya da yumruk vardır. Tava, kürek, balık, dirgen gibi 30'dan fazla doğaçlama silah bulunur. Hasar 12–25, **itme** (stamina harcar, 0.5 sn sersemletir), blok.
- **Sabırsız suikastı (TF2 Spy hissi):**
  - Sabırsız bıçağı (ya da bıçak yerine geçen skin: şemsiye, yelkovan, tava…) elindeyken birinin **arkasına geçince viewmodel'de bıçak kalkar ve saplamaya hazır pozisyona** girer (sadece saldırgan görür).
  - Koşullar: 1.5 m içinde, hedef önünde, hedef sana sırtı dönük, ikiniz de aşağı yukarı aynı yöne bakıyorsunuz (yandan ya da yüz yüze "sırt bıçağı" yok).
  - Hazır pozdayken `LMB` **anında öldürür** ve imza animasyonu oynar: kurban öne doğru kukla gibi yığılır, talaş patlaması olur. Sunucu lag telafisiyle doğrular.
  - Önden saldırıda normal dövüş olur (35 hasar/vuruş).
  - Her bıçak skininin **kendi hazır pozu ve saplama animasyonu** vardır (kozmetik değer).
- **Kalabalık bonusu:** Sabırsıza vuran her ek kasabalı için +%25 hasar. Grup halinde gezmek gerçek bir savunmadır.
- **Yaralanma:** kan yerine **boya/talaş damlaları** (bkz. sanat yönü). 60 sn boyunca iz bırakır, bu da delildir. Aksak yürüme animasyonu da görülür.
- **Silahlar (ana mod):** nadir loot. Tek mermili çakmaklı tabanca, av tüfeği, harpon. Gmod Murder kuralı geçerli: **masum kasabalıyı vuran** silahı düşürür, 30 sn bulanık görür ve o maç boyunca silah alamaz. Sırtta taşınan silah görünür, bu da sosyal bilgidir.

## 5. Sabırsız Maskesi (kimlik gizleme)
- Her Sabırsız, 1 sn'lik animasyonla **Maske + Pelerin** takabilir. Takınca isim etiketi gizlenir, silüet tek tipe döner, ses hafifçe bozulur, ayak izleri anonimleşir.
- Maskeyi takarken ya da çıkarırken görülen kişi yakalanır. Bu anı kovalamak ana gerilim kaynağıdır.
- Maskeliyken dünyada görünen silah skini lobideki kuşanılmış skinlerden **rastgele** seçilir. Kendi skinin yine de **viewmodel'de, yani senin ekranında** görünür. Skin sahipliği böylece kimliği ele vermez.
- Kurban hayalet olunca katilin gerçek skinini görmüş olur. Medyum bu bilgiyi hayaletten öğrenebilir: *"Altın şemsiyeydi!"*

## 6. Deliller
Çamurda, kumda ve karda **ayak izi** kalır (60–120 sn). Boya ve talaş damlaları, kırılmış eşyalar, açık bırakılmış kapılar, düşen eşyalar (anahtar, şapka) da iz bırakır. Bunlara ceset üstünde **yara tipi** (Adli Tabip görür: *"yassı ve metalik, tava olabilir"*) ve **tanık hafızası** eklenir. Kişisel **Defter** (`J`) gördüğün olayları otomatik not alır: *"Gece 2 – Liman yakınında çığlık duydum"*. Elle de not eklenebilir.
- **Vasiyet** (Last Will): evdeki masada yazılır, ölünce bulunur. Sahtekar değiştirebilir.

## 7. Gece
- Herkesin bir **evi** var: 2 katlı, mahzenli, gerçekten eşyalı (bkz. `04_Map_Village.md`).
- **Kilitli kapı:** kırmak 8 sn sürür ve gürültülüdür. Mobilyayla **barikat** kurulabilir (fizik). Pencereler kırılabilir. Saklanma yerleri: dolap, yatak altı, mahzen.
- **Gece Defteri:** yeteneklerin çoğu evdeki mum ışığında açılan defterden seçilir (hedef seç). Sonuçlar şafakta öncelik sırasıyla çözülür. Bazı yetenekler **fizikseldir**, oraya gitmek gerekir: katillerin öldürmesi, Bekçi devriyesi, Kundakçının yağ dökmesi, Avcının tuzağı.
- **Çığlık:** proximity voice gerçekten çalışır. Komşular duvarların arkasından boğuk sesle duyar. Evdeki **el çanı** 1 kez çalınabilir ve 60 m içindeki herkese yön gösterir.
- **Saat Kırıcı telsizi:** gece Sabırsızlar kendi özel ses kanalında konuşur (Bas-Konuş `R`). Kulak Misafiri bunu bozuk ses olarak duyar.

## 8. Görevler (Lockdown Protocol esinli)
Herkes her görevi yapabilir, fake atılabilir. Mesleğin sana 3 **imza görevi** verir, ekstra altın ve XP kazandırır. Görevler **Hazırlık barını** doldurur ve **maç içi altın** kazandırır (çarşıdan kilit, fener, ekmek, bandaj alınır).

| Tip | Örnekler |
|---|---|
| **Taşıma** | Balığı iskeleden balık pazarına, odunu fırına, yağ varilini fenere, ekmeği meyhaneye, mektubu postaneye götür. Fizikle taşınır, düşebilir, çalınabilir. |
| **İstasyon (mini oyun)** | Ağ ör, hamur yoğur, nal döv, çan ritmi çal, harita oku, kalıp dök, ilaç karıştır, saat kur. |
| **Ortak (2 kişi)** | Ağır sandığı birlikte taşı, feribot halatını çek, çanı yukarı kaldır, kapanı aç. Güven inşa eder ama katille yalnız kalma riski taşır. |
| **Doğrulanabilir** | Sonucu herkese görünür: yanan sokak lambası, tahtaya asılan balık, bayrak direği. Kasaba bunlarla birbirini "temize çıkarır". |
| **Zamanlı** | Gelgit düşükken mağaradan sandık çıkar, pazar gemisi limandayken mal indir. |
| **Hayalet görevleri** | Ölü kasabalılar ruh dünyasında küçük görevler yapar (%30 verimle bara katkı). |

**Sabotajlar (Sabırsızlar):** fener yağını kes (gece daha karanlık olur), sahte çan çal, kuyuyu zehirle (stamina düşer), belediye kapısını kilitle, sis makinesi aç, bent kapağını açıp mahzenleri su bassın, domuzları salıver (kaos). Her sabotaj bir onarım görevi doğurur.

### 8.1 İstasyon görevleri: mini oyunlar ve görsel görevler (uygulandı)
İstasyonda `E` → ekranın ortasında Among Us tarzı bir pencere açılır (`08_UI_UX.md §6.3`). Dünya arkada hafif bulanık ve
karartılmış görünür; karakter yerinde durur, bu yüzden **görev yapan savunmasızdır** (arkana kim geldi, pencerenin
kenarından seçebilirsin). Çıkmak: `Esc`, X, `W A S D` ya da `E`. **Kesilir:** hasar almak, itilmek (110 cm), toplantı/mahkeme,
ölüm. Çok aşamalı görevlerde **bitmiş aşamalar saklanır**, geri gelince kalınan yerden devam edilir.

**Hile koruması:** her aşama sunucuya ayrı bildirilir; sunucu görev, oturum jetonu ve aşama sırasını kontrol eder ve aşamanın
süresini **kendisi** ölçer. Aşama alt sınırının %90'ından kısa süren aşama reddedilir ("Fazla hızlı") ve baştan oynanır. Alt
sınırlar tipik sürenin ~%40'ı; mini oyunlar hızlı bir insanın bile bu sınırın altına inemeyeceği şekilde tempolu tasarlandı.
Botlar ve mini oyunu olmayan istasyonlar eski "yakında 4–7 sn dur" yolunu kullanır.

**Sahte görev (Sabırsız):** aynı pencere, aynı aşamalar, aynı süre kontrolü; dışarıdan aynı görünür (aynı `bWorking`
durumu, aynı duruş). Bitince görev kendi listesinden düşer ama Hazırlık barına **hiçbir şey eklenmez** ve görsel görevin
dünya etkisi **asla oynamaz**. Sadece Sabırsızın kendi ekranında küçük kırmızı "FAKING" etiketi görünür.

**Görsel görevler (masumiyet kanıtı):** tamamlanınca herkesin görebildiği/duyabildiği bir etki bırakır. Sahte yapan bunu
tetikleyemez, bu yüzden "çanı çaldığını duydum" gerçek bir alibidir.

| Görev (id) | Mini oyun | Aşama | Görsel etki |
|---|---|---|---|
| Kuyudan su çek (`DrawWater`) | Çıkrığı fareyle daire çizerek çevir (fazla hızlı = halat kayar), sonra kovayı dökmeden yalağa taşı (fareyle dengede tut) | 2 | – |
| İlan as (`PostNotice`) | İlanı sürükle panodaki boş yere koy, 4 köşesini raptiyele (rüzgârda uçuşur), buruşukları düzelt | 3 | Panoda yeni ilan belirir |
| Raporları dosyala (`FileReports`) | 8 rapor tek tek gelir, mühür rengi+işaretine göre çekmeceye koy; deftere imzayı noktalardan çiz (mürekkep hızı sınırlı) | 2 | – |
| Kilise çanını çal (`RingBell`) | Halka ipin tutamağına kapanınca ipi aşağı çek, ritimde 3 vuruş | 1 | **Çan bütün köyde 3 kez çalar** |
| Mumları yak (`LightCandles`) | Gösterilen sırayla mumları yak (Simon), 2 tur | 2 | Kilisede mum ışıkları |
| Mezarlara bak (`TendGraves`) | 8 otu yukarı çekerek sök (kökler ~0.5 sn direnir), her mezara kurdelesindeki renk/işaretle çiçek koy | 2 | – |
| Çivi döv (`ForgeNails`) | Körüğe bas, kayan yeşil bantta ısıyı tut (fazla sıcak ilerlemeyi yer); sonra ibre yeşildeyken 4 temiz vuruş (ıska metali soğutur) | 2 | – |
| Aletleri bile (`SharpenTools`) | Fare yüksekliği açıyı verir; açı yeşildeyken taşın bir ucundan öbürüne 10 vuruş (balta 15–22°, tırpan 30–38°) | 2 | – |
| Ekmek pişir (`BakeBread`) | Parlayan halkaya basarak 16 kez yoğur, somun ovalini çiz, ateşi odunla canlı tut ve altın renginde TAKE OUT | 3 | **Fırın bacasından duman** |
| Bira doldur (`PourAle`) | Basılı tut = dök, fare yüksekliği bardağı eğer (az köpük ama erken taşar); çizgide bırak (2 bardak) | 2 | – |
| Tezgâhı doldur (`StockStall`) | Malları etiket resmine, sonra fiyat etiketine göre raflara koy | 2 | – |
| Ağları onar (`MendNets`) | Kopuk ipleri aynı renk ve işaretli düğüme bağla (Among Us kabloları) | 2 | – |
| Balığı boşalt (`UnloadFish`) | İskele kalasından kayan 12 balığı denize düşmeden türünün kasasına at (uskumru, morina, mercan, yılan balığı) | 1 | – |
| Feneri doldur (`FuelLighthouse`) | Yağ kabını huninin üstünde tutarak çizgiye kadar dök, fitili titremeden kes | 2 | **Fener lambası parlar** |
| Havuç topla (`HarvestCarrots`) | 12 delikten çıkan yeşil havuçları yukarı sürükleyerek çek (8 tane), gri-mor çürüklere dokunma | 1 | – |
| Yalağı doldur (`FeedAnimals`) | Çuvalda basılı tut = kepçe dolar, yalağa taşı ve bırak; hızlı sallarsan dökülür (6 kepçe) | 1 | – |
| Odun kır (`ChopWood`) | Sallanan ibre yeşildeyken vur (5 kütük) | 1 | Kamp yerinde odun yığını büyür |
| Kayığı katranla (`FixBoat`) | Fırçayla çatlakları boya, fırça kurursa kovaya daldır (iki taraf) | 2 | – |
| Saati kur (`WindClock`, isteğe bağlı) | Anahtarı ters yönde çevir, gerilimi yeşilde bırak, yelkovanı nottaki saate getir | 2 | **Kasaba saati çalar** |
| Koileri besle (`FeedKoi`, isteğe bağlı) | Aç koinin önüne yem at; 5 koi × 2 lokma, yenmeyen yem suyu bulandırır | 1 | – |
| Liman fenerini yak (`LightHarbourLamp`, isteğe bağlı) | Kibriti kutuda hızla çak, alevi rüzgâra rağmen kapaktan fitile götür ve 2.5 sn tut | 2 | Liman feneri yanar |
| Un öğüt (`GrindFlour`, isteğe bağlı) | Tahılı hazneye dök, değirmen taşını yeşil hızda çevir | 2 | – |

Hedef süre görev başına 30–60 sn; şu anki tasarımlar normal bir oyuncu için ~15–30 sn (ilk denemede daha uzun) — PIE testinden sonra sayılar (balık, ot, vuruş, kepçe sayısı) artırılacak. Tek kaynak: `FKGChoreCatalog` (`Source/KillGodot/Chores/KGChoreTypes.cpp`).

## 9. Rastgele olaylar
Her gündüz veya gece %35 ihtimalle 1 olay tetiklenir. Olaylar oyuncu sayısına ve açık bölgelere göre filtrelenir.

| Olay | Etki |
|---|---|
| **Sis Çöküyor** | Görüş 25 m'ye iner, sesler daha uzağa taşınır. |
| **Fırtına** | Yağmur, gök gürültüsü proximity voice'u bastırır, kapılar çarpar. |
| **Dolunay** | Kurtadam dönüşür. Uluma tüm haritada duyulur. |
| **Pazar Gemisi** | Limana gemi yanaşır, nadir eşya satar (maç içi altınla). |
| **Gemi Enkazı** | Kıyıya **Godot Kolileri** vurur (loot, silah, lore). |
| **Veba Fareleri** | Fare sürüsü çıkar, Şifacı görevleri iki katına çıkar. |
| **Hayalet Gelgiti** | 30 sn boyunca yaşayanlar hayaletlerin fısıltısını duyar. |
| **Tutulma** | Gece erken başlar. |
| **Festival** | Herkes aynı festival maskesini takar. İsimler gizlenir, kimlik karmaşası olur. |
| **Yangın** | Rastgele bir ev yanar, kovadan kovaya zincirle söndürülür (ortak görev). |
| **Geç Kalan Çocuk** | Şafak duyurusu gelmez. Ölüleri kasaba kendisi bulmalı. |
| **Godot'dan Mektup** | Postaneye mektup gelir. İlk alan kişi bir rol hakkında ipucu okur. |
| **Çekilme** | Gelgit düşer, deniz mağaraları 90 sn açılır. |
| **Sis Kuşatması** | Bkz. §10 |

## 10. Köyü savunma: Sis Kuşatması
Nadir olarak (maç başı en fazla 1, N ≥ 10) gece sisten **Beklenmeyenler** (Unwaited) adlı sis yaratıkları köye saldırır.
- Gündüz yapılan **Marangoz ve Demirci görevleri** (barikat, çit, top güllesi, fener yağı) savunma gücünü belirler.
- Oyuncular surları, iskeleyi ve fener topunu tutar, çan kulesinden hedef gösterir.
- **Köy düşerse herkes kaybeder.** Sabırsızlar da savunmaya katılmak zorundadır, ama kaosu gizli cinayet için kullanabilir.
- Kuşatma 2 dk sürer, sonra normal gece devam eder. PvE olarak ayrı bir mod da olabilir.

## 11. Hayaletler
- Ölünce karakterin "ipleri kesilir" ve ruh ışığı olarak **hayalet formuna** geçersin: süzülürsün, kapılardan geçersin, dünyayla etkileşemezsin.
- **Ses:** hayaletler global hayalet kanalında birbirini duyar. Yaşayanları proximity ile duyarlar, yaşayanlar onları duyamaz. Tek istisna gece **Medyum**dur.
- Yaşayanların yazılı sohbetini görürler, katillerin telsizini duyamazlar.
- **Poltergeist:** gecede 1 kez küçük bir nesneyi devirebilirler (bardak, mum). Ürkütücü bir ipucudur, cooldown'ı vardır.
- **Ruh dünyası:** sadece hayaletlerin girebildiği gizli alanlar, lore sayfaları ve hayalet görevleri vardır. Ölen oyuncu da keşfetmeye devam eder.
- Seyirci modu: canlıları takip et, serbest kamera (hileyi önlemek için sadece öldükten sonra).

### 11.1 Araf Düellosu → Mezardan Dönüş (Hortlak)
Ölen oyuncu ölüm ekranında iki yoldan birini seçer: **Hayalet ol** (izle, keşfet, hayalet görevleri) ya da **Arafa gir** (kılıç düellosu).

**Araf Arenası:** sisin ortasında hayaletimsi bir iskele ya da batık korsan gemisinin güvertesi. Ruh dünyasındadır, yaşayanlar görmez ve duymaz.
- **Turnuva:** her gece başında Arafa girmiş ölüler arasında tek eleme **1v1 kılıç düellosu** kurulur. Tek sayıda katılımcı varsa biri bay geçer. En az 3 düellocu gerekir.
- **Dövüş:** hızlı ve okunaklı. Sağ/sol/üst yönlü kesme, blok, parry (tam zamanında blok karşı saldırı açar), tekme (bloku kırar), yan adım. **3 temiz vuruşu ilk yapan kazanır**, tur başına en fazla 45 sn. Kılıç skinleri kozmetik.
- **Seyirciler:** hayaletler düelloları izler ve maç içi **ruh jetonuyla** bahis oynar (gerçek para yok, sadece eğlence ve rozet).
- **Ödül:** turnuvayı kazanan ertesi **şafakta mezarlıktan Hortlak olarak doğar** ve mezardan çıkma sinematiği oynar.

**Hortlak kuralları (denge):**
| Kural | Neden |
|---|---|
| **Kimliği gizli:** kefenli, yüzü sargılı, herkese aynı görünen bir zombi. İsim etiketi yok. | Ölülerin bildiklerini doğrudan sızdırmasını engeller. |
| **Asıl tarafını gizlice korur.** Kasabalı hortlak kasabayla, Sabırsız hortlak Sabırsızlarla kazanır. | Ödül anlamlı kalır. Bir hortlağın saldırısı tek başına delil sayılmaz, çerçeveleme de olabilir. |
| **Konuşamaz:** sesi inilti sentezine dönüşür. Ağız yine ses şiddetiyle açılıp kapanır (komik). Yazılı sohbet ve "işaret et" emote'u yok. | Bilgi sızıntısını sınırlar. |
| **Oy veremez**, asılamaz. | Mahkeme dengesi bozulmaz. |
| 60 can, yavaş (2.8 m/s), **suçluluk cezası olmadan** herkes öldürebilir. | Kolay temizlenebilir bir tehdit ya da yardımcı. |
| Yakın dövüş 20 hasar. Taşıma görevleri ve sabotaj yapabilir. Rol yeteneği **yoktur**. | Rolü değil bedeni geri gelir. |
| Bir sonraki **akşam toplantısında** mezarına geri çekilir, yani sadece 1 gün yaşar. | Kısa bir kaos penceresidir, oyunu devralmaz. |
| Maçta **en fazla 1 kez** olur. N ≥ 10 olmalı ve 4'ten fazla kişi hayatta olmalı. | Son oyun anlarını bozmaz. |

Tasarım notu: Hortlak "ikinci şans" değil, **karmaşa ekleyen bir joker**tir. Kasaba hortlağa güvenemez, Sabırsızlar da hortlağın kime ait olduğunu bilmez.
- **Ayrı mod fikri:** *Araf Turnuvası*. Sadece kılıç düellosu olan hızlı bir arcade modu. Kılıç skinlerinin vitrini olur.

## 12. Sosyal özellikler
- **Proximity voice:** 3D ses, duvar arkası boğukluk (occlusion), bağırınca menzil artar, fısıltı modu (3 m).
- **Ağız animasyonu:** ses şiddetine göre çene açılır (R.E.P.O. tarzı). Kafa hafifçe sallanır, kaşlar kalkar.
- **Metin kanalları:** Yakın (20 m), Kasaba (global; gündüz, toplantı, mahkeme), Ölüler, Sabırsız (gece), Fısıltı (3 m, 1 kişiye, sonra). Faz başına kurallar, emoji ve tepkiler: `08_UI_UX.md §6.1`.
- **Emote çarkı (M2, yapıldı):** `G` çarkının dış halkasında 16 emote: el salla, işaret et, **suçla** (toplantı için
  işaret + bağır), alkış, coş, gül, dans, **Oltayı Çek** (balıkçı köyü dansı), yere otur, reverans, selam dur, omuz silk,
  facepalm, **sus** (kollar kavuşturulmuş şüpheli duruş), ağla, **tehdit** (boğaz kesme hareketi; Sabırsız havalı ama
  herkese açık, bir şey ele vermez). Sunucu yetkili: tepkilerle aynı fazlarda, hayaletler emote yapamaz, tüm beden
  emote'u hareket edince, saldırı/hasar/toplantı başlangıcında biter. Tüm beden emote'unda kamera Fortnite gibi dışarı
  açılır; el salla/işaret gibi jestler birinci şahısta kollarla oynar. Ayrıntı: `08_UI_UX.md §6.2`.
  Sonra: kavalı çal, sandalyeye otur (şimdilik gerçek sandalyeler `AKGSeat`), yemek ye, meyhanede iç (sarhoşluk ekranı sallar).
- **Meslek kıyafeti ve kozmetik kimlik:** oyuncu karakteri hatırlanabilir ve kişisel olur.
- **Arkadaş sistemi, parti, davet** (EOS), son oynananlar, "güvenilir oyuncu" etiketi.

## 13. Meslekler (herkesin açık kimliği)
Meslek **rol değildir**, herkes bilir. Lobide seçilir veya rastgele atanır. Her mesleğin bir aleti (kozmetik skin alabilir) ve 3 imza görevi var.

Balıkçı, Fırıncı, Demirci, Çiftçi, Fenerci, Marangoz, **Oduncu** (ağaç devirme, bkz. `01b_Village_Life_Fun.md §3`), Şifacı (aktar), Terzi, Tüccar, Biracı, Mezarcı, Postacı.

> Isınma turu, epilog, mini oyunlar (satranç, tavla…), NPC satıcılar ve meme içerikleri: **`01b_Village_Life_Fun.md`**

## 14. Anti-sinir bozukluğu
- Erken ölen hayalet olarak oynamaya devam eder.
- **Host çıkarsa** maç 3–8 sn "Host değişiyor…" ekranıyla devam eder (bkz. teknik doküman).
- AFK tespiti var: 90 sn hareketsiz kalan otomatik "uyuyan" olur ve oyu sayılmaz.
- Oy ile atma (lobide), raporlama, susturma, oyuncu başına ses seviyesi ayarı.

## 15. Balık tutma (`Source/KillGodot/Fishing/`, ITER-013)
Köyün en sakin işi ve en tehlikeli anı: balık tutarken dikkatin suyadır.

**Olta:** `FishingRod` eşyası (cepte olmalı). Balık pazarında **Madam Brine** oltası olmayana ödünç verir; fıçılardan da
çıkabilir. `H` oltayı çıkarır / kaldırır. Bıçak çekmek, eşya taşımak, toplantı, mahkeme, rol açılışı oltayı kaldırır.

**Atış:** `Sol tık` basılı tut = güç dolar (1.2 sn'de %100, fazla tutarsan geri düşer), bırak = atış. Görüş yönü + 14°
yukarı, 450..1600 cm/s; kıyıdan düz bakınca ~12 m, yukarı bakınca ~25 m. Sunucu yayı dünyaya karşı izler (duvar, çatı,
iskele = takıldı), suyu sınıflar: **açık deniz**, **liman havuzu** (sakin su bölgesi), **dere + değirmen göleti**
(`M_KG_PondWater` düzlemleri). Şamandıra gerçek dalga yüzeyinde (`FKGWaves`, sakin bölge dahil) sallanır.
**Koi göleti yasak:** atarsan bir koi seni tokatlar (geri savrulursun) ve bahçe **3 altın "koi vergisi"** keser.

**Isırık:** önce 0-3 **dürtme** (şamandıra hafif batar, tık sesi, olta ucu seğirir), sonra **gerçek ısırık**
(şamandıra dibe çekilir, sıçrama, kamerada titreşim, HUD'da "!"). Bekleme süresi üstel (ort. 14 sn, 2.5-40), yakındaki
balık sürüleri (15 m) ısırığı %150'ye kadar hızlandırır. **Vurma penceresi** türe göre 0.45-0.9 sn (+ gecikme payı).
Dürtmede vurursan balık kaçar ("Too early"), pencereyi kaçırırsan yeniden beklersin.

**Çekme mini oyunu** (`FKGReelSim`, 60 Hz sabit adım, deterministik): `Sol tık` basılı = sar (gerilim artar), bırak =
misina salınır (gerilim düşer). Balık sağa sola çeker; ters yöne `A`/`D` ile yönlendirmek gerilimi azaltır, balığı
çabuk yorar. Balık her yön değiştirdiğinde bir anlık **atılım** yapar (bırakma anı). Gerilim >= 1.0, 0.8 sn sürerse
**misina kopar**; <= 0.12, 2 sn sürerse **balık kaçar**; misina 36 m'yi aşarsa kopar. Yorgun balık hızla gelir.
Çekerken **yürüyemez, vuramazsın**; hasar alırsan irkilir, balığı kaçırırsın.

| Tür | Nerede | Zaman | Ağırlık | Zorluk (çekiş / dayanıklılık / çeviklik) | Vurma | Nadirlik |
|---|---|---|---|---|---|---|
| Uskumru | deniz, havuz (x1.3) | gündüz x1.3, gece x0.45 | 0.25-1.1 kg | 0.30 / 4.5 sn / 0.5 | 0.90 sn | Sıradan |
| Morina | deniz (havuz x0.35) | **gece x1.9**, gündüz x0.6 | 1.2-8.5 kg | 0.55 / 10 sn / 0.3 (ağır, uysal) | 0.75 sn | Sıradan |
| Somon | **dere**, deniz x0.25 | **şafak x2.2** | 1.8-6.5 kg | 0.70 / 12 sn / 1.0 (çevik) | 0.60 sn | Kasaba |
| Altın Sazan | deniz x0.35 (adanın sürüsü x9), dere/göl x0.6 | gece x1.8 | 2.5-11 kg | 0.88 / 20 sn / 1.2 (sabit sarma kopar) | 0.45 sn | Efsane (~%0.2, sürü yanında ~%0.6) |
| Koi | sadece koi göleti | - | - | kutsal: tutulamaz, vergi | - | - |

Ağırlık hafife eğilimli (u^1.8). Isırıkların %7'si (havuzda %14, derede %6) **çöp/hazine**: eski çizme, **şişede mesaj**
(8 kasaba sırrı), paslı anahtar, **kese** (yakalayınca 8-22 altın), inci, kemik, hazine haritası ("Fishing" ganimet tablosu).

**Yakalama:** balık ağırlığıyla cebe girer (`FKGItemEntry.Grams`, yığınlar toplar, bölünürken orantılı paylaşır); cep
doluysa ayağının dibine düşer. HUD'da av kartı: tür, kg, nadirlik, **kişisel rekor** (yerel `KGFishing` kaydı).
**Satış:** Madam Brine'ın tezgâhı (balık pazarı salonundaki balık masası, `E`): balık + deniz buluntusu -> altın.
Fiyat = değer x (0.6 + 0.9 x ağırlık oranı); dev altın sazan ~200+ altın.

**Sosyal çıkarım:** balık tutmak **ses yapar** (atış hışırtısı, makara cırcırı 18 m'den duyulur, sıçramalar) ve
**dikkat ister**: sararken görüş %12 daralır, kenarlar kararır, yürüyemezsin, sol/sağ tık oltaya gider. Katil için açık
pencere; kasabalı için "ben iskelede balık tutuyordum" mazereti herkesin gördüğü misina ve şamandırayla doğrulanır.
Görevlerle bağ kurulmadı (bilinçli): balık tutma Hazırlık barını doldurmaz, Sabırsız da balık tutabilir. Görevler
(MendNets, UnloadFish) mini oyunlarıyla kalır; balık maç içi altın kazandırır (çarşı eşyaları için).

**Ağ:** sunucu yetkili (atış başlangıcı doğrulanır, ısırık/tür/ağırlık `FKGRng`, çekme simülasyonu sahibin girdileriyle,
envanter). Sahip istemci atış yayını, gücü ve çekmeyi tahmin eder, sunucuyla yumuşak uzlaşır (balığın yön takvimi
birebir alınır). Herkes oltayı, misinayı, şamandırayı, sıçramaları ve avı (misinada sallanan balık) görür.
