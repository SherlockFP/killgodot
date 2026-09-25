# KILL GODOT — Harita: Morrowmere

## Genel ölçü ve his
- **Oynanabilir alan:** ~650 × 650 m. Etrafında 2–3 km'lik görsel alan var (dağlar, deniz ve sis). Görsel alan ucuz LOD, impostor ve billboard'larla yapılır.
- **Yürüme ölçüsü:** koşu 5.8 m/s. Haritayı bir uçtan öbür uca koşarak geçmek **~110 sn** sürer. Meydandan her mahalleye en fazla **35 sn**.
- **Yön bulma:** 4 dev simge her yerden görünür. **Fener** (batı burnu), **Çan Kulesi** (meydan), **Yel Değirmeni** (doğu çayırları), **Kuru Ağaç** (meydan ortası). Sis gelince bunların ışıkları rehber olur.
- **Ruh:** sıcak, canlı, doygun bir sahil köyü. Gündüz turkuaz deniz, yeşil çayır, kırmızı ve sarı çatılar. Gece lacivert ve mor gökyüzü, pencerelerden turuncu ışık.

## Kaba yerleşim (kuzey yukarıda)

```
                    ▲▲▲▲▲  DAĞLAR (sınır)  ▲▲▲▲▲▲▲▲▲
              ▲▲  [ESKİ MADEN]──(maden tüneli)──┐       ▲▲
            ▲▲   Terk edilmiş şapel             │   [KERESTE]  ▲▲
          ▲▲    ORMAN  ~~~~ şelale ~~ DERE ~~~~~│~~~ Oduncu      ▲▲
         ▲    Avcı kulübesi   ┊                 │   kulübesi      ▲
        ▲   [MAHALLE C:        ┊   [MEZARLIK]───┘  ÇAYIRLAR   [YEL DEĞ.]
       ▲     Tepe Evleri]      ┊    Kilise ⛪       Çiftlik, ahır   ▲
      ~    ┌──────────────┐    ┊   (katakomp)       bostan, köy maçı ▲
  S   ~    │  [MAHALLE A:  │  [MEYDAN] 🔔 Belediye   ─── [MAHALLE D:    ▲
  İ   ~ [FENER]  Kanal     │   Kuru Ağaç, Darağacı      Değirmen Yolu]  ▲
  S   ~  BURNU  Sırası]    │   Kuyu, Çay Ocağı                           ▲
      ~  Top    └──────────┘  [ÇARŞI] Fırın, Demirci, Terzi,           ▲
  D   ~  bataryası            Aktar, Mağaza, MEYHANE (Geç Kalan)       ▲
  E   ~~~~ [MAHALLE B: Sahil Evleri] ~~~~ [LİMAN] İskeleler, Balık Pazarı,
  N   ~~~~~ plaj ~~ taş sektirme ~~~~~~~   Depo, Gümrük, Vinç, Posta İskelesi
  İ   ~~~~~ [DENİZ MAĞARALARI (gelgit)] ~~~~~ Dondurmacı ~~~~ kayıklar ~~~
  Z   ~~~~~~~~~~~~~~~~~~~~~~~~ S İ S   D U V A R I ~~~~~~~~~~~~~~~~~~~~~~~
```

## Bölgeler
| Bölge | İçerik | Oynanış rolü |
|---|---|---|
| **Liman** | 3 iskele, balık pazarı, depo (2 kat, raflar), gümrük evi, vinç (fizik), Posta İskelesi (Koliler burada vurur), kayıklar (kürek çekilebilir), dondurmacı | Taşıma görevlerinin kalbi. Kalabalık ve gürültülü. Boğulmuş burada güçlü. |
| **Meydan** | Kuru ağaç, Vladimir ve Estragon heykelleri, darağacı sahnesi, belediye (toplantı salonu, arşiv, hücreler), çan kulesi, kuyu, çay ocağı, satranç masaları | Toplantı, mahkeme ve sosyal merkez. |
| **Çarşı** | Fırın, demirhane, terzi (kozmetik vitrini), aktar, genel mağaza, **Geç Kalan Meyhanesi** (2 kat + mahzen: kuka, dart, sahne, piyano) | İstasyon görevleri ve NPC dükkânları. |
| **Mahalleler A–D** | Her kümede 5 ev, toplam **20 oyuncu evi**, bahçeler, çitler, arka sokaklar | Gece savunması, ev baskınları. |
| **Fener Burnu** | 333 basamaklı fener, fenerci kulübesi, top bataryası, uçurum | Fener görevleri, Sis Kuşatması savunması, lore'un merkezi. |
| **Çayırlar ve Çiftlik** | Ahır, tavuk kümesi, bostan, yel değirmeni (içine girilir), korkuluklar, futbol sahası | Açık alan, uzun görüş hattı, zor saklanma. |
| **Mezarlık ve Kilise** | Kilise (kutsal su, çan), mezarlık (hortlak doğuş noktası), mozole, katakomp girişi | Rahip, Medyum ve Adli Tabip işleri. |
| **Orman ve Dere** | Kesilebilir ağaçlar, avcı kulübesi, şelale arkası mağara, kereste fabrikası | Oduncu işleri, Avcı tuzakları, sıkı saklanma. |
| **Dağ ve Eski Maden** | Maden rayları ve vagonlar (fizik), terk edilmiş şapel, gözetleme kayası | 15+ oyuncuda açılır. Gizem, lore, nadir loot. |
| **Sahil ve Deniz Mağaraları** | Plaj, taş sektirme, gelgit mağaraları (kaçak satıcı), batık kayık | Zamanlı erişim, kara pazar. |

