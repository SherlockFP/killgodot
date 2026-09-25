# Masa Oyunları — Satranç ve Dama (sonra: Yalancı Zarı, Tavla, kart) — Tasarım v0.1

> Durum: **Öneri (kullanıcı onayı gerekiyor)**, 2026-09-25. Hiçbir sprint kuyruğunu değiştirmez.
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
44 saniyedir (N = 12). Pay bitince oyun **ertelenir**: taşlar yerinde kalır, tahta yarın kaldığı yerden sürer.
Kalkarken tebeşir tahtasında bir satır belirir: *"Tom – Bartholomew · Gündüz 1 · 0:30–2:00 · ertelendi"*.

**3. Gündüz.** Meydanda bir ceset bulunur, çan çalar (SPRINT-021). Masadaki iki oyuncunun tahtası donar. Toplantıda
Tom "Ben 1:30–3:00 arası meydanda Ada ile satranç oynuyordum, tahtaya bakın" der. Tahtada satır gerçekten vardır. Ama
Ada imza atmamıştır: satırda *"Tom – Bir yabancı"* ve sonunda *"Bir yabancı kalktı 2:30"* yazar. Ceset 2:40
civarında düşmüştür. "Yabancı"nın Ada olduğunu sadece Tom söylüyor; Ada'nın alibisi, varsa, 2:30'da biter. Tom'unki
çana kadar sürer. Tartışma buradan başlar.

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
- **N < 8'de sadece T1 ve T3 açıktır** (2 tahta, en fazla 4 oturan). N ≥ 8'de 4 tahta. Gerekçe: 6 kişilik maçta 4 kişinin masada olması köyü boşaltır.
- Lore dokunuşu: T1 tahtasının başlığı sabittir: *"Karşılama Komitesi Satranç Kulübü — Finalist: G. (gelmedi)"*.

---

## 3. Mekanikler ve sayılar

### 3.1 Akış
1. **Otur** (`E`, mevcut `AKGSeat`). Karşı sandalye boşsa ipucu: "Rakip bekleniyor · Bir köylüye bak, `E`: oyuna çağır".
2. İki sandalye doluysa **tahta paneli** açılır: oyun (masanın oyunu), saat modu (faza göre otomatik), bahis (fare
   tekerleği, 0–10 Coin, sadece ilk oturan belirler), **imza** (açık/kapalı, her oyuncu kendisi için).
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
| Maç içi **Gündüz** | **Günlük pay** | Her oyuncunun her Gündüz için **düşünme payı** `KG_TABLE_ALLOWANCE` = 0.2 × Gündüz süresi, hamle başına +1 sn. Pay oyuncuya aittir, masaya değil: aynı gün başka masaya geçen yeni pay almaz. Payı biten oyuncunun sırası gelince oyun **ertelenir** (kaybetmez). Ayrıca oyuncu başına günlük oturum tavanı `KG_TABLE_SITTING_CAP` = 0.5 × Gündüz (duvar saati, aktif oyunda geçen). |
| **Epilog** | Uzatma | Ertelenmiş oyunlar iki saate +30 sn ve hamle başına +1 sn ile sürer. Epilog biterken bitmemiş oyun **beraberlik** ("Godot gelmedi, oyun yarına kaldı"). |

Sayılar (Gündüz = 150 + 6N sn, `KGGameMode.cpp:63-86`):

| N | Gündüz | Düşünme payı (0.2×) | Oturum tavanı (0.5×) |
|---|---|---|---|
| 6 | 186 sn | 37 sn | 93 sn |
| 8 | 198 sn | 40 sn | 99 sn |
| 12 | 222 sn | 44 sn | 111 sn |
| 16 | 246 sn | 49 sn | 123 sn |
| 20 | 270 sn | 54 sn | 135 sn |

