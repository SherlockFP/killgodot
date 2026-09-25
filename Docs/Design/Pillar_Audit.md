# Sütun Denetimi — bugünkü KillGo, charter'a göre (2026-09-25)

> Neye göre: `Docs/Design/KillGo_Pillars.md` (10 sütun + G1–G7).
> Nasıl: kod okuması (`Source/KillGodot/`), tasarım belgeleri, `Saved/Logs/MatchSmoke.log` (12 bot, tohum 7, saat
> x30) ve `Docs/Level/v2_Build_Report.md` §7'deki 20 botluk soak. Derleme ya da oyun çalıştırılmadı (salt okunur
> denetim). Rol üreticisi oranları, `KGRoleListGenerator.cpp`'nin Python'daki birebir kopyasıyla (N 6–20, N başına
> 4.000 liste) hesaplandı.
> Dört denetçinin raporları birleştirildi. Puanlar baş tasarımcının kararıdır.
>
> **Geçerlilik uyarısı (v1.1):** charter §0.2'ye göre bugün **hiçbir [B] sayısı geçerli değil**: botlar G6'yı
> sağlamıyor, tek koşu var ve MatchSmoke x30'da (bot oy zamanlayıcıları ölçeklenmiyor, botlar gerçek hızda yürüyor).
> Smoke ve 20 botluk soak'tan gelen her değer aşağıda **"anekdot"** diye işaretlidir: yön gösterir, başlangıç çizgisi
> değildir. Başlangıç çizgisi olarak sadece [L], [T] ve kod gerçekleri kullanılır; [B] başlangıç çizgisi v2
> SPRINT-020b ile gelir.

## Puan cetveli

**Kod puanı** (oyunun bugünkü hâli):
| Puan | Anlamı |
|---|---|
| 0 | Yok: ne kodda ne tasarımda |
| 1 | Kâğıtta var, kodda iskelet ya da tek bir parça |
| 2 | Kodda çalışan bir çekirdek var ama sütunun çoğu eksik |
| 3 | Sütun oynanabilir, hedeflerin yarısı ölçülüyor ve tutuyor (geçerli [B]/[T] ile) |
| 4 | Hedeflerin çoğu tutuyor, sadece ince ayar kaldı |
| 5 | Tüm hedefler N = 6, 8, 12'de tutuyor ve oyun testiyle doğrulandı |

- **Yarım puan:** x.5 = x seviyesinin tamamı karşılanıyor ve x+1 seviyesinin koşullarından en az biri karşılanıyor.
- **Tavan:** 3 ve üstü ölçüm ister. Geçerli [B] ölçümü olmadığı sürece hiçbir sütun kodda **2**'yi geçemez.

**Kâğıt puanı** (yazılı tasarımın hâli; charter hedeflerinin kaçının arkasında yazılı bir tasarım var):
| Puan | Anlamı |
|---|---|
| 0 | Tasarım yok |
| 1 | Sadece anılıyor |
| 2 | Hedeflerin < %50'sinin arkasında yazılı tasarım var |
| 3 | Hedeflerin ≥ %50'sinin arkasında yazılı tasarım var |
| 4 | ≥ %80'inin, iz ve karşı hamleleriyle birlikte |
| 5 | Hepsinin, lint edilebilir veriyle |

## Özet

