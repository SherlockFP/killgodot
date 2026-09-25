# Masa Oyunları — Satranç ve Dama (sonra: Yalancı Zarı, Tavla, kart) — Tasarım v0.2

> Durum: **Öneri (kullanıcı onayı gerekiyor)**, v0.2, 2026-09-25. Hiçbir sprint kuyruğunu değiştirmez. Sprint
> sözleşmesi: `Docs/Iterations/SPRINT-036-Tabletop.md` (036h, 036a–036e; §10).
>
> **v0.2 revizyonu (3 inceleme turundan sonra; özellik eklenmedi, sadece kesildi ve keskinleştirildi):**
> 1. **Oturma tavanları ölü zaman hedefleriyle uyumlu:** düşünme payı 0.2 → **0.12 × Gündüz**, günlük oturum tavanı
>    0.5 → **0.3 × Gündüz**, kesintisiz odak en fazla **45 sn** (sonra otomatik erteleme istemi).
> 2. **Küçük lobide maç içi masa yok:** aynı anda oynanan koltuk ≤ floor((N − tehdit) / 3); N ≤ 7'de bu 0 masa demek
>    (T1 sadece ısınma ve epilogda), N = 8–12'de 1, N = 16–20'de 2 masa.
> 3. **Tebeşir tahtası isim yazmaz** ("Beyaz – Siyah · 1:30–3:00 · Siyah kalktı 2:30"). İmza ve T-M4 kesildi. İsim
>    toplantıya sadece tanıkların defteri ve rakibin doğrulayıp yalanlayabileceği ORADAYDIM iddiasıyla gelir.
> 4. **Odak bedeli gerçekten bağlar:** dar tanıklık filtresi oturulan **bütün süre** boyunca geçerlidir; "Etrafa bak"
>    sadece kameradır. Odak menzili 4 → 8 m (tabancanın öldürme mesafesindeki atıcı tanımlanır); horoz tıkı odağı 3 sn
>    kırar.
> 5. **Para birimi:** Coin yerine Türkçe arayüz adı **Bakır** (kodda `Coin` eşyası); kaybedilen bahis, Bay Thimble'ın
>    sargısından eksilen bakırdır (tek harcama yeri, ganimet belgesi §3.5).
> 6. **Masa olayları ölü zamanı şişirmez:** kendi ürettiğin masa olayları S1.1'de algı sayılmaz; Ç12 yine dürüstçe Hayır.
> 7. **Sprintler bölündü:** 036h (StartMeeting oturanı kaldırır, bugünkü hata) → 036a saf kurallar + 2D panel → 036b ağ
>    masası → 036c 3D tahta görünümü → 036d masa botu → 036e deftere bağlama.
> Charter `KillGo_Pillars.md` §0.1'e göre bu **yeni bir sistemdir**: onaylanırsa `Docs/Backlog.md` "Proposed (needs
> approval)" bölümüne girer. Yol haritasındaki yeri M12 "Canlı köy"dür ("mini oyunlar (satranç, tavla, dart…)",
> `09_Roadmap_Gauntlet.md`). `01b_Village_Life_Fun.md` §2'deki satranç/dama/tavla/Yalancı Zarı satırlarının ayrıntılı
> hâlidir.
> **Sözleşme notu:** doğrulama borcu bugün 12 madde (`Docs/Process/VerificationDebt.md`, sınır ~10). LoopContract
> gereği bu belgenin sprintleri borç temizlenmeden başlamaz.
> Kullanıcının isteği: "Satranç, dama gibi şeyler; oyun modu değil, sosyal eğlence." Kullanıcı dengeyi ve içeriği
> tasarımcıya bıraktı. Aşağıdaki sayıların hepsi öneridir.

## 0. Tek cümle (HUD / ipucu metni, G3)

> **"Masada oturan görülür ve kayda geçer, ama etrafını az görür."**
> EN: *"Whoever sits at a board is seen and remembered, but sees little."*

Masa oyunu köyün "boş vakti" değildir, bir **alibi alışverişidir**: iki kişi birbirine zaman satın alır, karşılığında
etrafını görme yetisinden ve hareket özgürlüğünden vazgeçer. Oyunun kendisi (satranç, dama) maçın sonucunu hiç
etkilemez. Etkileyen şey **kimin, kiminle, ne zaman oturduğu ve ne zaman kalktığıdır**.

---

## 1. Oyuncu deneyimi

**Isınma (rolsüz, hasar kapalı).** Mira ile Tom meydandaki taş masaya oturur. Bir dakikalık "mermi" damasıyla
birbirini tanırlar. Seyirciler etrafta laf atar. Isınma bitince oyun yarım kalır; tahtaya "Godot gelince bitiririz"
yazılır.

**1. Gündüz (toplantısız tanışma günü).** Tom, Geç Kalan'ın pencere kenarındaki satranç masasına oturur, karşısı boş.
Yakındaki bir bota bakıp `E` ile onu çağırır. Bot "Geliyorum" der, oturur, oynarlar. Tom'un bugünkü düşünme payı
27 saniyedir (N = 12). Pay bitince oyun **ertelenir**: taşlar yerinde kalır, tahta yarın kaldığı yerden sürer.
Kalkarken tebeşir tahtasında bir satır belirir: *"Beyaz – Siyah · Gündüz 1 · 0:30–1:30 · ertelendi"*. Kimin oynadığını
tahta söylemez; o saatte meydandan geçenler söyler.

**3. Gündüz.** Meydanda bir ceset bulunur, çan çalar (SPRINT-021). Masadaki iki oyuncunun tahtası donar. Toplantıda
Tom "Ben 1:30–3:00 arası meydanda Ada ile satranç oynuyordum, tahtaya bakın" der. Tahtada satır gerçekten vardır:
*"Beyaz – Siyah · Gündüz 3 · 1:30–3:00 · Siyah kalktı 2:30"*. Tahta kimin oturduğunu söylemez. Ada "Evet, siyah bendim,
2:30'da kalktım" der; tezgâhların önünden geçen Mira ikisini masada gördüğünü defterinden paylaşır. Ceset 2:40
civarında düşmüştür. Ada'nın alibisi 2:30'da biter, Tom'unki çana kadar sürer. Tartışma buradan başlar.

**Epilog.** Roller açıktır, herkes meydanda. Yarım kalan oyunlar 30 saniye ek süreyle sürer. Bitmeyenler "Yarın
bitiririz" diye beraberlik sayılır.