**Neden bu kadar kısa:** maç 20–35 dk (S10.1). Masa bir ölü zaman mıknatısı olmamalı (S1.2: Gündüz ölü zaman payı
≤ %20). En kötü durumda bir oyuncu Gündüz'ün yarısını masada geçirir; beklenen kullanım çok daha azdır (bkz. T-M1).
**Sonuç:** maç içi satranç "yazışmalı satranç" gibi günlere yayılır. Tipik bir gündelik oyun (30–40 hamle, ~4 sn/hamle)
3–4 gün sürer; dama (20–25 hamle) 2 gün. Maçların bir kısmında satranç bitmez: bu bilinçli bir seçimdir, "Yarın" motifi
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

### 3.6 Bahis (Coin)
- 0–10 **Coin** (maç içi para, `UKGItemCatalog` `Coin`, `Inventory/KGItemCatalog.h`). Sadece Gündüz. İkisi de aynı
  miktarı koyar. Sunucu başlangıçta iki envanterden düşer ve kâseye koyar: kâsede **gerçek para yığını** görünür (en
  fazla 20 mesh, herkes görür).
- Ölçek: sandık 1–5, fıçı 1–3, sandık (Chest) 5–20 Coin düşürür (`Inventory/KGLoot.cpp`). 10 Coin kayda değer ama
  yıkıcı değil.
- **Sonuçlar:** kazanan 2× bahsi alır. Beraberlik ve maç sonuna kadar bitmeyen oyun: iade. **Ölüm:** hayatta kalanın
  payı ona döner; **ölenin payı masada fiziksel bir para yığını (`AKGPickup`) olarak kalır.** Onu alan herkes bir
  Pickup olayı bırakır (tanıklarıyla). "Ölünün bahsini kim aldı?" küçük ama gerçek bir iz.
- **Botlar bahse girmez** (bahis 0): insanların Coin'i bottan "çiftçilik"le toplaması, Coin kardeş tasarımdaki
  silah parçalarına/eşyalara dönüşecekse bir sömürü olur.
- Ev payı (meyhane kesintisi) MVP'de yok. Coin bir para yuvası (sink) gerekirse sonra T3/T4'e %10 eklenebilir.
- Uyum: Roadmap Onay R5'in "Korsan: mahkeme kararına bahis" fikri onaylanırsa aynı emanet (escrow) modülünü kullanır.

### 3.7 Tebeşir tahtası (masa kaydı) — dünyadaki iz
Her masanın yanında küçük bir kara tahta vardır. Son **3 oyunu** gösterir:

```
Tom – Bir yabancı · Gündüz 3 · 1:30–3:00 · ertelendi · Bir yabancı kalktı 2:30
```
- Saatler faz saatinde, **30 sn kovasına yuvarlanır** (`KG_TABLE_SLATE_BUCKET`). Defter kesin saati tutar; tahta
  kaba saati. Böylece Vaka Dosyası'nın ölüm zamanı aralığıyla kenarlarda belirsizlik kalır (S9.4 üst sınırı).
- **İmza** bir karardır: imzalı satır doğrulanabilir alibi, imzasız satır "Bir yabancı" yazar (ama rakip kim olduğunu
  bilir ve söyleyebilir). Yayıncı modunda isimler `KGStreamer::DisplayName` ile gelir (G7.5).
- Tahta **otomatik duyurulmaz** (S2.5): okumak için masaya gitmek gerekir.
- **Silmek:** herkes tahtayı silebilir (`E` basılı 2 sn). Silinen tahtada **"silinmiş" lekesi** kalır, bir sonraki oyun
  yazılana kadar. Silme bir defter olayıdır (görenler tanık). Sahte yolu: Sabırsız bir Kasabalının alibisini ortadan
  kaldırabilir ya da kendi "kalktı" satırını yok edebilir, ama leke silindiğini ilan eder.

### 3.8 Odak ve savunmasızlık
- Oyun başlayınca kamera **tahta görünümüne** geçer (§7). Tahta görünümündeyken oyuncunun **görme tanıklığı** daralır:
  koni `KG_TABLE_FOCUS_CONE` = ±30° (tahta yönünde), menzil `KG_TABLE_FOCUS_RANGE` = 4 m (normalde `KG_SIGHT_CONE`
  ±60°, `KG_SIGHT_DAY` 30 m). **Duyma değişmez.** Karşıdaki rakip bu koninin içindedir: rakip her zaman görülür ve
  4 m < `KG_IDENT_RANGE` olduğu için **tanımlanır** (maskesizse).