| Sütun | Kod (bugün) | Kâğıt (tasarım) | En büyük eksik |
|---|---|---|---|
| 1 Kararlar | **1.5** | 4 | Rapor, saklanma, yetenek yok; Kasaba için gece tamamen boş |
| 2 Bilgi | **1** | 4 | Olay kaydı, algı modeli, isim etiketi yok; ölüm anında rolüyle duyuruluyor |
| 3 İz ve delil | **0.5** | 4 | Ceset dışında hiçbir iz yok ve ceset okunmuyor |
| 4 Harita | **2** (tavan; tavansız 2.5) | 4 | Güçlü yerleşim ama gözcü noktaları, sabotaj, bölge kapıları ve değişen durum yok |
| 5 Rol etkileşimi | **0.5** | 3 | 51 rolden 49'unun oynanış kodu yok; karşı roller hedefleriyle nadiren buluşuyor |
| 6 Kötü taraf | **1.5** | 4 | Tek fiil backstab; aldatma rolleri N < 14'te hiç çıkmıyor |
| 7 İyi / nötr | **1** | 3 | Araştırma fiili yok; nötrlerin kodda hedefi yok |
| 8 Ölüm | **1** | 4 | Serbest uçan izleyici; hayalet görevi, poltergeist, Araf yok |
| 9 Toplantılar | **1.5** | 3.5 | Prosedür çalışıyor ama içerik yok; toplantıda ölüm var |
| 10 Tempo | **1** | 3 | Sabit zamanlayıcılar, boş geceler, bot sayılmayan faz süreleri |
| G1 Ses | **1** | 4 | Ses yok (sadece ağız bileşeni); yazı kanalları doğru kapsamlı |
| G2 Ölçek | **1.5** | 3.5 | Rol üreticisi ölçekleniyor; bölge kapıları yok, botlar N'ye sayılmıyor |
| G3 Okunabilirlik | **1** | 2 | Başlangıç havuzu yok; rol kartı var olmayan yetenekleri anlatacak |
| G4 Kontrollü varyans | **2** | 2.5 | `FKGRng` tohumlu; fener ve bot oyları sonucu belirleyen rastgelelik |
| G5 Anti-snowball | **0.5** | 2.5 | Koruma süresi, gündüz risk modeli, AFK yok; toplantı ve epilogda öldürme mümkün |
| G6 Botlar | **0.5** | 1 | Algı, hafıza, gerekçe yok; Sabırsız botlar gizli rolü okuyor |
| G7 Adalet / anti-cheat | **2** (tavan; tavansız 3) | 3.5 | `COND_OwnerOnly` ve sunucu doğrulaması sağlam; relevancy ve hayalet kuralları eksik; G7.1 iki süreçli testi yok |

**Ortalama (kod):** 10 sütunda 11.5 / 10 = **1.15** / 5. **Kâğıt:** 36.5 / 10 = **3.65** / 5.
**Teşhis:** tasarım iyi, oyunun iskeleti (faz makinesi, oy, mahkeme) çalışıyor, ama **ikisini bağlayan bilgi katmanı
hiç yok**. Son sprintler (balık, kazı, lore, harita 2) bu katmanı değil, çevresini büyüttü.

---

## Sütun sütun kanıt

### 1. Kararlar — 1.5
**Var:**
- Gündüz nereye gidileceği gerçek bir karar: 8 bölgeye yayılmış 22 görev (`Morrowmere_v2_Plan.md` §9), balık tutma,
  kazı, sandıklar.
- Sabırsız için bıçağı çekip çekmemek: çekmek görünür, botlar bile "bıçakla gezmek itiraftır" diye kınına sokuyor.
- Toplantıda suçla ([V]) ve Suçlu/Masum oyu.

**Yok:**
- Raporlamak, saklanmak ve yetenek kullanmak diye bir seçenek yok. Gündüz cesedi otomatik duyuruluyor
  (`KGGameMode.cpp:467-471`), rol yeteneği yok.
- Gece Kasaba için karar yok: Gece Defteri, barikat, el çanı yok. Gece fazı (75 + 3N sn) Kasaba için tamamen ölü zaman.
- Ölçüm yok: karar fiilleri kaydedilmiyor.

### 2. Bilgi oynanışı — 1
**Var (potansiyel):**
- Gerçek görüş hattı olan birinci şahıs dünya; haritada diğer oyuncular gösterilmiyor (`KGHUDMap.inl:5`, doğru karar).
- Adlandırılmış bölgeler ve konum bildirimi (`AKGMapInfo::Regions`): iddialar için ortak kelimeler.
- Herkese açık görsel görev etkileri (`AKGChoreFx`: çan, fener, duman, ilan, saat), sahte görevlerde tetiklenmiyor.
- Balık tutma 18 m'den duyuluyor, misina görünüyor (alibi olarak tasarlandı).
- Kazıda açık mezarlar 40 m'den duyuluyor ve "şüpheli" olarak kalıyor (`Dig/KGDigTypes.h`).

**Yok:**
- Olay kaydı ve algı modeli: kimin neyi gördüğü veri olarak hiçbir yerde yok.
- İsim etiketi yok (`Source` içinde hiç yok) ve ses yok. Tanık kimi gördüğünü söyleyemez.
- Ölüm duyurusu yeri, zamanı ve bulanı atıyor, rolü ise anında veriyor (`KGGameMode.cpp:470`, `AnnounceDawn`
  712-725).
