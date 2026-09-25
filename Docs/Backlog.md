# Backlog

İşaretler: `[ ]` yapılacak · `[~]` devam ediyor · `[x]` bitti (kapılardan geçti) · `[!]` blokajlı

## M0 — Temel
- [x] Vizyon, çekirdek GDD, köy hayatı, roller (51), lore, harita, sanat, ekonomi, UI, yol haritası dokümanları
- [x] Teknik araştırma raporu (UE 5.8.3 kaynak doğrulamalı)
- [x] Asset araştırma raporu (~300 aday, lisanslı, `Docs/Research/AssetResearch.md`)
- [x] Başlangıç paketleri indirildi (kullanıcı onayı 2026-09-24): 20 kaynak, ~2.3 GB, hepsi CC0, bağımsız denetimden geçti, şüpheli dosya yok. `Art/Source/` (git dışı; `Tools/Assets/approved_sources.json` ile yeniden indirilebilir). Atıflar `Docs/Credits.md`.
- [x] Teknik mimari dokümanı
- [x] Blender: palet texture üretici (`kg_make_palette.py`, 8×8, `Art/Textures/T_KG_Palette.png`)
- [x] Blender: kukla karakter prototipi + 3 render (sıralama, konuşma yakın çekimi, CS2 viewmodel maketi)
- [x] Blender: varyasyon sayfaları (`kg_variant_sheet.py`, 48 tohumlu Yüz, `Art/Concept/Variants/`)
- [x] UE proje iskeleti: uproject, Config, KillGodot modülü. GameMode/State/PlayerState, MatchClock, Rng, RoleListGenerator (51 rol), FP Character + Viewmodel (CS2 presetleri, TF2 backstab pozu), Mouth, Snapshot. 5 otomasyon testi. Derleniyor, testler yeşil (ITER-001).
- [x] Rol üreticisi mantığı Python'da simüle edildi: 6–20 oyuncu × 40 tohum = 600 liste, 0 bant dışı.
- [x] Git deposu + LFS ayarları (.gitignore, .gitattributes). İlk commit kullanıcı onayı bekliyor.
- [ ] Blender: asset işleme script'i (`kg_process_asset.py`: import → palet → LOD → UCX → FBX). İlk indirilen paketle test edilecek.
- [x] Sanat yönü: kukla REDDEDİLDİ → Quaternius köylüler (canlı renk ayarı) — ITER-002