**Duygu hedefi:** masa köyün **sahnesi**dir. Oturan herkes seyirciye açıktır. Oturmak bir güven jesti ("benimle
otur") ya da bir tuzak ("seni burada tutarken arkadaşım işini görüyor") olabilir.

---

## 2. Masalar: nerede, kaç tane (MVP 4 tahta)

Her masa bir `AKGBoardTable` aktörüdür: tahta, iki `AKGSeat` (karşılıklı, merkezler arası 0.9 m), bir satranç saati,
bir tebeşir tahtası ve bahis paralarının yığıldığı bir kâse. Masalar **çalışma zamanında layout JSON'daki
`tables[]` noktalarından** spawn edilir. v2 inşa hattına (`kg_build_village_v2.py`, I2) dokunulmaz (Roadmap kuralı).

| # | Yer (lore adıyla) | Oyun | Risk katmanı | Neden orada |
|---|---|---|---|---|
| T1 | Çeşme Meydanı, Geç Kalan'ın önündeki kafe masalarının ucu (taş masa) | Satranç | güvenli | Açık alan, herkes görür, toplanma halkasına ~20 m. Güçlü alibi, sıfır mahremiyet. |
| T2 | Çeşme Meydanı, doğu kenarındaki pazar tezgâhlarının ucu (taş masa) | Dama | güvenli | Aynı, ama tezgâh tenteleri görüşü yer yer keser. |
| T3 | Geç Kalan zemin kat, pencere yanı, **boş sandalyenin** masası | Satranç | orta | İçeride: az tanık, duvar arkası duyma × `KG_WALL_FACTOR`. Üçüncü sandalye Godot'nundur ("O yer ayrılmış."). |
| T4 | Geç Kalan zemin kat, ocak başı | Dama | orta | SPRINT-016'nın "Odun" görevi odunu bu ocağa teslim eder: taşıyıcılar masanın yanından geçer. |

- Meydan ve Geç Kalan "Kalp" bölgesindedir, Bölge Kapıları (031a) her N'de açıktır.
- **Maç içinde aynı anda oynanabilen masa sayısı** = floor(floor((N − tehdit) / 3) / 2) ([L] + sunucu kuralı; bir masa
  2 koltuktur). Dolu olunca oturma istemi "Masalar dolu" der. Sonuç:

  | N (tehdit) | 6–7 (1) | 8 (2) | 12 (3) | 16 (4) | 20 (5) |
  |---|---|---|---|---|---|
  | Maç içi masa | **0** | 1 | 1 | 2 | 2 |
  | Isınma ve epilog | T1 | 4 | 4 | 4 | 4 |

  Gerekçe: N = 6'da iki masadaki 4 kişinin karşılıklı alibisi, 1 Sabırsız ve 1 nötr varken bir gündüz cinayetinde
  şüpheliyi 2'ye indirir; çıkarım yapmadan tarlayı temizler. Bu yüzden N ≤ 7'de masa sadece ısınmada ve epilogda açıktır
  (sosyal eğlence kalır, alibi makinesi olmaz). Hangi masaların açık olduğu: T1, sonra T3, sonra T2, T4.
- Lore dokunuşu: T1 tahtasının başlığı sabittir: *"Karşılama Komitesi Satranç Kulübü — Finalist: G. (gelmedi)"*.

---

## 3. Mekanikler ve sayılar

### 3.1 Akış
1. **Otur** (`E`, mevcut `AKGSeat`). Karşı sandalye boşsa ipucu: "Rakip bekleniyor · Bir köylüye bak, `E`: oyuna çağır".
2. İki sandalye doluysa **tahta paneli** açılır: oyun (masanın oyunu), saat modu (faza göre otomatik), bahis (fare
   tekerleği, 0–10 Bakır, sadece ilk oturan belirler).
3. İkisi de `F: Hazır` der → oyun başlar. Beyaz/siyah: ilk oturan beyaz. Ertelenmiş bir oyun varsa (aynı iki
   oyuncu, aynı masa) o oyun kaldığı yerden sürer; renkler değişmez.
4. Oyun biter (mat, pat, terk, beraberlik) ya da **ertelenir** (pay bitti, biri kalktı, faz kesintisi, ölüm).
5. Sonuç tebeşir tahtasına yazılır, bahis ödenir.

**Kalkmak = ertelemek.** `E`/`Boşluk` (mevcut kalkma tuşu) oyunu kaybettirmez, erteler. Terk etmek (kaybetmek) ayrı ve
bilinçli bir tuştur (`R` basılı 1.5 sn). Kazara kaybetme yoktur. Kalkış saati tahtaya yazılır (bkz. §3.7): kalkmanın
bedeli bilgidir, puan değil.

### 3.2 Saat modları

| Nerede | Mod | Kural |
|---|---|---|
| Lobi (3D meyhane lobisi gelince), Isınma | **Mermi 1+1** (varsayılan), Blitz 3+2, Hızlı 5+3; Süresiz sadece lobide | Klasik Fischer saati. Süresi biten kaybeder; rakibin mat edecek malzemesi yoksa (Ş, Ş+F, Ş+A) beraberlik. Isınma bitince oyun sonuçsuz silinir. |
| Maç içi **Gündüz** | **Günlük pay** | Her oyuncunun her Gündüz için **düşünme payı** `KG_TABLE_ALLOWANCE` = 0.12 × Gündüz süresi, hamle başına +1 sn. Pay oyuncuya aittir, masaya değil: aynı gün başka masaya geçen yeni pay almaz. Payı biten oyuncunun sırası gelince oyun **ertelenir** (kaybetmez). Ayrıca oyuncu başına günlük oturum tavanı `KG_TABLE_SITTING_CAP` = 0.3 × Gündüz (duvar saati, aktif oyunda geçen) ve **kesintisiz odak tavanı** `KG_TABLE_FOCUS_MAX` = 45 sn: 45 sn boyunca tahta görünümünden çıkılmazsa oyun otomatik ertelenir ve istem "Başını kaldır: yarın devam" der (S1.1 gündüz p90 45 sn ile aynı sayı). |
| **Epilog** | Uzatma | Ertelenmiş oyunlar iki saate +30 sn ve hamle başına +1 sn ile sürer. Epilog biterken bitmemiş oyun **beraberlik** ("Godot gelmedi, oyun yarına kaldı"). |

Sayılar (Gündüz = 150 + 6N sn, `KGGameMode.cpp:63-86`):

| N | Gündüz | Düşünme payı (0.12×) | Oturum tavanı (0.3×) | Kesintisiz odak |
|---|---|---|---|---|
| 6–7 | 186 sn | maç içi masa yok | — | — |
| 8 | 198 sn | 24 sn | 59 sn | 45 sn |
| 12 | 222 sn | 27 sn | 67 sn | 45 sn |
| 16 | 246 sn | 30 sn | 74 sn | 45 sn |
| 20 | 270 sn | 32 sn | 81 sn | 45 sn |

**Neden bu kadar kısa:** maç 20–35 dk (S10.1). Masa bir ölü zaman mıknatısı olmamalı. Charter'ın tanımıyla (§0.3: son
15 sn'de algılanan olay ve karar fiili yoksa ölü saniye) tahta hamleleri karar fiili değildir (§4.1) ve odak algıyı
daraltır; yani masada geçen süre ölü zaman sayılır. v0.1'in 0.5 × tavanı tek oturuşta S1.1'in 45 sn gündüz p90'ını ve
kendi T-M1 koruyucusunu (p90 ≤ %30) aşmaya izin veriyordu. Şimdi en kötü durumda bir oyuncu Gündüz'ün %30'unu masada
geçirir ve hiçbir oturuş 45 sn'yi aşmaz. **Sonuç:** maç içi satranç "yazışmalı satranç" gibi günlere yayılır. Tipik
bir gündelik oyun (30–40 hamle, ~4 sn/hamle) 3–5 gün sürer; dama (20–25 hamle) 2–3 gün. Maçların bir kısmında satranç bitmez: bu bilinçli bir seçimdir, "Yarın" motifi
de budur. Satranç tutkunları için tam saatli oyun lobide, ısınmada ve epilogdadır.

Süreler `FKGMatchClock` kalıbıyla (kalan saniye, SaveGame) tutulur. `TimerManager` yok.

### 3.3 Faz kesintileri

| Faz | Masa | Not |
|---|---|---|
| Lobi, Isınma | Açık, saatli modlar, bahis **yok** | Isınmada rol yok, maç parası yok |
| Rol Kartı, Şafak | Kilitli | Tören / 12 sn |
| Gündüz | Açık, günlük pay, bahis var | |
| Toplanma (SPRINT-021, çan) | **Donar** | Saatler durur, tahta görünümü kapanır, ipucu "Çan! Toplanmaya git". Oyuncu kendisi kalkar. `KG_GATHER_WINDOW` sonunda halkada değilse 021'in kuralıyla **Geç Kalan** olur. Toplantıdan sonra aynı Gündüz'de ikisi de yeniden oturursa kalan paylarıyla sürer. |
| Akşam Toplantısı, Mahkeme | Ertelenir | `StartMeeting` herkesi halkaya ışınlar; önce oturanlar kaldırılmalıdır (bkz. §9 bağımlılık ve Riskler R-1). |
| Gece | Kilitli | Sokağa çıkma yasağı (GDD §1). Tahta durumu SaveGame'de. |
| Epilog | Açık, uzatma, bahis yok | Ölüler dahil herkes oynayabilir |
| Ölüm | Oyun biter ("yarım kaldı") | `AKGSeat::Tick` ölen oturanı zaten kaldırıyor. Bahis: §3.6. |

### 3.4 Satranç (MVP, tam kurallar)
- Standart FIDE hareketleri. **Rok:** şah ve kale hiç oynamamış, aradaki kareler boş, şah şu an şahta değil, geçtiği
  ve varacağı kare saldırı altında değil. **Geçerken alma:** sadece rakip piyon iki kare ilerlediği hamlenin hemen
  ardından. **Terfi:** Vezir, Kale, Fil ya da At (4'lü seçici, 5 sn içinde seçilmezse Vezir).
- **Bitişler:** mat; **pat** = beraberlik; aynı pozisyonun (sıra, rok hakları, geçerken alma hakkı dahil) **üçüncü
  tekrarı** otomatik beraberlik; **50 hamle kuralı** (100 yarım hamle piyon hamlesi/alma yok) otomatik beraberlik;
  **yetersiz malzeme** (Ş–Ş, Ş+F–Ş, Ş+A–Ş, aynı renk filli Ş+F–Ş+F) otomatik beraberlik; karşılıklı anlaşma (`B`,
  rakip 10 sn içinde `B` ile kabul); terk (`R` basılı 1.5 sn).
- Otomatik beraberlikler FIDE'nin "talep" kuralından sadedir: kısa maç, talep arayüzü yok.
- Çekirdek FEN okur/yazar, pozisyon tekrarı Zobrist anahtarıyla (sunucuda, en fazla 150 yarım hamle geçmişi).

### 3.5 Dama (MVP)
- **MVP varyantı: İngiliz daması (checkers):** 8×8, sadece koyu kareler, 12'şer taş, taş çapraz ileri bir kare gider;
  **alma zorunlu** (birden çok alma yolu varsa oyuncu seçer), **çoklu atlama** aynı hamlede zorunlu devam eder; son
  sıraya varan taş **dama** olur ve hamle orada biter; dama çapraz dört yöne bir kare gider (uçan dama yok).
- **Bitiş:** hamlesi kalmayan (taşı yok ya da kilitli) kaybeder; 80 yarım hamle alma ya da taş (dama olmayan) hamlesi
  yoksa beraberlik; üçüncü tekrar beraberlik; anlaşma ve terk satrançtaki gibi.
- **Açık soru (Onay T1):** Türk oyuncunun "dama"dan anladığı **Türk daması**dır (düz hareket, uçan dama, en çok taşı
  alma zorunluluğu). Çekirdek varyant parametresiyle yazılır (`EKGDraughtsVariant`); öneri: MVP İngiliz (EN/TR/RU
  için ortak, kuralı kısa), ilk ek varyant Türk daması (aynı tahta, aynı taşlar, yalnız kurallar).

### 3.6 Bahis (Bakır)
- 0–10 **Bakır** (maç içi para: kodda `UKGItemCatalog` `Coin` eşyası, `Inventory/KGItemCatalog.h`; Türkçe arayüz adı
  `01b_Village_Life_Fun.md` §5'e göre Bakır, meta para Altın ile karışmasın). Sadece Gündüz. İkisi de aynı miktarı koyar.
  Sunucu başlangıçta iki envanterden düşer ve kâseye koyar: kâsede **gerçek para yığını** görünür (en fazla 20 mesh,
  herkes görür).
- Ölçek: sandık 1–5, fıçı 1–3, sandık (Chest) 5–20 Bakır düşürür (`Inventory/KGLoot.cpp`); oyuncu kaplardan günde ~4
  Bakır toplar. **Bakırın tek harcama yeri** Bay Thimble'ın sargısıdır (ganimet belgesi §3.5, öneri 15 Bakır; dükkân
  ayrı bir iş). Yani 10 Bakırlık kayıp ~2/3 sargıdır: kayda değer ama yıkıcı değil. Yeni harcama yeri eklenmez.
- **Sonuçlar:** kazanan 2× bahsi alır. Beraberlik ve maç sonuna kadar bitmeyen oyun: iade. **Ölüm:** hayatta kalanın
  payı ona döner; **ölenin payı masada fiziksel bir para yığını (`AKGPickup`) olarak kalır.** Onu alan herkes bir
  Pickup olayı bırakır (tanıklarıyla). "Ölünün bahsini kim aldı?" küçük ama gerçek bir iz.
- **Botlar bahse girmez** (bahis 0): insanların bakırı bottan "çiftçilik"le toplaması bir sömürü olurdu.
- Ev payı (meyhane kesintisi) yok.
- Uyum: Roadmap Onay R5'in "Korsan: mahkeme kararına bahis" fikri onaylanırsa aynı emanet (escrow) modülünü kullanır.

### 3.7 Tebeşir tahtası (masa kaydı) — dünyadaki iz
Her masanın yanında küçük bir kara tahta vardır. Son **3 oyunu** gösterir. **İsim yazmaz**, sadece renkleri:

```
Beyaz – Siyah · Gündüz 3 · 1:30–3:00 · ertelendi · Siyah kalktı 2:30
```
- Saatler faz saatinde, **30 sn kovasına yuvarlanır** (`KG_TABLE_SLATE_BUCKET`). Defter kesin saati tutar; tahta
  kaba saati. Böylece Vaka Dosyası'nın ölüm zamanı aralığıyla kenarlarda belirsizlik kalır (S9.4 üst sınırı).
- **Neden isimsiz:** v0.1'de imzalı satır sunucunun yazdığı, sahtelenemez bir zaman alibisiydi. N ≤ 7'de tek Sabırsız
  varken bir cinayeti örten imzalı satır, Kasaba ortağını pratikte kesin aklıyordu (sert kanıt, S2.4); iki Sabırsız
  karşılıklı sertifikalı alibi toplayabiliyordu. Şimdi tahta sadece "o saatte bu masada bir oyun vardı ve siyah 2:30'da
  kalktı" der. **Kimin** oturduğu toplantıya sadece iki yoldan gelir: o saatte masayı gören tanıkların defteri (020b,
  024) ve oturanın ORADAYDIM iddiası; rakip bunu doğrulayabilir ya da yalanlayabilir. İmza kararı ve T-M4 kesildi.
- Tahta **otomatik duyurulmaz** (S2.5): okumak için masaya gitmek gerekir.
- **Silmek:** herkes tahtayı silebilir (`E` basılı 2 sn). Silinen tahtada **"silinmiş" lekesi** kalır, bir sonraki oyun
  yazılana kadar. Silme bir defter olayıdır (görenler tanık). Sahte yolu: Sabırsız bir kalkış saatini ortadan
  kaldırabilir, ama leke silindiğini ilan eder.

### 3.8 Odak ve savunmasızlık
- Oyun başlayınca kamera **tahta görünümüne** geçer (§7). **Oturulan bütün süre boyunca** (kameranın nereye baktığından
  bağımsız) oyuncunun **görme tanıklığı** daralır: koni `KG_TABLE_FOCUS_CONE` = ±30° (tahta yönünde), menzil
  `KG_TABLE_FOCUS_RANGE` = 8 m (= `KG_IDENT_RANGE`; normalde `KG_SIGHT_CONE` ±60°, `KG_SIGHT_DAY` 30 m). **Duyma
  değişmez.** Karşıdaki rakip bu koninin içindedir ve **tanımlanır** (maskesizse). 8 m, tabancanın öldürme mesafesinin
  (12 m) içindeki ön tarafı kapsar: önden yaklaşan atıcı tanımlanır.
- **Etrafa bak** (sağ tık basılı): **sadece kamera**. Kamera 0.15 sn'de serbest oturma bakışına döner (`AKGSeat` serbest
  yaw), ama tanıklık filtresi dar kalır (defter bu sürede de sadece koniyi yazar). Satranç saati akmaya devam eder.
  v0.1'de "Etrafa bak" tam algıyı geri veriyordu ve rakip sırasındayken bedelsizdi; odak bedeli bir UI vergisine
  dönüşüyordu.