- Defter (J), vasiyet, zaman çizelgesi yok. `HandleDeath`'te hâlâ `TODO(M5): body becomes evidence`
  (`KGCharacter.cpp:1317`).
- Botlar görevi tut-E yoluyla bitiriyor ve bu yol `AKGChoreFx::AuthTrigger`'ı hiç çağırmıyor (tek çağrı yeri
  `KGChoreComponent.cpp:243`). Bot soak'ları sıfır alibi sinyali üretiyor.

### 3. İz ve delil — 0.5
- Cesetler dünyada kalıyor (`BecomeGhost`), ama hiçbir sistem cesedi okumuyor.
- Açık mezarlar ve görsel görev etkileri kısmi izler.
- Ayak izi, boya/talaş, yara sınıfı, sürükleme izi, düşen eşya, kapı kaydı yok. GDD §4 ve §6'da tasarlandı, kodda yok.

### 4. Harita hareketliliği — 2 (tavan; tavansız 2.5, en güçlü sütun)
**Var:**
- Morrowmere v2: 8 bölge, halkalar ve teraslar; risk eğimi tasarlanmış (güvenli meydan/iskele, orta halkalar,
  tehlikeli dar geçitler ve Gelgit Sokağı) (`Morrowmere_v2_Plan.md` §8).
- 22 görev 8 bölgeye yayılmış; kule görevleri dikey yolculuğu, yani pusu noktalarını zorluyor.
- Toplanma (anekdot: tek koşu, 20 varış): 20/20 bot, medyan 21.3 sn, p90 32.9 sn, en fazla 33.4 sn (hedef p90 ≤ 30;
  S4.5 n_min 100 varış). Not: `StartMeeting` bugün herkesi halkaya ışınlıyor (`KGGameMode.cpp:584-611`); bu ölçüm
  yürüyüş testidir, oyunda henüz toplanma yürüyüşü yok.
- Bot gezinmesi 8 bölgenin hepsini ziyaret ediyor; 462/462 navmesh yolu tam.
- Balık tutma, kazı/yeraltı (devam ediyor), 16 sandık, 71 oturak: bölgelere sebep veriyor.

**Yok:**
- Gözcü noktaları (Belvedere, Saat Kulesi tepesi, Balkon Sokağı korkuluğu, fener galerisi) hiçbir yetenekte
  kullanılmıyor.
- Sabotaj yok, rastgele olay yok, gece durumu yok: "değişen durumlar" sıfır.
- Bölge Kapıları (GDD §3) kodda yok: 6 kişilik maç 20 kişilik haritada oynanıyor.
- Bölge sebepleri veri olarak etiketli değil (lint edilemez).

### 5. Rol etkileşimi — 0.5
- `KGRoleListGenerator.cpp` sadece veri: id, kategori, taraf, güç. `UKGRoleDefinition.Abilities` hiç doldurulmuyor.
- Her sistem rolü sadece "Sabırsız mı değil mi" diye okuyor: bıçak (`CanDrawBlade`, `KGCharacter.cpp:1231`), sahte
  görev (`KGChoreComponent.cpp:57`), bot avı ve oyu (`KGBotController.cpp:725-948`), kazanma kontrolü
  (`KGGameMode.cpp:481-513`).
- Gece Defteri, şafak çözücüsü, vasiyet, Dedektif grupları yok.
- **Kâğıtta (≈3/5):** gerçek bir karşı-ağ var (İftiracı→Şerif, Temizlikçi→Adli Tabip, Çoban köpeği→Casus/Suret,
  Rahip→Vampir/Kurtadam, Meyhaneci↔Seri Katil, Barut Ustası→Bombacı). Ama:
  - 5 rol sadece menüde, dünyada iz yok: Şerif, Dedektif, Kâhin, Muhbir, Baştan Çıkarıcı.
  - İzole ya da pasif: Koleksiyoncu, Korsan (taş-kâğıt-makas), Emekli Asker, Hayatta Kalan.
  - Üç ayna üçlü: Meyhaneci/Baştan Çıkarıcı/Cadı (engelle/yönlendir), Koruma/Emekli Asker/Pusucu (ziyaretçiyi öldür),
    Şerif/Kâhin/Muhbir (menüden kesin bilgi).
  - Karşı roller hedefleriyle nadiren buluşuyor:

