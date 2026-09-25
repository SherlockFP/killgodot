# KILL GODOT — Arayüz ve Deneyim

## 1. Görsel dil
- **Malzeme:** parşömen, balmumu mühür, pirinç saat parçaları, boyalı ahşap tabelalar. Ama **modern ve temiz** bir yerleşimle: bol boşluk, net hiyerarşi, yumuşak gölgeler.
- **Yazı tipleri:** başlıklar için gotik-koloni karakterli bir display font, gövde için okunaklı bir serif ya da sans. **Şart:** Türkçe (ğ ş ı İ ç ö ü) ve **Kiril** karakter desteği. Seçim OFL lisanslı Google Fonts'tan yapılır, liste asset araştırmasında.
- **Renk:** krem parşömen zemin, koyu mürekkep metin, vurgu için Fener Işığı turuncusu. Sabırsız UI'ı kırmızı, hayalet UI'ı camgöbeği.
- **Hareket:** her şey yaylanarak açılır (spring easing), 150–250 ms. Butonlar hover'da hafifçe büyür ve "tahta tık" sesi çıkarır. Sayfalar kitap sayfası çevirir gibi geçer.

## 1.1 Stil sayfası (SPRINT-025, `Source/KillGodot/UI/KGUITokens.h`)
Tek görsel dil: maç içi HUD (`UI/KGHUD.cpp`, canvas), görev paneli (`Chores/UI`, Slate) ve ön yüz (`UI/Menu`, Slate)
renk rollerini, yazı ölçeğini, boşluk ızgarasını, köşe yarıçaplarını ve hareketi **aynı başlıktan** okur; bir değişiklik
her yere birden iner.

| Rol | Değer | Nerede |
|---|---|---|
| `Ink` / `Night` / `Panel` / `PanelHi` | `#120D17` / `#1D1524` / `#2A1F31` / `#3A2B42` | kart zemini (üst→alt gradyan), satırlar |
| `Cream` / `CreamDim` / `Muted` | `#F6E7C8` / `#CDBB98` / `#8E7D6E` | metin, ikincil metin, başlık altı |
| `Gold` / `GoldLight` | `#F2C230` / `#FFE096` | pirinç: odak, saat, tuş kapağı halkası, çubuk üstü |
| `Lantern` | `#F28C28` | **görev işaretçileri**, izleyicideki aktif satır, kart üst şeridi |
| `Good` / `GoodLight` | `#8BD160` / `#AAECA0` | bitti, hazırlık çubuğu |
| `Crimson` / `CrimsonText` | `#C8102E` / `#FF485E` | Sabırsız, hasar, uyarı |
| `Ghost`, `Neutral`, `Night2`, `Dawn`, `Water` | `#4FD1C5`, `#B696F2`, `#8CACFF`, `#FF925C`, `#3FA7D6` | hayalet, tarafsız, gece, şafak, sıvı / eşya işaretçisi |

- **Yazı ölçeği (1080p px, Roboto):** Display 38 (saat) · Title 24 (faz adı, kart başlığı) · Heading 19 (satırlar,
  E ipucu) · Body 16 (adım satırı) · Caption 13 büyük harf, 2.5 aralık (`CHORES`, `VILLAGE PREPARATION`) · Micro 11 (etiket).
- **Boşluk ızgarası:** 8 px. Ekran kenarından karta 24, kartlar arası 16, kart içi 20, liste satırı 36.
- **Şekil:** kart köşesi 18, iç kutu 12, tuş kapağı 6; kartlar %72 saydam alacakaranlık, hap ipuçları %62.
- **Hareket:** giriş 0.25 sn ease-out-back (küçük yaylanma), çıkış 0.18 sn ease-out-cubic, toast 2.5 sn.
- **Yerleşim tablosu:** `UI/KGHudLayout.h::Compute(W, H, satır, hazırlık)` sabit kartların dikdörtgenlerini verir
  (izleyici, faz kartı, pusula, mini harita, can, kese, rol çipi, ipucu). HUD bu dikdörtgenlerden çizer;
  `KillGodot.HUD.Layout` testi 1280×720'den 3440×1440'a kadar hiçbirinin taşmadığını / çakışmadığını kanıtlar.
- **Ekran görüntüsü:** `Tools/Unreal/kg_hud_shots.ps1 -Sizes "1280x720 1920x1080" -Tag after` → `Saved/Screenshots/HUD/`
  (yakın / orta / uzak / arkada işaretçi, M basılı büyük harita, M dokunma tam harita, demo durumları).

## 2. Popup ve bildirimler ("güzel gözükmeli")
| Tip | Görünüm |
|---|---|
| **Duyuru** (şafak, ölüm) | Ekranın üstünden **sarkıtılan tahta tabela**. İpler sallanır, üstündeki mühür kırılarak açılır. |
| **Rol Kartı** | Tarot kartı. Arkası kozmetik, 3D dönerek açılır. Kenarlarında taraf rengi parlar. |
| **Toast** (görev bitti, bakır kazandın) | Sağ altta küçük parşömen şerit. Kıvrılarak açılır, 2.5 sn sonra kapanır. |
| **Onay diyaloğu** | Ortada, arka plan hafif bulanık. Balmumu mühürlü "Evet" butonu. |
| **Başarım** | Sol üstte pirinç çerçeveli rozet. Parıltı ve kısa bir çan sesi. |
| **Kasa sonucu** | Tam ekran. Nadirlik renginde ışık huzmeleri, konfeti, eşya 3D döner. |
| **Faz geçişi** | Ekranı saran saat kadranı animasyonu. Akrep ve yelkovan döner, yeni fazın adı yazılır. |

