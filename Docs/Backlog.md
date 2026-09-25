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

## NEXT (user, 2026-09-26): SPRINT-040 Storm Manor expansion (much bigger, secrets, hidden compartments, secret + manor-only chores, traps): Docs/Iterations/SPRINT-040-StormManor-Expansion.md, starts when the v2 cliff/stairs agent frees a main slot

## NEXT (user, 2026-09-25 evening): v2 harbour cliff lower + lighthouse to the corner
- [ ] The stepped rock wall around the harbour/Lighthouse Point (the user's screenshot: blocky stacked rock terraces above the basin water) must be MUCH lower: a gentle 2-4 m rocky shore, not a wall. Also cl_point.
- [ ] Move the lighthouse tower further out to the corner/tip of the headland, off the main view axis. Keep the FuelLighthouse chore reachable and nav 100%.
- Files: author_layout_v2.py/.json (cliff shoulders, lighthouse position), kg_build_terrain_v2.py; full rebuild + verify + invariants; before/after shots from the basin and the jetty.

## Sprint queue (bounded loop, 2026-09-25): start in this order as agent slots free (max 3 running)
- [~] SPRINT v2 polish (running): calm basin, §11.4 props, bots on v2, v2 as the default map, material fix
- [~] SPRINT underground: shovel/digging/loot (5 spot kinds, grave noise + open graves, treasure maps + scraps, 10 loot
  tables), well cellar, mine tunnel, catacombs (mausoleum door, key gate, vault), minimap underground layer + regions,
  `kg.Dig.*` verbs — `Source/KillGodot/Dig/`, `01_GDD_Core.md §16`, `Docs/Level/Underground.md`. Tests `KillGodot.Dig.*` (5)
  + `Tools/Unreal/kg_dig_smoke.ps1` pass; **PIE verification pending** (VerificationDebt #12)
- [~] SPRINT lore + brand (running): lore bible (TR), text bank, KillGo logo, key art, loading art
- [ ] SPRINT-016 physical, original chores (USER PRIORITY): Docs/Iterations/SPRINT-016-PhysicalChores.md
- [~] SPRINT-016 finisher, underground finisher, SPRINT-022 visual pass (running 2026-09-25 afternoon)
- [ ] SPRINT-025 chore clarity + hold-M big map + UI overhaul (USER): Docs/Iterations/SPRINT-025-ChoreClarity-UI.md
- [ ] SPRINT-026 movement: strafe bunny hop + stamina (USER): Docs/Iterations/SPRINT-026-Movement.md
- [ ] SPRINT-027 villager variety (per player, never per role) + viewmodel animation polish (USER): Docs/Iterations/SPRINT-027-CharactersAndViewmodel.md
- [ ] FOREST sprint (from the forest/loot/tabletop design): the user also wants the forest BIGGER, with survival chores
- [ ] SPRINT-022 v2 visual pass (USER REVIEW 2026-09-25): varied houses, geometry validator + Japan garden fix, realistic water, towers/landmarks: Docs/Iterations/SPRINT-022-V2VisualPass.md
- [ ] SPRINT-023 voice + talking mouths + TF2 voice commands + partner emotes: Docs/Iterations/SPRINT-023-VoiceSocial.md
- [~] SPRINT-015 match flow: server list first, role-reveal ceremony, streamer mode: Docs/Iterations/SPRINT-015-MatchFlow.md
  — `UI/Reveal/` (ceremony, owner-only reveal component, streamer mode + pseudonyms), tests `KillGodot.Reveal.*` (4),
  `Tools/Unreal/kg_lobby_smoke.ps1` (2 processes), `Tools/Unreal/kg_reveal_shots.ps1`; **PIE/real-session check pending**
- [ ] SPRINT-017 map 2 "Storm Manor", design only, needs user approval: Docs/Iterations/SPRINT-017-StormManor-Design.md
- [ ] SPRINT-018 map 2 build (after 017 is approved)
- [ ] SPRINT-019 spectator mode + replays ("record")
- [ ] VERIFICATION session with the user (Docs/Process/VerificationDebt.md): blocks new feature sprints once the debt is >10 items

## Proposed (needs approval)
- [proposal] Villagers (SPRINT-027a): a "Look" page in the cosmetics UI (UI sprint) that previews the 38 archetypes and calls `UKGProfileSave::SetPreferredLook` (today: `kg.Look.Lock <Archetype|none>`, `kg.Look.List`)
- [proposal] Villagers: put the 59 Quaternius-2019 bodies under `/Game/KillGodot/Env/Ext/Characters/` (Ultimate Animated Characters incl. Witch/Wizard/Pirate/Knight/Chef, Modular Men/Women, RPG) into the random pool. They are NOT on the UBC skeleton (`Tools/Unreal/dressing/pack_ext_characters.md`): needs (a) one IK Rig per family + one IK Retargeter villager->2019 rig, built headless (`unreal.IKRigController` / `IKRetargeterController` in a commandlet), (b) batch-retargeting the ~50 UAL clips + emotes + FP-independent body clips onto that skeleton, (c) a per-skeleton clip map in `AKGCharacter` (`AnimIdle`/`AnimWalk`... and every `LoadObject` of `A_KG_*` in Emote/Chores/Fishing) so `UKGBodyAnimInstance` plays the retargeted set, (d) cosmetics/gear attach bones renamed per skeleton. About a sprint of work; not done in 027a (limits)
- [proposal] Villagers: more body meshes from CC0 packs that share the UBC skeleton (Quaternius Ultimate Modular Men/Women, the RPG characters) once an asset agent imports them; the archetype table only needs a new `EKGVillagerBody` entry + path. The UBC "Superhero" full body is the underwear base mesh and must never ship as a variant
- [proposal] Villagers: cloth for the cape/apron (rigid bind-pose pieces today; a Chaos cloth or a spine-chain skin would swing while running); a bandana/coif mesh so the pirate and knight stop borrowing the ranger hood
- [proposal] Villagers: the per-slot outfit tint colours whole garments (M_KG_Character has one Tint); a mask texture per outfit (shirt / trousers / boots) would allow two-tone dyes
- [proposal] Role sash: a subtle emissive pulse at the trial and a cleaner-hidden variant (the Cleaner role hides RevealedRoleId, so no sash appears: that is already the safe default)
- [proposal] SPRINT-025: a real mesh outline on the marked station (custom depth stencil + a post-process outline material) instead of the screen-space glow; needs a material asset, so it is a content sprint
- [proposal] SPRINT-025: the emote wheel (Emote/) and the chat box (Chat/KGChatUI.cpp) read the shared palette through FKGMenuStyle but keep their own sizes/radii; move them onto UI/KGUITokens.h type scale + radii (outside this sprint's writable scope)
- [proposal] SPRINT-025: an "Easy / Normal" chore difficulty option in Settings -> Gameplay backed by a `kg.Chore.Easy` cvar, once the per-minigame constants are pulled into one KGMgTuning table (today each minigame owns its constants and the floors live in KGChoreTypes.cpp)
- [proposal] SPRINT-025: the held big map could zoom on you (a 2x view centred on the player) instead of the whole village, with the full village on the tap; test with players
- [proposal] SPRINT-025: `Tools/Unreal/kg_hud_shots.ps1` could also shoot a meeting, a trial and the death screen so the whole HUD has offscreen evidence
- [proposal] Streamer mode: hide the chore panel's "FAKING - WON'T COUNT" tag (Chores/ is outside SPRINT-015's scope): one line in `KGChoreComponent.cpp` where the panel is built, `.bFake(bFake && !KGStreamer::IsRoleHidden(PC))` (UI/Reveal/KGStreamerMode.h)
- [proposal] Streamer mode: nameplates and a kill feed do not exist yet; when they land they should use `KGStreamer::DisplayName` / `MaskText`. Chat name completion (Tab) and `/w <name>` still take real names while typing
- [proposal] Streamer mode: the Impatient backstab-ready crosshair tint and the drawn blade still tell the role on stream (they are world/viewmodel cues, not HUD text)
- [proposal] Reveal: the Impatient team chat is open during RoleReveal but hidden under the ceremony; show a small team-chat strip on the reveal screen for Clockbreakers
- [proposal] Reveal: illustrated role art on the card face (`UKGRoleDefinition::CardArt`) in place of the faded emblem; card flip / shuffle sounds
- [proposal] Reveal: a network smoke that forces a Clockbreaker pair (e.g. a `-KGRevealForceRoles` dev flag) so the owner-only teammates path is checked over the wire, not only in UIShots
- [proposal] UI shots: kg.UIShot renders menu pages into a gamma-corrected target, which washes the dark UI out (lobby/home shots look lavender); reveal pages now use raw sRGB like `kg.ChoreShot`. Switch every page after a look round
- [proposal] Pseudonyms: derive the per-match salt from a replicated, non-secret match id so every streamer in the same match shows the same pseudonyms (today each client picks its own)
- [proposal] Localisation: move role card copy (EN/TR table in `UI/Reveal/KGRoleCardText.cpp`) into ST_Lore / String Tables with RU
- [proposal] Dig: bots use the passages (nav links + a "go underground" roam target) and dig mounds now and then
- [proposal] Dig: third-person dig/hold body clips (kg_make_emotes.py Dig_Hold/Dig_Stroke) instead of the torch/sword stand-ins
- [proposal] Dig: dev panel buttons (Dig.Give / Dig.Spots near / Dig.Finish / Dig.Gate) in the WORLD tab; teleports already list the underground markers
- [proposal] Dig: Mourner's Tokens convert to profile gold / a cosmetic crate at the epilogue
- [proposal] Underground: a Coroner/Sexton role that can read who dug which grave (the server would record the digger per grave)
- [proposal] Lore: wire the 4 loading screens (T_KG_Load_*) into the loading card, with the line from the String Table
- [proposal] Lore: create ST_Lore (EN/TR/RU). It feeds the role card lines, notice-board posts, seeded epitaphs, bottle messages and town-crier lines.
- [proposal] Brand: T_KG_Logo as the splash/menu fallback; the app icon as the Windows .ico and store asset
- [proposal] Lighting: the Lighthouse Point cliff renders flat black at night; the night sky needs a PP grade
- [proposal] Dev: a spawn_actor helper for render tours
- [proposal] Nav (SPRINT-016 finding): the navmesh agent is 144 cm tall but the villager capsule is 180 cm, so head-height props are not cut out of the navmesh. Example: the Lantern_Wall dressing actor at Home 14's door (about -2088,-1520,965) stopped the water-run smoke on its way to the well, and bots got stuck there. Fix: `AgentHeight=184` for the RecastNavMesh in DefaultEngine.ini, then rebuild the v2 navmesh. Or give wall lanterns no pawn collision (dressing owner). The smoke autopilot and the bots now side-step these props, but a real player still bumps into them
- [proposal] Art: the vertex-colour furniture props (`M_KG_PropVC`: SM_KG_Firewood, SM_KG_Bread and others) render plain white in game captures. The dressing woodpiles use them too. World chores work around it with flat colours; check the material or the vertex-colour import
- [proposal] Audio: dedicated `S_WC_Knock` (three raps on a door) and `S_WC_Slosh` (bucket slosh) sounds from `kg_synth_sfx.py` + import. Today the bread knock uses `S_Chore_Stamp` and the bucket spill uses `S_Chore_Splash`
- [proposal] World chores, more verbs (user: pour/balance/knock/light/climb/crank/sort/hang/throw/team-carry). Four ideas: a "balance" chore (carry a full tray of mugs from the inn to the quay, where bumping into anyone spills them); a proper throw target (toss the mended net over the drying line from the quay steps); "sort" at the market (drag fish into three baskets by kind); and a two-person "hoist" (one cranks the harbour crane while the other guides the crate onto the boat)
- [proposal] World chores: bots never take the ladder chores (Bell and clock, Lighthouse oil), so humans are dealt them more often. Bots could climb through nav links on the tower ladders
- [proposal] World chores: the Twist_TwoCarry and pose villagers stand in the rest pose. A "carry" body pose (both arms forward) for anyone holding a chore item would read better in third person
- [proposal] SPRINT-026 (stabilisation 2026-09-26, acceptance 1 open): the movement smoke shows 15 CMC corrections at 100 ms ping, all after stamina runs out (~6 chain hops). The chain-hop gate (`bHopChainBlocked` / `HopGainScale` from `Stamina.bExhausted`) and the hop charge (`HopCounter` poll) live in `AKGCharacter::Tick`, outside the move stream: the server simulates batched client moves against a once-per-frame flag and client replays re-charge hops. Fix: a custom `FSavedMove_Character` carrying stamina, chain streak and `TimeSinceJumpPressed`, with the hop cost spent inside `UKGCharacterMovement` at hop time (and sprint drain moved with it, or the gate made stamina-independent). About half a sprint; evidence `Docs/Level/SPRINT-026_speed_curve.png`
- [proposal] SPRINT-026 tuning: the scripted good strafe reaches 1138 uu/s (1.96x sprint) against the "about 1.35x" soft cap because the taper `1 - over/cap` floors at 0.08 only near 1.9x cap. Steeper taper (square the factor) or a ceiling at ~1.5x cap; re-run `kg_move_smoke.ps1` to confirm