| Karşı rol | Hedefi maçta olma oranı |
|---|---|
| Çilingir | %9 |
| Adli Tabip | %10 |
| Meyhaneci | %12 |
| Avcı | %14 |
| Bekçi | %17 |
| Barut Ustası | %22 |
| Çoban | %28 |
| Rahip | %36 |

### 6. Kötü taraf — 1.5
**Var:**
- TF2 backstab (1.5 m, arkadan, aynı yöne bakarak). Bıçak çekilene kadar gizli, çekmek görünür.
- Yazılı takım kanalı.
- **Sahte görevler:** aynı panel, bara katkı yok, görsel etki yok (`KGChoreComponent.cpp:155`, `KGGameMode.cpp:784`).
  8 görsel görevle birlikte, bu oyunun şu anki en iyi iyi-kötü mekaniği: gerçek bir alibi/aldatma döngüsü.

**Yok:**
- Maske ve pelerin (GDD §5), ceset sürükleme/saklama, "Kayıp" duyurusu, sahte delil, sabotaj, kılık.
- Risk modeli: tanık yok, kalabalık bonusu yok, masumu öldürme cezası yok. 20 botlu soak'ta tek 240 sn'lik günde
  10 Sabırsız cinayeti (`v2_Build_Report.md` §7.3; anekdot, n = 1 gün); GDD "gündüz cinayeti çok riskli" diyor.
- **Yapısal kilit:** `BuildSlots` rastgele Saat Kırıcı yuvasını sadece ≥ 3 Saat Kırıcı varken, yani N ≥ 14'te açıyor.
  N < 14'te maçların **%0**'ında aldatma ya da delil rolü var. 14–20 arasında maç başına tam 1 tane.

### 7. İyi / nötr — 1
- Kasabanın proaktif araçları: görevler (Hazırlık barı), görsel görev alibisi, balık tutma alibisi, yumruk ve itme,
  suçla + oy. **Araştırma fiili yok.**
- Hazırlık barı dolunca fener **rastgele** bir Sabırsızı 10 sn kırmızı yakıyor (`KGGameMode.cpp:816`): çıkarım değil,
  piyango.
- Nötrlerin kodda hedefi yok: `CheckWinCondition` her nötrü "diğerleri" sayıyor, yani her maçta 1–3 oyuncu habersiz
  Kasaba. Asılan Soytarı hiçbir şey kazanmıyor.
- Yalnız katiller (Seri Katil, Kurtadam, Vampir) Saat Kırıcılarla aynı kovada: birbirini avlamıyor ve birlikte
  kazanıyor (`KGGameMode.cpp:42-48, 494-512`; `KGBotController.cpp:759, 908`). Sohbet ise takımı tarafa göre ayırıyor
  (`KGChatRules.cpp:87-89`): tutarsız.
- Kâğıtta nötrler ≈2.5/5: Soytarı, Cellat, Pozzo güçlü; Cadı (Sabırsızla kazanır), Hafızasız, Hayatta Kalan "zayıf
  kopya"; Koleksiyoncu ve Korsan izole. Benign/Chaos nötrler sadece N ≥ 12'de çıkıyor.

### 8. Ölüm — 1
- `BecomeGhost` serbest uçan izleyiciye geçiriyor (`KGCharacter.cpp:1426`); ceset dünyada kalıyor.
- Ölü sohbeti doğru kapsamlı: hayalet sadece Ölüler kanalına yazar, epilogda herkese (`KGChatRules.cpp:104-236`).
  Hayaletten canlıya ücretsiz kanal yok: bu doğru.
- Hayalet görevleri, poltergeist, ruh dünyası, Medyum, Araf Düellosu/Hortlak (GDD §11): hiçbiri kodda yok.
- Hayaletler her şeyi görebiliyor (serbest kamera): dışarıdan sızdırma riski (G7.3).
- Ufak kural ihlali: hayalete geçiş `TimerManager` ile 4 sn (`KGCharacter.cpp:1296`); oynanış süresi
  `FKGMatchClock`'tan gelmeli.

### 9. Toplantılar — 1.5
**Var:** prosedür uçtan uca çalışıyor (ITER-009: insan Saat Ustası botlarca suçlandı, yargılandı, asıldı). Suçla
[V], yarı + 1 çoğunlukla mahkeme, savunma 20 / karar 15 / son söz 8 sn, sohbet aşamaları, tellal satırları, HUD'da
sayım.