- **Horoz tıkı:** oturan oyuncu `KG_HEAR_COCK` (8 m) içinde bir tabanca horozu duyarsa odak 3 sn kırılır: kamera sese
  döner **ve** bu 3 sn boyunca tam algı geçerlidir (sistem olayı; oyuncu tetikleyemez).
- Oturan hareket edemez (`AKGSeat` MOVE_None). Kalkmak anlıktır ama yeri bellidir. Backstab geometrisi
  (`BackstabRange` 150 cm, arkadan) oturana da uygulanır.
- Sonuç: masa **ne sığınak ne tuzaktır**. Rakip bir tanıktır (bu korur). Ama odaktaki oyuncu çevresindeki cinayeti
  görmez (bu masadakileri kötü tanık yapar), sabit bir hedeftir ve Sabırsız bir rakip, maskeli bir takım arkadaşının
  işini "görmedim" diyerek örtebilir.
- Ekran kenarında %15 karartma (vinyet) odağı ve bedelini oyuncuya gösterir (Ç11). Kesintisiz odak 45 sn'de biter
  (§3.2).

### 3.9 Seyirciler
- Masaya 3 m'den bakan herkes HUD'da küçük saat şeridini görür. Özel kamera yok: taşlar 2–3 m'den okunacak boyuttadır.
- Masanın 3 m içinde, tahtaya bakarak ≥ 10 sn duran seyirci için sunucu bir `TableSpectate` olayı yazar
  (başlangıç/bitiş). Seyircinin algısı daralmaz (oyun onun değil).