- **Etrafa bak** (sağ tık basılı): kamera 0.15 sn'de serbest oturma bakışına döner (`AKGSeat` serbest yaw), algı normale
  döner, satranç saati akmaya devam eder. Bırakınca tahtaya döner.
- Oturan hareket edemez (`AKGSeat` MOVE_None). Kalkmak anlıktır ama yeri bellidir. Backstab geometrisi
  (`BackstabRange` 150 cm, arkadan) oturana da uygulanır.
- Sonuç: masa **ne sığınak ne tuzaktır**. Rakip bir tanıktır (bu korur). Ama odaktaki oyuncu çevresindeki cinayeti
  görmez (bu masadakileri kötü tanık yapar), sabit bir hedeftir ve Sabırsız bir rakip, maskeli bir takım arkadaşının
  işini "görmedim" diyerek örtebilir.
- Ekran kenarında %15 karartma (vinyet) odağı ve bedelini oyuncuya gösterir (Ç11).

### 3.9 Seyirciler
- Masaya 3 m'den bakan herkes HUD'da küçük saat şeridini görür. Özel kamera yok: taşlar 2–3 m'den okunacak boyuttadır.
- Masanın 3 m içinde, tahtaya bakarak ≥ 10 sn duran seyirci için sunucu bir `TableSpectate` olayı yazar
  (başlangıç/bitiş). Seyircinin algısı daralmaz (oyun onun değil).
- Seyirci hamle öneremez (sesle önerebilir, bu sosyal).

### 3.10 Önerilen sabitler (charter §0.3'e eklenmek üzere; onaylanmadan charter'a yazılmaz)

| Sabit | Değer | Anlamı |
|---|---|---|
| `KG_TABLE_ALLOWANCE` | 0.2 × Gündüz süresi / oyuncu / gün, +1 sn/hamle | Maç içi düşünme payı |
| `KG_TABLE_SITTING_CAP` | 0.5 × Gündüz süresi / oyuncu / gün | Aktif oyunda geçen en uzun duvar saati |
| `KG_TABLE_FOCUS_CONE` | ±30° | Tahta görünümünde görme konisi |
| `KG_TABLE_FOCUS_RANGE` | 4 m | Tahta görünümünde görme menzili |
| `KG_TABLE_SLATE_BUCKET` | 30 sn | Tebeşir tahtasındaki saat yuvarlaması |
| `KG_HEAR_TABLE` | 8 m | Taş vuruşu / saat düğmesi sesi (duvar arkası × `KG_WALL_FACTOR`) |
| `KG_TABLE_STAKE_MAX` | 10 Coin | Oyuncu başına bahis tavanı |

---

## 4. Çıkarım döngüsüne katkı