**Yok:**
- İçerik: Vaka Dosyası yok, delil nesnesi yok, alıntılanacak defter satırı yok, rol sonucu yok, ses yok, bot konuşması
  yok. Toplantı konuşmasının tek örneği `kg.Chat.Demo`'daki sabit sahte metin (`KGChatSubsystem.cpp:641-647`).
- Tetik: 2. günden itibaren her gün, bir şey olsun olmasın, sabit toplantı (`KGGameMode.cpp:316-338`).
- Güvenlik: `ServerAttack`'ta faz kontrolü yok (`KGCharacter.cpp:1080-1116`). MatchSmoke.log: 3. gün toplantısında
  bir köylü halkanın içinde bıçaklandı; epilog başladıktan sonra da bir ölüm var.
- Ölçüm: saat x30'da 63 sn'lik toplantı 2.1 gerçek saniye sürüyor, bot oy gecikmesi ise ölçeklenmemiş 4–14 gerçek
  saniye (`KGBotController.cpp:885`). Smoke: 7 toplantı, 0 yargılama, 0 asma; I4 yine de geçiyor. **Bu 0 bir saat hızı
  artefaktıdır** (botlar oy veremeden faz bitiyor), toplantı tasarımının ölçümü değil.

### 10. Tempo — 1
- Faz süreleri N'ye göre ölçekleniyor ama `GetPhaseDuration(…, GetNumPlayers())` botları saymıyor
  (`KGGameMode.cpp:61, 372`): 12 botlu maç 6 kişilik sürelerle oynanıyor (Gündüz 186, Toplantı 63, Gece 93 sn).
- Tırmanma mekanizması yok: 1. gün ile 5. gün aynı. Olay yok (bu "rastgele gürültü yok" açısından iyi, "değişen durum
  yok" açısından kötü).
- Smoke maçı 8 gün sürdü ve 3'e 3 yıpratmayla bitti; tek bir karar belirleyici olmadı (anekdot, x30, oylar çalışmadığı
  için gün sayısı da şişik).
- Gün süresi hesaplanabilir (kod gerçeği): 282 + 12N sn. N = 20'de gün 8.7 dk; Vizyon'un 20–35 dk'sı N = 20'de en fazla
  4 gün demek (charter S10.1).
- Koruma süresi yok, gündüz cinayet tavanı yok.

---

## Koruyucu kurallar

### G1 Ses — 1
`Voice/` sadece `KGMouthComponent` içeriyor. Ses bağlı mekanikler bloke: Kulak Misafiri'nin bozuk telsizi, Medyum
seansı, Gardiyan hücre kanalı, Şantajcı susturması (`kg.Chat.Silence` kancası var, `KGChatComponent.h:103`, ama
Şantajcı yok). Yazı kanalları faz/hayatta/takım kurallarıyla sunucuda, spam korumalı: iyi bir temel.

### G2 Ölçek — 1.5
Rol üreticisi 6–20 arası güç bütçesiyle ölçekleniyor (600 listede 0 bant dışı). Ama: Bölge Kapıları yok, faz süreleri
botları saymıyor, aldatma rolleri N < 14'te %0, Benign/Chaos nötrler N < 12'de yok.

### G3 Okunabilirlik — 1
Rol kartı ve HUD var. 51 rol için başlangıç havuzu ya da alıştırma yok. SPRINT-015'in açılış töreni "1–2 yetenek
satırı" gösterecek, ama o yeteneklerin hiçbiri yok.

### G4 Kontrollü varyans — 2
İyi: `FKGRng` tohumlu, maç tohumu tekrar üretilebilir, rol listesi tohumdan. Kötü: fener aydınlanması rastgele bir
Sabırsız seçiyor (`KGGameMode.cpp:816`); bot oyları `Hash01` ile (%25 rastgele suçlama, %60 suçlu yazı-tura,
`KGBotController.cpp:38-47, 928-946`). Tasarımda: Korsan taş-kâğıt-makas, Bombacı %50 kablo, GDD §9 faz başına %35
olasılıklı olay (önceden bildirim yok).