## Evler (20 oyuncu evi)
**4 arketip × 5 varyasyon.** Varyasyonlarda farklı renk, çatı, mobilya düzeni ve bahçe vardır.

| Arketip | Kat planı |
|---|---|
| **Balıkçı Kulübesi** (Sahil) | **Zemin:** giriş, mutfak ve salon açık plan, şömine. **Üst:** yatak odası, çocuk odası. **Mahzen:** ağlar, fıçılar, (bazılarında) kaçakçı tüneli kapağı. |
| **Kasaba Evi** (Kanal Sırası) | **Zemin:** hol, mutfak, yemek odası, salon. **Üst:** 2 yatak odası, çalışma masası (vasiyet), banyo küveti. **Mahzen:** kiler. |
| **Çiftlik Evi** (Değirmen Yolu) | **Zemin:** büyük mutfak, kiler, salon. **Üst:** yatak odası, samanlık bağlantısı. **Mahzen:** kök hücresi. |
| **Tepe Evi** (Tepe Evleri) | **Zemin:** salon, kütüphane, mutfak. **Üst:** yatak odası, balkon (Gözcü için harika). **Mahzen:** şarap mahzeni ve **gizli oda**. |

- Her evde en az **2 giriş** (ön kapı, arka kapı ya da mahzen kapağı), **3 saklanma yeri**, 1 çalışma masası (vasiyet ve Gece Defteri), 1 yatak, 1 şömine, pencereler (kırılabilir) ve **barikat yapılabilir mobilya** vardır.
- Mobilya fiziktir: masa devrilir, dolap kapıya itilir.
- **Evin sahibi:** kapıda isim tabelası ve oyuncunun kozmetik "ev teması" (ileride ev editörü) olur.
- **Optimizasyon:** her ev bir **Level Instance / Packed Level Actor**'dür. İç mekân mesh'leri ISM'e birleştirilir. Kapı kapalıyken iç mekân occlusion ile kesilir, uzak mesafede HLOD kabuğu gösterilir. Fizik mobilya sadece ev içindeyken uyanır.

## Yeraltı ağı

```
Ev mahzenleri (bazıları) ─► KAÇAKÇI TÜNELLERİ ─► KATAKOMPLAR (kilise altı) ─► ESKİ MADEN
         │                         │                        │
         └─► Meyhane mahzeni       └─► Kanalizasyon ─────────┴─► DENİZ MAĞARALARI (gelgit)
                                                                     │
                                  FENER BODRUMU: Yarının Saat Mekanizması (kilitli, sezonluk sır)
```
- **Erişim:** anahtarlar (loot), maymuncuk (Çilingir ve Kaçakçı), kırılabilir duvarlar, kollar, bulmacalar (çan sırası, heykel çevirme, piyano melodisi).
- **Kaçakçı**, maç başında tünel haritasını bilen tek roldür. Diğerleri keşfederek öğrenir. Bu bilgi bir oyuncudan diğerine "meta bilgi" olarak geçer ve bu da iyi bir şey.
- **Hayalet katmanı:** yeraltının bazı kısımları sadece hayaletlere açıktır (ruh dünyası). Lore sayfaları oradadır.

## Gizemler ve loot
- **Loot tabloları:** yaygın (ekmek, bandaj, ip), nadir (anahtar, fener yağı, tek kurşun), efsane (tabanca, lore sayfası, "Godot Kolisi").
- Loot her maç **tohumla (seed)** rastgele dağıtılır, ama sabit bir sıcaklık haritasına uyar: mezarlık gece daha değerli, maden daha riskli.
- **Kolektif sırlar (meta):** bazı bulmacalar tek maçta çözülmez. İpuçları sezon boyunca parça parça bulunur, topluluk birlikte çözer (ARG).

## Blockout planı (UE)
1. **Ölçek kalıbı:** 1 uu = 1 cm. Kapı 100×210 cm, kat yüksekliği 300 cm, merdiven basamağı 18 cm.
2. **Graybox:** Modeling Tools ve BSP ile arazi, yollar ve 4 ev arketipi. Bölge sınırları ve Bölge Kapıları.
3. **Oyun testi (6 ve 20 oyuncu botuyla):** yürüme süreleri, görüş hatları, saklanma yeri yoğunluğu.
4. **Sanat geçişi:** asset kitleriyle kabuk ve iç mekânlar, sonra foliage ve ışık.
5. **Performans geçişi:** HLOD, cull mesafeleri, ISM birleştirme, occlusion testleri.

## Ölçeklemede kapanan alanlar
- **6–9 oyuncu:** Mahalle A açık. B, C ve D'nin evleri "boş ev" olarak loot ve saklanma için açık kalır, ama Orman, Maden ve Çayırlar kapalıdır.
- **10–14 oyuncu:** Çayırlar, Mezarlık ve Mahalle B açılır.
- **15–20 oyuncu:** her yer açık.
