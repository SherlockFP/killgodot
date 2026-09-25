# KILL GODOT — Teknik Mimari

> Kaynak araştırma: `Research/TechResearch.md`. Motor kaynağı okunarak doğrulandı, UE 5.8.3.
> Bu doküman **kararları** tutar. Gerekçeler ve kaynaklar araştırma raporundadır.

## 1. Araç zinciri
| Bileşen | Karar |
|---|---|
| Motor | UE **5.8.3** (launcher). Push-model veya iOS EOS yaması gerekirse kaynak koddan derlemeye geçilir. |
| IDE / derleyici | **VS 2026 18.x** (MSVC ≥ 14.50.35723) ya da VS 2022 17.14 (MSVC ≥ 14.44.35211). Windows SDK 10.0.22621+. .NET motorla birlikte gelir. **Şu an PC'de VS yüklü değil, blokaj.** |
| Proje | `KillGodot.uproject`, C++ çekirdek + Blueprint içerik |
| Sürüm kontrolü | Git + **Git LFS** (uasset, umap, fbx, png, wav) |
| Blender | 5.2.2 LTS, headless Python pipeline (`Tools/Blender`) |
| UE otomasyon | Python commandlet (`-run=PythonScript -Script=`), sonra **Unreal MCP** eklentisi (5.8 Experimental) ile canlı editör kontrolü |
| Test | UE Automation (birim ve fonksiyonel), **Gauntlet** (listen host + N istemci + host'u öldürme testi) |

## 2. Modüller
```
Source/
  KillGodot/           # Oyun çekirdeği (runtime)
    Core/              # GameMode, GameState, PlayerState, PlayerController, MatchClock, Rng
    Roles/             # URoleDefinition, RoleListGenerator, win koşulları, gece çözümleyici
    Character/         # KGCharacter, FP kamera, viewmodel, hareket, etkileşim, fizik tutuş
    Combat/            # yakın dövüş, suikast, silahlar, hasar, maske
    Tasks/             # görev istasyonları, taşıma, sabotaj, hazırlık barı
    World/             # kapılar, kilitler, kırılabilirler, ağaç devirme, gün-gece, olaylar
    Voice/             # KGVoiceSubsystem (EOS RTC), ağız zarfı, kanal kuralları
    Online/            # KGSessionSubsystem (EOS lobi), KGMigrationSubsystem, snapshot
    Cosmetics/         # loadout, paint kit, item instance, doğrulama tokeni
    UI/                # CommonUI tabanlı widget'lar (menü, lobi, HUD)
    Minigames/         # satranç çekirdeği, tavla, zar…
  KillGodotEditor/     # Editör araçları (import, doğrulayıcılar)
```
**Eklentiler:**
- **Online:** OnlineSubsystemEOS, SocketSubsystemEOS, EOSVoiceChat, OnlineSubsystemSteam (PC mağaza girişi için)
- **Oynanış ve UI:** GameplayAbilities (GAS), EnhancedInput, CommonUI, StateTree (NPC)
- **Fizik ve efekt:** ChaosDestruction (Geometry Collection), Niagara
- **Otomasyon:** PythonScriptPlugin, Gauntlet

## 3. Oyun çerçevesi
| Sınıf | Görev |
|---|---|
| `AKGGameMode` | Sadece host'ta çalışır. Faz makinesi, rol dağıtımı (`UKGRoleListGenerator`), gece çözümleyici, olay yöneticisi, snapshot üretimi. |
| `AKGGameState` | Herkese açık durum: `Phase`, `PhaseRemaining` (MatchClock), `DayIndex`, `SuccessorRanks[]`, `MigrationEpoch`, hazırlık barı, açık bölgeler, aktif olaylar. |
| `AKGPlayerState` | Herkese açık kimlik: PUID, isim, **Yüz** ve kozmetik loadout, meslek, ev, hayatta/ölü/hortlak, açığa çıkmış rol (ölünce). |
| `UKGRoleComponent` (PlayerState altında) | **Gizli rol verisi**, `COND_OwnerOnly` ile sadece sahibine replike edilir. Takım arkadaşları ayrı bir `TeamInfo` aktörüyle bilgilendirilir. |
| `AKGPlayerController` | Giriş, UI, oy ve eylem RPC'leri. `IsPlayerMuted` override'ı (UE VOIP'e dönülürse). |
| `AKGCharacter` | FP karakter, bkz. §6. |
| `UKGMatchClock` | **Bütün oyun süreleri buradan** gelir, "kalan saniye" olarak. TimerManager oyun kritiği için **yasak**. |
| `FKGRng` | Tohumlu, akış konumu saklanan RNG. Snapshot'a girer. |

**Yetenekler:** rollerin yetenekleri **GAS** ile yazılır. `UKGGameplayAbility` alt sınıfları olur. Sarhoş, zehirli, korunuyor ve susturulmuş gibi durumlar `GameplayEffect` olarak tutulur. Gece Defteri seçimleri `FKGNightAction` kuyruğuna gider ve şafakta öncelik sırasıyla çözülür (bkz. `02_Roles.md`).

## 4. Ağ: P2P + host migration
**Taşıma:** `NetDriverEOS` üzerinden P2P listen-server. `RelayControl=AllowRelays`, yani önce doğrudan bağlantı, olmazsa relay. **EOS Lobby, maçın kalıcı nesnesidir** ve host gitse de yaşar.

### Host migration v1 (hedef ≤ 15 sn PC, çoğunlukla 7–10 sn)
1. **Snapshot:** her oyun aktörü `UKGSnapshotComponent` taşır. Kimlik kalıcı bir `FGuid`'dir, NetGUID **asla** kullanılmaz. Server'a ait değerler `SaveGame` bayraklı property'dir.
   - `FKGMatchSnapshot`: epoch, faz, kalan süre, RNG, PUID'ye göre oyuncu kayıtları (rol, envanter, oylar), aktör kayıtları. Sıkıştırılmış hali **≤ 32 KB** olmalı.
2. **Aktarım:** host her **2 sn**'de ve kritik olaylardan hemen sonra (ölüm, toplantı, rol değişimi) snapshot'ı **ilk 2 halef adayına** gönderir. Gizli alanlar sadece onlara gider.
3. **Algılama:** 4 Hz heartbeat. 2 sn sessizlik ve EOS lobi üye durumu birlikte kontrol edilir, yedek süre 10 sn. Motorun varsayılan 60 sn timeout'u kullanılmaz.
4. **Seçim:** puan = PC bonusu + ölçülen upload + NAT tipi + ping + kare süresi payı − mobil cezası. **Mobil asla host olmaz.** 13–20 oyunculuk maçlarda host adayının upload'ı **≥ 5 Mbps** olmalı.
5. **Kurtarma:** halef `KG_HOST_EPOCH` ve `KG_HOST_URL` üye attribute'larını yayınlar, sonra `OpenLevel(Map, "listen?KGMigrate=1")` ile snapshot'ı geri yükler. Diğer istemciler `ClientTravel("EOS:<PUID>:GameNetDriver:<ch>")` yapar. Oyuncular PUID ile yeniden bağlanır, "3… 2… 1…" geri sayımıyla oyun devam eder.
6. **Planlı çıkış:** menüden ya da Alt-F4 kancasıyla çıkan host önce **son snapshot'ı** gönderir. Veri kaybı olmaz ve algılama süresi 0'dır.
7. **Split-brain koruması:** monoton artan `MigrationEpoch` ve deterministik halef sırası.
8. **Test:** Gauntlet, localhost'ta 1 host + N istemci çalıştırır ve T sn sonra host process'ini öldürür. Loglarda `KGMigration: Restored epoch=2` aranır.

**Günlük kod kuralları (baştan zorunlu):**
- Oynanışla ilgili her aktörde `UKGSnapshotComponent` olur. Süreler `UKGMatchClock`'tan, rastgelelik `FKGRng`'den gelir.
- Oyun durumu UI'da ya da Blueprint yerel değişkeninde tutulmaz. Olaylar tekrar oynatılabilir (idempotent) ve sıra numaralı olur.

### Bant genişliği (20 oyuncu)
- İstemci başına ortalama **~16 KB/s**, tepe **32 KB/s**. Host upload'ı tipik 2.4 Mbps, tepede 4.9 Mbps.
- Ayarlar: `MaxClientRate=40000`, `NetServerMaxTickRate=30`, `GameSession MaxPlayers=20`, adaptif güncelleme frekansı açık. `UpdateNetSpeeds` **asla** çağrılmaz.
- **Bölge tabanlı relevancy:** Yüzey / Yeraltı / İç_<Bina>. Aynı bölgede olan, 25 m içindeki ya da toplantıdaki oyuncular relevant sayılır. Agresif dormancy uygulanır: kapılar, istasyonlar ve uyuyan fizik eşyası dormant olur.
- Replikasyon **legacy** sistemde kalır ama kod Iris'e hazır yazılır: registered subobject list, FastArray, push-model makroları. Iris ayrı bir dalda aylık test edilir.

## 5. Ses (EOS RTC)
| Faz | Ne |
|---|---|
| **Faz 1 (prototip)** | Lobi RTC odası. Her istemci 10 Hz'de ses seviyelerini ayarlar: yaşayan için mesafe (3 m'de 1.0, 25 m'de 0), duvar ya da bölge arkası yarı ses. Ölüler yaşayanlara **sessiz**dir. Toplantıda herkes tam ses duyar. Gece Sabırsız telsizi ayrı kural olarak işler. |
| **Faz 2 (playtest öncesi)** | `bUseManualAudioOutput` ile her konuşmacının PCM'i, kafa kemiğine takılı bir `UKGVoiceSynthComponent`'e akar. **Gerçek 3D, occlusion ve reverb** gelir, hayaletler için "ruhani" submix kullanılır. EOS SDK doğrudan çağrılır. |
| **Faz 3 (sertleştirme)** | Hayaletler ayrı bir lobi odasında olur. Yaşayanlar hayalet sesini **hiç almaz**, bu da hileye karşı korur. |

- **Ağız:** `RegisterOnVoiceChatBeforeRecvUnmixedAudioRenderedDelegate` konuşmacı başına PCM verir. Bu `Audio::FEnvelopeFollower`'a girer (attack 10 ms, release 120 ms). Oyun iş parçacığında AnimBP'nin `JawOpen` eğrisine (0–1) aktarılır, ~−45 dBFS kapısı ve gamma 0.6 uygulanır. Kendi ağzın için capture delegate'i kullanılır.
- **Susturma, engelleme, raporlama ilk günden var.** Yoksa oyun PEGI 18 alır.
- **Doğrulanacak:** EOS RTC odasının katılımcı sınırı 20 veya üstü mü? Değilse yedek plan: ODIN (4Players).

## 6. First-person karakter ve viewmodel (CS2 hissi)
- **Motorun kendi First Person Rendering'i** (5.8'de doğrulandı):
  - Kollar ve eldeki eşya: `SetOnlyOwnerSee(true)` + `FirstPersonPrimitiveType = FirstPerson`.
  - Kamera: `bEnableFirstPersonFieldOfView`, `FirstPersonFieldOfView = 70–74`, `bEnableFirstPersonScale`, `FirstPersonScale = 0.6`.
  - Tam gövde: `SetOwnerNoSee(true)` + VSM kapalı olduğu için `bCastHiddenShadow = true`. Böylece kendi gölgeni görürsün.
  - Mobil base pass da bu dönüşümü destekler.
  - Sonuç: silah **duvara girmez**, FOV'u ayrı ayarlanır.
- **Animasyon katmanları:**
  1. Kol AnimBP'sine eşya başına **Linked Anim Layer** (Lyra deseni).
  2. C++'ta **yay (spring) tabanlı** prosedürel katman: fare **sway**'i, adım fazına bağlı sekiz şeklinde **bob**, atış başına **recoil** yayları, iniş ve zıplama sarsıntısı. 240 Hz alt adımla çalıştığı için 30 fps'te de 300 fps'te de aynı hissi verir.
  3. Montajlar: çekme, saldırı, dolum ve **inspect**. Inspect ateş, dolum ya da nişan ile 0.1 sn blend'le iptal olur.
  4. Recoil desenleri silah başına `UCurveVector` ile tanımlanır, deterministik ve öğrenilebilir (CS hissi).
- **CS2 tarzı viewmodel ayarları** (oyuncu tercihi, `UKGViewmodelSettings`, yerel kaydedilir):

  | Ayar | CS2 karşılığı | Aralık |
  |---|---|---|
  | **Hazır konum 1 / 2 / 3** (Masaüstü / Kanepe / Klasik) | `viewmodel_presetpos` | Farklı X/Y/Z offset ve FOV setleri |
  | Offset X / Y / Z | `viewmodel_offset_x/y/z` | −2.5…2.5 / −2…2 / −2…2 (cm, ölçeklenmiş) |
  | Viewmodel FOV | `viewmodel_fov` | 54–68 (kamera FOV'undan ayrı) → `FirstPersonFieldOfView` |
  | Sağ / sol el | `cl_righthand` | Viewmodel aynalanır (arms root scale Y = −1, animasyon ve ateş noktası düzeltmesiyle) |
  | Bob miktarı | `cl_bob*` (eski) | 0–100% |
  | Sway / gecikme | — | 0–100% |
  | Inspect tuşu | `+lookatweapon` | varsayılan `F` |
  | Nişan alırken merkezle | `cl_prefer_lefthanded` vb. | aç/kapa |

  Offsetler kol kök kemiğine additive transform olarak uygulanır, FOV doğrudan kamera parametresine gider. Değişiklikler ayarlar ekranında **canlı önizlenir**.
- **Eldivenler ve kollar:** eldiven kozmetiği ve ceket kolu viewmodel'de görünür (CS2'deki gibi). Kukla bileği ve eldiven sınırı modelde temiz tutulur.
- **3. şahıs gövde:** blendspace + state machine. Ucuz, tahmin edilebilir, mobil dostu. Motion matching sonra, ölçeklenebilirlik anahtarının arkasında denenir. Uzak karakterlerde Animation Budget Allocator ve URO kullanılır.
- **Kukla rig'i** (sanat yönü onaylanırsa): rigid parçalar ve menteşeli çene kemiği. `JawOpen` doğrudan çene kemiğini döndürür.

## 7. Fizik
- **Karakterler** kinematik `CharacterMovementComponent` kullanır.
- **Tutulabilir eşyalar:** `PredictiveInterpolation` replikasyon modu.
  - Tutan istemci 30 Hz'de hedef noktayı gönderir. Sunucu `PhysicsHandle` ya da PD yayıyla gerçek gövdeyi taşır.
  - Tutan istemci eşyayı **yerel görsel takip** ile kendi elinde görür, bırakınca 150 ms'de gerçek duruma blend olur.
  - Çok kişi taşıyınca kuvvetler toplanır.
- **Bütçe:** en fazla 150 simüle gövde, aynı anda en fazla 20 uyanık. Uyuyan gövde dormant olur.
- **Kırılabilirler:** sadece **kırılma olayı** replike edilir (`{Id, nokta, impuls, seed}`). Parçalar her istemcide kozmetik simüle edilir, 5–10 sn sonra silinir. `bBroken` bayrağı geç katılan ve migration için tutulur. Mobilde önceden kırılmış mesh ve Niagara patlaması kullanılır.
- **Ağaç devirme:** HISM instance'ı ilk vuruşta tek bir aktöre döner. Devrilme menteşe kısıtıyla sunucuda simüle edilir. Aynı anda en fazla 3 ağaç devrilir.

## 8. Render ve optimizasyon ("ilk günden")
1. **Tek içerik standardı:** bütün master materyaller mobil ES3.1/Vulkan için derlenir. Toplam en fazla ~8 master materyal olur. Mobile Preview haftalık test edilir.
2. **PC:** deferred renderer, **Lumen ve VSM kapalı** (ileride opsiyonel "Ultra"), CSM 3 kaskad, TSR. **Mobil:** forward renderer, MSAA 4x. **Nanite bağımlılığı yok**, LOD'lar elle üretilir.
3. **Gün-gece:** tek hareketli güneş/ay, eğri tabanlı renk rampaları ve 3–4 önceden yakalanmış skylight cubemap karışımı. `UKGTimeOfDaySubsystem` sadece `TimeOfDay` float'ını replike eder.
4. **World Partition:** 64 m hücre, PC'de ~160 m, mobilde ~96–128 m yükleme mesafesi. Yeraltı ayrı Data Layer'dır. HLOD katmanları kullanılır. **Oynanış aktörleri `Is Spatially Loaded = false`** (replikasyon ve migration bozulmasın).
5. **Instancing:** çit, lamba, kasa gibi tekrarlayan eşyalar ISM/HISM ya da PCG ile yerleşir. Evler Packed Level Actor olur.
6. **Ölçeklenebilirlik:** 4 PC kademesi + Android/iOS cihaz profilleri. Mobil hedef 30 fps (sonra 60), dinamik çözünürlük.
7. **Profil:** 1. haftadan itibaren `stat unit`, `stat rhi`, Unreal Insights (`-trace=default,net`). Gauntlet 20-bot testi performans kapısı olarak kullanılır.

**PC performans hedefi:** RTX 3060 / Ryzen 5 3600 seviyesinde, 1080p Yüksek ayarda, 20 oyuncuyla **120 fps**. Host'un ek maliyeti ≤ 2.5 ms/kare.

## 9. Online servisler
- **OnlineSubsystemEOS** (OSSv1): `bUseEOSConnect=true`. Geliştirmede Dev Auth Tool, PC mağazada Steam ya da Epic girişi, mobilde özel DeviceID girişi (`EOS_Connect_CreateDeviceId`).
- **Yayın mağazası kararı sonraya kaldı:** Steam girişi `NativePlatformService=Steam` ile eklenir, EOS lobi, P2P ve ses aynı kalır.
- **Mobil blokajlar:** Android ve iOS hedef platformları yüklü değil. iOS, 5.8.3 EOS eklentilerinin izin listesinde yok. Bu yüzden mobil fazda proje-içi eklenti kopyası ya da Redpoint değerlendirilecek.

## 10. Kozmetik güvenliği (P2P)
- Sahiplik kaynağı: PC'de Steam Inventory (mağaza Steam olursa) ya da **küçük bir backend**.
- Oyuncu lobiye girince backend tarafından **Ed25519 ile imzalanmış** bir yetki tokeni sunar: `{PUID, loadout, son kullanma 2 saat}`. Her eş bunu oyuna gömülü açık anahtarla çevrimdışı doğrular. Doğrulanamayan kozmetik **varsayılan skin** olarak görünür, kimse atılmaz.
- XP, Altın ve MMR: maç sonucunu **her katılımcı** imzalı olarak raporlar, backend çoğunluk uyuşursa kabul eder. EOS Stats ve Player Data Storage ekonomi için **kullanılmaz**, sadece tercihler için.

## 11. Rol gizliliği ve güven
- Host her şeyi bilir. Bu listen-server'ın doğası, arkadaş oyunu için kabul edilebilir.
- Halef adaylarına giden snapshot v1'de düz gönderilir. **v2:** snapshot şifrelenir ve anahtar Shamir 3-of-N ile diğer eşlere bölünür.
- Rol ataması maç başında host tarafından imzalanır. Migration'da yeni host bu imzayı doğrular, böylece rol sahteciliği engellenir.

## 12. Geliştirici araçları: Dev panel ve `kg.*` komutları
Sadece geliştirme build'lerinde (`!UE_BUILD_SHIPPING`). Kod: `Source/KillGodot/Dev/` (`KGDevCommands` fiil tablosu, `UKGDevComponent`
client->host RPC ve hile durumu, `UKGDevSubsystem` F1 ve bileşen ekleme, `SKGDevPanel` Slate paneli) ve game mode kancaları
`Core/KGGameModeDev.cpp` (`DevAddBots`, `DevRemoveBots`, `DevJumpToPhase`, `DevForceWin`, `DevResetTasks`, `DevClockScale`).