### G5 Anti-snowball ve frustrasyon — 0.5
Koruma süresi yok, gündüz risk modeli yok, masumu öldürme cezası yok, AFK tespiti kodda yok (GDD §14'te var). Toplantı,
mahkeme ve epilogda öldürme mümkün. Tasarımda Şantajcı'nın tam susturması ses odaklı oyunda frustrasyon riski.

### G6 Botlar — 0.5
- Algı, hafıza, şüphe yok. Toplantı: 4–14 sn rastgele gecikme, en çok suçlanana katıl ya da %25 hash-rastgele aday.
- Sabırsız botlar başkalarının gizli `PrivateRoleId`'sini sunucuda okuyarak takım arkadaşını korumuyor, kusursuz
  koruyor: bir insan bunu ele verme olarak öğrenebilir.
- Sabırsız botlar her fazda, tanık kontrolü olmadan, sırtı dönük herkesi bıçaklıyor (`KGBotController.cpp:725-775`).
- Botlar konuşmuyor, rapor etmiyor, iddia etmiyor. Resmî olarak "dev tooling" (`KGBotController.cpp:38`), ama
  `kg.BotFill` ve lobi doldurma onları gerçek maçlara koyuyor.
- Soak ölçümleri sadece hareket (bölge, takılma, toplanma); sosyal metrik yok.

### G7 Adalet ve anti-cheat — 2 (tavan; tavansız 3)
İyi: `PrivateRoleId` `COND_OwnerOnly` (`KGPlayerState.cpp:22`); görev durumu, görev listesi ve aktif görev sahibe özel;
saldırı, itme, bıçak, görev sunucu RPC'si; görev tamamlama sunucuda doğrulanıyor (aşama sırası, oturum jetonu, ölçülen
süre alt sınırı); sohbet kanalları sunucuda filtreleniyor. Eksik: bölge tabanlı relevancy (M3), hayalet görüş
kuralları, bot kodunun gizli durumu okuması (bugün sadece dev botlarda).

---

## Bugünkü ölçümler

### Başlangıç çizgisi (geçerli: [L], [T], kod)
| Metrik | Bugün | Charter hedefi | Kaynak |
|---|---|---|---|
| N < 14'te aldatma rolü olan maç | %0 | %100 (temel kit) | Üretici simülasyonu [L] |
| Karşı rolün hedefiyle buluşması | %9–36 | ≥ %50 | Üretici simülasyonu [L] |
| Hizalanma dışı yeteneği kodda olan rol (S5.1 "uygulanan") | 0/51 (her sistem rolü sadece taraf olarak okuyor) | 8/51 (M5), sonra hepsi | Kod |
| Bot oy gerekçesi | %0 (hash) | Çekimser olmayanların ≥ %90'ı | Kod |
| 12 botlu maçta toplantı süresi | 63 sn (6 kişilik formül: bot sayılmıyor) | 81 sn (vakasızsa ≤ 30) | Kod (`GetPhaseDuration`) |
| Gün süresi | 282 + 12N sn (N = 6: 5.9 dk, N = 12: 7.1, N = 20: 8.7) | S10.1: maç 20–35 dk | Kod |
| Karar türü (S1.4) | 3 / 8 (nereye, güven, risk) | Medyan ≥ 4 | Kod |

### Anekdot (n = 1, geçerli [B] değil; sadece yön)
| Metrik | Bugün | Charter hedefi | Kaynak |
|---|---|---|---|
| Maç başına yargılama | 0 (7 toplantıda; x30 artefaktı) | Vakalı toplantıların ≥ %60'ı | MatchSmoke.log (x30, 12 bot, tohum 7) |
| Toplantı/Mahkeme/Epilog'da ölüm | 2 (1 maçta) | 0 | MatchSmoke.log (x30). Mekanizma gerçeği: `ServerAttack`'ta faz kontrolü yok (kod). |
| 240 sn'lik günde Sabırsız gündüz cinayeti (20 bot) | 10 | Sabırsız başına ≤ 1 | v2_Build_Report §7.3 (tek gün) |
| Toplanma p90 / en fazla | 32.9 / 33.4 sn | ≤ 30 / ≤ 35 sn | v2_Build_Report §7.3 (20 varış; n_min 100) |
| Maç uzunluğu | 8 gün, yıpratma | N = 12'de 3–5 gün (20–35 dk), ≥ %40 asma/yetenekle biter | MatchSmoke.log (x30) |
| Kasaba / Sabırsız kazanma oranı | Ölçülmüyor | [O] %38–62 / %25–50 (~150 maç) | — |

