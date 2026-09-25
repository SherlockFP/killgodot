# Sütun Yol Haritası — sınırlı sprint önerileri (2026-09-25, v1.1)

> **Her madde: Önerildi (kullanıcı onayı gerekiyor).** Hiçbiri Backlog'a taşınmadı, hiçbiri kuyruğu değiştirmedi.
> Onaylanan madde `Docs/Backlog.md` sprint kuyruğuna ve `Docs/Iterations/SPRINT-0xx-*.md` dosyasına kopyalanır.
> Neye göre: `KillGo_Pillars.md` v1.1 (hedef kodları S1.1 … G7.5, sabitler §0.3, kapı kuralları §0.2). Neden:
> `Pillar_Audit.md`.
> Sprint numaraları geçicidir (018 ve 019 kuyrukta dolu; öneriler 020'den başlar).
> **v1.1:** ölçülebilirlik ve maliyet incelemesi uygulandı. Büyük sprintler bölündü (020, 025, 026, 027, 031),
> 032 sprint listesinden çıktı, onaylanmamış yeniden tasarımlar Onay listesine (R1–R8) taşındı, kapılar denge
> bantlarından mekanizmalara çevrildi. Yeni özellik eklenmedi.

## Kurallar (LoopContract'tan ve charter §0.2'den)
- Her sprint tek hedef, 2–6 sabit kabul kriteri, açık yazılabilir kapsam. Kriterler sprint ortasında değişmez.
- Hata başına en fazla 3 düzeltme denemesi; görsel için 2 bakış turu; iki turda ölçülebilir ilerleme yoksa dur.
- `run_invariants.ps1` önce ve sonra geçer. Bozan sprint geri alınır.
- En fazla 3 yazan ajan aynı anda çalışır. Paralel ajanlar aynı dosyaya yazmaz. `KGGameMode.cpp`, `KGCharacter.cpp`,
  `KGHUD.cpp`, `KGBotController.cpp` ve `Roles/KGRoleDefinition.*` birçok sprintin ortak dosyasıdır; bu yüzden
  aşağıdaki sıra **seridir**.
- **Doğrulama önce gelir.** Doğrulama borcu 11 madde (> 10). LoopContract'a göre doğrulama oturumu yeni sprintlerden,
  020a dahil, **önce** gelir. Bu bir seçenek değil, sözleşmenin kuralıdır.
- **Kapılar mekanizmadır, denge değil.** Bir sprintin kabulü şunlardan oluşur: [T] testleri, [U] bot uyum kontrolleri
  (yol çalışıyor ve logluyor; yüzde bandı yok) ve [R] render ölçümleri. [B] tasarım bantları kabul soak'unda
  **sadece rapor** olarak yazılır; bir [B] bandı ancak §0.2 kural 1 (n_min, %95 aralığı) ve kural 2 (bot
  parametreleri dondurulmuş ve soak başlığında) sağlanınca kapı olabilir. Denge bantları (S9.4, S7.2, S6.5, S10.1)
  en erken 028'den sonra, kullanıcı onayıyla kapıya döner.
- **Kabul soak'u küçüktür:** N = 8 ve 20, 2 sabit tohum, gün 1–3, toplam ≤ ~45 dk; x1 ya da global zaman genişlemesi
  (hareket de ölçeklenir). Tüm saniye eşikleri **oyun saati** (`FKGMatchClock`) saniyesidir. Büyük tohum × N
  matrisleri `-Nightly` modunda koşar, sadece rapor yazar, hiçbir sprinti bekletmez. Saf doğruluk sayımları
  mantık-seviyesi simülasyonla ya da (020a'dan sonra) x30'da yapılır.
- **Mesafeler sabit adıyla yazılır** (`KG_IDENT_RANGE`, `KG_HEAR_KILL`…; charter §0.3). Kabul satırı sayı tekrar etmez.
- **Dünyaya nesne koyan sprintler** (pano, masalar, hapishane, sis duvarı) nesneyi **çalışma zamanında layout JSON
  noktasından** spawn eder; v2 inşa hattına (`kg_build_village_v2.py`, `verify_v2_build.py` = I2) dokunmaz. Hatta
  dokunmak gerekiyorsa inşa kancası, I2 kontrolü ve yeniden inşa (~8 dk) kapsama yazılır ve v2 inşa sahibiyle sıra
  ayarlanır.
- **Başlık değişiklikleri** (USTRUCT/UPROPERTY alanları) sprint başına tek bir toplu editör-kapanış döngüsünde yapılır.

## Sıra ve gerekçe

| Sıra | Sprint | Maliyet | Ana sütun | Neden bu sırada |
|---|---|---|---|---|
| 0 | Doğrulama oturumu (kuyrukta) | — | — | Borç > 10; sözleşme gereği önce |
| 1 | **SPRINT-020a** Smoke gerçekten oylar: faz kontrolü, bot sayan faz süresi, ölçeklenen bot zamanlayıcıları | S | S9, S10, G6 | Minik; sonraki her ölçümün önkoşulu |
| 2 | **SPRINT-020b** Olay Defteri + tanık filtresi + ölçüm koşumu | L | S2, G7 | Her şey bundan okur |
| 3 | SPRINT-021 Ceset raporu → toplanma → acil toplantı + Vaka Dosyası | L | S9, S1, S2 | "Raporla" kararı; faz makinesi değişir |
| 4 | SPRINT-022 Tarafa sadık kazanma çözücüsü + ilk nötr hedefleri | M | S7, S5 | Kazanma oranı ölçümleri bunsuz yanlış |
| 5 | SPRINT-023 Bot zihni v1 + I5 (mekanizma kapısı) | M | G6, S9 | Ölçüm aracı |
| 6 | SPRINT-024 Defter (J), iddialar, Kasaba Panosu, isim etiketi | L | S2, S9, G1 | Toplantıya delil |
| 7 | SPRINT-025a Maske, koruma süresi, kalabalık bonusu | M | S6, G5, S10 | Aldatma kiti, 1. kısım |
| 8 | SPRINT-025b Ceset taşıma/saklama + "Kayıp" | M | S6, S3 | Yeraltı sprintine (mahzen) bağlı |
| 9 | SPRINT-026a Fiziksel izler | M | S3 | Delil yaşam döngüsü |
| 10 | SPRINT-026b Sabotaj → onarım (016'nın üstüne) | S | S4, S6 | 016'nın sabotajını deftere bağlar |
| 11 | SPRINT-027a Gece Çözücü + Gece Defteri + Şerif + Doktor | L | S5, S1 | Rol sistemi başlar |
| 12 | SPRINT-027b Saat Ustası + Tetikçi + vasiyet | M | S5, S6 | Kötü takımın gecesi |
| 13 | SPRINT-028 Bekçi, Gardiyan, Casus + sokağa çıkma yasağı | L | S5, S6, S4 | M5 çekirdek sekizi tamamlanır |
| 14 | SPRINT-029 Rol etkileşim lint'i + başlangıç havuzu (**seri**) | S | S5, G3 | 027a ve 015 ile ortak dosya |
| 15 | SPRINT-030 Ölüm oynanışı v1 | M | S8, G7 | Erken ölenlerin maçı |
| 16 | SPRINT-031a Bölge Kapıları | M | G2, S4 | Küçük lobiler |
| 17 | SPRINT-031b AFK (+ R2 onaylanırsa fener) | S | G5, G4 | Küçük, bağımsız |
| — | ~~SPRINT-032~~ Yakınlık sesi | — | G1 | **Sprint değil: engelli.** Kullanıcı EOS ürününü oluşturur (`DefaultEngine.ini` o zamana kadar null online), sonra M3/M4 (`09_Roadmap_Gauntlet.md`). |

**M5 ile ilişki:** 020a–028 birlikte `09_Roadmap_Gauntlet.md`'deki M5'i (faz makinesi, 8 çekirdek rol, toplantı,
oylama, mahkeme, asma, kazanma, hayaletler, vasiyet) charter'ın merceğiyle tamamlar. M5'in sonundaki "arkadaşlarla ilk
gerçek test", [O] hedeflerinin ilk ölçüm noktasıdır (planlanan hacim ~150 maç).

---

## SPRINT-020a — Smoke gerçekten oylar (S)

**Hedef (kullanıcının göreceği):** toplantıda, mahkemede ve epilogda kimse ölmez. 12 botlu maç 12 kişilik sürelerle
oynanır. x30 smoke ilk kez gerçekten yargılar.

**Charter kodları:** S9.1, S10.7, G6.5, G7.2 (ilk red testi).

**Kullanıcıya görünen değişiklikler:** botlu maçlarda fazlar uzar (12 botla gündüz 186 → 222 sn, toplantı 63 → 81,
gece 93 → 111); toplantıda saldırı reddedilir.

### Kabul (sabit)
1. **Test:** `ServerAttack` Meeting, Trial ve Epilogue fazlarında reddedilir.
2. **Test:** faz süresi botlar dahil oyuncu sayısıyla hesaplanır (12 bot → Gündüz 222 sn).
3. **Test:** bot karar zamanlayıcıları `DevClockScale` ile ölçeklenir.
4. **Smoke:** `kg_match_smoke.ps1` (x30) ≥ 1 Trial ve 0 Toplantı/Mahkeme/Epilog ölümü görür. Bu kontrol **I4b** olarak
   eklenir ve bu sprint birleşene kadar sadece rapordur; birleşince I4'ün yerine geçer (paralel sprintler kırmızıya
   dönmesin diye).
5. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Character/KGCharacter.cpp` (sadece `ServerAttack` faz kontrolü), `Core/KGGameMode.cpp` (sadece `SetPhase` satırındaki
`GetNumPlayers()` çağrısı), `AI/KGBotController.cpp` (sadece zamanlayıcı ölçekleme), `kg_match_smoke.ps1`,
`run_invariants.ps1`, testler.

### Kuyrukla ilişki
015 `KGGameMode.cpp`, 016 `KGCharacter.cpp` yazıyor. 020a onlardan sonra ya da onların son adımı olarak koşar.

---

## SPRINT-020b — Olay Defteri + tanık filtresi + ölçüm koşumu (L)

**Hedef:** oyunda olan her önemli şey (cinayet, bıçak çekme, kapı kırma, görev, kazı, balık, ceset algısı) tek bir
sunucu kaydına, kimin gördüğü/duyduğu ile birlikte düşer. Bir başsız koşu, defterden hesaplanabilen charter
hedeflerini sayılarla raporlar.

**Charter kodları:** S2.1, S2.2, G7.1. Ölçülen: S2.1, S2.2, S4.2, S4.3 (bölge payı kısmı; risk katmanı alanı 031a ile),
S6.2, S9.1, S9.2, S10.1, S10.4, S10.5.

**Kullanıcıya görünen değişiklikler:** botlar tut-E ile bitirdikleri görsel görevlerde artık çan çalar, duman çıkarır
(`AuthTrigger`). Başka ekran değişikliği yok.

### Kabul (sabit)
1. **Tanık testi** `KillGodot.Evidence.Witness`: senaryolu gündüz backstab'ı ve 5 gözlemci, mesafeler sabitlerden
   okunur: A `KG_IDENT_RANGE` − 2 m, LOS, bakıyor; B `KG_IDENT_RANGE` + 4 m, LOS, bakıyor; C `KG_HEAR_KILL` ×
   `KG_WALL_FACTOR` − 1 m, duvar arkası; D `KG_IDENT_RANGE` − 2 m, arkası dönük; E `KG_HEAR_KILL` + 5 m, arkası dönük.
   Beklenen: `bSaw && bIdentified` sadece A; `bSaw` A ve B; `bHeard` A, B, C, D; E için hepsi yanlış. Defter SaveGame
   arşiv turundan aynı olay ve tanık listeleriyle çıkar.
2. **Gizlilik:** iki süreçli pencereli olmayan smoke'ta (kg_chat_smoke kalıbı) istemci, tanık olmadığı 0 kayıt alır
   (G7.1).
3. **Yayıcılar tam:** 20 botlu x1 bir günde logdaki her `died (killer: …)` satırının bir Kill olayı vardır; Kasabalı
   botların tut-E ile bitirdiği her görsel görev `AKGChoreFx` olayı üretir (%100), Sabırsız sahteleri 0 üretir. Defter
   + tanık + ceset algısı maliyeti sunucuda ≤ 0.25 ms/kare, p95, N = 20'de bir gün boyunca (CPU; başsız ölçülebilir).
4. **Ölçüm koşumu:** `Tools/Gauntlet/kg_pillar_soak.ps1` kabul modu (N 8 ve 20, 2 tohum, gün 1–3, ≤ 45 dk)
   `Saved/KG_PillarSoak.json` yazar: yukarıdaki 10 metrik, her biri n, %95 aralığı ve geçer/kalır/sonuçsuz durumuyla;
   başlıkta bot parametreleri. `-Nightly` modu var, kapı değil. Sonuç `Pillar_Audit.md`'ye "başlangıç çizgisi v2 (bot
   sınırlı, 023'e kadar)" olarak eklenir.
5. `run_invariants.ps1` geçer; pillar soak I5 olarak **sadece rapor** modunda eklenir.

### Yazılabilir kapsam
- Yeni `Source/KillGodot/Evidence/`: `UKGEventLedger` (**sadece sunucu** world subsystem, replike olmaz), `FKGEvent`,
  saf `FKGWitnessFilter`, ceset algısı (canlı → ceset LOS kontrolü, sabit hızda, bütçe içinde).
- `Core/KGPlayerState.*`: oyuncunun tanık olduğu kayıtlar için `COND_OwnerOnly` FastArray.
- `Core/KGGameState.*`: defterin SaveGame durumu için `UKGSnapshotComponent` (bugün GameState'te yok).
- Kayıt anahtarı: `UI/Reveal/KGStreamerMode.h` `PlayerKey()` (PUID, yoksa net id, bot ise ad). "PUID" değil: oyun
  bugün null/LAN online'da, botların PUID'i yok.
- Tek satırlık yayıcı kancaları: `Character/KGCharacter.cpp` (ServerAttack, HandleDeath, ServerSetAssassinBlade, TickTask
  → AuthTrigger), `World/KGDoor.cpp`, `Dig/KGDigManager.cpp`, `Fishing/` (atış/çekiş), `Chores/KGChoreComponent.cpp`,
  `Chores/WorldChores/` (016'nın adım kancası).
- `Tools/Gauntlet/kg_pillar_soak.ps1` (yeni), `run_invariants.ps1`, `Private/Tests/KGEvidenceTests.cpp` (yeni),
  `Docs/05_Tech_Architecture.md` (yeni bölüm), `Docs/Design/Pillar_Audit.md`.

### Sınırlar
3 düzeltme denemesi / kontrol; plato durdurması. UI yok. Başlık değişiklikleri tek editör-kapanış döngüsünde.

### Ertelenen ölçümler
S1.1, S1.3 (karar fiillerinin çoğu henüz yok) → 024 ve 027a sonrası; S3.3 → 021; S4.4 (oyuncu çifti LOS'u) → 023.

### Kuyrukla ilişki
016 → 020b sıralı (ortak: `KGCharacter.cpp`, `Chores/`). Yeraltı sprintinin kazı dosyalarına 020b, o sprint bitince
dokunur.

---

## SPRINT-021 — Ceset bulunur, duyurulmaz: rapor → toplanma → acil toplantı + Vaka Dosyası (L)

**Hedef:** gündüz cesedi artık kendiliğinden duyurulmaz. Onu gören canlı oyuncu "Alarm ver" der, çan çalar, köylüler
meydana **yürür**, toplantı bir Vaka Dosyası ile açılır. Konuşacak bir şey yoksa akşam toplantısı kısa sürer.

**Charter kodları:** S2.5, S1.4 (raporla türü), S9.2, S9.3, S9.7, S4.5b. Ölçülen (rapor): S3.3, S3.4.

**Faz makinesi kuralları** (Onay R6'ya bağlı; kabul metninin parçası):
- Acil toplantı Gündüz'ün içinde bir **Toplanma** alt durumuyla başlar (`KG_GATHER_WINDOW`). Acil toplantıda ışınlama
  yoktur; `StartMeeting`'in bugünkü halka ışınlaması sadece akşam toplantısında kalır.
- Toplanma + toplantı (+ varsa yargılama) boyunca Gündüz saati durur. Sonra Gündüz **kalan süresiyle** sürer.
  `GetNextPhase`: acil Meeting → Day (devam); akşam Meeting → Night (bugünkü gibi).
- 1. günde rapor ve acil toplantı serbesttir (1. günün akşam toplantısı yine yoktur).
- Tavan: çan/acil toplantı oyuncu başına günde 1, toplam günde 2. Tavanı aşan raporlar bir sonraki toplantının Vaka
  Dosyası'na eklenir; hiçbir ceset raporlanamaz hâle gelmez.
- Acil toplantıdan sonra yeni vaka açılmadıysa akşam toplantısı vakasızdır (≤ 30 sn).

### Kabul (sabit)
1. **Test:** gündüz cinayeti `GS->Announcement`'ı boş bırakır; ilk canlı algılayan (defterde BodySeen) `bCanReport`
   alır; rapor 2 sn içinde Toplanma'ya geçer; `CaseFile {Victim, RegionId, Reporter, TimeBucket, WoundClass}` dolu,
   `RevealedRoleId` karar ya da şafak öncesi boş.
2. **Test:** Toplanma `KG_GATHER_WINDOW` sürer; pencere sonunda halkada olmayan Geç Kalan olur ve o tur oy veremez;
   toplantıdan sonra Gündüz kalan süresiyle (± 1 sn) sürer; aynı gün 3. rapor çan çalmaz, akşam Vaka Dosyası'na eklenir.
3. **[U] Soak** (kabul modu): Kasabalı botun algıladığı her ceset için log `KG_CASE report_s=…` ya da
   `KG_NOREPORT reason=…` yazar. Yüzde bandı yok; bot refleks parametreleri soak başlığında. S3.3, S3.4 ve S4.5b
   raporda yazılır.
4. **Test:** o gün vaka açılmadıysa akşam toplantısı ≤ 30 sn; vaka varsa 45 + 3N sn.
5. Ekran dışı UIShot: Vaka Dosyası kartıyla toplantı HUD'u. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Core/KGGameMode.cpp` (`OnCharacterDied`, `GetNextPhase`, `StartMeeting`, Toplanma alt durumu, `AnnounceDawn`: isim +
bölge), `Core/KGGameState.*` (CaseFile, durdurulan gündüz saati), `Core/KGPlayerState.*` (Geç Kalan), yeni
`Core/KGCaseFile.h`, `Character/KGCharacter.cpp` (Rapor etkileşimi), `UI/KGHUD.cpp` (sadece DrawMeeting vaka kartı),
`AI/KGBotController.cpp` (rapor refleksi, meydana yürüme), `Chat/KGChatSubsystem.cpp` (tellal satırı), testler,
`01_GDD_Core.md` §1 (20 sn → `KG_GATHER_WINDOW`, R6 onaylanırsa), `08_UI_UX.md`.

### Sınırlar
3 deneme / kontrol; UI için 2 bakış turu. UPROPERTY değişiklikleri tek bir toplu editör-kapanış döngüsünde.

### Kapsam dışı → öneri
Ceset saklama ("Kayıp") → 025b. Yara sınıfını okuyan Adli Tabip → rol sprintleri.

---

## SPRINT-022 — Tarafa sadık kazanma çözücüsü + Soytarı, Cellat, Hayatta Kalan (M)

**Hedef:** her taraf kendi koşuluyla kazanır. Yalnız katil, Saat Kırıcılarla birlikte kazanamaz. Asılan Soytarı
kazanır ve maç sürer. Epilog birden çok kazanan grubu gösterir.

**Charter kodları:** S7.3, S7.5, S5.1 (kısmen), S6.5/S7.2'nin ölçülebilmesi.

### Kabul (sabit)
1. **Test** `KillGodot.Core.WinResolver`: ≥ 15 tablo vakası geçer (son iki kişi Seri Katil ve Saat Kırıcı; 2. gün asılan
   Soytarı; hedefi gece öldürülen Cellat → Soytarı olur; Kasaba ile birlikte kazanan Hayatta Kalan; herkes ölü →
   berabere).
2. **Mantık simülasyonu** (maç koşmadan, saf çözücü): 10.000 rastgele bitiş durumunda Saat Kırıcıları ve bir yalnız
   katili birlikte kazanan listeleyen sonuç 0; mevcut her nötr için bir sonuç var. (x30 gece koşusu `KG_NEUTRAL`
   satırlarını sadece raporlar.)
3. **Sabit tohumlu senaryo:** geliştirici rol zorlama seçeneğiyle (bu sprintte eklenir, sadece dev) zorlanmış Soytarı
   2. gün asılır → log `KG_WIN Fool` yazar ve faz akışı Gece'ye devam eder.
4. Bot av ve oy filtreleri tarafa göre çalışır ve **sadece kendi rolünü ve kendi takım listesini** okur (G6.1):
   Saat Kırıcı bot takım arkadaşlarını bilir; yalnız katil bot kendisi dışındaki herkesi hedef sayar; başkalarının
   rolü okunmaz. Fener havuzu (sunucu) tarafa göre.
5. Epilog UIShot'u iki kazanan grup gösterir (ör. Kasaba + Hayatta Kalan). `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `Core/KGWinResolver.*` (saf, UObject'siz, `FKGRoleListGenerator` kalıbı, durumu SaveGame), `Core/KGGameMode.cpp`
(sadece CheckWinCondition'ı çözücüye yönlendirme, Cellat hedef ataması, `AssignRoles`'ta dev rol zorlama),
`Core/KGGameState.*` (`Winner` tek `EKGAlignment` yerine replike kazanan grup dizisi: başlık değişikliği, editör-kapanış
döngüsü), `Core/KGPlayerState.*` (sahibe özel Cellat hedefi), `AI/KGBotController.cpp` (sadece taraf filtreleri),
`UI/KGHUD.cpp` (sadece DrawEpilogue), testler, `02_Roles.md`.

### Sınırlar
3 deneme / kontrol. Yeni rol yeteneği yok; sadece kazanma koşulları.

---

## SPRINT-023 — Bot zihni v1 + dedüksiyon invariantı I5 (M)

**Hedef:** botlar sadece kendi gördükleri ve duyduklarıyla şüphelenir, gerekçeli suçlar ve oy verir. Sabırsız botlar
tanık varken öldürmekten kaçınır. I5, çıkarımın **mekanizmasının** çalıştığını kapı olarak doğrular.

**Charter kodları:** G6.1, G6.2, G6.3, G6.4 (→ S2.2), S9.6. Ölçülen (rapor): S2.7, S4.4, S6.2, S6.3, S9.4, S7.2 [B].

### Kabul (sabit)
1. **Test:** saf `FKGBotMind` (tohumlu `FKGRng`) ≥ 12 tablo vakası geçer. **G6.1 grep kontrolü** `run_invariants`'a
   eklenir: `Source/KillGodot/AI/` altında `KGBotView.cpp` dışında `PrivateRoleId`, `AlignmentOf`, özel defter ve gece
   seçimi erişimcilerine 0 referans. `FKGBotView` sadece algılanan olayları ve kendi takımının verisini açar.
2. **[U]** Bot suçlama ve oylarının %100'ü `KG_VOTE bot=… target=… reason=<eventId|abstain>` yazar; `reason=random` 0.
3. **I5 kapısı (mekanizma, §0.2 kural 1 ile):** (a) madde 2; (b) sonraki toplantıya canlı ulaşan tanımlı tanık bot,
   o toplantıda katili ≥ %60 suçlar (n_min 40, altı sonuçsuz); (c) Sabırsız bot cinayetlerinde tanımlı tanık oranı S2.2
   üst sınırını (%20) geçmez (n_min 200; kabul soak'unda genellikle sonuçsuz, gece koşusunda karar verir). Sonuçsuz I5'i
   kırmaz.
4. **Rapor (kapı değil, 028'e kadar):** asma doğruluğu, Kasaba kazanma oranı, maç süresi, vakalı toplantının yargılamaya
   ulaşma oranı, Sabırsız başına gündüz cinayeti. Gece koşusu: 20 bot × 10 tohum.
5. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `AI/KGBotMind.*`, `AI/KGBotView.*`, `AI/KGBotController.cpp`, `Tools/Gauntlet/kg_pillar_soak.ps1`,
`run_invariants.ps1`, testler, `Docs/Design/Pillar_Audit.md` (başlangıç çizgisi v3).

### Sınırlar
3 deneme / kontrol. **Plato kuralı özellikle önemli:** ağırlık ayarı iki turda metrikleri iyileştirmezse dur ve açık
bırak. Ağırlıklar koşudan önce dondurulur ve soak başlığına yazılır. Sohbet satırları bu sprintte yok (iddialar 024'te).

---

## SPRINT-024 — Defter (J), yapılandırılmış iddialar, Kasaba Panosu, bakınca isim etiketi (L)

**Hedef:** oyuncu `J` ile tanık olduğu olayları bölge adı ve faz saatiyle okur. Toplantıda vakayla ilgili satırı tek
tuşla "iddia" olarak paylaşır. Çeşmedeki Kasaba Panosu günün herkese açık olaylarını (çan, fener, duman…) isimsiz
listeler. Yakındaki birine bakınca adı görünür.

**Charter kodları:** S2.3, S9.5, G1.1, G3.5, G6.6, G7.5, S2 kural 4 (kimlik).

### Kabul (sabit)
1. **Test:** sahibin tanık olduğu 3 olay → doğru `AKGMapInfo` bölge adları ve saatleriyle 3 defter satırı; tanık
   olmadığı 2 olay → 0 satır. Başka istemciler senin satırlarından 0 alır; paylaşılan iddia `ClaimRef` taşıyan bir
   sohbet satırı olarak gelir.
2. **Test:** Kasaba Panosu günün olaylarını saat sırasıyla doğru bölge adlarıyla listeler ve şafakta sıfırlanır.
   Sunucu her çelişkiyi (charter §0.4) `KG_CONTRA` olarak loglar (S9.5 ölçümü). HUD'da "Pano ile çelişiyor" işareti
   **sadece Onay R3** kabul edilirse eklenir; yoksa oyuncular panoyu kendileri karşılaştırır.
3. **Soak:** 2. gün toplantısına kadar canlı botların p10'u ≥ 3 defter satırına sahip ([B], bot gezinme parametreleri
   dondurulmuş). **[U]** toplantı başına toplam bot iddiası: vakalı toplantıda 2 ile min(8, canlı/2) arası, vakasızda
   0–2. S9.5 (medyan iddia, çelişki oranı) raporda.
4. **Test:** isim etiketi `KG_IDENT_RANGE` − 1 m'de LOS ile görünür, + 1 m'de ve maskeliyken yok. `KG_Cap_nameplate.png`
   görsel kontrol içindir. Ad, 015'in yazdığı takma ad fonksiyonundan gelir (`UI/Reveal/KGStreamerMode.h`, G7.5).
5. UIShot'lar: ≥ 5 satırlı defter; vaka + 3 ilgili satırlı toplantı HUD'u; ≥ 5 girişli Kasaba Panosu.
   `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `UI/Journal/`, `Evidence/` (iddia türleri), `Chat/` (iddia mesaj türü), yeni `World/KGTownBoard.*` (çalışma
zamanında layout JSON'daki pano noktasından spawn; inşa hattı değişmez), `UI/KGHUD.cpp` (isim etiketi, toplantı şeridi),
`AI/` (bot iddiaları), testler, `08_UI_UX.md`, layout JSON'da pano noktası (sadece veri).

### Sınırlar
3 deneme / kontrol; her ekran için 2 bakış turu.

### Kapsam dışı → öneri
Vasiyet yazma masası (027b ile gelir), elle notlar v2, sesli iddia.

---

## SPRINT-025a — Maske, koruma süresi, kalabalık bonusu (M)

**Hedef:** her lobi büyüklüğünde her Sabırsız maske takabilir (takarken görülmek risk). Gündüz cinayeti gerçekten
riskli olur: 1. gündüzün ilk 90 sn'si koruma, kalabalık bonusu.

**Charter kodları:** S6.1, S6.4, S2.2, S10.2.

**Önkoşul:** sprintin ilk adımı, mevcut CC0 paketlerinde maske/pelerin asset'i olup olmadığını doğrulamaktır. Yoksa
sprint durur ve kullanıcıya sorar (indirme yok).

### Kabul (sabit)
1. **Test:** maske 1 sn'de takılır; takılıyken isim etiketi gizli ve `bIdentified` yanlış; takarken görülmek, tanık
   filtresinden geçen tanıklarla bir `MaskOn` olayı üretir.
2. **Test:** 1. gündüzün ilk 90 sn'sinde bıçak çekilemez; Sabırsıza vuran her ek kasabalı +%25 hasar (GDD §4).
3. **[U]** Sabırsız botların maske yolu çalışır ve `KG_MASK` loglar. S6.1 ve S2.2 raporda.
4. Alacakaranlıkta köylü gözünden ekran görüntüsü: isim etiketsiz maskeli figür. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `Character/KGMaskComponent.*`, `Character/KGCharacter.cpp` (bıçak koruma süresi), `Combat/KGHealthComponent.*`
(kalabalık bonusu), `AI/KGBotController.cpp` (maske), mevcut CC0 paketlerinden maske/pelerin asset yolları, testler,
`01_GDD_Core.md` §5.

### Sınırlar
3 deneme / kontrol; maske görünümü için 2 bakış turu. Ses bozma (maskeli ses) ses işine kalır.

---

## SPRINT-025b — Ceset taşıma ve saklama, "Kayıp" (M)

**Hedef:** Sabırsız cesedi taşıyıp kuyuya, denize ya da mahzene saklayabilir; şafakta "Kayıp" duyurulur, rol açıklanmaz.

**Charter kodları:** S6.1, S3.1, S3.4.

**Bağımlılık:** mahzen, çalışan yeraltı sprintine bağlı. O bitmediyse sprint kuyu ve denizle başlar, mahzen sonra
eklenir.

### Kabul (sabit)
1. **Test:** ceset bugün fizik objesi değil (ölü `AKGCharacter`; `HandleDeath` klip/ragdoll ve kapsül çarpışması kapalı,
   `BecomeGhost` sadece possess'i bırakır). Ölü karakter açık bir **taşınma moduna** geçer: sunucuda taşıyana bağlanır,
   çarpışma kapalı, sürükleme izi olayı yazılır; bırakınca yere düşer.
2. **Test:** kuyuya/denize/mahzene bırakılan ceset şafak duyurusunda "Kayıp" olur (rol yok, rapor yok).
3. **[U]** Sabırsız botların saklama yolu çalışır ve `KG_HIDE` loglar; bot sadece tanık filtresi (`KG_SIGHT_*`,
   `KG_HEAR_*`) tanık göstermediğinde saklar. S3.4 ve S6.1 raporda.
4. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Character/KGCharacter.cpp` (taşınma modu), `Core/KGGameMode.cpp` (Kayıp duyurusu), `AI/KGBotController.cpp` (saklama),
testler, `01_GDD_Core.md` §5.

---

## SPRINT-026a — Fiziksel izler (M)

**Hedef:** cinayet boya/talaş izi, yumuşak zeminde ayak izi ve sürükleme izi bırakır; katil kuyuda yıkanabilir (görülerek).

**Charter kodları:** S3.1, S3.2, S3.5.

### Kabul (sabit)
1. **Test:** cinayetten sonra yumuşak zeminde 20 m yürüyüş ≥ 12 leke ve ≥ 15 ayak izi bırakır. Her iz, **kendi oluşma
   anından** charter §3 tablosundaki ömrü içinde kaybolur (leke 45–60 sn, ayak izi 60–120 sn, sürükleme 60 sn). Yıkanma
   7 sn sürer; bittikten sonraki 1 sn içinde yeni leke oluşmaz; yıkanma, tanık filtresinden (`KG_HEAR_WASH` + görüş)
   geçenlerin tanık olduğu bir Washing olayı üretir.
2. **Test:** iz havuzu hiç 256'yı geçmez.
3. **[R]** 256 izde GPU ≤ 0.2 ms, p95, 60 sn, 1080p, referans makine, ekran dışı render turu (soak'ta değil).
4. **[U]** botların taze iz takip yolu çalışır ve `KG_TRAIL follow result=…` loglar (yüzde bandı yok).
5. `KG_Cap_trail_day.png` ve `KG_Cap_trail_night.png`: izin yönü (cesetten katile) 10 m'den çekilmiş 1080p görüntüde
   belirlenebilir; sabit 2 bakış turunda değerlendirilir. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `Evidence/Traces/`, yumuşak zemin: layout JSON'da bölge/teras alanı (sadece veri) ve ayak sesi yüzeyinin bu alandan
okunması (`KGCharacter.cpp` ~1605 bugün mesh adından tahmin ediyor, yedek "Grass"), yeni decal malzemesi yolları,
`AI/` (iz takibi), testler, `01_GDD_Core.md` §6.

---

## SPRINT-026b — Sabotaj → onarım (016'nın üstüne) (S)

**Hedef:** 016'nın sabotajı (zehirli yalak, sönük lamba) Olay Defteri'ne düşer ve bir onarım görevi doğurur.

**Charter kodları:** S3.1, S4.6, S6.1.

### Kabul (sabit)
1. **Test:** 016'nın sabotajı (`Chores/WorldChores/`, `SabotageSecs` 3 sn) tanıklarıyla bir Sabotage olayı yazar ve bir
   onarım görevi doğurur; onarım, diğer görevler gibi Hazırlık barına katkı verir. Sönük lamba o bölgenin ışık şiddetini
   ölçülebilir düşürür.
2. S4.6 raporda. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Chores/WorldChores/` (016 bittikten sonra), `World/` (lamba), `AI/` (sabotaj), testler, `01_GDD_Core.md` §8.

### Kuyrukla ilişki
016'yı **yeniden yazmaz**: onun sabotajını deftere ve onarım modeline bağlar.

---

## SPRINT-027a — Gece Çözücü + Gece Defteri + Şerif + Doktor (L)

**Hedef:** evdeki mum ışığında Gece Defteri açılır; roller seçim yapar; şafakta sonuçlar `02_Roles.md`'deki öncelik
sırasıyla çözülür. Şerif ve Doktor **`02_Roles.md`'nin tanımladığı gibi** yapılır (Şerif: gece seçer, sonuç şafakta;
Doktor: gece korur). Her kullanım defterde bir olaydır. Dünyaya bağlı yeniden tasarım (Şerif'in gündüz yüz yüze
sorgusu, Doktor'un kapıya yürümesi) **Onay R1**'e bağlıdır; onaylanırsa bu sprint onu yapar ve 015'in kart metnini
(`UI/Reveal/KGRoleCardText.cpp`, bugün "Night: question one villager") aynı değişiklikte günceller.

**Charter kodları:** S5.1, S5.3, S5.4, S2.6 (R1'e bağlı), S1.4.

### Kabul (sabit)
1. **Test** `KillGodot.Roles.NightResolver`: ≥ 15 tablo vakası (Doktor vs Sabırsız öldürmesi → hedef baygın, ölü değil;
   tohumlu, süreler `FKGMatchClock`'tan).
2. **Test:** gece ortasında çözücü durumu SaveGame arşiv turundan geçer ve şafak sonuçları aynıdır. (Maç
   snapshot/restore testi değil: `UKGSnapshotComponent::WriteRecord/ReadRecord`'ın bugün çağıranı yok, o M6 işi.)
3. **[U]** 022'nin rol zorlama seçeneğiyle: her iki rolün yeteneği soak'ta en az bir kez kullanılır ve her kullanım
   bir defter olayı yazar.
4. **Rapor:** S1.1-gece **sadece Şerif ve Doktor taşıyanlar için**; S5.3.
5. İki rolün gerçek yetenek satırlarıyla açılış kartı ve Gece Defteri UIShot'ları. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `Roles/Abilities/`, `Roles/KGNightResolver.*`, `Roles/KGRoleDefinition.*` (Abilities doldurulur; USTRUCT, editör
kapanışı), yeni `UI/NightLedger/`, ev masaları (çalışma zamanında 16 ev noktasından spawn), `Core/KGGameMode.cpp` (şafak
çözümü kancası), `AI/` (bot rol kullanımı), testler, `02_Roles.md`. `UI/Reveal/KGRoleCardText.cpp` sadece R1 onaylanırsa.

### Sınırlar
3 deneme / kontrol; UI için 2 bakış turu. İki rolden fazlası bu sprintte yapılmaz.

---

## SPRINT-027b — Saat Ustası + Tetikçi + vasiyet (M)

**Hedef:** Saat Ustası Tetikçi'ye emir işaretler, Tetikçi uygular. Vasiyet masası gelir.

**Charter kodları:** S5.1, S5.4, S6.1.

### Kabul (sabit)
1. **Test:** Gece Çözücü'ye ≥ 10 yeni vaka: emir → Tetikçi uygular; Tetikçi yoksa (N 6–7'de tek Sabırsız) Saat Ustası
   kurban hakkını kendisi kullanır (`02_Roles.md`: "kendisi kullanır ya da Tetikçi'ye emir verir"); Saat Ustası ölürse
   Tetikçi lider olur.
2. **Test:** vasiyet masada yazılır, ölüm açılışında gösterilir; Sahtekar kancası boş bırakılır.
3. **[U]** zorlanmış rollerin yetenek yolu çalışır ve loglar.
4. Kart ve vasiyet masası UIShot'ları. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Roles/Abilities/`, `Roles/KGNightResolver.*`, vasiyet masası (çalışma zamanı spawn), `UI/`, `AI/`, testler, `02_Roles.md`.

---

## SPRINT-028 — Bekçi, Gardiyan, Casus + sokağa çıkma yasağı (L)

**Hedef:** gece sokakta olmak bir şey ifade eder: Bekçi fenerle devriye gezer, yasağı çiğneyeni kelepçeler ve maskesini
düşürür; Gardiyan birini hapseder; Casus kurbanın kılığına girer. Gözcü noktaları ilk kez kullanılır.

**Charter kodları:** S5.1, S5.4, S4.1 (gözcü noktaları), S6.1, S2 kural 4.

**Önkoşul (Onay R7):** v2 haritasında hapishane yok (`morrowmere_layout_v2.json` ve `Morrowmere_v2_Plan.md`'de
bulunmuyor). Sprint başlamadan kullanıcı var olan bir yapıyı seçer; hapishane o yapının layout JSON noktasından
çalışma zamanında kurulur.

### Kabul (sabit)
1. **Test:** Gece Çözücü'ye ≥ 10 yeni vaka (Gardiyan Saat Ustası'nın emrini engeller; Bekçi maskeli Tetikçi'yi
   kelepçeler ve maskesini düşürür; Casus kılığı toplantı çanında bozulur).
2. **[U]** zorlanmış üç rolün yetenek yolu çalışır; her kullanım bir defter olayı yazar.
3. **Rapor:** gece dışarıda olan Kasaba dışı oyuncunun Bekçi'ye yakalanma oranı (hedef %20–50; bot parametreleri
   dondurulmuş).
4. Sekiz çekirdek rolün hepsinin açılış kartı gerçek satırlarla (022 + 027a/b + 028). `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Roles/Abilities/` (3 yeni rol), `Roles/KGNightResolver.*`, `Character/` (kelepçe QTE, kılık), hapishane (çalışma
zamanı spawn), `AI/`, testler, `02_Roles.md`.

### 028'den sonra
Denge bantları (S9.4, S7.2 [B], S6.5 [B], S10.1) I5 kapısı adayıdır; kullanıcı onaylarsa kapı olur.

---

## SPRINT-029 — Rol etkileşim lint'i + başlangıç havuzu (S, seri)

**Hedef:** rol etkileşimi düzyazıdan veriye döner. Yeni oyuncular için lobide bir başlangıç havuzu seçeneği vardır.

**Charter kodları:** S5.1 (beyan), S5.2 (rapor), G3.1, G3.2, G2.4.

**Neden seri:** `FKGRoleInfo` bir USTRUCT (başlık değişikliği, editör kapanışı); 027a aynı `Roles/KGRoleDefinition.*`
dosyasını yazar; çalışan 015 her katalog rolünü `KGRoleCardText`'e karşı test eder. 027a ve 015'ten sonra koşar.

### Kabul (sabit)
1. **Test** `KillGodot.Roles.InteractionGraph`: 51 rolün hepsi ≥ 2 kenar ve boş olmayan `WorldFootprint` taşır (beyan).
2. Her rol kartı metni gönderilen her dilde ≤ 25 kelime (G3.1).
3. Başlangıç havuzu (14–16 rol) üreticide bir **lobi seçeneği** olarak var ve testli. Hesap takibi yok.
4. **Rapor (kapı değil):** 10.000 tohumda her karşı rolün hedefiyle buluşma oranı; N 10–13'te öldürmeyen kötü rol payı
   (R4 için başlangıç çizgisi).

### Yazılabilir kapsam
`Roles/KGRoleDefinition.*` içindeki `FKGRoleInfo` (Partners, Counters, WorldFootprint alanları),
`Roles/KGRoleListGenerator.*` (sadece başlangıç havuzu seçeneği; `BuildSlots` değişmez),
`Private/Tests/KGRoleListGeneratorTests.cpp`, `02_Roles.md` (sadece veri sütunları).

### Kapsam dışı → Onay listesi
Nötr revizyonu, Korsan ve Bombacı, `BuildSlots` değişikliği (R4, R5).

---

## SPRINT-030 — Ölüm oynanışı v1: hayalet görevleri, poltergeist, hayalet görüş kuralları (M)

**Hedef:** erken ölen oyuncunun maçı sürer: hayalet görevleri (%30 verimle bara katkı), gecede 1 poltergeist (bardak,
mum devirme; kayda düşer), ruh dünyasında keşif hedefleri. Hayaletler canlıların rolünü görmez, sadece kendi katilini
bilir.

**Charter kodları:** S8.1, S8.2, S8.3, G7.3.

### Kabul (sabit)
1. **Test:** hayaletin her fazda ≥ 1 eylemi var; hayalete geçiş `FKGMatchClock` ile (bugünkü 4 sn'lik TimerManager
   kaldırılır).
2. **Test:** poltergeist hayalet başına gecede ≤ 1, her kullanım yakındaki canlıların duyduğu bir olay; hayaletten
   canlıya başka kanal 0.
3. **Replikasyon testi:** hayalet istemcisi canlıların `PrivateRoleId`'sini almaz; katilin kimliği hayalet olunca
   sahibe özel gelir.
4. **[U]** ölü botların her hayalet eylem yolu çalışır ve loglar (S8.2 [O]'da ölçülür). Hayalet HUD'u UIShot'u.
   `run_invariants.ps1` geçer.

### Yazılabilir kapsam
Yeni `Character/Ghost/`, `Character/KGCharacter.cpp` (sadece BecomeGhost), `Chores/` (hayalet görevi bayrağı), `UI/`
(hayalet HUD'u), `AI/` (ölü bot davranışı), testler, `01_GDD_Core.md` §11.

### Kapsam dışı → öneri
Araf Düellosu ve Hortlak (GDD §11.1): ayrı sprint. Medyum seansı: ses işinden sonra.

---

## SPRINT-031a — Bölge Kapıları (M)

**Hedef:** 6 kişilik maç küçük bir köyde, 20 kişilik maç tüm köyde oynanır.

**Charter kodları:** G2.2, G2.3 (rapor), S4.2 (rapor), S4.3 (risk katmanı alanı).

### Kabul (sabit)
1. Layout JSON'da her bölgeye `MinPlayers` (açılma eşiği) ve `RiskTier` alanı eklenir; `verify_v2_build.py` bu
   alanları lint eder (v2 inşa sahibiyle sıra ayarlanır; I2 geçer). GDD §3'ün v1 kümeleri kullanılmaz.
2. **Test:** kapı testi eşikleri JSON'dan okur; N = 6, 10, 15'te açık bölgeler JSON'la aynı; kapalı bölgelerde görev ve
   ev ataması 0.
3. **Rapor:** G2.3 ve S4.2 (N 6 ve 20).
4. Kapalı bölge sınırı (sis duvarı/tabela, çalışma zamanında JSON noktalarından spawn) ekran görüntüsü.
   `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`World/KGMapInfo.*` (bölge kapıları), `Core/KGGameMode.cpp` (görev/ev ataması filtresi), layout JSON (sadece veri
alanları), `Tools/Level/verify_v2_build.py` (lint), `AI/` (kapalı bölgeden kaçınma), testler.

### Kuyrukla ilişki
Storm Manor (017/018) aynı kapı alanını kullanır.

---

## SPRINT-031b — AFK (+ R2 onaylanırsa fener) (S)

**Hedef:** 90 sn hareketsiz kalan "uyuyan" olur. Fener aydınlanması ya yeniden tasarlanır (R2) ya da kaldırılır (G4.2).

**Charter kodları:** G5.5, G4.2.

### Kabul (sabit)
1. **Test:** AFK 90 sn → uyuyan, oyu sayılmaz.
2. **Test:** R2 onaylandıysa fener aynı tohum ve aynı sahte görev dağılımıyla her zaman aynı bölgeyi gösterir (sonucu
   belirleyen rastgelelik 0); onaylanmadıysa fener aydınlanması kaldırılmıştır ve Hazırlık barı dolunca başka bir
   rastgele ifşa yoktur.
3. `run_invariants.ps1` geçer.

### Yazılabilir kapsam
`Core/KGGameMode.cpp` (AFK, fener), `Core/KGPlayerState.*` (uyuyan), testler.

### Kapsam dışı
Dünya Olayları (Sis, Fırtına, Pazar Gemisi) M12 "Canlı köy" içeriğidir; bu sprintte yok (R8).

---

## Yakınlık sesi (eski SPRINT-032) — engelli, sprint listesinde değil

**Engel:** kullanıcı EOS ürününü oluşturur; `DefaultEngine.ini` o zamana kadar null online çalışır. Sonra M3 çevrimiçi
yığın ve M4 (`09_Roadmap_Gauntlet.md`). Bu tek bir sprint değil, bir kilometre taşıdır.
**Zamanı gelince ölçütler:** menziller ve duvar (G1.2, dB/Hz) iki süreçli testte; sessize al / engelle / raporla her
oyuncu satırında (G1.5); gecikme p95 ≤ 200 ms **iki uçta loglanan ses paketi zaman damgalarından** (G1.3), iki makine
gerekir. O zamana kadar G1.1 (her ses mekaniğinin sessiz yolu) tüm sprintlerde geçerlidir.

---

## Kuyruktaki sprintlere etkisi (hiçbiri sessizce değiştirilmedi)

Aşağıdakiler **öneridir**. Başlamamış sprintlerin kabul kriterlerini yalnız kullanıcı değiştirir.

### Çalışan sprintler (v2 polish, yeraltı, lore + marka)
Değişiklik önerilmez. Not: kazıdaki açık mezar olayları 020b'de tek satırlık bir yayıcıyla kayda bağlanır; yeraltı
sprintinin dosyalarına 020b, o sprint bittikten sonra dokunur. 025b'nin mahzeni yeraltı sprintine bağlıdır. v2 inşa
hattı v2 polish sprintinindir; aşağıdaki sprintler onu değiştirmez (031a hariç, sıra ayarıyla).

### SPRINT-016 Fiziksel görevler (kullanıcı önceliği) — sırası korunur
- **Değişiklik yok, sıra aynı.** Kesişen taşıma rotaları tanık ve pusu üretir: S2 ve S4'ün tam istediği şey.
- **Bugünkü kod:** `Chores/WorldChores/` yazılıyor; sabotaj zaten içinde (`SabotageSecs` 3 sn, zehirli su / sönük
  lamba). 026b bunu yeniden yazmaz, deftere ve onarım modeline bağlar.
- **Öneri (kabule eklemeden, uygulama notu olarak):** dünya görevi adımları (al, taşı, teslim et, kullan, sabote et)
  tek bir `EmitChoreStepEvent` kanca fonksiyonundan geçsin; 020b gelene kadar boş kalır. Böylece 020b, 016'nın
  dosyalarını yeniden açmadan bağlanır.
- Görünür görev sonuçları (yanan lamba, dolu yalak) 024'teki Kasaba Panosu'nun doğal girdileridir.
- **Sıra önerisi:** 016 → 020a → 020b (ortak dosyalar: `KGCharacter.cpp`, `Chores/`).

### SPRINT-015 Maç akışı — sıra korunur, üç çakışma notu
- **Rol kartı metni:** 015 kartları `UI/Reveal/KGRoleCardText.cpp`'de yazıyor (ör. Şerif "Night: question one
  villager"), `02_Roles.md` ile uyumlu. Yeteneklerin kodu henüz yok. G3.4 gereği öneri: kodu olmayan roller için kart
  hedefi, tarafı, mesleği ve lore satırını göstersin; yetenek satırları 027a/b ve 028 ile açılsın. R1 onaylanırsa
  Şerif/Doktor satırları 027a'da değişir.
- **Yayıncı modu ve takma ad:** 015 `UI/Reveal/KGStreamerMode.h`'de `PlayerKey()` ve takma ad tablosunu yazdı. 020b
  defter anahtarı ve 024'teki isim etiketi, defter ve Vaka Dosyası aynı fonksiyonları kullanır (G7.5).
- **"Sabırsızlar takım arkadaşlarını görür":** 022'den sonra "takım" = aynı taraf (Saat Kırıcılar). Yalnız katiller
  kimseyi görmez. 015 önce koşarsa bugünkü kural kalır, 022 düzeltir.
- **Dosya çakışması:** 015 `KGGameMode.cpp` ve `KGHUD.cpp` yazar; 020a–022 ile paralel koşamaz.

### SPRINT-017 Storm Manor (sadece tasarım) — sıra korunur, charter uyum bölümü önerilir
Başlamadan önce kullanıcı onaylarsa `StormManor_Plan.md`'ye bir "Charter uyumu" bölümü eklenebilir:
- her oda ≥ 3 var olma sebebi (S4.1), lint edilebilir etiketlerle, `MinPlayers` ve `RiskTier` alanlarıyla;
- tanık geometrisi: görüş hatları, gözcü noktaları, karanlık/aydınlık bölgeler (`KG_SIGHT_*`; S2.1, S4.3);
- **şimşek aydınlatmaları** rastgele bir ifşa değil, tohumlu ve önceden okunabilir bir ritim olsun (G4);
- **elektrik kesintisi sabotajı** 026b'deki sabotaj → onarım modelini kullansın;
- 14–20 oda N = 6'da çok büyük: Bölge Kapıları (031a, G2.2) kanat kanat kapansın.
Kabul kriterleri aynı kalır; sadece plan belgesinin içeriğine bir bölüm eklenir.

### SPRINT-018 Storm Manor inşası
031a'dan (bölge kapıları) ve 026b'den (sabotaj) sonra koşarsa bu sistemleri hazır bulur. Önce koşarsa sonradan bağlanır;
engel değildir.

### SPRINT-019 İzleyici modu + tekrar
Öneri: 024'ten sonra koşsun. Olay Defteri zaten bir zaman çizelgesidir; epilog "gerçekte ne oldu" sekmesi ve tekrarın
olay işaretleri ondan okunursa maliyet düşer.

---

## Onay listesi (kullanıcı için)

### Genel
- [ ] Charter'ı (`KillGo_Pillars.md` v1.1) oyunun tasarım sütunları olarak kabul et, ya da değiştirilecek hedefleri işaretle.
- [ ] §0.3 sabitlerini kabul et (görüş 30/12/5 m, tanımlama = isim etiketi 8 m, duyma yarıçapları, toplanma 35 sn).
- [ ] SPRINT-020a ve 020b'yi onayla (doğrulama oturumundan sonra; en yüksek kaldıraç).
- [ ] 015 için rol kartı ve takma ad notlarını kabul et / reddet.
- [ ] 017 için "Charter uyumu" bölümünü kabul et / reddet.
- [ ] 021–031b sırasını onayla ya da yeniden sırala.

### Tasarım değişiklikleri (her biri ayrı; onaylananlar Backlog "Proposed (needs approval)"a kopyalanır)
- [ ] **R1 — Bilgi rollerinin dünyaya bağlanması:** Şerif gündüz yüz yüze 4 sn'lik görünür sorgu, Doktor gece hastanın
  kapısına yürür (ayak izi); aynı ilke Dedektif, Kâhin, Muhbir ve Baştan Çıkarıcı için. Hayır → 027a `02_Roles.md`'deki
  gibi yapar, S2.6 bu roller için muaf.
- [ ] **R2 — Fener aydınlanması:** rastgele bir Sabırsız yerine en çok sahte görevin yapıldığı bölgeyi gösterir.
  Hayır → fener aydınlanması kaldırılır (031b).
- [ ] **R3 — Kasaba Panosu otomatik çelişki işareti** ("Pano ile çelişiyor"). Hayır → pano sadece listeler.
- [ ] **R4 — Nötr revizyonu ve üretici:** 8 nötrün hedefleri (Cadı, Hafızasız, Hayatta Kalan "X tarafıyla kazan"
  olmaktan çıkar) ve `BuildSlots`'un N 10–13'te öldürmeyen bir kötü rol açması (denge değişikliği).
- [ ] **R5 — Korsan ve Bombacı:** Korsan taş-kâğıt-makas yerine mahkeme kararına bahis; Bombacı %50 kablo yerine
  okunabilir kablo ipucu (G4.2'den taşındı).
- [ ] **R6 — Acil toplantı kuralları (021):** toplanma 35 sn (GDD §1'deki 20 sn yerine), acil toplantıda ışınlama yok,
  sonrasında Gündüz kaldığı yerden sürer, 1. gün rapor serbest, tavanı aşan raporlar akşam Vaka Dosyası'na.
- [ ] **R7 — Hapishane yeri (028):** v2'de var olan hangi yapı?
- [ ] **R8 — Dünya Olayları:** M12'ye bırak, ya da 031b'ye önceden bildirilen tek bir olay türü ekle.