**Kurallar:** aynı anda en fazla 3 toast görünür, fazlası kuyruğa girer. Önemli duyurular oynanışı engellemez, kenarda kalır. "Bir daha gösterme" seçeneği var.

## 3. Ana Menü
- **Menü arka planı canlı 3D bir sahnedir:**
  - Alacakaranlıkta liman. Fener ışını sisi süpürür, **hacimsel bulutlar** yavaşça akar, deniz dalgalanır.
  - Oyuncunun karakteri iskelede oturup olta sallar ve **kendi kozmetiklerini giyer**.
  - Rüzgâr pelerini ve bayrakları dalgalandırır.
- **Solda** dikey menü (tabelalar):

  | Menü | İçerik |
  |---|---|
  | **Oyna** | Hızlı Maç · Lobi Tarayıcı · Lobi Kur · Arcade |
  | **Dolap** | Karakter ve kozmetik. 3D döndürülebilir karakter, silah ve alet **inspect** önizlemesi |
  | **Almanak** | Battle pass |
  | **Koliler** | Kasalar |
  | **Ev** | Önizleme, ileride editör |
  | **Mağaza** | |
  | **Profil** | Rütbe, istatistik, başarımlar, lore sayfaları, fotoğraf albümü |
  | **Ayarlar** | |
  | **Çıkış** | |

- **Logo:** "KILL GODOT". "O" harflerinden biri çatlamış bir saat kadranı. Arada bir yelkovan tıklar.
- **Müzik:** klavsen ve akordeonla yeniden yorumlanmış denizci şarkısı. Gece temasında daha ürkütücü bir versiyonu var.
- Sezon temalı menü sahneleri de kozmetik olarak satılır.
- Easter egg: 3 dk dokunulmazsa karakter uyur ve Haberci Çocuğun kayığı geçer.

## 4. Lobi Tarayıcı
- **Liste satırı:** lobi adı, host ülkesi bayrağı, **ping**, oyuncu sayısı (6–20), mod, dil etiketi (TR/EN/RU), mikrofon zorunlu mu, şifreli mi, **"En İyi Host"** rozeti, arkadaş sayısı.
- **Filtreler:** bölge, mod, oyuncu aralığı, dil, boş yer var mı, şifresiz, ranked/casual.
- **Hızlı Katıl** en iyi eşleşmeye girer. **Kod ile Katıl** 6 haneli kod kullanır. Arkadaş davetleri EOS üzerinden gider.
- Sonuçlar gelirken satırlar kayarak dolar. Boş durum mesajı: *"Kimse gelmedi. Godot da."*

## 4.1 Maç akışı: sunucu listesi → lobi → rol töreni (SPRINT-015)
- **Oyna** doğrudan sunucu tarayıcısını açar; **Oyun kur** tarayıcının başlığındaki düğmedir. Kurmak lobi odasını açar
  (`UI/Menu/SKGLobbyRoom`). Koltuklar adı kendi taşır (`FKGLobbyEntry::Name`): katılan oyuncu, oyuncu durumu
  replike olmadan önce, her makinede **1 sn içinde adıyla** görünür (`Tools/Unreal/kg_lobby_smoke.ps1`).