- Seyirci hamle öneremez (sesle önerebilir, bu sosyal).

### 3.10 Önerilen sabitler (charter §0.3'e eklenmek üzere; onaylanmadan charter'a yazılmaz)

| Sabit | Değer | Anlamı |
|---|---|---|
| `KG_TABLE_ALLOWANCE` | 0.12 × Gündüz süresi / oyuncu / gün, +1 sn/hamle | Maç içi düşünme payı |
| `KG_TABLE_SITTING_CAP` | 0.3 × Gündüz süresi / oyuncu / gün | Aktif oyunda geçen en uzun duvar saati |
| `KG_TABLE_FOCUS_MAX` | 45 sn | Kesintisiz odak; sonra otomatik erteleme |
| `KG_TABLE_FOCUS_CONE` | ±30° | Oturulan bütün süre boyunca görme konisi |
| `KG_TABLE_FOCUS_RANGE` | 8 m (= `KG_IDENT_RANGE`) | Oturulan bütün süre boyunca görme menzili |
| `KG_TABLE_MAX_TABLES` | floor(floor((N − tehdit) / 3) / 2) | Maç içinde aynı anda aktif masa |
| `KG_TABLE_SLATE_BUCKET` | 30 sn | Tebeşir tahtasındaki saat yuvarlaması |
| `KG_HEAR_TABLE` | 8 m | Taş vuruşu / saat düğmesi sesi (duvar arkası × `KG_WALL_FACTOR`) |
| `KG_TABLE_STAKE_MAX` | 10 Bakır | Oyuncu başına bahis tavanı |

---

## 4. Çıkarım döngüsüne katkı