---

## Kopuk sistemler

Her satır bir sistemin ötekiyle konuşmadığı bir yerdir. Çoğu Olay Defteri ile kapanır.

| # | Sistem A | Sistem B | Kopukluk |
|---|---|---|---|
| 1 | Rol kataloğu | Diğer her şey | `Abilities` hiç doldurulmuyor; her sistem sadece "Sabırsız mı" diye okuyor. Güç bütçesi var olmayan yetenekleri dengeliyor. |
| 2 | Cesetler | Delil / toplantı | Ceset dünyada kalıyor ama hiçbir şey onu okumuyor; ölüm duyurusu adı ve rolü zaten veriyor. |
| 3 | Harita | Roller | v2 §8 gözcü noktaları, saklanma yerleri ve risk eğimi tasarladı; hiçbir yetenek kullanmıyor. |
| 4 | Evler | Gece | Herkesin `HouseIndex`'i ve kendine kilitlenen sandığı var; sokağa çıkma yasağı, gece ziyareti, kapı kırma, barikat, kutsama yok. |
| 5 | Balık tutma | Maç | Kasten bara ve rollere bağlanmadı (GDD §15). Tek bağı alibi, o da kayda düşmüyor. |
| 6 | Ekonomi | Roller | Para, altın, balık satışı, kazı ganimeti var; eşya kataloğunda rol aleti (kilit, fener, bandaj, kutsal su, yağ, tuzak, tabanca) yok. |
| 7 | Sohbet | Roller / oy | Şantajcı kancası var, Şantajcı yok. "Suçla" emote'u [V] suçlama oyundan ayrı. |
| 8 | Hazırlık barı | Kötü taraf | Fener, kimin hangi görevi yaptığından bağımsız rastgele bir Sabırsızı yakıyor; barı yarışmak için sabotaj yok. |
| 9 | Görsel görev etkileri | Botlar / kayıt | Tut-E yolu `AuthTrigger` çağırmıyor; etkiler kime ait olduğu belli olmadan, kaydedilmeden geçiyor. |
| 10 | Faz süresi | Lobi büyüklüğü | `GetNumPlayers()` botları saymıyor. |
| 11 | Bot zamanlayıcıları | Dev saat hızı | Oy gecikmeleri `DevClockScale` ile ölçeklenmiyor; smoke hiç oylamıyor. |
| 12 | Kazanma kontrolü | Taraflar | İki kova; yalnız katiller ve nötrler yanlış çözülüyor. Sohbet ise tarafa göre ayırıyor. |
| 13 | SPRINT-015 rol açılışı | Yetenekler | "Hedef + 1–2 yetenek satırı" gösterecek; yetenekler yok. |
| 14 | SPRINT-015 yayıncı modu | İsim etiketi | "İsim etiketlerinde" takma ad istiyor; oyunda isim etiketi yok. |
| 15 | Kazı / açık mezar | Toplantı | Mezar "şüpheli" ve 40 m'den duyuluyor, ama kimse bunu kaydedemiyor ya da toplantıya getiremiyor. |
| 16 | Test kapsamı | Çekirdek döngü | 2 rol testi (katalog, üretici), kazanma/toplantı/mahkeme/gece için 0 test; balık 5, emote 7, sohbet 7 test. |

## Güçlü yanlar (korunacak)
1. **Sahte görev + görsel görev:** hazır bir alibi/aldatma döngüsü. Olay Defteri'ne bağlanınca charter'ın model mekaniği olur.
2. **Harita v2:** risk eğimi, halkalar, ≤ 35 sn toplanma, bölgelere yayılmış görevler.
3. **Teknik disiplin:** `FKGRng`, `FKGMatchClock`, `COND_OwnerOnly`, sunucu doğrulaması, başsız smoke'lar. Olay Defteri
   aynı kalıpla (saf mantık, SaveGame, tohumlu) kurulabilir.
4. **Tasarım belgeleri:** 51 rol, GDD §4–§11: charter'ın istediği neredeyse her şey kâğıtta var.
5. **SPRINT-016 fiziksel görevler:** taşıma rotaları kesişiyor, bu da tanık ve pusu demek. Charter'a en uyumlu sprint.