- Host başlatınca maç **ısınma olmadan doğrudan `RoleReveal` fazına** geçer. Köy, tören bitene kadar görünmez.
- **Rol anı** (`UI/Reveal/SKGRoleReveal`, SPRINT-037; SPRINT-015'in krupiye masası kullanıcı tarafından "sıkıcı,
  okunmuyor" diye reddedildi). Hedef: Among Us "Impostor" ekranı enerjisi, **2 saniyede okunur**. Akış: karanlık oda →
  kart dönerek karanlıktan yükselir → "SEN KİMSİN?" yazısı altında kart titrer, ışık birikir (saat tıkırtılı riser) →
  **kart döner: beyaz flaş, ekran taraf rengine boyanır, kamera vuruşu + sarsıntı, patlama** (Kasaba konfeti, Sabırsız
  kor kıvılcımı, Nötr sim) **ve taraf sesi** (`S_Reveal_Town|Impatient|Neutral`) → üç şey çarpar:
  - **DEV rol adı** (büyük harf, Türkçe İ/I doğru, koyu kontur), üstten iner ve oturur;
  - **taraf bandı** kendi renginde: "SEN SABIRSIZSIN" / "KASABAYLA BEKLİYORSUN" / "KENDİ OYUNUNU OYNUYORSUN";
  - **tek satır, düz sözcüklerle ne yaptığın** (≤ 6 sözcük, 51 rolün hepsi EN/TR: `KGRoleCardText.cpp` `DoRows`).
  Ana ekranda toplam **≤ 12 sözcük** (test `KillGodot.Reveal.MomentText`); paragraf yok.
- **Suç ortakları** (Sabırsız takımlar): kartın iki yanına sırayla çıkan **büyük silüetler**, başlarının üstünde ad
  (yayıncı modunda takma ad) ve küçük rol adı - Among Us dizilişi, tablo değil.
- **Detaylar isteğe bağlı:** **Tab basılı tut** → hedef, yetenekler, lore sözü (ve yayıncı notu) bir panelde. Oyun
  içinde HUD rol kartı da aynı bilgiyi verir. Alt satır: `BOŞLUK HAZIRIM` · `TAB DETAY`, en altta kalan süre çubuğu.
- Görseller: `Tools/UI/kg_make_reveal_art.py` → `Art/UI/Reveal` (fener / hançerli kırık cep saati / iki yüzlü maske,
  3 köylü büstü), içe aktarma `Tools/Unreal/kg_import_reveal_art.py` → `/Game/KillGodot/UI/Reveal`. Sesler
  `Tools/Audio/kg_synth_sfx_reveal.py` → `kg_import_audio.py --only S_Reveal_`. Doku yoksa boyalı yedekler çizilir.
- Zamanlama sunucuda: faz `FKGMatchClock` ile **6 sn** (`KGReveal::PhaseSeconds`, sözleşme 5–7). Vuruşlar: dönüş
  0–0.7, bekleyiş 0.7–1.1, çevirme 1.1–1.3 (**patlama 1.2**), ad 1.25, bant 1.4, satır 1.7, ortaklar 1.9, hepsi
  ekranda 2.4. **Hazır** 1.6'dan itibaren (**Boşluk / A / sol tık**); herkes hazırsa sunucu fazı 0.6 sn'ye (kararma)
  indirir. Tören bitince beden girdisi geri verilir, köy kararmanın altından belirir.
- Gizlilik: rol `AKGPlayerState::PrivateRoleId` (`COND_OwnerOnly`); suç ortakları ve hazır sayacı her insan
  oyuncunun kontrolcüsündeki `UKGRevealComponent`'te, tüm alanlar `COND_OwnerOnly` (test `KillGodot.Reveal.OwnerOnly`).
- Kanıt: `Tools/Unreal/kg_reveal_shots.ps1` → `Saved/UIShots/reveal-<rol>-<aşama>_<W>x<H>.png`, aşamalar
  `spin | tease | flip | slam | role | details` (+ `:ready`, `:streamer`); `kg.UIShot reveal:<Rol>:<aşama>`.

## 5. Lobi: Geç Kalan Meyhanesi (3D)
- Oyuncular meyhanede **karakter olarak** doğar. Yürürler, konuşurlar (proximity voice) ve mini oyun oynarlar: dart, satranç, bilek güreşi, piyano.
- **Arka bahçede atış poligonu** var: silahlar ve skinler burada serbest, inspect gösterisi yapılır.
- **Hazır olmak:** uzun masaya **oturmak**. Masa doldukça mumlar yanar.
- **Sohbet paneli** (sol altta): Lobi kanalı, emoji ve hızlı cümleler. Mesaj geldikçe kaydırma çubuğu parşömen gibi açılır.
- **Ses göstergesi:** konuşanın başının üstünde küçük dalga simgesi ve tabii **açılıp kapanan ağız**. Oyuncu listesinde ses seviyesi kaydırıcısı, sustur, engelle, raporla.
- **Host paneli** (duvardaki pano):
  - mod, rol listesi (şablonlar veya özel editör), süreler, bölge kapıları (otomatik veya elle), meslek seçim modu, hortlak mekaniği aç/kapat
  - maksimum oyuncu, ranked/casual, oyuncu atma ve yasaklama
- **Rol listesi önizlemesi** duvarda tarot kartı dizisi olarak durur. Kartların arkası kapalı, sadece slot kategorileri görünür.

## 6. Maç içi HUD (minimal ve diegetic)
| Öğe | Nasıl |
|---|---|
| Faz ve saat | Sol üstte **cep saati**. Kapak açıktır, gece ay simgesi görünür. |
| Rol | `Tab` basılı tutulunca tarot kartı elde belirir (viewmodel'de). |
| Görevler | `Q` ile parşömen listesi açılır. |
| Defter | `J`. Otomatik olay notları ve elle notlar, vasiyet. |
| Etkileşim | Nesneye bakınca küçük ipucu çıkar. Tutma ve fırlatma gücü göstergesi var. |
| Can ve stamina | Kalp ve nefes simgeleri, sadece değiştiğinde görünür. |
| Sohbet | `Enter`/`T`. Sol altta, can kartının üstünde. Kanallar `Tab` ile: KASABA / YAKIN / SABIRSIZ / HAYALETLER (bkz. §6.1) |
| Toplantı | Ekranın altında oy şeridi. Oyuncu portreleri, oy sayıları, "El kaldır" tuşu. |
| Hayalet | Soluk camgöbeği vinyet, ruh dünyası haritası, Araf kuyruğu butonu. |
| Balık tutma | Güç çubuğu (nişangâhın sağı), ısırıkta "!" + STRIKE, çekmede gerilim çubuğu (mavi gevşek / yeşil iyi / kırmızı kopma), av kartı sağda (bkz. §6.4) |

## 6.1 Yazılı sohbet, emoji ve tepkiler
Kod: `Source/KillGodot/Chat/` (kurallar `FKGChatRules`, ağ `UKGChatComponent`, arayüz `FKGChatUI`). Kurallar **sunucuda**
uygulanır; istemci sadece kanal hapını griye çeker. Mesaj yalnızca okumaya hakkı olan oyuncunun bağlantısına gider
(Client RPC), yani ölü ve Sabırsız sohbeti başkasının ağına hiç düşmez.

**Kanallar (kim yazar, kim okur)** — etiketler TR; İngilizce arayüzde TOWN / NEAR / IMPATIENT / GHOSTS / CRIER:
| Kanal | Etiket | Kim okur |
|---|---|---|
| Kasaba (All) | `KASABA` altın | Herkes. **Ölüler de okur** (GDD §11). |
| Yakın | `YAKIN` gök mavisi | Gönderene 20 m içindeki yaşayanlar + tüm hayaletler (seyirci). |
| Sabırsız (takım) | `SABIRSIZ` kırmızı | Aynı takımdaki **yaşayan** üyeler (Saat Kırıcılar; ileride Vampir/Veba). Yalnız katil ve nötrlerin kanalı yok. Ölüler telsizi duyamaz. |
| Hayaletler | `HAYALETLER` camgöbeği | Sadece hayaletler. Yaşayanlar asla görmez. |
| Tellal (System) | `TELLAL` parşömen | Duyurular, faz geçişleri, suçlamalar, "X oy verdi", kendi ölümün. Oyuncu yazamaz. |

**Faz başına açık kanallar (yaşayan oyuncu için):**
| Faz | Kasaba | Yakın | Sabırsız | Tepki balonu |
|---|---|---|---|---|
| Lobi / Isınma | ✔ | ✔ | – | ✔ |
| Rol Kartı | – | – | ✔ (takım tanışır) | – |
| Şafak | – (tellal konuşur) | ✔ | – | ✔ |
| Gündüz | ✔ | ✔ | – | ✔ |
| Toplantı | ✔ | – | – | ✔ |
| Mahkeme | Savunma (ilk 20 sn) ve son söz (son 8 sn): **sadece sanık**. Karar (15 sn): herkes. | – | – | ✔ |
| Gece | – (sokağa çıkma yasağı) | – | ✔ | – |
| Son Ekran | ✔ (hayaletler dahil) | ✔ | – | ✔ |

- **Hayaletler** her fazda sadece HAYALETLER kanalına yazar (Son Ekranda Kasaba'ya da). Tepkileri balon değil, hayalet sohbetine düşen bir satırdır (bedenleri yok).
- **Şantajcı** hedefi (`UKGChatComponent::SetSilenced`) yazamaz ama tepki/emote kullanabilir. **Hortlak** ne yazar ne tepki verir.
- İsim renkleri oyuncu başına sabittir (EOS PUID, yoksa isim hash'i). Paletten Sabırsız kırmızısı ve hayalet camgöbeği çıkarıldı: renk asla taraf ele vermez.

**Spam ve temizlik (sunucu):** en fazla 140 karakter (emoji 1 karakter), 12 emoji, aynı karakter en fazla 6 kez üst üste,
boşluklar tek boşluğa iner ve kırpılır; kontrol, bidi-override, sıfır genişlikli karakterler ve yabancı özel alan
karakterleri silinir, "zalgo" işaretleri harf başına 1'e iner. Hız sınırı: 4 mesajlık patlama, sonra 1.5 sn'de 1; aynı
mesaj 8 sn içinde tekrar gönderilemez. Tepkiler: 3'lük patlama, saniyede 1.

**Tuşlar:** `Enter`/`T` aç, `/` komutla aç, `Tab`/`Shift+Tab` kanal değiştir, `Esc` kapat, `↑`/`↓` gönderilenleri geri çağır,
`PgUp`/`PgDn` geçmişte kaydır. Yazarken girdi UI-only olur: karakter durur, imleç görünür; kapanınca oyun girdisi geri gelir.
Boştayken son 8 satır görünür ve ~12 sn sonra solar; son 50 satır saklanır.

**Emoji:** Slate renkli emoji fontu çizemediği için oyunun kendi 32'lik çıkartma seti var (`Tools/UI/kg_make_emoji.py`,
PIL ile prosedürel; `Art/UI/Emoji/`, `/Game/KillGodot/UI/Emoji/T_KG_EmojiAtlas(_Small)`, UI grubu, mip yok). Metinde
her emoji tek bir özel alan karakteridir (U+E100+i) ve satır içinde resim olarak çizilir. Yazım: `:skull:` gibi kısa
kodlar (takma adlar da: `:rip:` `:lol:` `:+1:`), `:)` `:D` `:(` `;)` `:o` `<3` `>:(` `:/` `o/` `xD`, yapıştırılan gerçek
Unicode emojiler (💀 → kafatası). Giriş kutusunun sağındaki gülen yüz seçiciyi açar: tık = ekle, sağ tık = tepki,
`Ctrl+1..8` = hızlı tepki. Set: laugh, sus, skull, thumbsup, thumbsdown, heart, wave, angry, smile, cry, shock, sweat,
zzz, wink, clap, pray, knife, eyes, ghost, candle, fire, fish, bell, anchor, question, exclamation, crown, clock (çatlak
saat, logo), mask (Sabırsız maskesi), lantern, mug, moon. **Sıra ağda indeks olarak gider: sadece sona ekle.**

**Tepkiler ve emote köprüsü:** `G` basılı tut → tek çark, iki halka: kısa fare hareketi iç halkadaki 8 emojiyi
(ya da `1-8`), daha uzun hareket dış halkadaki 16 emote'u seçer; bırakınca gönderilir. Balon 2.8 sn kafanın üstünde
yaylanır, 35 m içindekilere (ve hayaletlere) gider, duvar arkasında gizlenir, kendi balonun ekranın alt ortasında
görünür (emote kamerasındayken kendi kafanın üstünde). Emote'lar da aynı sohbet rölesinden geçer: balon + "* İsim el
sallıyor *" satırı + beden animasyonu (§6.2).

### 6.2 Emote'lar (`Source/KillGodot/Emote/`)

**Katalog (tek kaynak `FKGEmoteCatalog`, satır ekle = emote ekle; çark sırası):**

| Emote | Komut (takma adlar) | Beden | Döngü | Birinci şahıs | Klip |
|---|---|---|---|---|---|
| El salla | `/wave` (hi, hello, bye) | üst gövde | – | kol jesti | yazıldı |
| İşaret et | `/point` (look, there) | üst gövde | – | kol jesti | yazıldı |
| Suçla! | `/accuse` (blame, jaccuse) | üst gövde | – | kol jesti (point) | yazıldı |
| Alkış | `/clap` (gg, bravo) | üst gövde | – | kol jesti | yazıldı |
| Coş | `/cheer` (cheers, yay) | üst gövde | – | 3. şahıs | yazıldı |
| Gül | `/laugh` (lol, haha) | tüm beden | – | 3. şahıs | yazıldı |
| Dans | `/dance` (dance1, boogie) | tüm beden | ✔ | 3. şahıs | UAL1 `Dance_Loop` |
| Oltayı Çek | `/reel` (dance2, fishdance) | tüm beden | ✔ | 3. şahıs | yazıldı |
| Yere otur | `/sit` (sleep, rest) | tüm beden | ✔ (giriş + döngü) | 3. şahıs | yazıldı |
| Reverans | `/bow` (pray, thanks) | tüm beden | – | 3. şahıs | yazıldı |
| Selam dur | `/salute` (o7, respect) | üst gövde | – | kol jesti | yazıldı |
| Omuz silk | `/shrug` (idk) | üst gövde | – | 3. şahıs | yazıldı |
| Facepalm | `/facepalm` (fp, ugh) | üst gövde | – | 3. şahıs | yazıldı |
| Sus | `/sus` (think, hmm) | üst gövde | ✔ (8 sn) | 3. şahıs | UAL2 `Idle_FoldArms_Loop` + baş hareketi |
| Ağla | `/cry` (sob, sad) | tüm beden | – | 3. şahıs | yazıldı |
| Tehdit | `/threaten` (angry, yourenext) | üst gövde | – | 3. şahıs | yazıldı |

"Yazıldı" = `Tools/Blender/kg_make_emotes.py` (başsız Blender, UBC iskeleti, 30 fps, yerinde) →
`Tools/Unreal/kg_import_emotes.py` → `/Game/KillGodot/Characters/Villager/Anims/Emotes/A_KG_Emote_*`. Kol jestleri
`kg_make_fp_arms2.py` → `A_FP2_emote_wave|point|clap|salute`. **Tehdit** Sabırsız havalıdır ama herkes kullanabilir, hiçbir
şey ele vermez.

**Kurallar (sunucu, `FKGEmoteRules`, testler `KillGodot.Emote.*`):** tepkilerle aynı fazlar (gece, rol açılışı ve göç
yok); **hayaletler emote yapamaz** (tepkileri hayalet sohbetine düşer); ölü beden, yüzme, merdiven, oturak, elde eşya →
yok; tüm beden emote'ları için dur (hareket, havada, çömelik → "Olduğun yerde dur"). Hız sınırı: 3'lük patlama, sonra
2 sn'de 1 (sohbetin tepki sınırının üstüne). **Biter:** tüm beden emote'u hareket edince (sahip anında keser, sunucu hızla
doğrular), saldırı/itme/etkileşimde, hasar alınca, ölümde, toplantı/mahkeme/gece başlayınca; tek seferlikler klip bitince,
üst gövde döngüleri 8 sn'de.

**Ağ:** `UKGEmoteComponent` (karakterin alt bileşeni) → `FKGEmotePlayback {Emote, Serial}` push-model replike; her makine
bedenin `UKGBodyAnimInstance` emote katmanını oynatır (üst gövde maskesi: spine_01 %35, spine_02 %75, spine_03 ve yukarısı
%100; bacaklar yürüyüşte kalır).

**Birinci şahıs:** tüm beden emote'larında (ve kol jesti olmayan üst gövde emote'larında) kamera 0.45 sn'de gövdenin
arkasına/etrafına açılır (tüm beden: 145° döner, 270 cm; üst gövde: omuz üstü 185 cm), duvarlardan süpürmeyle korunur,
0.35 sn'de kafaya döner; bu sırada kollar/eşya gizli, kendi bedenin ve kozmetiklerin görünür, beden fareyle dönmez
(etrafına bakabilirsin), nişangah yerine "WASD Hareket et: dur" ipucu. El salla / işaret / suçla / alkış / selam dur
birinci şahısta kalır ve kollarda jest oynar (eldeki bıçak jest boyunca gizli).

**Tuşlar:** `G` çarkı (dış halka), `Shift+1..4` favoriler (varsayılan wave, point, dance, sit; `kg.Emote.Favorites`; aynı
tuş tekrar = dur), `/wave /dance /sit ...`, `/stop`, `/emotes`. Çarkta favorilerin köşesinde altın rakam.

**Geliştirici:** `kg.Emote <id|stop|list>`, `kg.Emote.Bots <id|all|stop>` (host), `kg.Emote.Cam [0|1]` (kendi bedenine
3. şahıstan bak), `kg.Emote.CamDistance|CamDistanceUpper|CamOrbit|CamBlendIn|CamBlendOut`; dev panelde "Me" sekmesinde
EMOTES bölümü. Ağ testi: `Tools/Unreal/kg_emote_smoke.ps1`.

**Komutlar:** `/all /near /team /dead <metin>`, `/r <emoji>`, `/mute` `/unmute <isim>` (yerel), `/report <isim>` (şimdilik
log'a delil yazar; EOS rapor arayüzü M3), `/emojis`, `/emotes`, `/stop`, `/help`. Geliştirici: `kg.Chat.Say|React|Emote|Open|Close|
Silence|System|Dump|Demo`, `kg.Chat.WheelSensitivity`.

**Henüz yok:** Fısıltı (3 m, tek kişi) kanalı, lobi (meyhane) kanalı M10'da; metinler LOCTEXT ile, String Table'a
taşınacak (`ST_UI`).

### 6.3 Görev mini oyunları (`Source/KillGodot/Chores/UI/`)
Tasarım ve liste: `01_GDD_Core.md §8.1`. Ekran (`SKGChorePanel`):
- **Pencere:** ekranın ortasında, genişliğin ~%74'ü / yüksekliğin ~%84'ü (hangisi önce dolarsa), köşeleri 26 yuvarlak koyu
  alacakaranlık paneli (`FKGMenuStyle` paleti, Roboto). Dışında dünya **hafif bulanık (3.5) ve %38 karartılmış** görünür:
  arkandan gelen biri seçilebilir. Açılışta 0.22 sn'lik küçük yaylanma.
- **Başlık:** `CHORE` (fener turuncusu, geniş aralık; görsel görevlerde `· VISIBLE TO OTHERS`), görev adı (Black 25),
  altında altın renkli tek satır talimat ("Grab the crank and circle it clockwise…"). Sağda aşama noktaları: bitti = yeşil
  tik, şimdiki = nabız atan altın halka, sonraki = soluk; altında aşama adları. Sağ üstte X.
- **Gövde:** 640×400 mantıksal tuval, her mini oyun kendi sahnesini çizer (kod çizimi, doku yok). İlk hareket için
  nabız atan halka / akan ok ipucu, ilerleme sayaçları, iri tıklama hedefleri (≥ 28 birim).
- **Geri bildirim:** yükselip sönen yazı ("Clean split!", "Too fast - it slips!"), hata sallantısı, küçük sesler
  (`S_UI_*`, `S_Chore_*`, 2D). Aşama bitince yeşil parlama + "AŞAMA DONE" damgası (0.8 sn), son aşamada "CHORE DONE"
  damgası ve `S_TaskDone`, 1.45 sn sonra pencere kapanır.
- **Sunucu reddi:** kırmızı çerçeveli şerit "Too quick to be real - once more!" ve aşama baştan. **Kesinti:** "You were
  hit!", "Pushed away from the chore", "The meeting bell! Chores wait." 0.7 sn görünür, sonra kapanır.
- **Etiketler:** Sabırsız kendi ekranında kırmızı `FAKING - WON'T COUNT` görür (başkası asla görmez); dev pratikte
  camgöbeği `PRACTICE`; alt satırda `AUTO-WIN`.
- **Girdi:** açıkken UI girdisi (imleç görünür, karakter yerinde). `Esc` / X / `W A S D` / `E` = ayrıl (ilerleme saklanır).
  Fare: tıkla, sürükle, daire çiz; bazı oyunlarda `Space` de çalışır.
- **İstasyon ipucu:** mini oyunlu istasyonda saniye yazmaz; yarım kalan çok aşamalıda "(continue 2/3)".
- **Ekran görüntüsü:** `kg.ChoreShot all` (ekran dışı, `-game -RenderOffScreen`) → `Saved/UIShots/chore_<Id>_s<Aşama>_<W>x<H>.png`;
  `Tools/Unreal/kg_chore_shots.ps1 -What all`.

### 6.3.1 Görev görünürlüğü, tek E ipucu ve M basılı büyük harita (SPRINT-025)
Kullanıcı: "görevler basit ve görünür olsun; M basılı tutunca harita büyüsün; UI çok kötü."
- **Dünya işaretçisi** (`UI/KGHUDChoreMarkers.inl`): her açık görevin **şimdiki adımı** için istasyonun / eşyanın
  üstünde fener turuncusu bir raptiye + altında metre (`12 m`). Uzaklıkla küçülür (6 m'de tam, 60 m'de %55), zemine
  yumuşak bir parıltı düşer (post-process yok, kod çizimi). Ekran dışında ya da arkandayken merkezden çıkan ışın
  boyunca kenara kenetlenir ve ok olur (üst kartlar ve can kartı için 150 px'lik bant boş bırakılır). En yakın
  / çalışılan görev "yanar": adım adı raptiyenin üstünde. Görev paneli açıkken, toplantı ve yargılamada çizilmez.
  Sabırsız'ın sahte görevleri aynı listelerden geçer: işaretçi ve satır bire bir aynıdır, kimse başkasınınkini görmez.
- **Görev izleyici** (sol üst): `CHORES 1 / 4` + yeşil ilerleme; her görev **tek kısa satır**:
  `[durum] Ad · sonraki adım ……… 12 m`. Panel görevlerinde adım = aşama adı (`Crank 1/2`), dünya görevlerinde adım
  etiketi (`Carry the bucket to the trough 1/2`, `Pick up your bucket`). Aktif satır fener bandı + raptiye, bitmişler
  yeşil tik + üstü çizili. Adım satırı yer kalmayınca "..." ile kısalır.
- **Tek ipucu** (`DrawActionPrompt`): nişangâhın altında hap: **E tuş kapağı + etrafında ilerleme halkası + fiil**
  (`E  Draw water`, çalışırken `E  Mend the nets  62%`). Tut-E istasyonu, panel istasyonu, dünya adımı (krank, kes,
  dök, sabotaj) hepsi bu hap; aynı anda asla iki ipucu çizilmez.
- **Kolay ayar** (`Chores/UI`): mini oyunlar cömert pencere / az tekrar; `KillGodot.Chores.Timing` testi her mini
  oyunu betikli oyuncuyla (AutoPlay, 60 Hz, 3 tohum) oynatır: medyan ≤ 28 sn (+~12 sn yürüme = 40 sn hedefi) ve
  sunucu tabanı (`StageMinSeconds × 0.9`) kusursuz oyuncudan hızlı olmalı. `KillGodot.Chores.FailKeepsStage`:
  10 sn çöp girdi + sunucu reddi aşamayı asla geri almaz.
- **M:** dokunma tam haritayı açıp kapar (eskisi gibi); **basılı tutma** (≥ 0.3 sn) büyük haritayı tuş basılıyken
  gösterir, bırakınca mini haritaya döner (`UI/KGMapInput.h`, `KillGodot.HUD.MapInput`). Tam haritada her açık adım
  raptiye + görev adı (aktif olan turuncu), sen, yerler, lejant; başlıkta `M  Close · hold to peek` /
  `Release to close`. `kg.Map.Debug 1|2` ekran görüntüsü için basılı / açık zorlar.

### 6.4 Balık tutma HUD'u (`Source/KillGodot/UI/KGHUDFishing.inl`)
Tasarım: `01_GDD_Core.md §15`. Hepsi canvas çizimi, `FKGPainter` paleti.
- **İpucu şeridi** (altta ortada, etiket çipi + tuş kapakları): hazır `[LMB] Hold to cast · [H] Put the rod away`,
  dolarken `Release to cast · [RMB] Cancel`, beklerken `Strike when it bites · [RMB] Reel in`, ısırıkta `[LMB] STRIKE!`,
  çekerken `[LMB] Hold to reel · [A / D] Steer against it · [RMB] Let it go`.
- **Güç çubuğu:** nişangâhın 70 px sağında dikey hap, "CAST" başlığı, yüzde; %85 üstü altın (tatlı nokta çizgisi).
- **Isırık:** nişangâhın üstünde zıplayan altın daire içinde "!" + "STRIKE!" (vurma penceresi boyunca).
- **Çekme paneli** (520x132, altta): başlık "FISH ON!" / "LINE STRAINING - LET GO!" (kırmızı, panel titrer ve kızarır) /
  "SLACK LINE - REEL!" (mavi parıltı); sağda kalan mesafe (m). **Gerilim çubuğu** 0-1.3: gevşek bölge mavi, iyi bölge
  (0.25-0.85) yeşil, kopma bölgesi (>1) kırmızı, beyaz ibre. Altında balık dayanıklılığı (turuncu hap) ve balığın
  çektiği yön oku + karşı tuş kapağı (`A`/`D`).
- **Tünel görüş:** çekerken kenarlarda koyu vinyet + görüş alanı %12 daralır (`kg.Fish.FocusFOV`), ısırıkta kamera tekmesi.
- **Av kartı** (sağ orta, 5.5 sn, içeri kayar): üst bant nadirlik renginde, "CAUGHT!", tür adı (30 px), "3.42 kg - Rare",
  "NEW PERSONAL BEST! (was ...)" / "Your first Salmon!" / "Personal best: ...", "Madam Brine pays ~N gold"; şişede mesaj
  kartta okunur, kese "+N gold coins". Kopma: "LINE SNAPPED" (kırmızı) + "It felt like a 6.2 kg Cod..."; kaçış
  "IT GOT AWAY"; koi: "SACRILEGE!" (pembe) + kesilen vergi.
- **Bildirimler** (ortanın altında çip, 3 sn): rod yok, çok erken, ısırık kaçtı, uzaklaştın, ödünç olta, satış, cep dolu, irkildin.
- **Dev katmanı:** `kg.Fish.Tension` sol ortada tahmin vs sunucu gerilim/dayanıklılık/mesafe, yön takvimi, seri numaraları.

### 6.5 Yayıncı modu (Ayarlar → Oynanış → Yayıncı modu, SPRINT-015)
Tamamen yerel: diğer oyuncular hiçbir fark görmez. `UKGGameUserSettings::bStreamerMode` + `StreamerPeekKey`
(varsayılan **Tab**; Sol Alt / Caps Lock / Q seçilebilir). Kod: `UI/Reveal/KGStreamerMode`.
- **Rol gizli:** tören bittikten sonra HUD'daki rol çipi "ROL GİZLİ · basılı tut" çipine döner, rol kartı çizilmez.
  Tuş basılıyken rol görünür. (Görevlerdeki "FAKING" etiketi `Chores/` altında: kanca önerisi Backlog'da.)
- **Takma adlar:** diğer oyuncuların adı maç boyu sabit, köy temalı takma adlarla değişir ("Foggy Herring"):
  lobi, sohbet (gönderen + metin içindeki adlar), tellal duyuruları, toplantı sayımı, dava, epilog, tören suç ortakları,
  tarayıcıdaki host adları. Eşleme dünya başına rastgele tuzla (`FKGPseudonyms`): maç içinde sabit, maçtan maça farklı,
  hiçbir zaman gerçek ad değil (test `KillGodot.Reveal.Pseudonyms`). Kendi adın değişmez.
- **Maskeleme:** lobi katılım kodu `••••••`, host ekranındaki LAN adresi ve tarayıcıdaki kod/adres kutusu gizli.
- Dev: `kg.Streamer 1|0|dump`, komut satırı `-KGStreamer`.

## 7. Dil ve lokalizasyon
- **Çıkış dilleri:** **Türkçe, İngilizce, Rusça.** Sonra Almanca, İspanyolca, Portekizce (BR), Lehçe, Çince ve daha fazlası eklenebilir.
- **Kurallar:**
  - Tüm metinler UE **String Table**'larında tutulur (`ST_UI`, `ST_Roles`, `ST_Lore`, `ST_Memes`). Kodda sabit metin olmaz.
  - Yazı tipleri Türkçe ve Kiril karakterleri destekler. Rusça metinler ~%30 daha uzun olabilir, UI esnek genişlikli tasarlanır.
  - Çoğul ve cinsiyet kuralları için UE'nin ICU format metinleri (`{0}|plural(...)`) kullanılır. Rusçanın çoğul formları farklıdır, buna dikkat.
  - **NPC ve kukla sesleri:** anlamsız, sevimli bir **kukla dili** (Simlish tarzı). Seslendirme lokalizasyonu gerekmez, altyazı çevrilir.
  - Lobi tarayıcısında dil etiketi olur, eşleştirme dil tercihine göre öncelik verir.
  - Meme içerikleri dile özgü olabilir: `DT_MemeRotation` satırlarına dil maskesi eklenir.

## 7.1 Ayarlar → Viewmodel (CS2 tarzı)
- **Hazır Konum 1 / 2 / 3** butonları ve bunların yanında X/Y/Z offset ile FOV kaydırıcıları.
- Sağ/sol el anahtarı, bob ve sway kaydırıcıları.
- Ekranın sağında **canlı önizleme**: seçili eşya (tava, El Topu, şemsiye…) eldeyken ayarlar anında görünür. "Inspect'i oynat" butonu var.
- Profil kodu paylaşımı: viewmodel ayarların kısa bir koda dönüşür (ör. `KG-VM-2-0.5-1-0-62`), arkadaşın yapıştırır.

## 8. Erişilebilirlik ve kontroller
- Renk körlüğü paletleri. Taraf renkleri ayrıca simge ve desenle ayrışır.
- Altyazılar (NPC ve olay sesleri). Ekran sarsıntısı ve bulanıklık kaydırıcıları.
- **Bas-Konuş / Açık Mikrofon / Ses Aktivasyonu** ayarları, mikrofon testi, gürültü bastırma.
- Tüm tuşlar değiştirilebilir. Gamepad desteği CommonUI ile gelir. Mobil için dokunmatik düzen (sonraki faz).