Charter döngüsüne göre: **KARAR** (otur / kalk / imzala / sil / çağrıldığında gel) → **İZ** (defter olayı + tebeşir
tahtası + para yığını) → **BİLGİ** (zaman aralıklı alibi, kalkış saati, kimin kiminle oturduğu) → **ŞÜPHE** ("çandan 40
sn önce kalktı") → **TOPLANTI** (ORADAYDIM iddiası, tahtayla çelişki) → **SONUÇ** → yeni karar ("bugün onunla
oturmayacağım").

### 4.1 Defter olayları (SPRINT-020b'nin `UKGEventLedger`'ına yazar; 020b gelene kadar boş kanca)

| Olay | Aktör | Tanık (020b filtresi) | Kim okur |
|---|---|---|---|
| `TableGameStart {masa, beyaz, siyah, oyun, bahis, imzalar}` | iki oyuncu | görenler/duyanlar (`KG_HEAR_TABLE`) | defter (J, 024), bot zihni (023), epilog |
| `TableStand {oyuncu, sebep: kendisi/pay/çan/faz/ölüm}` | kalkan | görenler + rakip | defter, Vaka Dosyası ilgili parça (S2.3) |
| `TableGameEnd {sonuç, sebep}` | iki oyuncu | görenler | epilog, bahis |
| `TableSpectate {seyirci, başlangıç, bitiş}` | seyirci | oyuncular + görenler | defter |
| `SlateWiped {silen}` | silen | görenler | defter, Kasaba Panosu (024, isimsiz "meydan tahtası silindi") |
| `StakeTaken {alan, miktar}` | ölünün yığınını alan | görenler | defter |

Her olay tek satırdır, bölge adı + faz saati ile (G3.5). Oyun içindeki **tek tek hamleler olay değildir** ve **karar
fiili sayılmaz** (S1.3'ü şişirmemek için). Karar fiilleri: otur, kalk, imzala/imzalama, sil, çağrıya gel.

### 4.2 Beş kanca (kullanıcının önerileri + tasarımcının eklediği)
1. **Karşılıklı alibi, ama dar.** Masa kaydı iki oyuncuyu **bir zaman aralığı** için aklar, taraflarını değil. Bir
   cinayeti dışlar, takımı dışlamaz. Sabırsızlar da kullanır: biri oturup alibi toplar, takım arkadaşı öldürür.
   "Hep birlikte oturuyorlar" başlı başına bir desendir.
2. **Odaktayken savunmasız ve kör** (§3.8). Masadakiler güçlü bir alibi ama zayıf bir tanıklık alır. Bu, bilgi
   bütçesini dengeler: alibi arttıkça tanık azalır.
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
| S1.1, S1.2 | **Risk (koruyucu):** odaktaki oyuncu daha az olay algılar. Pay ve tavan bunu sınırlar. | [O], aşağıdaki T-M1 |

**Özelliğin kendi koruyucu metrikleri** (charter bandı değil, sadece rapor; onaylanırsa §0.2 kurallarıyla):

| Kod | Metrik | Hedef | Ölçüm | n_min |
|---|---|---|---|---|
| T-M1 | Gündüz canlı oyuncu-saniyelerinin aktif masa oyununda geçen payı | Oyuncu p90 ≤ %30; lobi ortalaması ≤ %10 | [O] ([B] sadece bilgi) | 40 oyuncu-gündüz |
| T-M2 | O gün ≥ 1 masa oyunu oynanmış vakalı toplantılarda masa olayına bağlı ≥ 1 iddia | ≥ %20 | [B] (024 sonrası) | 30 toplantı |
| T-M3 | Oturan oyuncunun saniye başına öldürülme oranı ÷ ayakta Gündüz oranı | 0.5–1.5× (ne sığınak ne tuzak) | [B] gece koşusu | 40 cinayet (masada) |
| T-M4 | İmza oranı | %50–90 (%100 = karar değil) | [O] | 60 oyun |

---

## 5. Karşı hamleler ve kötüye kullanım

| Durum | Kim, neden | Karşı hamle / kural |
|---|---|---|
| Alibi çiftçiliği (iki Sabırsız birbirine alibi) | Sabırsız | Kayıt doğrudur ama dardır; üçüncü takım arkadaşının cinayetini örtmez. "Hep aynı ikili" deseni tartışılır. |
| Sabırsız rakip, maskeli takım arkadaşının cinayetini "görmedim" diye örter | Sabırsız | Rakip **her zaman** tanık olarak defterde yazar: yalanı 024 ile çelişki üretir. Seyirciler ikinci tanıktır. |
| Masada saklanmak (toplantıya gelmemek, iş yapmamak) | Herkes | Günlük pay + oturum tavanı; çanda donma; `KG_GATHER_WINDOW` sonunda Geç Kalan; AFK (G5.5) masa hamlelerini etkinlik sayar ama tavan yine keser. |
| Tahtayı silerek alibi yok etmek | Sabırsız | Silmek 2 sn, görünür, "silinmiş" lekesi kalır, defter olayıdır. Defterdeki kayıt silinmez. |
| Coin aklama / takım arkadaşına para aktarma | Sabırsız | Zaten para düşürerek (`AKGPickup`) mümkün. Bahis tavanı 10, her aktarım görünür (kâse) ve defterde. Yeni sömürü yok. |
| Bottan Coin çiftçiliği | Güçlü oyuncu | Botlar bahse girmez. |
| Rakibin payını yakmak için yavaş oynamak | Trol | Saat hamle yapanın payından yer: yavaş oyuncu **kendi** payını yakar. |
| Koltuk işgali (oturup başlamamak) | Trol | Hazır olmadan 20 sn geçerse ikinci sandalyedeki ipucu "Kalk ya da hazır ol"; 40 sn'de sunucu kaldırır (sadece aktif oyun yokken). |
| Davet spam'i (bot/oyuncu) | Trol | 10 sn'de 1 çağrı; aynı hedefe 60 sn'de 1. |
| Hamle RPC spam'i / geçersiz hamle | Hileci | Sunucu her hamleyi kural çekirdeğiyle doğrular; saniyede en fazla 4 hamle RPC'si; yanlış faz, oturmayan, sırası olmayan, geçersiz hamle reddedilir (G7.2 red testi). |
| Motor (Stockfish) kullanmak | Hileci | Maç içinde sonucun maça etkisi yok; sıralama/ELO yok, meta ödül yok. Kazanmanın tek getirisi bahis ve epilog unvanı ("Köyün Büyükustası"). |
| Masa ile gizli işaretleşme | Sabırsız | Değersiz: takım kanalı zaten var. |
| Ölü oyuncunun hamle söylemesi | Hayalet | Ölüler canlının oyununa ses/yazı ile ulaşamaz (S8.3 kanalları). Epilogda herkes serbest. |

**Frustrasyon kuralı (G5.6):** masadaki tek olumsuz etki (daralan algı) önceden gösterilir (vinyet + ipucu metni) ve
bir karşı fiili vardır (Etrafa bak, kalk).

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
| Sağ tık basılı | Etrafa bak | Odak kapanır (§3.8) |
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
| Sunucu-özel durum | Zobrist tekrar geçmişi (≤ 150 × 8 B), emanet, imzalar: `UPROPERTY(SaveGame)`, replike olmaz. |
| Çizim | Tahta başına tek tahta mesh'i + taş türü başına bir ISM (satranç 6, dama 2), renk instance başına özel veri ile palet materyalinde. 4 tahtada ≤ 32 çizim çağrısı. Taşlar 25 m'de gizlenir, 8 m'de LOD1 (%25). Satranç tahtası yakında ≤ 16k üçgen. |
| Ses | Taş vuruşu ve saat düğmesi: 2 ses, `KG_HEAR_TABLE` 8 m zayıflama, eşzamanlı sınır tahta başına 2. |
| Host migration | `UKGSnapshotComponent` tahta aktöründe; FEN + saatler (kalan saniye) + paylar + emanet + imzalar + tebeşir satırları SaveGame. Aktör uzamsal yüklenmez (World Partition akıtmaz). |

---

## 9. Bağımlılıklar

### 9.1 Var olan kod (değişmeden kullanılır)
| Dosya | Ne için |
|---|---|
| `World/KGSeat.*` | Oturma, replike `Occupant`, oturma kamerası, `E/Boşluk` ile kalkma, ölü oturanı kaldırma (`Tick`). Tahta aktörü iki koltuğu **sahiplenir** ve `GetOccupant()`'ı okur: `KGSeat` dosyasında değişiklik yok. |
| `Inventory/KGPlayerExtrasSubsystem.*` | "Sıfır entegrasyon" kalıbı: yeni `UKGTabletopSubsystem` her `AKGPlayerState`'e `UKGTabletopRPCComponent` ekler (aynı kalıp, ayrı dosya). |
| `Inventory/KGItemCatalog.*`, `KGInventoryComponent.*`, `World/KGPickup.*` | Coin, emanet, ölünün yığını |
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
| Kardeş tasarımlar (paralel) | Kırılabilir kutu → silah parçaları: menzilli silah oturan hedefi daha savunmasız yapar (T-M3 izlenmeli). Orman kampı "bir gece hayatta kal": Yalancı Zarı'nın kamp ateşi versiyonu için doğal yer (§10 Sonra). |

---

## 10. MVP ve sonrası

### TT-1 — Masa oyunları çekirdeği (M) · 020b'den bağımsız yapılabilir
**Hedef (kullanıcının göreceği):** meydanda ve Geç Kalan'da 4 masa. İki oyuncu (ya da oyuncu + bot) satranç veya dama
oynar, maç içinde günlere yayılır, çanda/gecede donar, tebeşir tahtasına yazılır, isteyen Coin koyar.

**Kabul (sabit, 6):**
1. **Test** `KillGodot.Tabletop.Perft`: satranç perft değerleri: başlangıç d1–4 = 20 / 400 / 8 902 / 197 281;
   "Kiwipete" (`r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -`) d1–3 = 48 / 2 039 / 97 862;
   pozisyon 3 (`8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -`) d1–4 = 14 / 191 / 2 812 / 43 238; pozisyon 5
   (`rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8`) d1–3 = 44 / 1 486 / 62 379. İngiliz daması başlangıç
   d1–5 = 7 / 49 / 302 / 1 469 / 7 361 (sprint başında bağımsız bir kaynağa karşı teyit edilir). Ayrıca ≥ 20 kural
   vakası: şahtan geçen rok reddi, geçerken almanın sadece hemen ardından, Ata terfi, pat, üçlü tekrar, 50 hamle,
   yetersiz malzeme, damada zorunlu alma, çoklu atlama, dama olunca hamlenin bitmesi.
2. **Test:** yarım oyunun (FEN, saatler, paylar, tekrar geçmişi, emanet, tebeşir satırları) SaveGame arşiv turu birebir
   aynı; Toplantı/Mahkeme/Gece fazlarında saatler 0 sn ilerler; payı biten oyuncunun sırasında oyun ertelenir, kaybetmez.
3. **İki süreçli smoke** `Tools/Unreal/kg_table_smoke.ps1` (penceresiz, kg_chat_smoke kalıbı): istemci sunucuya karşı
   "Çoban matı"nı RPC ile oynar; iki makinede son FEN ve "mat" aynı; istemcinin geçersiz, sırasız ve oturmadan
   gönderdiği hamleler reddedilir (G7.2); 5 Coin bahis kazanana **bir kez** geçer.
4. **[U]** Çağrılan bot 15 sn içinde oturur ve bir damayı bitirir (bot–bot, `kg.Table.Start Checkers Bot Bot`);
   `KG_TABLE` satırları yazılır. Başsız `stat`: 4 tahta + 2 arayan bot, N = 20'de tabletop sunucu maliyeti ortalama
   ≤ 0.3 ms/kare, p99 ≤ 1.0 ms.
5. **Görseller** (2 bakış turu): `KG_Cap_table_square.png` (T1, 5 m, gündüz), tahta görünümü UIShot'u (orta oyun,
   yasal hamle noktaları, saat şeridi), 3 satırlı tebeşir tahtası.
6. `run_invariants.ps1` geçer; tabletop smoke I3'e eklenir.

**Yazılabilir kapsam:** yeni `Source/KillGodot/Tabletop/` (`FKGChessRules`, `FKGDraughtsRules`, `FKGTableBot`,
`AKGBoardTable`, `UKGTabletopRPCComponent`, `UKGTabletopSubsystem`, `FKGBoardState`), yeni `UI/Tabletop/`,
`AI/KGBotController.cpp` (sadece çağrı kancası), `Dev/KGDevCommands.cpp` (sadece `kg.Table.*` fiilleri),
`Core/KGGameMode.cpp` (sadece §9.2'deki tek satır), layout JSON `tables[]` (sadece veri),
`Tools/Blender/kg_make_tabletop.py` + yeni `Art/`/`Content` yolları (T2 = a ise), `Tools/Unreal/kg_table_smoke.ps1`,
`Private/Tests/KGTabletopTests.cpp`, `Docs/05_Tech_Architecture.md` (yeni bölüm), `Docs/01b_Village_Life_Fun.md` §2
(satırlar). Klasör adı: `05_Tech_Architecture.md` `Minigames/` diyor; `Chores/UI/KGMinigame*` bu adı zaten görev
panelleri için kullandığı için `Tabletop/` önerilir.

**Dev fiilleri:** `kg.Table.List`, `kg.Table.Start Chess|Checkers [Bot] [Bot]`, `kg.Table.Move e2e4`,
`kg.Table.Fen [FEN]`, `kg.Table.Allowance 999`, `kg.Table.Shot`.

**Sınırlar:** 3 düzeltme denemesi / kontrol, UI için 2 bakış turu, plato durdurması. Başlık değişiklikleri tek toplu
editör-kapanış döngüsünde. Paralel sprintlerle ortak dosya: `KGBotController.cpp`, `KGGameMode.cpp`,
`KGDevCommands.cpp` → seri sırada.

### TT-2 — Masayı çıkarım döngüsüne bağla (S) · 020b + 021 + 024 sonrası
**Kabul (sabit, 5):**
1. **Test:** tahta görünümündeki oyuncu, arkasında 5 m'de ve 60° açıdaki backstab'ı **görmez** (duyar); masanın
   karşısındakini görür ve tanımlar; "Etrafa bak" basılıyken normal algı.
2. **Test:** §4.1'deki 6 olay tanık listeleriyle deftere düşer; tebeşir tahtası silinince "silinmiş" lekesi ve
   `SlateWiped` olayı; çan anında oturan herkes tek `TableStand {çan}` kümesinde.
3. **Test:** masa olayından ORADAYDIM iddiası `ClaimRef` ile paylaşılır; tahtayla çelişen iddia `KG_CONTRA` üretir.
4. **[U]** botlar vakayla örtüşen masa olayını iddia olarak kullanır ve loglar. T-M1, T-M2, T-M3 raporda (kapı değil).
5. `run_invariants.ps1` geçer.

**Yazılabilir kapsam:** `Tabletop/`, `Evidence/` (olay türleri, algı daralması kancası), `AI/` (iddia), testler.

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
| Ç1 | Karar | **Evet** | Otur (güçlü, dar alibi + daralan algı + sabit hedef) ya da dolaş; imzala ya da imzasız kal; çandan önce kalk ya da kal; tahtayı sil ya da silme; ölünün bahsini al ya da alma. Her seçeneğin risk/bilgi sonucu farklı. |
| Ç2 | İz | **Evet** | Tebeşir tahtası, kâsedeki para yığını ve kalkış saati dünyada, herkesin görebileceği izlerdir (bugün bile). Defter olayları 020b ile (§4.1). |
| Ç3 | Karşı hamle | **Evet** | Alibi dar ve kaba (30 sn kova); tahta silinebilir ama leke kalır; odaktaki oyuncu "Etrafa bak" ile algısını geri alır; Sabırsız rakip "görmedim" diyebilir ama defter onu tanık yazar. |
| Ç4 | Ölçüm | **Evet** | S2.3 (+1 ilgili parça), S9.5 (iddia/çelişki), S1.4 (güven), S4.4 (karşılaşma); koruyucu T-M1 (masa payı p90 ≤ %30) ve T-M3 (0.5–1.5×) ile S1.2'yi kötüleştirmediği gösterilir. |
| Ç5 | Bağlantı | **Evet** | Defter, toplantı (iddia), bot zihni, epilog (zaman çizelgesi, unvan), ekonomi (Coin), harita (risk katmanı, bölge kapısı). |
| Ç6 | Ölçek | **Evet** | N = 6'da 2 tahta (az kişiyi köyden çekmemek için), N ≥ 8'de 4 tahta; paylar Gündüz süresiyle ölçeklenir. N = 6'da alibi görece güçlüdür (4 şüpheliden 2'sini dışlar), 30 sn kova bunu yumuşatır. |
| Ç7 | Varyans | **Evet** | Satranç ve dama deterministik; bot seçimleri tohumlu `FKGRng`; masanın sonucu maçı belirlemez. Zar (Sonra) sadece masa oyununun içinde. |
| Ç8 | Bot | **Evet** | Botlar çağrılınca oynar, çanda kalkar, sadece açık tahta durumunu okur; 024 sonrası masa olaylarını iddia olarak kullanır. MVP'de kendiliğinden oturmazlar (bilinçli). |
| Ç9 | Sessiz yol | **Evet** | Tamamı fare + HUD; iddia yapılandırılmış satır; çağrı cevabı NEAR sohbet satırı. |
| Ç10 | Okunabilirlik | **Evet** | "Masada oturan görülür ve kayda geçer, ama etrafını az görür." |
| Ç11 | Frustrasyon | **Evet** | Daralan algı vinyetle önceden görünür; "Etrafa bak" ve kalkmak cevaptır; kalkmak oyunu kaybettirmez. |
| Ç12 | Tempo | **Hayır** | Erken evreye (1. gün, ısınma, epilog) hizmet eder ama charter'ın ölü zaman tanımıyla odaktaki oyuncu **daha az** olay algılar; ölü zamanı azaltmaz, sadece pay ve tavanla sınırlar. |

**Sonuç: Ç1–Ç4 Evet, diğerlerinden 7/8 Evet → geçer** (öneri olarak). Koşul: Ç2 ve Ç4'ün defter/ölçüm kısmı 020b ve
024'e bağlıdır; TT-2 onlardan önce bitirilemez. 020b veya 024 iptal edilirse masa, tebeşir tahtası iziyle kalan ama
ölçülemeyen bir yan etkinlik olur.

---

## 12. Onay listesi (kullanıcı için)
- [ ] **T0** Masa oyunlarını Backlog "Proposed" listesine al (M12 içeriği; doğrulama borcundan sonra).
- [ ] **T1** Dama varyantı: MVP İngiliz (çapraz) mı, Türk daması (düz) mı? Öneri: İngiliz, hemen ardından Türk.
- [ ] **T2** Taşlar: Blender'da başsız üretim (izin gerekmez, öneri) mi, KayKit Board Game Bits (CC0, indirme izni) mi?
- [ ] **T3** Maç içi pay: 0.2 × Gündüz (satranç çoğu maçta bitmez) uygun mu? Daha cömert seçenek 0.3 × (T-M1 riski).
- [ ] **T4** Bahis 0–10 Coin ve ölünün payının masada kalması.
- [ ] **T5** §3.10 sabitleri charter §0.3'e eklensin mi?
- [ ] **T6** §9.2'deki `StartMeeting` oturan-kaldırma düzeltmesi (bugünkü 71 koltuğu da etkiler) ayrı küçük iş olarak öne alınsın mı?

## 13. Riskler
- **R-1 (bugün var):** `StartMeeting` oturan karakteri kaldırmadan ışınlıyor; `AKGSeat::Tick` hareketi her karede
  kapattığı için oturan oyuncu toplantı halkasında donabilir ve kalkınca koltuğa geri ışınlanır. Masalardan bağımsız,
  mevcut 71 koltukta yaşanabilir.
- **R-2:** Masa ölü zaman mıknatısı olabilir (S1.2). Tedbir: pay, tavan, faz kesintileri, T-M1 raporu. Plato olursa
  pay 0.15×'e iner.
- **R-3:** Alibi çok güçlü çıkarsa asma doğruluğu S9.4'ün %75 tavanını aşar. Tedbir: 30 sn kova, alibinin taraf değil
  zaman temizlemesi, imzasız seçenek.
- **R-4:** Masa çok tehlikeli çıkarsa (T-M3 > 1.5×, özellikle kardeş tasarımın menzilli silahıyla) kimse oturmaz.
  Tedbir: odak konisi genişletilir ya da odak sadece kendi sıranda uygulanır.
- **R-5:** `KGBotController.cpp`, `KGGameMode.cpp` ve `KGDevCommands.cpp` birçok sprintin ortak dosyası; TT-1 seri
  sırada koşmalı.
- **R-6:** Satranç taşı asset'i yok; T2 kararı gecikirse TT-1'in görsel kabulü (madde 5) bekler.
- **R-7:** Taş vuruşu sesi yeni bir `KG_HEAR_*` sabiti ister; charter'ın tek sabit tablosu kuralı gereği onaysız eklenmez.