## M1 — Araç zinciri
- [x] Derleyici: VS 2026 **Build Tools** 18.10 (`D:\VS2026\BuildTools`, MSVC 14.50.35739 LTSC + 14.51, Win SDK 22621, .NET FX 4.8 SDK). Tam VS IDE, EdgeUpdate grup ilkesi WebView2'yi engellediği için kurulamadı (ilke kullanıcının kararı; değiştirilmedi).
- [x] İlk derleme (G1) — ITER-001
- [x] Greybox varsayılan harita (Python commandlet), runtime input, oyunun ilk açılışı + otomatik ekran görüntüsü
- [x] `Tools/Gauntlet/run_gates.ps1` G1–G2 (G3/G4 M3/M7'de)
- [x] İlk Automation testleri: 5/5 yeşil
- [x] Unreal MCP eklentisini dene (canlı editör kontrolü) — ITER-005 (kg_live.py, kg_mcp.py, kg_remote.py)

## M2 — FP karakter
- [x] KGCharacter: hareket (yürü/koş/çömel/zıpla), stamina + tükenme — ITER-002
- [x] FP kamera + native First Person Rendering, gövde gölgesi, Quaternius FP kollar
- [x] Viewmodel prosedürel katman: sway / bob / recoil / inspect / TF2 backstab pozu
- [x] Etkileşim sistemi (IKGInteractable), kapı (aç/kilit/kır)
- [~] Fizik tutuş / taşıma / fırlatma (yerel) ✅ — döndürme eksik
- [~] Yakın dövüş: vuruş, itme, hasar, backstab ✅ — blok eksik
- [~] Tava elde + prosedürel saldırı/inspect ✅ — gerçek FP animasyon seti eksik

## QA biletleri (QA-001'den açık kalanlar — sprint başında önce bunlar)
- [x] **P1** Köylü saçı kafadan kopuk — ITER-004 (saç Head kemiğine bağlandı)
- [x] **P1** Köylü yüzü gölgede çamur rengi → rim + gölge kaldırma + mürekkep kontur — ITER-006 (toon ramp → P3 aşağıda)
- [ ] **P1** Eşya tutuşu: Grip soketi + eşya DataAsset'i (ölçek, yön, el pozu), şemsiye ucu hedefe (QA-002-02)
- [x] **P2** Backstab bıçağı: şemsiye → gerçek bıçak (Hunters Knife), sap avuçta — ITER-005
- [ ] **P2** Tava modeli düşük kalite → daha iyi CC0 tava/alet modelleri
- [x] **P2** Nişangâh + minimal HUD — ITER-004 (+ "E" etkileşim ipucu hâlâ eksik)
- [x] **P2** Toon/kontur/rim ışığı (karakterler arka plandan ayrışmıyor) — ITER-006 (custom-depth kontur, duvar arkasından görünmez)
- [ ] **P3** 3 bantlı toon ramp (özel shading; şimdilik rim + gölge kaldırma yeterli)
- [ ] **P2** Kapı senaryosu screenshot zamanlaması (açılma anı) + kapının açılma yönü
- [ ] **P3** Autoshot'a "viewmodel ekran dışında" otomatik kontrolü
- [ ] **P2** Inspect: eşya soketi etrafında dönüş, profil gösterimi (QA-002-05)
- [ ] **P2** Perf ölçümü güvenilirliği + ~430 ms takılmanın kaynağı (QA-002-08)
- [x] **P2** FP el pozu eşyayı kavrasın + ten rengi köylüyle uyumlu (QA-002-06) — ITER-005 (Drillimpact FP kolları, tutuş hand_R'ye bağlı)
- [ ] **P2** Vuruş klibinin tepe karesinde ön kol ekranı kaplıyor → klip/FOV/kol ölçeği ayarı
- [ ] **P3** FP kol kıyafet kolu/eldiven kozmetiği (şu an çıplak kol)
- [x] **P1** Editör kapandığında: tam derleme + run_gates — ITER-006 (G1 ✅ G2 8/8 ✅)

## Köy (ITER-007'den — görünür içerik önce)
- [x] L_Morrowmere ilk hali: arazi, deniz, 15 ev (2 kat, iç eşya), meydan, yol, iskele, doğa — ITER-007
- [x] Masum boş el + yumruk, bıçak sadece çekilince — ITER-007
- [x] Ev kapıları: KGDoor + kit kapı mesh'i — ITER-008
- [x] Gece/akşam: fenerler, sokak lambaları, iç ışık, faza göre gün/gece — ITER-008
- [x] Maç döngüsü: otomatik başlangıç, botlar, rol kartı, HUD, ölüm, kazanma, tur sonu — ITER-008
- [x] Toplantı oylaması → yargılama → darağacı + bot oyları — ITER-009
- [x] Gece avı: kalıcı av hedefi, takılınca yan adım — ITER-009
- [x] Hayalet izleyici (serbest uçan) — ITER-009
- [ ] Karakter çeşitliliği: kadın/erkek, kıyafet renkleri, saç, yüz (her oyuncu farklı görünsün)
- [ ] Oyuncu adı (makine adı yerine), isim etiketi yakında
- [ ] Hayalet kamera cesedin dışında başlasın; diğer oyuncuları izleme
- [ ] İskele direkleri + balıkçı kayıkları + ağlar (pirate props .blend → glb)
- [ ] Meydana kuyu/çeşme + ilan panosu (görev/oylama noktası)
- [ ] Köylü NPC'ler dolaşsın (esnaf, balıkçı)
- [ ] Çimen yoğunluğu (ISM/foliage), rüzgâr

## Kullanıcı istekleri 2026-09-24 (öncelik sırası — görünür içerik)
- [x] Köy yeniden tasarımı: layout JSON, kıvrımlı sokaklar, bölgeler (liman, meydan, demirci, fırın, han, kilise+çan kulesi, mezarlık, çiftlik, oduncu, fener burnu) — ITER-010
- [x] Among Us görev istasyonları v1 (18 görev, tut-E, hazırlık barı, fener ifşası, botlar görev yapar, NavMesh) — ITER-010
- [x] Ghost of Tsushima tarzı rüzgârlı çayır (prosedürel çim öbekleri, HISM ~22k) — ITER-010
- [x] Gerçek su: dalgalı saydam deniz, yüzme/dalma, su altı, kaldırma kuvveti (fıçı/kayık), balık sürüleri — ITER-011
- [x] Ses: prosedürel ortam + oyun sesleri — ITER-011
- [x] Kırılabilir kutular, tırmanılabilir kuleler, yükleme kartı, yaprak rüzgârı, yoğun orman — ITER-011
- [ ] Japon bahçesi + ikonik mekânlar: denizde torii (karşıdaki ada), koi göleti + ahşap iskele + bahçe lambaları, taş fenerler, sakura/akçaağaç/bambu, pagoda, yatai (asset ajanı çalışıyor)
- [ ] Karşıda küçük ada (arazi) + torii
- [ ] İskele yeniden tasarımı (korsan paketi iskele parçaları, direkler, halatlar)
- [ ] Gökyüzü/bulut geliştirme (VolumetricCloud ayarları, daha dramatik gün batımı)
- [ ] Yakın alan yoğun çim (oyuncu çevresinde dinamik hücreler) + HISM ağaçlar (FPS dostu yoğunluk)
- [ ] Envanter + sandık (Minecraft gibi: ganimeti evdeki sandığa koy) + oturma (sandalye/bank)
- [ ] Harita tanımı/tema sistemi: ev = gameplay birimi, görünüş = tema skini (ileride uzay istasyonu, SpongeBob kasabası)
- [x] Balık tutma: olta (H), atış gücü + yay, gerçek dalgada şamandıra, dürtme/ısırık/vurma, çekme mini oyunu, 4 tür + koi vergisi, ağırlıklı av + kişisel rekor, çöp/hazine, Madam Brine tezgâhı (satış + ödünç olta), FP/TP animasyonlar, sesler, dev fiilleri, testler + ağ smoke — ITER-013
  - [ ] Balık tutma görsel doğrulama: FP olta tutuşu/misina başlangıcı oyunda, TP olta yönü (hand_r), tezgâh yerleşimi, av kartı
  - [ ] Balık tutma devamı: yem/olta yükseltmesi (Madam Brine), kayıktan balık tutma, evde akvaryum/koleksiyon, Balık Avı Düellosu (panayır), host göçünde olta durumu (şimdi sıfırlanır), bot balıkçılar, çizme mesh'i (şimdi küp)
- [ ] Ev içleri ayrı mekân: kapı → kısa yükleme → detaylı iç mekân (ayrı "interior" bölgesi), çıkış kapısı, kapı zili (içeridekiler duyar), ev sahibi kilitler; ev sahipliği (PlayerState.HouseIndex)
- [ ] Şehir merkezi: park (çeşme, banklar, ağaçlar, çiçek tarhı), daha çok prop, pazar yeri kalabalığı, kilise gerçek kilise görünümü
- [ ] Yeraltı: kuyudan inilen mahzen; mezarlıkta kazarak inilen katakomb (loafbrr Mines&Cave paketi)
- [ ] Kazma mekaniği: kürek, kazı tümsekleri, ganimet (altın, anahtar, kemik, hazine sandığı), altın ekonomisi
- [~] Görev mini oyunları v1: 22 görevin hepsine Among Us tarzı mini oyun (18 + 4 isteğe bağlı), bulanık dünya üstünde
  Slate penceresi, çok aşamalı görevlerde ilerleme saklanır, kesintiler (hasar/itilme/toplantı/ölüm), sunucu doğrulaması
  (aşama sırası + oturum jetonu + sunucunun ölçtüğü süre ≥ alt sınır), Sabırsız sahte görevi (aynı panel, sayılmaz, görsel
  etki yok), 8 görsel görev (`AKGChoreFx`: çan, fener, baca dumanı, ilan, mumlar, liman feneri, saat, odun yığını),
  `kg.Chore.Play/AutoWin`, `kg.ChoreShot`, dev panel — `Source/KillGodot/Chores/`, `01_GDD_Core.md §8.1`, `08_UI_UX.md §6.3`,
  ITER-014. Testler (`KillGodot.Chores.*`) + ekran dışı görüntüler + başsız ağ testi (`Tools/Unreal/kg_chore_smoke.ps1`);
  **PIE'de doğrulama bekliyor** (görsel etkilerin haritadaki yeri/ölçeği, panel hissi, sesler, gerçek oyuncu süreleri)
- [ ] Görevler v2 kalanlar: fiziksel taşıma görevleri (R.E.P.O.), görevler arası zincir (un → hamur → fırın), ağaç kesme
  (The Forest gibi devrilen ağaç), gözlemcilere görev animasyonu (üçüncü şahıs "çalışıyor" pozu), HUD listesinde aşama (2/3),
  botlar için mini oyun süreleri, Sabırsız sabotajları
- [ ] Görev etkilerinin yerleşimi: ilan panosu yönü, fırın bacası, fener lamba odası, odun yığını yerleri için layout JSON'a `fx_at` noktaları
- [ ] Botlar kapı açabilsin (iç mekân görevleri)
- [~] Sohbet paneli + emoji + tepki balonları: faz/hayatta/takım kanalları sunucuda, spam koruması, 32'lik prosedürel emoji seti, tepki çarkı (G), `/wave` emote köprüsü — `Source/KillGodot/Chat/`, `08_UI_UX.md §6.1`. Testler + başsız ağ testi (`Tools/Unreal/kg_chat_smoke.ps1`) geçti; **PIE'de görsel doğrulama bekliyor**
- [~] Emote çarkı + animasyonlar: 16 emote (`FKGEmoteCatalog`; 2 UAL + 14 başsız Blender klibi, 4 birinci şahıs kol
  jesti), sunucu yetkili replike durum, üst gövde katmanı (`UKGBodyAnimInstance`), 3. şahıs emote kamerası, `G` çarkında
  emote halkası, `Shift+1..4`, `/wave /dance ... /stop`, `kg.Emote*` + dev panel — `Source/KillGodot/Emote/`,
  `08_UI_UX.md §6.2`. Testler (`KillGodot.Emote.*`, 7) + başsız ağ testi (`Tools/Unreal/kg_emote_smoke.ps1`) geçti;
  **PIE'de görsel doğrulama bekliyor** (kliplerin okunurluğu, kamera açılışı, çark düzeni, jestler)
- [ ] Emote v2: kaval çal, yemek ye/iç, NPC mim taklidi, kozmetik emote'lar (dükkân), emote sesleri, botların ara sıra emote yapması

## M3 — Ağ çekirdeği
- [x] M3a: Saldırı/backstab/itme/etkileşim/bıçak/sprint sunucu RPC'leri + MulticastSwing + istemcide ölüm (OnRep) — ITER-006 (listen-server + 1 istemci PIE'de doğrulandı)
- [ ] Kapı + fizik tutuşun 2 oyunculu PIE testi (`kg_net_test.py door/interact`)
- [ ] Fizik proplarının replikasyonu (bReplicates + bReplicateMovement), fırlatma istemcide görünsün
- [ ] Sprint/stamina → CMC SavedMove bayrağı (düzeltme titremesi olmasın)
- [ ] KGSessionSubsystem: EOS giriş (DevAuth), lobi kur/ara/katıl, attribute'lar
- [ ] NetDriverEOS P2P bağlantı, localhost IpNetDriver fallback
- [ ] MatchClock, FKGRng, UKGSnapshotComponent, PUID tabanlı oyuncu kayıtları
- [ ] Bölge tabanlı relevancy, dormancy, net ayarları (MaxPlayers=20)
- [ ] Fizik tutuşun ağ versiyonu (server otorite + yerel görsel takip)
- [ ] 20 bot soak testi (Gauntlet)

## Sonraki milestone'lar
M4–M18 maddeleri, milestone başladığında `09_Roadmap_Gauntlet.md`'den buraya ayrıntılı olarak açılır.