Charter döngüsüne göre: **KARAR** (otur / kalk / sil / çağrıldığında gel / ORADAYDIM de) → **İZ** (defter olayı + tebeşir
tahtası + para yığını) → **BİLGİ** (zaman aralıklı alibi, kalkış saati, kimin kiminle oturduğu) → **ŞÜPHE** ("çandan 40
sn önce kalktı") → **TOPLANTI** (ORADAYDIM iddiası, tahtayla çelişki) → **SONUÇ** → yeni karar ("bugün onunla
oturmayacağım").

### 4.1 Defter olayları (SPRINT-020b'nin `UKGEventLedger`'ına yazar; 020b gelene kadar boş kanca)

| Olay | Aktör | Tanık (020b filtresi) | Kim okur |
|---|---|---|---|
| `TableGameStart {masa, beyaz, siyah, oyun, bahis}` | iki oyuncu | görenler/duyanlar (`KG_HEAR_TABLE`) | defter (J, 024), bot zihni (023), epilog |
| `TableStand {oyuncu, sebep: kendisi/pay/çan/faz/ölüm}` | kalkan | görenler + rakip | defter, Vaka Dosyası ilgili parça (S2.3) |
| `TableGameEnd {sonuç, sebep}` | iki oyuncu | görenler | epilog, bahis |
| `TableSpectate {seyirci, başlangıç, bitiş}` | seyirci | oyuncular + görenler | defter |
| `SlateWiped {silen}` | silen | görenler | defter (panoya düşmez, S2.5) |
| `StakeTaken {alan, miktar}` | ölünün yığınını alan | görenler | defter |

Her olay tek satırdır, bölge adı + faz saati ile (G3.5). Oyun içindeki **tek tek hamleler olay değildir** ve **karar
fiili sayılmaz** (S1.3'ü şişirmemek için). Karar fiilleri: otur, kalk, sil, çağrıya gel. **S1.1 sayımı:** oyuncunun
kendi ürettiği masa olayları (kendi `TableGameStart/Stand`, kendi hamle sesi) onun için algılanan olay sayılmaz; masada
geçen süre dürüstçe ölü zaman olarak ölçülür ve tavanlar onu sınırlar.

### 4.2 Beş kanca (kullanıcının önerileri + tasarımcının eklediği)
1. **Karşılıklı alibi, ama dar ve sertifikasız.** Tahta sadece bir oyunun olduğunu ve saatlerini söyler; kimin
   oturduğunu tanıklar ve iddialar söyler. Doğrulanan bir iddia iki oyuncuyu **bir zaman aralığı** için aklar, taraflarını değil. Bir
   cinayeti dışlar, takımı dışlamaz. Sabırsızlar da kullanır: biri oturup alibi toplar, takım arkadaşı öldürür.
   "Hep birlikte oturuyorlar" başlı başına bir desendir.
2. **Oturduğun sürece dar tanık** (§3.8). Masadakiler bir alibi ama zayıf bir tanıklık alır; "Etrafa bak" bunu
   değiştirmez. Bu, bilgi bütçesini dengeler: alibi arttıkça tanık azalır.
3. **Çan anı.** Çan çaldığında (021) tahtalar donar ve sunucu o anda oturan/izleyen herkesi tek bir
   `TableStand {sebep: çan}` kümesiyle yazar: Toplanma penceresindeki "son dakika cinayeti" için hazır bir alibi
   anlık görüntüsü. İpucu, **çandan önce kalkanlardan** gelir: tebeşir tahtası kalkış saatini yazar.
4. **Bahis.** Parayla oynanan oyun bir sosyal bağ (ya da düşmanlık) ve görünür bir para transferidir. Ölünün masada
   kalan payı küçük bir ahlak testi ve bir izdir.
5. **Masa başı konuşma.** İki kişinin yan yana, uzun süre, sabit durduğu tek yer masadır. Seyirci **kimin kiminle**
   konuştuğunu görür ama (sesli sohbette, SPRINT-023) mesafe yüzünden hepsini duymaz. Fısıltı modu (G1.2, 3 m) masa
   başında otomatik olabilir: **Sonra** (§10).

### 4.3 Hangi charter hedefleri, nasıl ölçülür
| Kod | Etki | Ölçüm |
|---|---|---|
| S2.3 | Vakalı toplantıda vakaya ilgili bilgi parçası: masa olayları aynı bölge/±90 sn içindeyse +1 `EventId` | [B], 020b + 024 sonrası |
| S9.5 | ORADAYDIM iddiası masa olayına bağlanır; tahtayla çelişen iddia `KG_CONTRA` üretir | [B] |
| S1.4 | "Güven" türü: masa alibisi bir **kefil olma** biçimidir (iddia paylaşınca) | [B] [O] |
| S4.4 | Masadaki çift sürekli bir karşılaşmadır (≤ 15 m, LOS, ≥ 3 sn) | [B] |
| S1.1, S1.2 | **Risk (koruyucu):** masadaki süre ölü zaman sayılır (kendi olayların hariç). Pay (0.12×), tavan (0.3×) ve 45 sn kesintisiz odak tavanı bunu S1.1'in 45 sn gündüz p90'ının içinde tutar. | [O], aşağıdaki T-M1 |

**Özelliğin kendi koruyucu metrikleri** (charter bandı değil, sadece rapor; onaylanırsa §0.2 kurallarıyla):

| Kod | Metrik | Hedef | Ölçüm | n_min |
|---|---|---|---|---|
| T-M1 | Gündüz canlı oyuncu-saniyelerinin aktif masa oyununda geçen payı | Oyuncu p90 ≤ %30; lobi ortalaması ≤ %10 | [O] ([B] sadece bilgi) | 40 oyuncu-gündüz |
| T-M2 | O gün ≥ 1 masa oyunu oynanmış vakalı toplantılarda masa olayına bağlı ≥ 1 iddia | ≥ %20 | [B] (024 sonrası) | 30 toplantı |
| T-M3 | Oturan oyuncunun saniye başına öldürülme oranı ÷ ayakta Gündüz oranı; tabancalı ve tabancasız maçlar **ayrı** | 0.5–1.5× (ne sığınak ne tuzak) | [B] gece koşusu | 40 cinayet (masada), her kol |
| T-M5 | Masada geçen kesintisiz odak | Maks 45 sn (yapı gereği, [T]); oyuncu-gündüz p90 masa payı ≤ %30 (T-M1) | [T] [O] | — |

---

## 5. Karşı hamleler ve kötüye kullanım

| Durum | Kim, neden | Karşı hamle / kural |
|---|---|---|
| Alibi çiftçiliği (iki Sabırsız birbirine alibi) | Sabırsız | Tahta isim yazmaz: alibi sadece tanıkların gördüğü kadardır. Kayıt dardır; üçüncü takım arkadaşının cinayetini örtmez. "Hep aynı ikili" deseni tartışılır. Maç içi masa sayısı tavanlı (§2). |
| "Etrafa bak" ile tam tanık olup alibi de toplamak | Herkes | Etrafa bak sadece kameradır; dar tanıklık oturulan bütün süre geçerli (§3.8). |
| Masadaki sabit hedefe tabancayla ateş | Sabırsız | Odak 8 m'de tanımlar; horoz tıkı odağı 3 sn kırar; T-M3 tabancalı maçlarda ayrı izlenir. |
| Sabırsız rakip, maskeli takım arkadaşının cinayetini "görmedim" diye örter | Sabırsız | Rakip **her zaman** tanık olarak defterde yazar: yalanı 024 ile çelişki üretir. Seyirciler ikinci tanıktır. |
| Masada saklanmak (toplantıya gelmemek, iş yapmamak) | Herkes | Günlük pay + oturum tavanı + 45 sn kesintisiz odak; çanda donma; `KG_GATHER_WINDOW` sonunda Geç Kalan; AFK (G5.5) masa hamlelerini etkinlik sayar ama tavan yine keser. |
| Tahtayı silerek alibi yok etmek | Sabırsız | Silmek 2 sn, görünür, "silinmiş" lekesi kalır, defter olayıdır. Defterdeki kayıt silinmez. |
| Bakır aklama / takım arkadaşına para aktarma | Sabırsız | Zaten para düşürerek (`AKGPickup`) mümkün. Bahis tavanı 10, her aktarım görünür (kâse) ve defterde. Yeni sömürü yok. |
| Bottan bakır çiftçiliği | Güçlü oyuncu | Botlar bahse girmez. |
| Rakibin payını yakmak için yavaş oynamak | Trol | Saat hamle yapanın payından yer: yavaş oyuncu **kendi** payını yakar. |
| Koltuk işgali (oturup başlamamak) | Trol | Hazır olmadan 20 sn geçerse ikinci sandalyedeki ipucu "Kalk ya da hazır ol"; 40 sn'de sunucu kaldırır (sadece aktif oyun yokken). |
| Davet spam'i (bot/oyuncu) | Trol | 10 sn'de 1 çağrı; aynı hedefe 60 sn'de 1. |
| Hamle RPC spam'i / geçersiz hamle | Hileci | Sunucu her hamleyi kural çekirdeğiyle doğrular; saniyede en fazla 4 hamle RPC'si; yanlış faz, oturmayan, sırası olmayan, geçersiz hamle reddedilir (G7.2 red testi). |
| Motor (Stockfish) kullanmak | Hileci | Maç içinde sonucun maça etkisi yok; sıralama/ELO yok, meta ödül yok. Kazanmanın tek getirisi bahis ve epilog unvanı ("Köyün Büyükustası"). |
| Masa ile gizli işaretleşme | Sabırsız | Değersiz: takım kanalı zaten var. |
| Ölü oyuncunun hamle söylemesi | Hayalet | Ölüler canlının oyununa ses/yazı ile ulaşamaz (S8.3 kanalları). Epilogda herkes serbest. |

**Frustrasyon kuralı (G5.6):** masadaki tek olumsuz etki (daralan algı) önceden gösterilir (vinyet + ipucu metni) ve
bir karşı fiili vardır (kalk; 45 sn'de otomatik erteleme).

---

## 6. Botlar

**Kural (G6.1):** bot sadece tahtanın herkese açık durumunu ve kendi algısını okur. Rakibin rolü hiçbir karara girmez.

| Konu | Karar |
|---|---|
| Ne zaman oynar | **Sadece bir oyuncu çağırınca** (MVP). Oturan oyuncu ≤ 12 m'deki bota bakıp `E` basar. |
| Kabul kuralı | Faz Gündüz/Isınma/Epilog; elinde taşınan eşya yok; son 20 sn'de ceset/çığlık algılamadı; o gün payı var. Olasılık **Kasaba ve Sabırsız botlarda aynıdır** (0.6, `FKGRng` tohumu bot+gün): farklı olsaydı bir ele verme olurdu (G6.3 ruhu). Parametreler soak başlığında dondurulur (§0.2 kural 2). |
| Cevap | NEAR sohbet satırı + (023 gelince) ses komutu: "Geliyorum" / "Sonra". 15 sn içinde oturamazsa vazgeçer. |
| Seviye | Maç içinde **Kalfa**. Lobide seçilebilir: **Çırak** (derinlik 1, %20 ikinci en iyi hamle, tohumlu), **Kalfa** (satranç alfa-beta 2 yarım hamle + sadece alma aramasında 4'e kadar; dama derinlik 6), **Usta "Anselm"** (yinelemeli derinleşme, 4 yarım hamle, düğüm bütçesi). "Anselm" sadece bir zorluk etiketidir, karakter görünmez (lore: Anselm'in akıbeti bilinmez). |
| Değerlendirme | Satranç: materyal (P 100, A 320, F 330, K 500, V 900) + kare tabloları. Dama: taş 100, dama 160, ilerleme bonusu. |
| Düşünme süresi | min(5 sn, max(1.5 sn, kalan payın %10'u)). Hesabın kendisi zaman dilimli, görünen gecikme "insansı" bekleme. |
| Kalkış | Çanda 1–3 sn içinde kalkar ve toplanmaya gider; ≤ `KG_HEAR_SCREAM` içinde çığlık duyarsa kalkar; payı bitince erteler. |
| Terk / beraberlik | Satrançta art arda 3 kendi hamlesinde değerlendirme ≤ −900 ise terk eder. Beraberlik teklifini değerlendirme ≤ +50 ise kabul eder. Oyunlar kısa kalır. |
| Bahis | Her zaman 0. |
| İddia (SPRINT-023/024 sonrası) | Botun kendi defterinde vakayla örtüşen `TableGameStart/Stand` varsa ORADAYDIM iddiası olarak kullanır; `KG_VOTE ... reason=<eventId>` ile aynı kalıp. |
| Log ([U]) | `KG_TABLE bot=… act=accept|decline|sit|move|stand|resign reason=…` |
| Kendiliğinden oturma | **Sonra.** Botların alibi için kendiliğinden masaya oturması G6 ve bot zihni (023) olgunlaşınca. |

---

## 7. UI/UX

**Ana yol: dünyadaki 3D tahta + birinci şahıs tahta görünümü.** Panel ikinci planda (erişilebilirlik ve başsız
UIShot'lar için).

**Tahta görünümü (kamera):** oturma kamerasından (`AKGSeat` CameraDrop 34, CameraBack 18) 0.35 sn'lik geçişle: 20 cm
öne, pitch −52°, FOV 55°. Tahta ekran yüksekliğinin ~%65'i. **Rakibin göğsü ve yüzü ekranın üst kenarında görünür**:
konuşan ağız (SPRINT-023) ve ifade oyunun parçasıdır. Kafa sallanması kapalı. Kenar vinyeti %15.

**Girdi** (Enhanced Input; tahta bağlamı karakter bağlamının üstünde öncelikli, sadece aktif oyunda):

| Girdi | Eylem | Not |
|---|---|---|
| Sol tık / sürükle-bırak | Taş seç → hedef kare | Seçilince yasal kareler nokta (ayar: "Yardımcı işaretler", varsayılan açık) |
| Sağ tık basılı | Etrafa bak | Sadece kamera; tanıklık dar kalır (§3.8) |
| `F` | Hazır | Tahta bağlamında karakterin `Inspect`'inin önüne geçer |
| Fare tekerleği | Bahis 0–10 (başlamadan önce, ilk oturan) | |
| `B` | Beraberlik teklif et / kabul | 10 sn |
| `R` basılı 1.5 sn | Terk et (kaybet) | Karakterin `RotateHeld`'inin önüne geçer; halka dolum göstergesi |
| `E` / `Boşluk` | Kalk (oyun ertelenir) | Mevcut `AKGSeat` davranışı |
| `Tab`, `T`, `G`, `Z/X/C`, `V` | Değişmez | Rol bakışı, sohbet, emote, ses komutları, bas-konuş |

**HUD (minimal, diegetic ağırlıklı):**
- Masadaki **pirinç satranç saati** gerçek süreyi gösterir. HUD'da üst ortada tek şerit: `Sen 0:32 ● 0:28 Rakip`,
  altında ince "Bugünkü pay" çubuğu. Sıra kimdeyse o taraf parlak.
- Son hamle: kaynak ve hedef karede soluk altın ton. Şah çekilen şahın karesi kırmızı halka.
- Terfi: karenin üstünde 4'lü radyal seçici.
- Bitiş: kısa tost ("Şah mat! Tom kazandı", "Pat", "Ertelendi: yarın devam") + tebeşir tahtasına yazılan satırın
  animasyonu + paranın kâseden kazanana kayması.
- Alt ipucu satırı (≤ 120 karakter, lore §6): `Sol tık: oyna · Sağ tık: etrafa bak · E: kalk (yarın sürer)`.
- Renk körlüğü: taşlar şekille ayrılır; renkler palet dokusundan yüksek kontrastlı krem/mürekkep.

**Panel (yedek, "Düz görünüm" ayarı):** aynı kural çekirdeğini `FKGMgPainter` (`Chores/UI/KGMinigame.h`, 640×400
mantıksal tuval) ile 2D çizer. Düşük uçlu makineler ve `kg.ChoreShot` kalıbındaki başsız UIShot'lar için. Seyirciler
paneli göremez (dünyada bakarlar).

**Taşlar ve tahta (asset):** projede satranç/dama asset'i yok (Content/Art'ta taranmadı: 0 sonuç). İki yol, **Onay T2**:
(a) `Tools/Blender/kg_make_tabletop.py` ile başsız üretim (tornalanmış profiller; at için ekstrüzyon + pah), palet
dokusu, taş başına 150–400 üçgen; (b) KayKit Board Game Bits (CC0, `Docs/Research/AssetResearch.md` satır 580):
**indirme kullanıcı izniyle** (ad, kaynak, boyut). Öneri (a): izin gerektirmez, stil paletle uyumlu.

**Metin:** tüm yazılar String Table'dan (EN/TR/RU). Taş adları TR: Şah, Vezir, Kale, Fil, At, Piyon; Dama: taş, dama.

---

## 8. Performans

| Kalem | Bütçe / tasarım |
|---|---|
| Sunucu CPU | Kural doğrulama hamle başına ≤ 0.05 ms. Bot araması zaman dilimli, kare başına ≤ 0.5 ms toplam; 4 tahta + 2 arayan botla ortalama ≤ 0.3 ms/kare, p99 ≤ 1.0 ms (N = 20). Arama düğüm bütçesi: satranç 20k, dama 50k. |
| Replikasyon | `FKGBoardState` sıkıştırılmış: 64 kare × 4 bit = 32 B + sıra, rok/geçerken bayrakları, iki saat (int16 desisaniye), son hamle (2 B), durum ≈ **44 B**. Push-model, sadece hamlede; hamleler arası `DORM_DormantAll`, hamlede `FlushNetDormancy`. Hamle RPC'si ~8 B, güvenilir. Iris-uyumlu (kayıtlı alt nesne yok, düz struct). |
| Relevancy | Tahta aktörünün `NetCullDistance` ≈ 40 m (tahtayı görebilen herkes). Tebeşir tahtası metni aynı aktörde. |
| Sunucu-özel durum | Zobrist tekrar geçmişi (≤ 150 × 8 B), emanet, oyun başına günlük pay ve odak sayaçları: `UPROPERTY(SaveGame)`, replike olmaz. |
| Çizim | Tahta başına tek tahta mesh'i + taş türü başına bir ISM (satranç 6, dama 2), renk instance başına özel veri ile palet materyalinde. 4 tahtada ≤ 32 çizim çağrısı. Taşlar 25 m'de gizlenir, 8 m'de LOD1 (%25). Satranç tahtası yakında ≤ 16k üçgen. |
| Ses | Taş vuruşu ve saat düğmesi: 2 ses, `KG_HEAR_TABLE` 8 m zayıflama, eşzamanlı sınır tahta başına 2. |
| Host migration | `UKGSnapshotComponent` tahta aktöründe; FEN + saatler (kalan saniye) + paylar + emanet + tebeşir satırları SaveGame. Aktör uzamsal yüklenmez (World Partition akıtmaz). |

---

## 9. Bağımlılıklar

### 9.1 Var olan kod (değişmeden kullanılır)
| Dosya | Ne için |
|---|---|
| `World/KGSeat.*` | Oturma, replike `Occupant`, oturma kamerası, `E/Boşluk` ile kalkma, ölü oturanı kaldırma (`Tick`). Tahta aktörü iki koltuğu **sahiplenir** ve `GetOccupant()`'ı okur: `KGSeat` dosyasında değişiklik yok. |
| `Inventory/KGPlayerExtrasSubsystem.*` | "Sıfır entegrasyon" kalıbı: yeni `UKGTabletopSubsystem` her `AKGPlayerState`'e `UKGTabletopRPCComponent` ekler (aynı kalıp, ayrı dosya). |
| `Inventory/KGItemCatalog.*`, `KGInventoryComponent.*`, `World/KGPickup.*` | Bakır (`Coin`), emanet, ölünün yığını |
| `Core/KGMatchClock.h`, `Core/KGRng.h` | Saatler ve bot tohumu |
| `Core/KGTypes.h` (`EKGPhase`), `Core/KGGameState` | Faz kesintileri |
| `Chores/UI/KGMinigame.h` (`FKGMgPainter`) | 2D panel ve UIShot'lar |
| `UI/Reveal/KGStreamerMode.h` | Tebeşir tahtasında takma adlar, `PlayerKey()` defter anahtarı |
| `Dev/KGDevCommands.cpp` (`KGDevPrivate::BuildVerbs`) | `kg.Table.*` fiilleri |
| `AI/KGBotController.*` | Çağrıya cevap, koltuğa yürüme (küçük kanca) |

### 9.2 Gereken küçük düzeltme (mevcut kod)
- **`AKGGameMode::StartMeeting` (`Core/KGGameMode.cpp` ~584-611) oturan karakteri kaldırmadan ışınlıyor.**
  `AKGSeat::Tick` her karede `DisableMovement()` çağırdığı için akşam toplantısında oturan biri halkada hareketsiz
  kalabilir ve kalkınca koltuğun yanına geri ışınlanır. Bugün de 71 koltuk için geçerli (Riskler R-1). Düzeltme tek
  satır: ışınlamadan önce `AKGSeat::StandUpCharacter(Alive[i])`. `KGGameMode.cpp` ortak dosya olduğu için seri sırada
  yapılır.

### 9.3 Yol haritası (`Pillar_Roadmap.md`) ve çalışan sprintler
| ID | İlişki |
|---|---|
| SPRINT-020a | Toplantıda saldırı reddi (S9.1): toplantı sırasında masada oturan da korunur. |
| **SPRINT-020b** | Olay Defteri + tanık filtresi. §4.1 olayları ve §3.8 odak daralması buna yazar. 020b'den önce MVP boş bir `EmitTableEvent` kancası taşır (016'nın `EmitChoreStepEvent` önerisiyle aynı kalıp). |
| **SPRINT-021** | Çan, Toplanma alt durumu, Geç Kalan, Vaka Dosyası'nın zaman kovası. Çan anı (§4.2/3) bunu bekler. |
| SPRINT-023 (Roadmap: bot zihni) | Bot iddiaları, G6.1 grep kuralı. |
| **SPRINT-024** | Defter (J), ORADAYDIM iddiası, `KG_CONTRA`, Kasaba Panosu, isim etiketi (`KG_IDENT_RANGE`). T-M2 ve S9.5 katkısı bunu bekler. |
| SPRINT-025a | Maske: masadaki cinayette tanımlanamayan fail. |
| SPRINT-031a | Bölge Kapıları: `tables[]` noktalarına `MinPlayers` (T2/T4 için 8). |
| SPRINT-031b | AFK: masa hamleleri etkinlik sayılır. |
| Onay R5 | Korsan'ın "mahkeme kararına bahis"i onaylanırsa bahis emanetini paylaşır. |
| SPRINT-016 (çalışıyor) | Odun görevi Geç Kalan ocağına teslim ediyor: T4 trafiği. Dosya çakışması yok. |
| SPRINT-023-VoiceSocial (çalışıyor) | Yakınlık sesi, konuşan ağız, ses komutları, taş-kâğıt-makas ortak emote'u (sunucu çözümlü sonuç kalıbı). Masa bunlara dokunmaz, sadece kullanır. |
| SPRINT-015 | Yayıncı modu ve takma adlar (tebeşir tahtası). |
| SPRINT-022-V2VisualPass (çalışıyor) | Meydan/meyhane süslemesi; masa noktaları JSON'da veri olarak, inşa hattına dokunmadan. |
| Kardeş tasarımlar | Ganimet: tabanca ve parça bahsi yok; odak menzili 8 m ve horoz tıkı kuralı ortak (§3.8); Bakır tek para, tek harcama yeri sargı. Orman kampı: Yalancı Zarı'nın kamp ateşi versiyonu için doğal yer (§10 Sonra). |

---

## 10. MVP ve sonrası

### Sprint dilimleri (sözleşme: `Docs/Iterations/SPRINT-036-Tabletop.md`)

| Dilim | İçerik | Boy | Önkoşul |
|---|---|---|---|
| **036h** | Hata düzeltmesi: `StartMeeting` ışınlamadan önce oturanı kaldırır (§9.2, bugünkü 71 koltuk) | XS | Roadmap 020a ile aynı seri pencere (`KGGameMode.cpp`) |
| **036a** | Saf kurallar: `FKGChessRules`, `FKGDraughtsRules`, `FKGTableBot` araması, perft testleri, `FKGMgPainter` 2D panel; kendi dosyasında konsol komutu | M | Yok; sadece yeni dosyalar, 020b ile paralel koşabilir |
| **036b** | Ağ masası: `AKGBoardTable`, RPC bileşeni, alt sistem, pay/tavan/45 sn odak, faz donması, isimsiz tebeşir tahtası, Bakır bahsi, snapshot, masa sayısı tavanı, iki süreçli smoke | M | 036a, 036h |
| **036c** | 3D tahta görünümü + taşlar (Blender başsız) + odak vinyeti | M | 036b; T2 kararı |
| **036d** | Masa botu dünyada: çağrıya cevap, yürüme, oynama, çanda kalkma | S | 036b, Roadmap 021, 023 |
| **036e** | Deftere bağlama: olaylar, dar tanıklık filtresi, çan anı kümesi, ORADAYDIM ve `KG_CONTRA` | S | 036b, Roadmap 020b, 021, 024 |

### Sonra (öncelik sırasıyla, her biri ayrı öneri)
1. **Yalancı Zarı** (Geç Kalan barı, 2–6 kişi): oyunun içinde blöf oyunu, sosyal çıkarıma en yakın masa oyunu.
   Zar `FKGRng` ile sunucuda; sonucu maçı etkilemez (G4 temiz). Kardeş tasarımın orman kampında **kamp ateşi zarı**
   olarak da yer alabilir.
2. **Türk daması** varyantı (Onay T1'e göre MVP'ye de girebilir).
3. **Masa başı fısıltı:** iki oturan konuşunca ses otomatik fısıltı menziline (G1.2, 3 m) iner, seyirci kimin konuştuğunu
   görür ama duymaz. SPRINT-023 ve G1.2'ye bağlı.
4. **Masayı devirmek** (itme ile): taşlar fizikle saçılır, mantık durumu korunur, oyun ertelenir; devirmek bir defter
   olayıdır, masa başına günde 1.
5. **Tavla** (çay ocağı yapılınca; 01b §4 Çaycı Rıza).
6. **Uzak ve tehlikeli masa:** Dere Boyu'ndaki oduncu kampında kütüğe oyulmuş dama ya da Taş Çember'de bir tahta
   (risk katmanı "tehlikeli"): gizli pazarlıkların yeri.
7. **Kart oyunu** (Pişti ya da blöf tabanlı bir oyun): Yalancı Zarı tutarsa.
8. **Seyirci yan bahisleri**, hamle geçmişi/PGN dışa aktarma, 3D meyhane lobisindeki masalar.
9. **Meyhaneci** rolüne gece masa (sokağa çıkma yasağını çiğnemenin riski): sadece rol sprintlerinde, charter §0.1'e
   göre yeni yetenek önerisi olarak.

**Planlanmayan:** sıralama/ELO, masa oyunundan XP ya da Altın (çiftçiliği ve motor hilesini ödüllendirir), maç içinde
tam saatli satranç (ölü zaman).

---

## 11. Charter kontrol listesi (Ç1–Ç12)

Kural: Ç1–Ç4 zorunlu, kalan 8'den ≥ 6 Evet. Bu **yeni** bir mekaniktir: zorunlu maddede Hayır = ret.

| # | Soru | Cevap | Gerekçe |
|---|---|---|---|
| Ç1 | Karar | **Evet** | Otur (dar alibi + daralan algı + sabit hedef) ya da dolaş; kiminle oturacağın (güven); çandan önce kalk ya da kal; tahtayı sil ya da silme; ORADAYDIM de ya da deme; ölünün bahsini al ya da alma. Her seçeneğin risk/bilgi sonucu farklı. |
| Ç2 | İz | **Evet** | Tebeşir tahtası, kâsedeki para yığını ve kalkış saati dünyada, herkesin görebileceği izlerdir (bugün bile). Defter olayları 020b ile (§4.1). |
| Ç3 | Karşı hamle | **Evet** | Tahta isimsiz ve kaba (30 sn kova): alibi tanık ister; tahta silinebilir ama leke kalır; rakip ORADAYDIM iddiasını yalanlayabilir; Sabırsız rakip "görmedim" diyebilir ama defter onu tanık yazar; oturan 45 sn'de kalkar. |
| Ç4 | Ölçüm | **Evet** | S2.3 (+1 ilgili parça), S9.5 (iddia/çelişki), S1.4 (güven), S4.4 (karşılaşma); koruyucu T-M1 (masa payı p90 ≤ %30), T-M3 (0.5–1.5×, tabancalı kol ayrı) ve T-M5 (45 sn kesintisiz odak) ile S1.1/S1.2'yi kötüleştirmediği gösterilir. |
| Ç5 | Bağlantı | **Evet** | Defter, toplantı (iddia), bot zihni, epilog (zaman çizelgesi, unvan), ekonomi (Bakır → sargı), harita (risk katmanı, bölge kapısı), ganimet (horoz tıkı odağı kırar). |
| Ç6 | Ölçek | **Evet** | Maç içi masa sayısı floor(floor((N − tehdit)/3)/2): N ≤ 7'de 0 (masa ısınma ve epilogda), N = 8–12'de 1, N = 16–20'de 2; paylar Gündüz süresiyle ölçeklenir. Küçük lobide alibinin tarlayı temizlemesi riski böylece kapandı. |
| Ç7 | Varyans | **Evet** | Satranç ve dama deterministik; bot seçimleri tohumlu `FKGRng`; masanın sonucu maçı belirlemez. Zar (Sonra) sadece masa oyununun içinde. |
| Ç8 | Bot | **Evet** | Botlar çağrılınca oynar, çanda kalkar, sadece açık tahta durumunu okur; 024 sonrası masa olaylarını iddia olarak kullanır. MVP'de kendiliğinden oturmazlar (bilinçli). |
| Ç9 | Sessiz yol | **Evet** | Tamamı fare + HUD; iddia yapılandırılmış satır; çağrı cevabı NEAR sohbet satırı. |
| Ç10 | Okunabilirlik | **Evet** | "Masada oturan görülür ve kayda geçer, ama etrafını az görür." |
| Ç11 | Frustrasyon | **Evet** | Daralan algı vinyetle önceden görünür; kalkmak cevaptır ve oyunu kaybettirmez; 45 sn'de otomatik erteleme; horoz tıkı odağı kırar. |
| Ç12 | Tempo | **Hayır** | Isınma ve epilogda sosyal eğlencedir, ama charter'ın ölü zaman tanımıyla masadaki süre ölü zamandır (kendi olayların sayılmaz); ölü zamanı azaltmaz, sadece pay, 0.3× tavan ve 45 sn odak tavanıyla sınırlar. |

**Sonuç: Ç1–Ç4 Evet, diğerlerinden 7/8 Evet → geçer** (öneri olarak). Koşul: Ç2 ve Ç4'ün defter/ölçüm kısmı 020b ve
024'e bağlıdır; 036e onlardan önce bitirilemez. 020b veya 024 iptal edilirse masa, tebeşir tahtası iziyle kalan ama
ölçülemeyen bir yan etkinlik olur.

---

## 12. Onay listesi (kullanıcı için)
- [ ] **T0** Masa oyunlarını Backlog "Proposed" listesine al (M12 içeriği; doğrulama borcundan sonra).
- [ ] **T1** Dama varyantı: MVP İngiliz (çapraz) mı, Türk daması (düz) mı? Öneri: İngiliz, hemen ardından Türk.
- [ ] **T2** Taşlar: Blender'da başsız üretim (izin gerekmez, öneri) mi, KayKit Board Game Bits (CC0, indirme izni) mi?
- [ ] **T3** Maç içi pay 0.12 × Gündüz, oturum tavanı 0.3 ×, kesintisiz odak 45 sn (satranç çoğu maçta bitmez; tam saat lobide, ısınmada, epilogda). Alternatif: rakibin hamlesini S1.1'de algılanan olay saymak (metrik tanımı değişir, kullanıcı onayı ister).
- [ ] **T4** Bahis 0–10 Bakır ve ölünün payının masada kalması.
- [ ] **T7** Tebeşir tahtası isim yazmaz, imza yok.
- [ ] **T8** N ≤ 7'de maç içi masa yok (sadece ısınma ve epilog); maç içi masa sayısı formülü (§2).
- [ ] **T5** §3.10 sabitleri charter §0.3'e eklensin mi?
- [ ] **T6** §9.2'deki `StartMeeting` oturan-kaldırma düzeltmesi (bugünkü 71 koltuğu da etkiler) ayrı küçük iş olarak öne alınsın mı?

## 13. Riskler
- **R-1 (bugün var):** `StartMeeting` oturan karakteri kaldırmadan ışınlıyor; `AKGSeat::Tick` hareketi her karede
  kapattığı için oturan oyuncu toplantı halkasında donabilir ve kalkınca koltuğa geri ışınlanır. Masalardan bağımsız,
  mevcut 71 koltukta yaşanabilir.
- **R-2:** Masa ölü zaman mıknatısı olabilir (S1.2). Tedbir: pay 0.12×, tavan 0.3×, 45 sn odak, faz kesintileri, T-M1
  raporu. Plato olursa pay 0.10×'e iner.
- **R-3:** Alibi çok güçlü çıkarsa asma doğruluğu S9.4'ün %75 tavanını aşar. Tedbir: isimsiz tahta, 30 sn kova, alibinin
  taraf değil zaman temizlemesi, masa sayısı tavanı, N ≤ 7'de maç içi masa yok. Nöbet defteri (orman) gece alibisi
  kaynağıdır; ikisi birlikte S9.4 raporunda izlenir.
- **R-4:** Masa çok tehlikeli çıkarsa (T-M3 > 1.5×, özellikle kardeş tasarımın menzilli silahıyla) kimse oturmaz.
  Tedbir: odak konisi genişletilir ya da odak sadece kendi sıranda uygulanır.
- **R-5:** `KGBotController.cpp`, `KGGameMode.cpp` ve `KGDevCommands.cpp` birçok sprintin ortak dosyası; 036b ve 036d seri
  sırada koşmalı.
- **R-6:** Satranç taşı asset'i yok; T2 kararı gecikirse 036c bekler (036a ve 036b 2D panel ve yer tutucu taşlarla ilerler).
- **R-7:** Taş vuruşu sesi yeni bir `KG_HEAR_*` sabiti ister; charter'ın tek sabit tablosu kuralı gereği onaysız eklenmez.