- **Aç/kapat:** `F1` ya da `kg.Dev`. Panel sağa yaslı, kaydırılabilir; WASD çalışmaya devam eder, fare paneli kullanır.
  Sekmeler: Match, Bots, Me, World, Chores, Debug. Her buton bir fiil satırıdır; butonun üstüne gelince `kg.*` karşılığı görünür.
- **Tek yol:** panel, konsol (`kg.Bot.Add 5` ya da `kg.Dev Bot.Add 5`), otomasyon ve ajanlar (`FKGDev::Execute(World, Satır)`)
  aynı sunucu fonksiyonlarını çağırır. `kg.Dev.Help` bütün listeyi loga yazar.
- **Yetki:** `[host]` fiilleri sadece host (listen server / standalone) çalıştırır. Client isteği `UKGDevComponent::ServerRun`
  ile gider ve `kg.Dev.AllowClients 1` değilse reddedilir; client paneli o zaman salt okunur. Görünüm fiilleri (stat, perf,
  viewmodel, HUD demo) her makinede yereldir.
- **Test:** `KillGodot.Dev.VerbTable`, `KillGodot.Dev.ServerVerbs` (başsız sunucu dünyası: bot ekle/çıkar, faz atlama, rol, kazanma).
- **Görev mini oyunları** (`Source/KillGodot/Chores/`, tasarım `01_GDD_Core.md §8.1`, arayüz `08_UI_UX.md §6.3`):
  `UKGChoreComponent` (AKGCharacter alt nesnesi) sunucu oturumu açar (`AuthOpen`: görev, aşama, rastgele jeton), sahibine
  `ClientOpen` ile paneli açtırır, her aşama raporunu `FKGChoreRules::CheckStage` ile doğrular (görev/jeton/aşama sırası +
  sunucunun kendi ölçtüğü süre ≥ aşama alt sınırı × 0.9; anlık bitirme yok) ve kesintileri her tick kontrol eder (ölüm,
  toplantı/mahkeme, hasar, 110 cm'den fazla itilme). Çok aşamalı görevlerde aşama `Saved` dizisinde kalır. Son aşama
  görevi listeden düşer; `AKGGameMode::OnTaskCompleted` Sabırsız için hiçbir şey saymaz (sahte). Görsel görevler
  `AKGChoreFx` (her zaman ilgili, sunucu ilk kullanımda yaratır, `Events` dizisi replike) ile herkese oynar; sahtede asla.
  Arayüz: `SKGChorePanel` (Slate, `FKGMenuStyle` paleti) + `FKGMinigame` (saf C++, 640×400 mantıksal tuval,
  `FKGMgPainter`), 22 oyun `Chores/UI/KGMinigames_*.cpp`. Mini oyunu olmayan istasyon ve botlar eski "yakında dur" yolunu
  kullanır. Sesler `Tools/Audio/kg_synth_sfx.py` (`S_Chore_*`, `S_UI_*`), içe aktarma + efekt materyalleri
  `Tools/Unreal/kg_import_chore_assets.py` (commandlet). Testler: `KillGodot.Chores.Catalog|Rules|MinigameLogic|ServerValidation|FakeTask`.

| Komut | Ne yapar |
|---|---|
| `kg.Match.Start [Seed]` · `kg.Match.Restart` | Maçı başlat (lobi varsa geri sayımı başlatır) · haritayı yeniden yükle |
| `kg.Match.Phase <Warmup\|RoleReveal\|Dawn\|Day\|Meeting\|Trial\|Night\|Epilogue\|Lobby>` | Faza atla (roller dağıtılmadıysa dağıtır, Trial'a bir bot çıkar, atlama kimseyi asmaz) |
| `kg.Match.Freeze [0\|1]` · `kg.Match.Skip` · `kg.Match.Time <s>` · `kg.Match.Speed <x>` | Sayacı dondur · fazı bitir · kalan süre · faz saati hızı |
| `kg.Match.Win <Town\|Impatient\|Neutral>` · `kg.Match.Reveal` · `kg.Match.Seed` | Kazananı ilan et · bütün rolleri aç · maç tohumu |
| `kg.Bot.Add [N]` · `kg.Bot.Fill <N>` · `kg.Bot.Remove [N\|İsim]` · `kg.Bot.RemoveAll` | Bot ekle · N oyuncuya tamamla · en yenileri ya da isimle çıkar · hepsini çıkar |
| `kg.Bot.AI [0\|1]` · `kg.Bot.List` · `kg.Bot.Goto <İsim\|#i>` | Bot beyinlerini dondur (`kg.BotAI`) · listeyi loga yaz · arkasına ışınlan |
| `kg.Me.Role <RolId> [Oyuncu]` | Gizli rolü ayarla (katalog: `FKGRoleListGenerator::GetDefaultCatalog`) |
| `kg.Me.God` · `kg.Me.Fly` · `kg.Me.Speed <x>` | Ölümsüzlük · uçma/noclip (Space yukarı, Ctrl/C aşağı) · hareket hızı çarpanı |
| `kg.Me.Heal` · `kg.Me.Stamina` · `kg.Me.Kill [Oyuncu]` · `kg.Me.Revive [Oyuncu]` · `kg.Me.Blade` | Can · stamina · öl/hayalet ol · dirilt · bıçağı çek/koy |
| `kg.Me.Give <Eşya> [N]` · `kg.Me.Coins [N]` · `kg.Me.Gold [N]` | Cebe eşya (`kg.ListItems`) · maç içi Coin · profil altını (yerel) |
| `kg.Spawn.Chest [Kilitli] [Benim]` · `kg.Spawn.Seat [Stool\|Chair\|Bench]` · `kg.Spawn.Pickup <Eşya> [N]` · `kg.Spawn.Loot [Tablo] [Seed]` · `kg.Spawn.Crate [N]` | Önüne sandık / oturak / yerden alınan eşya / ganimet / kırılabilir kasa |
| `kg.World.Goto <Yer\|X Y Z>` · `kg.World.Places` | Adlı yere ışınlan (AKGMapInfo bölgeleri, `KG_Loc_*` etiketleri, yoksa `Tools/Level` layout JSON) · listele |
| `kg.World.Look <Day\|Dusk\|Night\|Dawn>` · `kg.World.Sun <Pitch> [Yaw] [Lux]` | Güneş önizlemesi (yerel, sonraki faz geri karıştırır) |
| `kg.World.Hide <Grass\|Foliage\|Dress\|Village> [0\|1]` · `kg.World.Stat <fps\|unit\|net\|...>` · `kg.World.NavMesh` · `kg.World.ChoreMarkers` | Perf testi için grup gizle · stat · navmesh · görev işaretleri (yerel) |
| `kg.Chore.List` · `kg.Chore.Done [Id\|all]` · `kg.Chore.Reset [all]` · `kg.Chore.Goto <Id>` | Görevler: listele · tamamla · yeniden aç / herkese yeniden dağıt · istasyona ışınlan |
| `kg.Chore.Play <Id>` [host] · `kg.Chore.AutoWin [0\|1]` · `kg.ChoreShot <Id\|all>[@Aşama] [WxH] [fake]` | Mini oyunu olduğun yerde aç (sunucu doğrular; listende yoksa pratik, sayılmaz) · mini oyunlar kendini oynar (her aşama sunucu alt sınırından hemen sonra biter; yerel) · panelleri ekran dışı PNG'ye çiz (`Tools/Unreal/kg_chore_shots.ps1`, `Saved/UIShots/chore_*.png`) |
| `kg.Fish.Give [Rod\|<Tür> [kg]\|<Eşya>]` · `kg.Fish.Bite [Tür\|Eşya]` · `kg.Fish.Land` · `kg.Fish.Sell` · `kg.Fish.Market` [host] · `kg.Fish.Tension [0\|1]` · `kg.Fish.Species` | Balık tutma: olta / tartılı balık / çöp ver · hemen ısırık (gerekirse oltayı çıkarıp atar) · oltadaki balığı çek · cebi sat · önüne Madam Brine tezgâhı · gerilim hata ayıklama katmanı · tür tablosunu logla (`01_GDD_Core.md §15`, ağ testi `Tools/Unreal/kg_fish_smoke.ps1`) |
| `kg.Emote <Id\|stop\|list>` · `kg.Emote.Bots <Id\|all\|stop>` [host] · `kg.Emote.Cam [0\|1]` | Emote oynat (sohbet rölesi + sunucu kuralları, `08_UI_UX.md §6.2`) · bütün botlara emote · kendi bedenine 3. şahıstan bak |
| `kg.Debug.HUDDemo <0-5>` · `kg.Debug.VM <Preset\|FOV\|X\|Y\|Z\|Bob\|Sway\|Left> <v>` · `kg.Debug.VMTune <Prop> <v>` | HUD demo durumları · viewmodel ayarı (`ApplySettings`) · viewmodel tuning (yansıma) |
| `kg.Debug.Dump` · `kg.Debug.Net` · `kg.Dev.Help` | Durumu loga (`KG_DUMP`) ve panoya kopyala · ağ istatistiği · komut listesi |

Önceki araçlar da duruyor: `kg.Act`, `kg.SkipPhase`, `kg.BotFill`, `kg.AutoStart`, `kg.WarmupSeconds`, `kg.EpilogueSeconds`,
`kg.GiveItem`, `kg.SpawnChest`, `kg.SpawnSeat`, `kg.SpawnPickup`, `kg.SpawnLoot`, `kg.GiveGold`, `kg.HUDDemo`, `kg.After`.
