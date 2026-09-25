# Ganimet, Zanaat ve Çakmaklı Tabanca (Loot + Crafting + The Gun) — Tasarım v1

> **Durum:** Öneri, kullanıcı onayı gerekiyor (2026-09-25). Hiçbir kod, seviye ya da mevcut belge değişmedi.
> **Kaynak:** kullanıcının isteği: kırılabilir kutulardan şansa bağlı bandaj ve silah parçaları; parçalar birleşince GMod
> Murder'daki gibi bir silah olur. Kullanıcı "ben yüzeysel söyledim, dengeyi ve içeriği sen düşün" dedi.
> **Charter:** `Docs/Design/KillGo_Pillars.md` v1.1. Her sayı bir hedef koduna bağlıdır (S…, G…). 12 maddelik kontrol
> listesi §12'de.
> **Kapsam kuralı (charter §0.1):** Bu belgedeki içeriğin çoğu zaten kâğıtta var: GDD §4 "Silahlar (ana mod): nadir
> loot, tek mermili çakmaklı tabanca… masum kasabalıyı vuran silahı düşürür, 30 sn bulanık görür ve o maç boyunca silah
> alamaz", `02_Roles.md`'de İnfazcı'nın gizli tabancası, Kaçakçı'nın zuladan tabancası, Barut Ustası'nın "kaçak silah"
> sezgisi, Doktor'un 40 canlık bandajı ve `01b_Village_Life_Fun.md`'de Bay Thimble'ın bandaj satışı. **Yeni olan iki
> sistem** var: parçadan zanaat (GDD'deki "doğrudan loot tabanca" yerine) ve Ganimet Yönetmeni (günlük bütçe). İkisi de
> Backlog "Proposed (needs approval)" maddesidir.
> **Kardeş belgeler:** orman tehlikesi, orman görevleri ve masa oyunları başka ajanlarca ayrı belgelerde yazılıyor.
> Arayüzleri §10.4'te.

---

## 0. Özet (tek ekranda)

1. Köyde **kırılabilir kaplar** (sandık, fıçı, küp) var. 2 yumrukta kırılırlar, ganimet saçarlar ve **her şafakta yeniden
   bütündürler**: köy aynı sabaha uyanır, ama içleri her sabah başkadır.
2. Kaplardan çoğunlukla sıradan eşya çıkar (para, yiyecek, hurda). **Sargı bezi** (%11) ve **tabanca parçaları** daha
   nadirdir.
3. Parçalar serbest bir zar değildir. **Ganimet Yönetmeni** (sunucu, tohumlu `FKGRng`) her şafakta dünyadaki bulunmamış
   parça stoğunu N'ye göre sabit bir bütçeye tamamlar. Silah sayısı böylece varyanslı değil, öngörülebilir olur (G4).
4. Tabanca **3 farklı parçadan** yapılır: **Namlu, Çakmak, Kabza**. Aynı parçanın ikincisi tek başına işe yaramaz, bu
   yüzden **takas** gerekir: "Kimde çakmak var?" (güven kararı).
5. Zanaat sadece **Demirhane tezgâhında** yapılır (Dere Boyu): 8 sn, 35 m'den duyulan örs sesi, ocak parlaması, Kasaba
   Panosu'nda isimsiz satır ("Demirhane: 2 numaralı tabanca dövüldü, Gündüz 2 14:20"). **Ocak 1. gün soğuktur.**
6. **Çakmaklı tabanca:** tek atım, horoz çekilmeden ateş etmez, 12 m içinde öldürür, uzakta yaralar. Ateş 60 m'den
   duyulur ve duman bırakır. Yedek mermi (Kâğıt Fişek) nadir, tavan 1.
7. **Tekleme yazı-tura değildir:** sadece barut ıslakken olur (suya düşen, denize itilen), bu da görünür ve kurutulabilir.
8. **Masum cezası (GDD §4, aynen):** tabancayla **masum birini öldüren** silahı düşürür, 30 sn bulanık görür ve maç boyu
   silah tutamaz. Kural atanın tarafına bakmaz; bu yüzden "yanlışlıkla vurdum" diyen kasabalı ile kasten vuran Sabırsız
   dışarıdan aynı görünür. Çıkarımın asıl malzemesi budur.
9. **Anti-snowball:** N'ye göre silah tavanı (N ≤ 7: 0, 8–11: 1, 12–15: 2, 16–20: 3), tavandayken parça yok, oyuncu
   başına 1 tabanca, taşınan tabanca **belde görünür**, Sabırsız için tabanca pratikte tek kullanımlık.
10. **Beklenen sayılar** (10.000 tohumlu model, Ek A): 3. günün sonunda N = 8'de ~0.7, N = 12'de ~1.2, N = 20'de ~1.9
    tabanca dövülmüş olur. Tabanca ölümleri tüm ölümlerin ~%4–8'i. Kasaba kazanma oranına kaba etkisi +1.5–2.5 puan:
    bu, Kasaba kazanma oranı için tek, monoton bir **ayar düğmesi**dir.

---

## 1. Oyuncu deneyimi

### 1.1 Bir kasabalının 2. günü (N = 12)
Şafakta iskeleye iniyorsun. Dün kırılan balık kasaları yine yerinde, üstlerinde çiğ var. Görevin Değirmen Yolu'nda. Yolda
iki sandığı yumrukluyorsun: birinden 3 altın, öbüründen tahta talaşı arasında pirinç bir **çakmak mekanizması**
çıkıyor. Cebinde dünden kalma bir **namlu** var. Eksik tek parça **kabza**.

Akşam toplantısında bunu söylersen herkes duyar. Katil de duyar. Söylemezsen kimse sana kabza vermez. Fısıltı kanalından
güvendiğin Fırıncı'ya soruyorsun: "Kabzan var mı?" Var, ama o da senden bir şey istiyor: "Dün gece çanı kim çaldı,
biliyor musun?" Takas bir bilgi alışverişine dönüyor.

3. günün öğleninde Demirhane'desin. Tezgâha basıyorsun: 8 saniye boyunca örs çınlar, ocak kıvılcım saçar, Dere Boyu'ndaki
herkes bunu duyar. Arkana bakıyorsun. Kimse yok. Ya da var, ve ağacın arkasında bekliyor. Tabanca belinde, herkesin
görebileceği yerde. Artık bir **hedef** ve bir **şüphelisin**: "Neden silaha ihtiyaç duydun?"

### 1.2 Bir Sabırsızın aynı günü
Senin için tabanca bir silahtan çok bir **araç**tır. Bulduğun parçaları kimseye vermezsin (inkâr). Takımın parça
topluyorsa tabancayı sen dövebilirsin, ama kasabalıyı onunla öldürürsen ceza sana da işler: silah düşer, 30 sn göremezsin,
o gecenin Kurban Hakkı yanar. Buna değer mi? Bazen evet: kalabalığın önünde bıçak çeken birini "vurup" kahraman gibi
görünmek, ya da elinde silahı olan kasabalıyı denize itip barutunu ıslatmak. Bir de en sevdiğin hamle: 2 numaralı
tabancayı Ahmet'in bahçesine bırakmak. Ama Demirhane'de 2 numarayı kimin dövdüğünü **gören** biri varsa çerçeve çöker.

### 1.3 Hissiyat hedefleri
- **Kutu kırmak** her zaman yapılabilen, kısa, tatmin edici bir ara iştir (talaş patlaması, ses, sürpriz). Ölü zamanı
  keser (S1.1), ama görevin yerine geçmez: kutudan bar dolmaz.
- **Parça bulmak** küçük bir heyecan, **tabanca yapmak** büyük ve **gürültülü** bir karardır.
- **Tabancayı taşımak** güç ve yük birlikte: herkes görür, herkes sorar, katil seni ilk hedefler.
- **Ateş etmek** maçın en pahalı tek hamlesidir: tek atım, 60 m'lik ses, masumu vurmanın ağır bedeli.

---

## 2. Referanslar ve alınan dersler

| Kaynak | Ne alındı | Ne alınmadı, neden |
|---|---|---|
| GMod Murder | Ganimet → tabanca; masumu vuran silahı düşürür ve görüşü bozulur; silah yerde kalır, başkası alabilir | Her maçta baştan 1 silahlı "bystander" yok: KillGo'da çıkarımın ana yolu toplantıdır, silah bunun yerine geçmemeli (S10.5). 5 rastgele ganimet → silah değil, 3 **farklı** parça: takas sosyal etkileşim yaratır. |
| Town of Salem: Vigilante | Sınırlı mermi; masumu vurmanın bedeli | Gece menüden atış yerine fiziksel, duyulan, görülen atış (S2.6, S5.3). İnfazcı'nın kendi rol tabancası `02_Roles.md`'deki gibi kalır. |
| Town of Salem: Sheriff | Suçlamanın gerekçesi olarak rol bilgisi | Şerif'e silah verilmez. Şerif sonucu atışı haklı çıkarabilir; İftiracı bunu yanlış atışa çevirebilir (§5.3). |
| TF2 / CS2 | Kuşanma, horoz, geri tepme ve inspect gibi viewmodel hissi (FPArms2) | Rastgele mermi sapması yok (G4): mermi namlunun baktığı yere gider. |

---

## 3. Mekanikler

### 3.1 Kaplar (containers)
Bugünkü kod: `World/KGBreakable.*`. `Health = 35`, yumruk `MeleeDamage = 20` (`KGCharacter.h:257`), bekleme
`MeleeCooldown = 0.6 sn`. Yani **2 yumruk, ~0.6–1.2 sn**. Kırılınca `S_CrateBreak` sesi, 7 yerel talaş parçası,
`FKGLoot::RollAndSpawn` ile ganimet ve `SetLifeSpan(15)` ile yok olma.

| Tür | Mesh (mevcut) | Can | Tablo | Harita payı (hedef) | Not |
|---|---|---|---|---|---|
| Sandık (Crate) | `Crate_Wooden` | 35 | `Crate` | %50 | En zengin; parçaların en olası yeri |
| Fıçı (Barrel) | `Barrel` | 35 | `Barrel` | %30 | Barut fıçısı teması: Fişek en çok burada |
| Küp (Pot/Urn) | `Vase_2`, `Vase_4` | 20 (öneri; bugün 35) | `Pot`, `CryptUrn` | %20 | Tek yumrukta kırılsın: hızlı, ucuz |

**Değişiklikler (öneri):**
1. **Şafakta yenilenme.** Kırılan kap şafakta aynı yerde yeniden belirir (`FKGMatchClock` ile Şafak fazının 12 sn'sine
   yayılır, kare başına ≤ 6 spawn). Lore: *"Şafakta kırık sandıklar yine bütündür. Köy aynı sabaha uyanır; içlerindeki
   ise her sabah başkadır."* Sayılabilir ve anti-farm'dır: bir kap günde bir kez ganimet verir.
2. **Tohum düzeltmesi (bugünkü bir risk).** Seviyeye yerleştirilmiş kapların `LootSeed`'i 0'dır.
   `FKGLoot::MakeRng(LootSeed, this)` maç tohumunu değil bu değeri karıştırır (`KGBreakable.cpp`, TakeDamage), yani **her
   maçta aynı kap aynı eşyayı verir** ve oyuncular bunu ezberleyebilir. Yönetmen kap tohumunu
   `hash(MaçTohumu, Gün, PersistentId)`'den verir.
3. **Host migration:** kaplarda bugün `UKGSnapshotComponent` yok (CLAUDE.md kuralı). Kırık/bütün durumu ve yönetmenin
   yerleştirdiği içerik SaveGame olur.
4. **Kırma sesi olaydır:** `KG_HEAR_BREAK` (öneri 20 m) içindekiler duyar. Olay Defteri'ne düşer (§5.1). "14:05'te
   sahil fıçıları kırılırken oradaydım" gibi küçük bir alibi parçası olur.

**Kaç kap var?** v2 raporu: inşa hattında 21 (`v2_Build_Report.md` §2), giydirmede 28 (§5; bölge dökümü
`Saved/KG_V2_DressReport.json`: liman 5, sokaklar 4, kilise 3, kırsal 5, sahil 4, Dere Boyu 5; döküm 26 veriyor, fark
sayım kaynağından), yeraltında ~8 (kuyu kilerinde 3 fıçı, kript salonunda 3 küp ve depodaki fıçılar;
`Docs/Level/Underground.md`). Toplam **~57**. Bölge Kapıları (Roadmap 031a) açık alanı N'ye göre küçülteceği için
günlük aktif kap hedefi:

| N | 6 | 8 | 12 | 16 | 20 |
|---|---|---|---|---|---|
| Aktif kap C(N) (hedef) | 33 | 38 | 48 | 54 | 60 |
| Açık bölge başına en az | 3 | 3 | 3 | 3 | 3 |

Eksik kalanlar (N = 20'de ~3–5 kap) **çalışma zamanında layout JSON'daki `loot_spots` noktalarından** spawn edilir. v2 inşa
hattına (`kg_build_village_v2.py`, I2) dokunulmaz (Roadmap kuralı; SPRINT-022-V2VisualPass inşa hattının sahibi).
Yönetmen, gerçekte açık kap sayısını okur ve bütçeyi orantılar: `P_eff = P(N) · C_gerçek / C(N)`.

### 3.2 Ganimet tabloları (Katman A: statik, her kapta bağımsız)
`Inventory/KGLoot.cpp` tabloları korunur. Değişiklik: **Sargı Bezi** ve **Hurda** (Paslı Çivi) eklenir, eski ağırlıklar
biraz kısılır. Silah parçaları ve Fişek bu tablolarda **yoktur**: onları Yönetmen koyar (Katman B).

**Sandık** (1–2 çekiliş, "boş" ağırlığı 20, toplam 122):

| Eşya | Adet | Ağırlık | Çekiliş başına | Kapta ≥ 1 |
|---|---|---|---|---|
| Altın | 1–5 | 35 (40'tı) | %28.7 | %40.8 |
| **Sargı Bezi** | 1 | **14** | %11.5 | **%16.6** |
| Elma | 1–2 | 12 (15) | %9.8 | %14.4 |
| Ekmek | 1 | 8 (10) | %6.6 | %9.8 |
| İp / Mum | 1 / 1–2 | 8 / 8 | %6.6 | %9.8 |
| **Hurda (Paslı Çivi)** | 1–3 | **6** | %4.9 | %7.3 |
| Anahtar / Kemik | 1 | 4 / 4 | %3.3 | %4.9 |
| Eski Yüzük | 1 | 2 | %1.6 | %2.4 |
| Hazine Haritası | 1 | 1 | %0.8 | %1.2 |
| (boş) | | 20 | %16.4 | Tamamen boş kap: %9.5 |

**Fıçı** (1 çekiliş, boş 30, toplam 132): Elma 30, Uskumru 20, Altın 20, Morina 10, **Sargı 8 (%6.1)**, **Hurda 6**, İp 5,
Olta 3. Tamamen boş: %22.7.
**Küp** (1 çekiliş, boş 35, toplam 106): Altın 45, Mum 10, **Sargı 6 (%5.7)**, İnci 5, Eski Yüzük 5. Tamamen boş: %33.
**CryptUrn / Cellar / Chest:** değişmez (yeraltı ve kilitli sandıklar zaten ödüllü; Chest bir kez yuvarlanır).

**Karışık ortalama:** kap başına sargı olasılığı **%11.2**. Yaşayan oyuncu günde ~2.5–3 kap açarsa **günde ~0.3 sargı**,
yani 3 günde ~1 sargı. Can 100 ve yenilenmediği için (`KGHealthComponent.h`) bu, "her kavgadan sonra tam can" değil, "bir
kez toparlanma" demektir.

### 3.3 Ganimet Yönetmeni (Katman B: sunucu, günlük bütçe)
Tek sorumluluk: silah ekonomisini **sayılabilir** tutmak. Saf mantık (`FKGLootDirector`, UObject'siz, SaveGame durumu,
`FKGRng` tohumlu), `FKGRoleListGenerator` kalıbında.

Her **Şafak**'ta:
1. **Tavan kontrolü.** Dünyadaki tabanca sayısı (belde, yerde, sandıkta; batanlar hariç) `G_max(N)`'e eşitse o gün
   **parça konmaz** (yerine Katman A çalışır).
2. **Stok tamamlama.** Bulunmamış parça stoğu (dünden kırılmamış kaplarda kalanlar dahil) `P(N)`'e tamamlanır.
3. **En nadir tür önce.** Yeni parça, dünyada (kap + cepler) en az bulunan türden seçilir; eşitlikte tohum karar verir.
   Setler böylece tamamlanabilir kalır ve "hep namlu çıkıyor" varyansı olmaz (G4).
4. **Yer seçimi:** ağırlıklı rastgele kap, kap başına en fazla 1 parça. Ağırlıklar: sandık 3, fıçı 2, küp 1. Oyuncunun
   evinin 8 m içindeki kaplar hariçtir (ev kampı yok).
5. **Fişek:** stok = dünyadaki dolu olmayan tabanca sayısı, en fazla silah sayısı kadar. Ağırlıklar: fıçı 3, sandık 1,
   küp 1.
6. **Oyun sonu koruması:** yaşayan ≤ 4 ise yeni parça ve Fişek konmaz (S10.5).
7. **1. gün:** parçalar konur ama **ocak soğuktur**: zanaat 2. günün şafağından itibaren açılır (S10 tempo: erken oyun
   keşiftir; S10.2).
8. **Log:** `KG_LOOTDIR day=… seeded=… stock=… guns=… cap=…` (host migration testinde aynı tohum aynı çıktıyı verir).

| N | ≤ 7 | 8–11 | 12–15 | 16–19 | 20 |
|---|---|---|---|---|---|
| `G_max(N)` (aynı anda var olan tabanca) | **0** (lobi seçeneğiyle 1) | 1 | 2 | 3 | 3 |
| `P(N)` = 2 + yuvarla(0.7·N): günlük parça stoğu | 0 (seçenekle 6) | 8–10 | 10–12 | 13–15 | 16 |

**Neden N ≤ 7'de kapalı?** N = 6'da tek Sabırsız vardır (GDD §3). Doğru tek bir atış maçı toplantısız bitirir: S10.5
("son eleme asma ya da rol yeteneği", ≥ %40) ve Sabırsızın frustrasyonu (G5) açısından kabul edilemez. N ≤ 7'de kaplar,
sargılar ve ipuçları kalır. Lobi seçeneği "Tabanca: açık" ile 1 tabanca olur, ocak **3. günden** itibaren yanar.

**Kap türüne göre etkili parça olasılığı** (şafakta, belli bir kabın parça içerme olasılığı; stok / ağırlık payı):

| N | Sandık | Fıçı | Küp |
|---|---|---|---|
| 8 (P = 8, 38 kap) | %28 | %18 | %9 |
| 12 (P = 10, 48 kap) | %27 | %18 | %9 |
| 20 (P = 16, 60 kap) | %35 | %23 | %12 |

(Tavandayken bu sütunlar 0'a iner; oyuncu bunu doğrudan görmez, ama Kasaba Panosu'ndaki dövülme satırlarından
çıkarabilir.)

### 3.4 Yeni eşyalar (`UKGItemCatalog`, `Inventory/KGItemCatalog.cpp`)

| Id | TR / EN | Yığın | Değer | Nadirlik | Etiketler | Nerede görünür |
|---|---|---|---|---|---|---|
| `Bandage` | Sargı Bezi / Bandage | 2 | 3 | Sıradan | Medical, Tool | Cep (gizli) |
| `GunBarrel` | Namlu / Barrel | 3 | 12 | Kasaba | GunPart, Contraband | Cep (gizli); yerde parıltı |
| `GunLock` | Çakmak Mekanizması / Flintlock | 3 | 12 | Kasaba | GunPart, Contraband | Cep |
| `GunStock` | Kabza / Grip | 3 | 12 | Kasaba | GunPart, Contraband | Cep |
| `Cartridge` | Kâğıt Fişek / Paper Cartridge | 1 | 8 | Kasaba | Ammo, Contraband | Cep |
| `Flintlock` | Çakmaklı Tabanca / Flintlock Pistol | — (kemer) | 60 | Nadir | Weapon, Contraband | **Kemerde, herkese görünür** |
| `Scrap` | Paslı Çivi / Rusty Nails | 10 | 1 | Sıradan | Junk | Cep |
| `WitnessNote` (sonra) | Tanık Pusulası / Witness Note | 3 | 2 | Kasaba | Quest, Clue | Cep |

- Parça yığını 3'tür: takas malzemesi olarak birikebilir, ama cep 12 slottur (`KGInventoryComponent.h`, Capacity 12).
- **Kemer slotu** (yeni, 1 slot): tabanca cebe girmez. Taşıyorsan görünür. Görünmemesi için sandığa koymalısın.
- Tabancanın **seri numarası** vardır (Demirhane damgası "No. 1, 2, 3…"). İncelenince görünür (§3.9).

### 3.5 Sargı Bezi: sayılar ve mevcut hasarla karşılaştırma

| Kaynak (kod) | Değer |
|---|---|
| Can | 100, doğal yenilenme yok (`Combat/KGHealthComponent.h`, `MaxHealth = 100`; `Heal()` var) |
| Yumruk ve önden bıçak | 20 (`MeleeDamage`, `KGCharacter.h:257`). GDD §4 önden bıçak için 35 diyor: tutarsızlık, bu belge değiştirmez. |
| Bekleme | 0.6 sn → 5 yumruk ≈ 2.4 sn'de ölüm |
| Backstab | 1000 (anında, `KGCharacter.cpp:1132`) |
| İtme | 25 stamina, 650 itki, 0.5 sn sersemletme (GDD) |

| Kullanım | Açma (iptal serbest) | Sonra | Toplam | Kısıt |
|---|---|---|---|---|
| Kendine | 1.0 sn | 5 sn boyunca 6 can/sn | **+30** | %50 hız; koşma, zıplama, saldırı, silah çekme yok |
| Başkasına (1.5 m, hedef sabit) | 1.0 sn | 6 sn boyunca ~6.7 can/sn | **+40** (+10 yardım bonusu) | İkisi de sabit. Görünür, Olay Defteri'nde "X, Y'yi sardı" |
| Doktor (rol) | 0.5 sn | 2.5 sn | **+40** | `02_Roles.md` Doktor: "Gündüz bandajla 40 can verir". Doktor her şafakta cebine 1 sargı alır (rol kiti). |

- **Kesilme:** hasar almak, itilmek, toplantı çanı, saldırmak, koşmak. Açma bittiyse sargı harcanmıştır ve o ana kadarki
  can kalır. Kalan iyileşme kaybolur. Bu, "1 sn'de iptal" istismarını kapatır.
- **Kavgada işe yaramaz** (5 sn'de 30 can, rakip 2.4 sn'de 100 hasar verir). Kavga **sonrası** içindir. Hedef bu.
- **İz:** kullanılan sargı yerde **Kanlı Sargı** bırakır (90 sn, §5.2). "Burada biri yaralandı" = bilgi.
- **Delil etkileşimi (sonra, 026a ile):** tabancanın ağır yarası ve bıçak yarası boya/talaş damlası bırakır (charter §3).
  Sargı damlayı durdurur. Yaralı katil için sargı iz silme aracıdır, yani sargıların da çıkarım değeri vardır.

### 3.6 Zanaat: Demirhane tezgâhı
- **Yer:** Dere Boyu'ndaki Demirhane avlusu (ForgeNails noktası (−91.5, 10.5), meydana ~21.8 sn;
  `Morrowmere_v2_Plan.md` görev tablosu). Mevcut örsün yanındaki **tezgâh** (`KG_Props/Workbench`, 202 × 102 cm)
  çalışma zamanında layout JSON noktasından spawn edilen etkileşimli bir aktördür (`AKGForgeBench`). ForgeNails görevinin
  örsüyle çakışmaz.
- **Şart:** cepte her türden 1 parça (Namlu + Çakmak + Kabza), kemer boş, 2. gün ya da sonrası, `G_max` dolmamış, faz
  Gündüz ya da Gece.
- **Eylem:** `E` basılı, **8 sn**, tezgâhın 1.5 m içinde, hareketsiz. Her saniye bir örs darbesi (`KG_HEAR_FORGE`, öneri
  35 m). Ocak parlar (gece 80 m'den görünür turuncu ışık). Kesilirse (hasar, itilme, hareket, çan) ilerleme sıfırlanır,
  parçalar kalır.
- **Sonuç:** tabanca **dolu** (1 atım) olarak kemere geçer ve bir seri numarası alır. Kasaba Panosu (Roadmap 024):
  *"Demirhane: 2 numaralı tabanca dövüldü (Gündüz 2, 14:20)."* İsim yazmaz. Olay Defteri: `Craft {crafter, serial, time,
  witnesses}`.
- **Tavan doluysa** tezgâh reddeder: *Hodge: "Köyde yeterince barut kokusu var."*
- **Neden tek yer?** Tek yer bir **darboğaz** yaratır: kasabalılar birbirine eşlik eder (güven), katiller pusu kurar
  (risk), herkes "14:20'de Dere Boyu'nda kim vardı?" diye sorar (S2.3). Riski, 8 sn'lik kısa süre ve 35 m'lik duyulma
  dengeler.
- **Bölge Kapıları uyarısı (031a):** Dere Boyu, tabancanın açık olduğu her N'de (≥ 8) açık kalmalıdır. Kalamıyorsa o N'de
  `G_max = 0` olur.

### 3.7 Çakmaklı tabanca

| Özellik | Değer | Gerekçe |
|---|---|---|
| Kuşanma / kaldırma | 0.8 sn / 0.6 sn, sesli, görünür | Uyarı işareti (Ç11) |
| Horoz (RMB basılı) | 0.4 sn; tık sesi `KG_HEAR_COCK` 8 m. RMB bırakılınca horoz iner (güvenli). | Ateşten önce okunabilir bir niyet |
| Ateş (LMB, horoz çekiliyken) | Çakmak gecikmesi 0.12 sn (tavada parlama), sonra atış | Tarihî his; tepki payı |
| İsabet | **Hitscan, rastgele sapma yok.** Mermi namlunun gösterdiği yere gider. Yürürken viewmodel salınımı **deterministik** artar (yürüme döngüsünün sinüsü). Sunucu lag telafisi (`ValidatedViewStart`). | G4.1: sonucu belirleyen zar 0 |
| Hasar | ≤ 12 m: **100** (öldürür). 12–25 m: 100 → 50 doğrusal. 25–40 m: 50 → 25. > 40 m: mermi düşer. | Yakın mesafe cesaret ister |
| Ağır yara (≥ 50, ölmedi) | 30 sn %80 hız + damla izi (026a gelince), sargı durdurur | Kaçan kurban delil taşır |
| Mermi | Dövülünce 1 dolu. Yedek: 1 Kâğıt Fişek (cepte en fazla 1). | "Sınırlı" = maçta tabanca başına tipik 1–2 atış |
| Dolum | 5 sn, hareketsiz (dönebilir), harbi sesi 10 m | Atıştan sonra savunmasızlık |
| Ses ve görüntü | Atış `KG_HEAR_SHOT` (öneri 60 m), 6 sn duman bulutu (atanın yerini işaretler), gece namlu alevi 40 m'den | Silah gizli bir cinayet aracı değildir |
| Faz kilidi | Toplantı, Mahkeme ve Epilog'da çekilemez (S9.1). 1. gündüzün ilk 90 sn'si zaten ocak soğuk. | S9.1, S10.2 |
| Oyuncu başına | 1 tabanca (kemer). Kemer doluyken zanaat ve yerden alma yok. | Anti-snowball |

**Tekleme (misfire): yazı-tura değil, durum.**
- Taşıyan beline kadar suya girerse (yüzme, denize itilme, iskeleden düşme) tabanca **Islak** olur. Islakken tetik
  sadece tavada parlama yapar: atış yok, 1.5 sn yeniden horoz.
- Islaklık 60 sn kuruda, ya da bir ateşin (ocak, fırın, ocaklık, kamp ateşi) 3 m içinde 10 sn kalınca biter.
- **İşaret:** sahibin HUD'unda damla ikonu. Başkaları 60 sn boyunca kemerdeki tabancadan su damladığını görür (Ç11).
- **Karşı hamle:** tabancalı birini **denize itmek**, mevcut itme mekaniğiyle (650 itki) bir katilin gerçek cevabıdır.

### 3.8 Masum cezası (karar tablosu)
**Masum** = ölenin tarafı "tehdit" kümesinde değil. Tehdit kümesi: Saat Kırıcılar, Yalnız Katiller, Vampir Soyu, Veba
ve Hortlak (GDD §11.1: "suçluluk cezası olmadan herkes öldürebilir"). Kasaba ve öldürmeyen nötrler masumdur. Taraflar
Roadmap 022'nin çözücüsünden okunur.

| Atan | Sonuç | Ceza |
|---|---|---|
| Kim olursa | Tehdit öldü | Yok |
| Kim olursa | **Masum öldü** | Tabanca olduğu yere düşer. 30 sn bulanık ve renksiz görüş, %80 hız, "gözlerini kapatma" animasyonu (görenler görür). **Maç boyu** tabanca tutamaz, dövemez, yerden alamaz. |
| Sabırsız | Masum öldü | Yukarıdaki ceza, **ve** o gecenin Kurban Hakkı düşer (`02_Roles.md`: "Gündüz öldürmek o gecenin hakkından düşer") |
| Kim olursa | Masum ölümcül vuruş yedi ama korundu (Doktor → baygın, Hayatta Kalan yeleği, zırh) | **Yok** (ölüm yok) |
| Kim olursa | Masum ağır yaralandı, ölmedi | **Yok**; olay deftere düşer |
| İnfazcı'nın **rol** tabancası | Masum öldü | Rol kuralı: ertesi gece vicdan azabından ölür (`02_Roles.md`). Ganimet cezası işlemez. |

**Neden sadece ölümde?** Yaralamada ceza olsaydı, bir kasabalı birini uzaktan hafifçe vurup "ceza geldi mi?" diye
bakarak **yaşayan** birinin masumiyetini kesinleştirebilirdi. Bu, charter S2.4'ün yasakladığı cinayet dışı bir sert kanıt
kaynağı olurdu. Ölümde ceza, ölenin tarafını erkenden sızdırır ama o bilgi zaten şafakta ya da kararda açılır. **Atanın**
tarafı ise belirsiz kalır (yanlışlıkla vuran kasabalı mı, kasten vuran Sabırsız mı?). Bu belirsizlik tasarımın kalbidir.

### 3.9 Taşıma, saklama, çalma, batırma, yerleştirme
- **Taşımak:** kemerde, 3. şahısta kalçada pirinç parıltılı küçük bir tabanca. ~15 m'den okunur. Toplantıda da görünür:
  *"Neden silahın var?"*
- **Saklamak:** evdeki kilitli sandık (`AKGStorageChest`, sahip kilidi), herkese açık sandık (risk), ya da bir yere
  bırakmak. Saklanan tabanca görünmez, ama gerektiğinde elinde de değildir. Kazıyla gömmek sonraki iştir (§11).
- **Silahsızlandırma:** kuşanılmış (çekili) tabancası olan birini **önden itmek** tabancayı her zaman düşürür
  (deterministik). Kemerdeki tabanca itmeyle düşmez. Bu, "çekmek" ile "kemerde tutmak" arasında gerçek bir karardır.
- **Cesetten almak:** ölünce kemerdeki tabanca, parçalar ve Fişek cesedin yanına düşer (pickup). Yerden almak görünür
  ve olay defterine düşer (`GunPickup`). Baygın düşen (Doktor koruması) de tabancasını düşürür.
- **Batırmak:** denize ya da derin suya düşen/atılan tabanca batar ve yok olur. Sıçrama 15 m'den duyulur, olay
  `GunSunk`. Tavan ertesi şafak boşalır. Sabırsızın inkâr hamlesidir.
- **Yerleştirmek (çerçeveleme):** tabancayı ya da parçaları birinin bahçesine, kapısının önüne, açık sandığına
  bırakmak. Tabancanın **seri numarası** Demirhane olayına bağlıdır. 2 numarayı Mehmet'in dövdüğünü gören biri varsa,
  Ahmet'in bahçesinde bulunan 2 numara bir **çelişki**dir (charter §0.4, S9.5). Kilitli ev sandığına yerleştirmek sadece
  İftiracı'nın fiziksel sahte delili ya da Çilingir'in kilidiyle olur (rol sprintleri).
- **Kaçakçı'nın tabancası** (`02_Roles.md`, maçta 1 kez) **damgasızdır** ("No. —"). Tavan dışıdır. Damgasız bir tabanca
  bulmak "köyde bir Kaçakçı var" demektir: kimin olduğunu değil, ne olduğunu söyleyen yumuşak bir ipucu.

### 3.10 Faz kuralları özeti

| Faz | Kap kırma | Sargı | Zanaat | Tabanca çekme/ateş |
|---|---|---|---|---|
| Şafak | Evet (kaplar yenileniyor) | Evet | Hayır | Hayır |
| Gündüz | Evet | Evet | 2. günden itibaren | Evet (1. gün zaten yok) |
| Toplantı / Mahkeme / Epilog | Hayır | Evet | Hayır | **Hayır** (S9.1) |
| Gece | Evet (yasak saatinde dışarıda olmak riskli) | Evet | Evet (ocak 80 m'den görünür) | Evet |

---

## 4. Sayılarla denge

### 4.1 Oyuncu başına günlük parça
Model (Ek A, 10.000 tohum): yaşayan oyuncu başına günde **0.55–0.65 parça**. Yani tek başına bir set (3 farklı parça)
~5 günde tamamlanır. **Takas olmadan tabanca yok gibidir.** Bu kasıtlı: tabanca sosyal bir üründür.

### 4.2 Tabanca sayısı: 2. ve 3. gün (ortalamalar, 10.000 tohum)

| N | Tavan | P(N) | Dövülen: 2. gün sonu | Dövülen: 3. gün sonu | Var olan: 3. gün sonu | ≥ 1 tabanca: 2. gün / 3. gün |
|---|---|---|---|---|---|---|
| 6 (varsayılan) | 0 | 0 | 0 | 0 | 0 | %0 / %0 |
| 6 (lobi seçeneği, ocak 3. gün) | 1 | 6 | 0 | 0.46 | 0.44 | %0 / %46 |
| 8 | 1 | 8 | 0.31 | 0.69 | 0.60 | %31 / %67 |
| **12** | 2 | 10 | **0.52** | **1.23** | 1.10 | %49 / %84 |
| 16 | 3 | 13 | 0.62 | 1.51 | 1.37 | %57 / %89 |
| **20** | 3 | 16 | **0.82** | **1.91** | 1.77 | %71 / %95 |

"Var olan" < "dövülen": batırılan, yanlış atıştan sonra kaybolan ve ölenden kalıp kimsenin almadığı tabancalar.
S10.1'e göre maç N = 12'de 3–5, N = 20'de 3–4 gün sürer. Yani tipik bir N = 12 maçında 1–2, N = 20 maçında 2–3 tabanca
görülür.

### 4.3 Kasaba kazanma oranı: kaba etki ve ayar düğmesi
Hedef: GDD §3'ün uzun vadeli ayarı Kasaba **%48–52**. Charter'ın kapısı S7.2: [O] %38–62, merkez ~%50, n_min 67 maç.
Tabanca bir Kasaba aracıdır, bu yüzden **artı** yönde iter. Tasarım hedefi: **tabanca açık ile kapalı arasındaki fark
≤ +3 puan.**

Kaba model: `Δ ≈ D · (p · V_k − (1 − p) · V_t)`
- D = maç başına tabanca ölümü. Tabanca-gün × 0.25 atış eğilimi × 0.75 isabet. Model: N = 8: ~0.3, N = 12: ~0.55,
  N = 20: ~0.95.
- p = kasaba atışlarının doğru (tehdit) olma oranı. Hedef bandın ortası 0.55 (§4.5, L2).
- V_k, V_t = bir tehdidin ya da bir kasabalının ölümünün Kasaba kazanma olasılığına etkisi. Parite oranından kaba:
  N = 12'de V_k ≈ 10, V_t ≈ 4 puan; N = 20'de 7 ve 3; N = 8'de 14 ve 6.

| N | D | Δ (puan) |
|---|---|---|
| 8 | 0.31 | ≈ +1.6 |
| 12 | 0.57 | ≈ +2.1 |
| 20 | 0.94 | ≈ +2.4 |

Bu bir **tahmin**, ölçüm değil. Gerçek kapı [O] ve gece soak'udur (§4.5). Sabırsızın "kasten yanlış atış" hamlesi kill
sayısını değiştirmez (Kurban Hakkı düşer), zamanlamasını ve kimliğini değiştirir. Bu yüzden modele girmedi.

**Ayar düğmeleri (sırayla, her biri tek sayı):**
1. `P(N)` ±%25 (en yumuşak: silah daha erken ya da geç gelir)
2. `G_max(N)` ±1
3. Ocak günü (2 → 3)
4. Ölüm eşiği mesafesi (12 m → 8 m)

Bugün smoke'larda Sabırsız yıpratmayla kazanıyor (anekdot, `Pillar_Audit.md`). Kasaba hedefin altında kalırsa aynı
düğmeler ters yönde kullanılır. Rol üreticisinin güç bütçesine dokunmadan Kasaba oranını kaydırmanın yolu budur.

### 4.4 Anti-snowball kuralları (G5)
1. **Tavan** `G_max(N)`; tavandayken parça konmaz.
2. **En nadir tür önce** + günlük tamamlama: şans serisi yok, kıtlık serisi yok.
3. **Takas zorunluluğu:** 3 farklı parça ve kişi başına ~0.6 parça/gün.
4. **Oyuncu başına 1 tabanca**, kemerde ve görünür. Tabancalı hedef olur.
5. **Tek atım**, yedek en fazla 1, dolum 5 sn.
6. **Masum cezası:** kaybedilen silah + 30 sn + maç boyu yasak. Sabırsız için ek olarak Kurban Hakkı.
7. **Ölen taşıyanın her şeyi yere düşer**, suya düşerse batar.
8. **Ocak 1. gün soğuk;** zanaat 8 sn ve 35 m'den duyulur.
9. **Oyun sonu:** yaşayan ≤ 4 iken yeni parça ya da Fişek yok.
10. **Tarafsız yönetmen:** bütçe sadece N'ye ve açık kap sayısına bakar, kimin kazandığına **bakmaz**. Gizli bilgiyle
    lastik bant yok (G5.1'in geri dönüşü rol ve toplantıdan gelmeli).

### 4.5 Bu özelliğin ölçümleri (charter §0.2 etiketleriyle)

| Kod (öneri) | Hedef | Değer | Ölçüm | n_min |
|---|---|---|---|---|
| L1 | Dövülen tabanca (2. ve 3. gün), tavan ihlali | §4.2 tablosu ± %20; tavan ihlali 0 | [L] mantık simülasyonu (10.000 tohum), [T] | — |
| L2 | Kasabalı (tehdit olmayan) atıcıların tabanca ölümlerinde doğru hedef oranı | %45–70 (alt: çıkarım çalışıyor; üst: silah bir sert kanıt makinesi değil) | [O]; [B] sadece rapor | 60 tabanca ölümü |
| L3 | Tabanca ölümlerinin tüm ölümlere oranı | ≤ %15 | [B] [O] | 40 |
| L4 | Kasaba kazanma oranı farkı, tabanca açık − kapalı | ≤ +3 puan | Mantık-seviyesi simülasyon + [O] toplam. Kapı değil, rapor (tam maçla anlamlı fark ~2.000 maç/kol ister). | — |
| L5 | Bot yolları: kır, sar, takas, dök, ateş et, batır | Her yol çalışır ve loglar; `reason=random` 0 | [U] | — |
| L6 | Toplantı, Mahkeme ve Epilog'da tabanca çekme | Reddedilir | [T] | — |

---

## 5. Çıkarım döngüsüne katkı

Charter'ın döngüsü: KARAR → İZ → BİLGİ → ŞÜPHE → TOPLANTI → SONUÇ → YENİ KARAR. Bu özellik halkaların her birine
bir şey ekler:

| Halka | Ne ekler |
|---|---|
| Karar | Kutu mu görev mi; parçayı kime veririm; ne zaman döverim; tabancayı taşır mıyım saklar mıyım; çeker miyim; ateş eder miyim |
| İz | Kırma sesi, örs sesi, ocak parlaması, atış sesi ve dumanı, seri numarası, kanlı sargı, ıslak tabanca, sersem atıcı |
| Bilgi | Kasaba Panosu'ndaki dövülme satırı; "kimin belinde silah var?"; yaranın sınıfı "kurşun" |
| Şüphe | Parça isteyen herkes şüphelidir; parça vermeyen herkes de. "Tabancan var ama neden hiç kullanmadın?" |
| Toplantı | Vaka Dosyası'nda yara sınıfı "Kurşun" → silahı olanlar ilk soruya çekilir; seri numarası ile tanık çelişkisi |
| Sonuç | Asma, yanlış atış cezası, tabancanın el değiştirmesi |

### 5.1 Olay Defteri olayları (Roadmap 020b'nin `UKGEventLedger`'ı)

| Olay | Yayıcı | Tanık filtresi | Kamuya açık? |
|---|---|---|---|
| `ContainerBreak {who, kind, region}` | `AKGBreakable::TakeDamage` | Görüş + `KG_HEAR_BREAK` | Hayır |
| `LootPickup {who, itemClass}` (sadece GunPart, Ammo, Weapon) | `AKGPickup` | Görüş (`KG_IDENT_RANGE` içinde eşya türü okunur) | Hayır |
| `ItemGiven {from, to, itemClass}` | yere bırak → al (MVP) / uzat (sonra) | Görüş | Hayır |
| `Craft {who, serial}` | `AKGForgeBench` | Görüş + `KG_HEAR_FORGE` | **Evet, isimsiz** (Kasaba Panosu) |
| `GunDraw {who, serial}` | kemer bileşeni | Görüş | Hayır |
| `Shot {who, serial, hitWho, dmg}` | tabanca bileşeni | Görüş + `KG_HEAR_SHOT` | Duyan herkesin HUD'unda "Silah sesi! (Dere Boyu)", isimsiz (kazıdaki "Birisi mezar kazıyor…" kalıbı, GDD §16) |
| `GunPenalty {who}` | ceza | Görüş | Hayır |
| `GunPickup / GunSunk {who, serial}` | pickup / su | Görüş + sıçrama 15 m | Hayır |
| `Bandage {who, target}` | sargı | Görüş | Hayır |

Defter (J, Roadmap 024) satır örnekleri (G3.5, tek satır):
- *"Gündüz 2, 14:20 — Demirhane'den örs sesi duydum."*
- *"Gündüz 2, 15:02 — Liman'da silah sesi duydum."*
- *"Gündüz 3, 09:40 — Aylin, Kerem'i sardı (Çeşme Meydanı)."*

### 5.2 İz tablosu (charter §3 biçiminde)

| Delil | Kaynak | Ömür | Karşı hamle | Sahte yolu |
|---|---|---|---|---|
| Atış dumanı | Ateş | 6 sn | Kalabalıkta ya da gece ateş et | — |
| Barut isi (ellerde) (026a ile) | Ateş etmek | 60 sn | Kuyuda yıkan (7 sn, `KG_HEAR_WASH`) | — |
| Seri numaralı tabanca | Zanaat | Batana kadar | Batır | Başkasının evine bırak |
| Kanlı Sargı | Sargı kullanımı | 90 sn | Kendi evinde sarın | İftiracı'nın "kanlı bez"i (`02_Roles.md`) |
| Islak tabanca (damlama) | Suya girmek | 60 sn | Ateş başında kurut | — |
| Sersem atıcı | Masum cezası | 30 sn | Görülmeden vur | — |
| Kırık kap kırıntısı | Kırma | 9–12 sn (talaş) + kap şafağa kadar yok | — | — |

Yeni iz türleri S3.1 ve S3.2'ye sayılır: her birinin bir ömrü ve bir karşı hamlesi var, ikisinin sahte yolu var.

### 5.3 Rollerle etkileşim (hepsi `02_Roles.md`'deki mevcut metne bağlı, yeni rol yok)

| Rol | Etkileşim | Hangi sprintle |
|---|---|---|
| **İnfazcı** (Vigilante) | Kendi **gizli** rol tabancası (3 mermi, masumda ertesi gece ölür) aynı `FKGGunRules` koduyla ama "RoleGun" profiliyle yapılır. Ganimet tabancası "kamusal güç", onunki "gizli güç". Ganimet tabancasını da alabilir; ganimet kuralı geçerli olur. | Roller (027a sonrası) |
| **Şerif** | Şerif'in "Şüpheli" sonucu atışa gerekçe olur. İftiracı'nın çerçevesi bu yüzden masum atışa dönüşebilir (rol→rol etkileşimi, S5.4). | 027a, R1 |
| **Doktor** | Sargı 40 can ve 2 kat hızlı; her şafak 1 sargı. Gece koruması tabanca atışına da işler (baygın, ceza yok). Ağır yarayı ve izini durdurur. | 027a |
| **Barut Ustası** | "20 m içindeki kaçak silahları sezer": cepteki parça, Fişek ve tabancalar HUD'da (türü, kimde olduğu değil) belirir. Bir rolün cep gizliliğini delen tek meşru kanal; sunucuda hesaplanır, botlar için de aynı. | Rol sprinti (029 kenarları) |
| **Kaçakçı** | Takıma damgasız tabanca (tavan dışı). Damgasız tabanca = "köyde Kaçakçı var". | Rol sprinti |
| **İftiracı** | Parça ya da tabanca yerleştirmek; kilitli ev sandığına fiziksel sahte delil. | Rol sprinti |
| **Adli Tabip** | Yara sınıfı **"Kurşun, yakın/uzak"** (ölüm mesafesi 12 m eşiğinden). Damgalı mı damgasız mı (bilye çapı). S2.6'ya uygun: dünya nesnesine bağlı rol bilgisi. | 021 (yara sınıfı) + rol sprinti |
| **Bekçi** | Sokak yasağında kelepçelenen kişinin tabancası hapse gider. | 028 |
| **Koruma / Saat Ustası zırhı / Hayatta Kalan yeleği / Seri Katil gece zırhı** | Ölümcül atışı bir kez emer; ceza tetiklenmez. | 027a/b |
| **Temizlikçi** | Tabanca ölümünü "Kayıp"a çevirebilir, ama atış 60 m'den duyulmuştur: sessiz cinayet değildir. | 025b |
| **Hortlak** (GDD §11.1) | Tabanca tutamaz; vurulursa ceza yok. | 030 sonrası |
| **Hayaletler** | Tabancayı ve parçaları görmez (cep gizliliği korunur, G7.3). | 030 |

### 5.4 Örnek zincir (toplantıya ne gelir)
1. Gündüz 3, 11:05: Liman'da silah sesi. Kasaba Panosu'nda Gündüz 2'den "No. 1 dövüldü" satırı var.
2. Ceset raporlanır (021). Vaka Dosyası: yara "Kurşun, yakın".
3. Deniz, Defter'inden paylaşır: *"Gündüz 2, 14:20, Demirhane'de Emre'yi tezgâhta gördüm"* (tanımlı tanık).
4. Emre: *"Evet dövdüm, ama tabancam dün gece çalındı."* Kemerinde tabanca yok.
5. Barut Ustası: *"Sabah Çeşme'de birinin cebinde parça sezdim."* (kim olduğunu bilmiyor).
6. Selin: *"Atıştan hemen sonra Liman'da dumanın içinden biri koşarak kaçtı. Sersem değildi."* Şafakta ölenin kasabalı
   olduğu açılırsa atıcı **mutlaka** cezalı ve sersemdi. Koşarak kaçan kişi ya atıcı değildi, ya da Selin yalan söylüyor.
Bu tartışmayı tek bir bilgi parçası değil, **dört** bilgi parçası (S2.3 medyan ≥ 3) ve bir çelişki (S9.5) besler.

---

## 6. Karşı hamleler ve istismar vakaları

| # | İstismar | Kural / karşı hamle |
|---|---|---|
| 1 | Sabırsızın parça biriktirerek inkârı | En nadir tür önce + günlük tamamlama; Barut Ustası sezgisi; ölünce her şey düşer |
| 2 | Demirhane'de pusu | 8 sn kısa; örs 35 m'den duyulur; eşlikle gel. Kabul edilen bir risk. |
| 3 | Dövüp batırarak tavanı tüketmek | Batırma tavanı ertesi şafak boşaltır (1 günlük inkâr); sıçrama olayı defterde |
| 4 | RDM ve trol atışları | Masum cezası + maç boyu yasak; mevcut raporlama; 1 atım + 5 sn dolum |
| 5 | Masumiyet testi (yaralayıp cezaya bakmak) | Ceza sadece ölümde (§3.8) |
| 6 | Takım arkadaşını vurup güven kazanmak (bus) | İzinli; Town of Salem'in klasik hamlesi. Takıma pahalıya patlar. |
| 7 | Kap ezberleme | Kap tohumu = maç tohumu × gün × kalıcı kimlik (bugünkü `LootSeed = 0` riski düzelir) |
| 8 | Gece kap tarlası | Kaplar sadece şafakta yenilenir |
| 9 | Sargıyı 1 sn'de iptal | Açma bitince harcanır |
| 10 | Kavgada sargıyla tank | 5 sn, %50 hız, hasar keser |
| 11 | Toplantıda silah | Çekilemez (S9.1); kemerde görünmesi sosyal bilgi |
| 12 | Duvar arkasından atış | Hitscan dünya çarpışmasına takılır; sunucu `ValidatedViewStart` |
| 13 | Hızlı zanaat hilesi | Süre sunucuda `FKGMatchClock` ile |
| 14 | Takas dolandırıcılığı (parçayı alıp dövmemek) | Tasarımın parçası: çıkarım malzemesi; `ItemGiven` defterde |
| 15 | Host migration'da eşya kopyalama | Kemer, cepler ve yönetmen SaveGame; kayıt anahtarı `PlayerKey()` (`UI/Reveal/KGStreamerMode.h`) |
| 16 | Parçaya bakarak rol okuma | Parçalar herkese düşer; Sabırsız da döver; Kaçakçı tabancası tavan dışı |
| 17 | Sersem atıcıyı öldürmek | İzinli (30 sn açık hedef): masum öldürmenin gerçek bedeli |

---

## 7. Botlar (G6)

Kural: bot sadece kendi algısını ve kendi takımının verisini okur (G6.1). **Başkasının cebini okumak yasak.** G6.1 grep
listesine `UKGInventoryComponent::FindForPlayer` / `FindForPawn`'ın `AI/` altında `KGBotView.cpp` dışında kendisi
olmayan bir oyuncuyla çağrılması eklenir. Barut Ustası botu sezgisini rol yeteneğinden (sunucu olayı) alır.

| Davranış | Tetik (sadece kendi algısı) | Log ([U]) |
|---|---|---|
| Kap kırma | Rotanın 6 m içindeki kap, sapma ≤ 8 sn, olasılık `p_break` (dondurulmuş) | `KG_LOOT bot=… kind=… items=…` |
| Sargı | Can ≤ 60 ve 15 m içinde algılanan tehdit yok | `KG_BANDAGE bot=… hp=…` |
| Takas (Kasaba) | Eksik parçayı isteyene güven puanı ≥ `τ_bot` (Roadmap 023 bot zihni) | `KG_TRADE from=… to=… reason=<eventId>` |
| Zanaat | Set tamam, tavan boş, gündüz; Demirhane'ye yürü | `KG_CRAFT bot=… serial=…` |
| Ateş (Kasaba) | (a) son 60 sn'de X'in cinayetinin **tanımlı tanığı**, ya da (b) bıçağı çekili X 6 m içinde ve yaklaşıyor | `KG_SHOT bot=… target=… reason=<eventId>` |
| İnkâr ve batırma (Sabırsız) | Tanık filtresi kimseyi göstermiyorsa | `KG_SINK bot=… serial=…` |
| Kasten yanlış atış (Sabırsız) | Sadece bahane olayı varken (hedefin tabancası çekili) ve Kurban Hakkı kullanılmamışsa | `KG_BUS/KG_FAKESHOT … reason=<eventId>` |

- **Nişan adaleti:** bot tepkisi ≥ 0.4 sn, dönüş hızı sınırlı, nişan hatası `e_bot` sabit ve dondurulmuş; insanla aynı
  deterministik salınıma tabidir. Aimbot yok.
- `reason=random` 0 (G6.2). Parametreler soak başlığına yazılır (§0.2 kural 2).
- **Bağımlılık:** tanımlı tanıklık ve güven puanı 020b ile 023'ü ister. O zamana kadar botlar sadece kırar ve sarar;
  tabanca yolu kapalıdır. Bugünkü dev botlarının gizli rol okuması (`KGBotController.cpp:725-948`) tabanca kararına
  asla bağlanmaz.

---

## 8. UI / UX

### 8.1 Envanter (`Inventory/KGInventoryUI.cpp`, Slate)
- 12 slotlu cep ızgarasının üstünde **Kemer** slotu (1). Tabanca oraya düşer. Dolu kemer yanında seri no, mermi
  pipleri (● dolu, ○ yedek) ve ıslaklık damlası.
- **Set izleyici:** cepte parça varsa küçük bir şerit: `Namlu ✓  Çakmak ✓  Kabza —`. Set tamamsa şerit altın olur ve
  **sadece senin** haritanda ve pusulanda Demirhane işareti belirir (016'nın yol noktası çizimi, `UI/KGHUDMap.inl`).
- İkon: bugünkü sistem gibi renkli karo + glif (asset gerekmez). Namlu "Nm", Çakmak "Çk", Kabza "Kb", Fişek "F",
  Sargı "+", Hurda "Çv".
- **Tooltip (G3.4, gizli kural yok; G3.1, ≤ 25 kelime):**
  - Tabanca: *"Tek atım. Masumu öldürürsen tabanca düşer, 30 sn göremezsin ve maç boyu silah tutamazsın."* (14 kelime)
  - Parça: *"Namlu + Çakmak + Kabza = tabanca. Demirhane tezgâhında dövülür (2. günden itibaren)."*
  - Sargı: *"Kendine 30 can (6 sn), başkasına 40 can. Hasar alırsan yarıda kalır."*
- Metinler String Tables'tan gelir (EN/TR/RU). Yayıncı modu (SPRINT-015) defter ve pano satırlarındaki isimleri maskeler.

### 8.2 HUD
- **Zanaat:** tezgâhta `E basılı: Tabanca döv (8 sn)`, halka ilerleme çubuğu ve her darbede ekran sarsıntısı.
- **Duyulan olaylar:** kazıdaki kalıp. *"Silah sesi! — Dere Boyu"* (60 m), *"Demirhane'de örs sesi"* (35 m). İsim yok.
- **Ceza ekranı:** 30 sn radyal bulanıklık, renksizleşme, vinyet (balık çekmedeki vinyet tekniği) ve yazı *"Masum
  birini vurdun. Tabanca elinden düştü."*
- **Sargı:** can çubuğunun üstünde yeşil dolan bir "iyileşme gölgesi". Kesilince kırmızı çizgi.
- **Horoz:** nişangâhın yanında küçük horoz ikonu (iniş / kalkış).

### 8.3 Birinci şahıs (FPArms2 rig)
Mevcut: `SK_KG_FPArms2`, `weapon_r` kemiği (X uç, Z sırt), klipler `Tools/Blender/kg_make_fp_arms2.py` ile üretilir,
tutuşlar `KGCharacter.cpp` içindeki `KGFP2::Grips` tablosunda (`KGCharacter.cpp:51`). **Doğrulama borcu:** FPArms2
viewmodel'i henüz gerçek oturumda görülmedi (`VerificationDebt.md` #1). Yeni klipler bu borcu büyütür. Tabanca sprinti
FPArms2 doğrulamasından **sonra** gelmeli.

| Klip (yeni) | Süre | Not |
|---|---|---|
| `pistol_draw` / `pistol_holster` | 0.8 / 0.6 sn | Kalçadan |
| `pistol_idle` | döngü | Hafif nefes; yürüyüş salınımı deterministik |
| `pistol_cock` | 0.4 sn | Başparmak horozu çeker |
| `pistol_aim` | poz | RMB basılı; FOV 62 → 54 |
| `pistol_fire` | 0.5 sn | Tavada parlama, geri tepme (`Viewmodel->AddRecoil`) |
| `pistol_misfire` | 0.6 sn | Parlama, tık, bakış |
| `pistol_reload` | 5 sn | Fişeği ısır, barut dök, harbi |
| `pistol_inspect` | 2 sn | Seri no'yu kameraya çevirir (knife_inspect kalıbı) |
| `bandage_self` / `bandage_other` | döngü | İki el sarar / öne uzanır |
| `forge_hammer` | döngü | Tezgâhta çekiç (ya da 3P + kamera sarsıntısı) |
| `cover_eyes` | 30 sn döngü | Ceza |

`FGripDef` satırı: `SM_KG_Flintlock` (uç +X, sırt +Z, tutuş kabzanın ortası).

### 8.4 Üçüncü şahıs ve varlıklar
- **3P animasyonlar mevcut:** `Content/KillGodot/Characters/Villager/Anims/A_KG_Pistol_Aim_Down/Neutral/Up`,
  `A_KG_Pistol_Idle_Loop`, `A_KG_Pistol_Reload`, `A_KG_Pistol_Shoot`. Kalça soketi (kemer) Quaternius villager
  iskeletine eklenmeli.
- **Mesh'ler:** tabanca (FP ≤ 8k tris, 3P/pickup ≤ 2k) ve 3 parça pickup'ı, **Blender başsız hattıyla** yeni yollarda
  (`Art/`, `Tools/Blender/`), palet dokusu `T_KG_Palette`. İndirme gerekirse kullanıcıya sorulur (CLAUDE.md). Sanat
  yönü: canlı toon, pirinç + koyu ceviz; karakter kuralı (Quaternius) etkilenmez.
- **VFX / SFX:** namlu alevi + duman (Niagara, ≤ 64 parçacık), tavada parlama, örs kıvılcımı; ses: atış, horoz, tık,
  harbi, örs, sargı hışırtısı, sıçrama. `KGAudio::At` kalıbı.
- **Tezgâh:** `KG_Props/Workbench` + `Anvil` + `WeaponStand` (asset kataloğunda var).

---

## 9. Performans

| Kalem | Bütçe | Not |
|---|---|---|
| Aktif kap | ≤ 64, uyuyan fizik gövdesi | Bugün 57; `SetSimulatePhysics` bütünken açık |
| Şafak yenilenmesi | ≤ 6 spawn/kare, 12 sn'ye yayılı; toplam ≤ 2 ms tepe | Şafakta oyuncu hareketi az |
| Talaş | Kırma başına 7 → N ≥ 16'da ya da > 60 yerel parça varken 4; yerel toplam ≤ 120 | Sadece istemci, 9–12 sn ömür |
| Pickup | Günde ~70 replike pickup; ganimet olmayanlar şafakta temizlenir (parça, tabanca ve Fişek hariç); tavan 150 | Dormant replikasyon |
| Hitscan | Atış başına 1 trace + lag telafisi | İhmal edilebilir |
| Barut Ustası sezgisi | 1 oyuncu, 0.5 sn'de bir 20 m küre sorgusu | < 0.01 ms |
| Yönetmen | Sadece şafakta; < 0.5 ms | Saf mantık |
| Defter olayları | Maç başına < 150 yeni olay | 020b'nin ≤ 0.25 ms/kare bütçesi içinde |
| Replikasyon | Kemer: push-model tek struct `{ItemId, Serial, bLoaded, bWet, bCocked}`; cep `COND_OwnerOnly` FastArray (değişmez) | Iris uyumlu |
| Render | Duman ve alev ≤ 0.1 ms p95 (1080p, referans makine, [R]) | S3.5 ile aynı ölçüm turu |

---

## 10. Bağımlılıklar

### 10.1 Mevcut kod (okunarak doğrulandı)
| Dosya | Kullanılan | Gereken değişiklik (sprintte) |
|---|---|---|
| `World/KGBreakable.*` | `Health` 35, `LootTable`, `LootSeed`, `Burst`, `SetLifeSpan(15)` | Şafak yenilenmesi, tohum düzeltmesi, `UKGSnapshotComponent`, olay kancası, küp canı 20 |
| `Inventory/KGLoot.*` | `FKGLoot::Roll`, tablolar Crate/Barrel/Pot/Chest/Grave/Fishing/Dig*/Crypt*/Cellar, `MakeRng` | Tablo ağırlıkları (§3.2); yönetmen kap içeriğini önceden belirleyebilsin |
| `Inventory/KGItemCatalog.*` | Tek kaynak katalog, `EKGRarity`, etiketler | 7 yeni eşya (§3.4) |
| `Inventory/KGInventoryComponent.*`, `KGInventoryRPCComponent.*` | Capacity 12, `COND_OwnerOnly`, `ServerDrop`, `ServerTransfer` | Kemer slotu (ayrı bileşen daha iyi: cep kodu değişmez) |
| `World/KGPickup.*`, `World/KGStorageChest.*` | Yer eşyası; kilitli ev sandığı | Tabanca pickup'ı; sandıkta tabanca |
| `Character/KGCharacter.*` | `ServerAttack` (faz kontrolü yok: 020a), `ServerShove`, taşıma, `HandleDeath`, `ValidatedViewStart`, `KGFP2` | Girdi kancaları (ateş, horoz, sargı), itme → silahsızlandırma, ölümde düşürme. **Seri dosya.** |
| `Combat/KGHealthComponent.*` | 100 can, `Heal()` | Zamana yayılı iyileşme (`FKGMatchClock`) |
| `Character/KGStamina.h` | 100, koşu 18/sn, yenilenme 16/sn | Sargıda koşu yok |
| `Core/KGGameMode.cpp` | `EnterPhase(Dawn)`, `OnCharacterDied`, faz süreleri (`:62-86`) | Şafak kancası → yönetmen |
| `Core/KGMatchClock.h`, `Core/KGRng.h` | Süreler, tohum | — |
| `Dig/` | 40 m duyulma ve "Birisi…" HUD kalıbı | Sonra: tabancayı gömmek |
| `Fishing/` | Suya düşme ve yüzme durumu | Islaklık tetikleyicisi |
| `Chores/` + SPRINT-016 `Chores/WorldChores/` | Taşıma, yol noktası çizimi | Demirhane işareti aynı çizimle |
| `AI/KGBotController.cpp` | Gezinme | §7 yolları. **Seri dosya.** |
| `Dev/KGDevCommands.cpp` | `kg.Spawn.Crate`, `kg.Spawn.Loot` | Yeni: `kg.Loot.Director`, `kg.Me.Give Flintlock`, `kg.Gun.Wet`, `kg.Gun.Penalty` |

### 10.2 Yol haritası (Roadmap ID'leri, `Pillar_Roadmap.md`)
| ID | Neden gerekli |
|---|---|
| **020a** | `ServerAttack` faz kontrolü; tabanca atışı aynı red kuralına bağlanır (S9.1) |
| **020b** | Olay Defteri ve tanık filtresi: Ç2 bunsuz Hayır'dır → **tabanca MVP'si 020b'den önce gelmez** |
| 021 | Vaka Dosyası yara sınıfı "Kurşun"; raporlama |
| 022 | "Masum" tanımı için tarafa sadık çözücü (tehdit kümesi) |
| 023 | Bot zihni: güven puanı, tanımlı tanıklık, G6.1 grep'i |
| 024 | Kasaba Panosu (dövülme satırı), Defter (J), isim etiketi |
| 025a | Maskeli atıcı tanımlanamaz; kalabalık bonusu tabancaya uygulanmaz (öneri) |
| 025b | Cesetle birlikte tabancanın taşınması/saklanması |
| 026a | Barut isi, ağır yara damlası, sargının damlayı durdurması |
| 026b | Sonra: "ıslak barut" sabotajı (Sabırsız fıçıları sulandırır) |
| 027a / 027b | Doktor (sargı, koruma), Şerif; İnfazcı rol tabancası; zırhlar; Kurban Hakkı |
| 028 | Bekçi kelepçesi → tabanca hapse |
| 029 | Etkileşim grafiği kenarları (Barut Ustası, Kaçakçı, İnfazcı) |
| 030 | Hayaletler parçaları görmez |
| 031a | Dere Boyu açık kalmalı (N ≥ 8); açık kap sayısı → `P_eff` |
| R1, R8 | Şerif yeniden tasarımı; Dünya Olayı "Gemi Enkazı: G. kolileri" (GDD §9) ileride tam set getirebilir |

### 10.3 Çalışan sprintlerle çakışma
- **SPRINT-016** (fiziksel görevler): `KGCharacter.cpp` taşıma kancaları ve `Chores/` onundur. Bu özellik 016'dan
  sonra gelir; yol noktası çizimini paylaşır.
- **SPRINT-022-V2VisualPass:** inşa hattının ve giydirmenin sahibi. Tezgâh ve ek kaplar çalışma zamanında JSON
  noktalarından spawn edilir; inşa hattına dokunulmaz.
- **SPRINT-023-VoiceSocial:** telsiz menüsüne öneri satırları: *"Silahı var!"*, *"Kimde kabza var?"* (sessiz yol G1.1
  zaten yazı ve emote ile sağlanır).
- **SPRINT-015:** yayıncı modu takma adları pano ve defter satırlarında kullanılır.
- **SPRINT-017/018 Storm Manor:** kaplar ve bir "silah odası" tezgâhı aynı sistemle çalışır (layout JSON `loot_spots`,
  `forge_bench`).
- **Not:** Iteration klasöründeki SPRINT-022/023 numaraları, Roadmap'teki 022/023 önerileriyle çakışıyor. Bu belge
  Roadmap önerileri için "Roadmap 022" yazar.

### 10.4 Kardeş tasarımlarla arayüz (başka ajanlar yazıyor)
- **Orman tehlikesi (kurtlar, sis):** kurtlar tabancayla vurulabilir mi? Öneri: evet, ama atış yine 60 m'den duyulur ve
  mermi harcanır; kurt ölümü masum cezası vermez. Sis yaratığına ateş etmek sesi çeker. Orman kapları (terk edilmiş
  kamp sandığı) aynı yönetmenden beslenir: tehlikeli bölgede kap ağırlığı ×1.5 (risk/ödül, S4.1).
- **Orman görevleri (ateş yakma, kamp):** kamp ateşi ıslak tabancayı kurutur (§3.7).
- **Masa oyunları (satranç, dama):** bağlantı yok. Sadece eşyalar masada bahis konusu olmaz (tabanca ve parça bahsi
  yok).

---

## 11. MVP ve sonrası

### 11.1 Önerilen bölme (her biri ayrı Backlog "Proposed" maddesi)

**Parça A — "Kutular + Sargı" (S, 020a'dan sonra koşabilir, görünür ilerleme):**
- Şafakta kap yenilenmesi, tohum düzeltmesi, snapshot; Katman A tabloları (Sargı, Hurda); küp canı 20.
- Sargı Bezi (kendine ve başkasına, kesilme), Doktor değerleri daha sonra 027a'da.
- Kanlı Sargı prop'u (90 sn).
- Botlar: kır + sar yolu ([U]).
- Olay kancaları boş fonksiyon olarak (`EmitLootEvent`); 020b gelince bağlanır.

Kabul (öneri, sabit):
1. **Test:** kap şafakta yeniden belirir; aynı maç tohumu + gün aynı ganimeti verir; farklı tohum farklı verir; kap
   başına ganimet dağılımı 100.000 çekilişte §3.2 tablosuna ±%1 uyar.
2. **Test:** sargı kendine +30/6 sn, başkasına +40; açma bitmeden iptal eşyayı korur; hasar kalan iyileşmeyi keser;
   can 100'ü geçmez; süreler `FKGMatchClock`'tan.
3. **[U]** 8 botlu x1 soak'ta her botun `KG_LOOT` ve (canı ≤ 60 olanların) `KG_BANDAGE` satırı var.
4. UIShot: sargı gölgeli can çubuğu; kutu kırılma anı (`KG_Cap_crate_burst.png`). `run_invariants.ps1` geçer.

**Parça B — "Çakmaklı v1" (L, 020b + 021 + 022 + 023'ten ve FPArms2 doğrulamasından sonra):**
- Parçalar, Fişek, Yönetmen (bütçe, tavan, en nadir tür, oyun sonu), Demirhane tezgâhı.
- Tabanca: kemer, çek/horoz/ateş/dolum, hasar tablosu, ıslaklık, masum cezası, silahsızlandırma, ölümde düşürme, batma.
- Olaylar ve Kasaba Panosu satırı. Botların tabanca yolları. FP klipleri ve mesh'ler.

Kabul (öneri, sabit):
1. **Test** `KillGodot.Gun.Rules` (saf `FKGGunRules`): ≥ 16 tablo vakası: hasar düşüşü sınırları, §3.8'deki ceza
   tablosunun her satırı, ıslak tekleme (deterministik), faz reddi (S9.1), tavan ve kemer kuralları.
2. **Test** `KillGodot.Loot.Director` (saf): 10.000 tohumda `G_max` ihlali 0; en nadir tür kuralı; oyun sonu kesintisi;
   §4.2 tablosu mantık simülasyonuyla ± %20 (L1).
3. **Test:** zanaat 8 sn sunucu süresi, 2. günden önce ve tavanda red; `Craft` olayı tanık listesiyle defterde; pano
   satırı isimsiz.
4. **[U]** Kasabalı bot atışlarının %100'ü `KG_SHOT … reason=<eventId>`; `random` 0; Sabırsız `KG_SINK` yolu çalışır.
5. UIShot'lar: kemer + set izleyicili envanter, tezgâhta ilerleme, ceza ekranı, FP nişan (`KG_Cap_flintlock_fp.png`),
   2 bakış turu. `run_invariants.ps1` geçer.

Yazılabilir kapsam (B): yeni `Source/KillGodot/Weapons/` (`FKGGunRules`, `UKGBeltComponent`, `UKGGunComponent`), yeni
`Loot/KGLootDirector.*`, yeni `World/KGForgeBench.*`, `Inventory/KGItemCatalog.cpp`, `Inventory/KGLoot.cpp`,
`Character/KGCharacter.cpp` (sadece kancalar), `Combat/KGHealthComponent.*`, `AI/KGBotController.cpp` (tabanca yolları),
`UI/` (kemer, HUD), `Tools/Blender/kg_make_fp_arms2.py` (yeni klipler) + yeni mesh yolları, layout JSON (`forge_bench`,
`loot_spots`: sadece veri), testler, `01_GDD_Core.md` §4 (güncelleme). Başlık değişiklikleri tek editör-kapanış döngüsünde.

### 11.2 Sonra (her biri ayrı öneri)
- **Tanık Pusulası** (ipucu eşyası): gerçek bir defter olayının bölge + saatini **isimsiz** söyleyen not
  (`N/6` adet/gün, küplerde). Sahtekar sahte pusula bırakabilir. 020b + 024 ister.
- Barut isi ve yıkama, ağır yara damlası (026a).
- Tabancayı gömmek (Dig), Hodge'a hurda/parça satmak, "Uzat" ile doğrudan eşya verme.
- Rol bağları: Barut Ustası sezgisi, Kaçakçı tabancası, İftiracı yerleştirmesi, Adli Tabip yara okuması, İnfazcı rol
  tabancası.
- Dünya Olayı "Gemi Enkazı" (R8): kıyıya tam set içeren bir G. kolisi (tavan içinde).
- GDD §4'teki av tüfeği ve harpon: **önerilmez** (tavanı ve okunabilirliği bozar). Kozmetik tabanca skinleri
  `07_Economy_Cosmetics.md`'deki "Silah skinleri (arcade)" satırıyla.

---

## 12. Charter kontrol listesi (12 madde, dürüst)

Kural: Ç1–Ç4 zorunlu; kalan 8'den ≥ 6 Evet. Bu **yeni** bir mekanik: zorunlu bir Hayır reddedilmesi demektir.

| # | Soru | Cevap | Gerekçe |
|---|---|---|---|
| Ç1 | Karar | **Evet** | Kutu mu görev mi; parçayı kime vereyim (güven); döveyim mi (35 m gürültü); tabancayı taşıyayım mı saklayayım mı; çekeyim mi; ateş edeyim mi; sargıyı şimdi mi sonra mı. Her birinin farklı bir risk ya da bilgi sonucu var. |
| Ç2 | İz | **Evet** (020b koşuluyla) | Zanaat, atış, alma, batırma, sargı ve kırma defter olaylarıdır; örs 35 m'den, atış 60 m'den duyulur, pano satırı herkese açık. **Koşul:** Parça B, 020b'den önce gelirse Ç2 Hayır olur. Bu yüzden sıralama kabulün parçasıdır. |
| Ç3 | Karşı hamle | **Evet** | Denize itip barutu ıslatmak, önden itip silahı düşürmek, cesetten almak, batırmak, Doktor koruması, zırhlar, Demirhane pususu, parça inkârı, çerçeveyi seri numarasıyla bozmak. |
| Ç4 | Ölçüm | **Evet** | S1.4 (karar türü: Risk = silah çek, Güven = parça ver; bugün medyan 3 → hedef ≥ 4), S6.1 (Sabırsızın öldürme dışı fiilleri: batır, yerleştir, kasten yanlış atış), S2.3 (atış başına duyan tanık → vaka başına +1–3 bilgi parçası), S3.1/S3.2 (+5 iz türü, hepsinin ömrü ve karşısı var), S4.1 (açık her bölgede ≥ 3 kap = "risk/ödül" sebebi), S10.3 (tabancalar 2. günden itibaren → tırmanma). Kendi bantları: L1–L6 (§4.5). Not: "silah çek / parça ver" charter §0.4 karar fiili listesinde yok; ekleme önerisi §13'te. |
| Ç5 | Bağlantı | **Evet** | Defter, Kasaba Panosu, Vaka Dosyası (yara sınıfı), roller (İnfazcı, Doktor, Barut Ustası, Kaçakçı, İftiracı, Adli Tabip, Bekçi, zırhlılar), botlar, harita (Demirhane, kaplar), epilog zaman çizelgesi ("No. 2'yi kim dövdü"). |
| Ç6 | Ölçek | **Hayır** | N = 6–7'de tabanca **varsayılan olarak kapalı**: tek Sabırsız varken doğru tek bir atış maçı bitirir. Orada sadece kaplar ve sargı çalışır. N ≥ 8'de tavan ve bütçe N ile ölçeklenir. |
| Ç7 | Varyans | **Evet** | Parça yerleşimi tohumlu kurulum rastgeleliğidir ve tavanla sınırlıdır. Atışta rastgele sapma yok; tekleme sadece ıslaklıkla, görünür ve deterministik. Kap tablosu zarları sonucu belirlemez: günlük bütçe toplamı sabitler. |
| Ç8 | Bot | **Evet** (020b + 023 koşuluyla) | Her yol sadece kendi algısıyla çalışır ve gerekçe loglar. Başkasının cebini okumak G6.1 grep'iyle yasak. |
| Ç9 | Sessiz yol | **Evet** | Takas yazıyla ya da yere bırak/al ile yapılır; duyulan olaylar HUD yazısı; iddialar defterden. Ses gerekmez. |
| Ç10 | Okunabilirlik | **Evet** | "Kutulardan 3 farklı parça topla, Demirhane'de tabanca dök; masumu öldürürsen tabancayı kaybedersin." |
| Ç11 | Frustrasyon | **Evet** | Belde görünen tabanca, 0.8 sn çekme, 8 m'den duyulan horoz tıkı, nişan pozu. Cevap: siper, itmek, kalabalık, Doktor. Ceza kartta önceden yazılı. Islaklık damlayarak görünür. Vurulup ölmek ölümdür (G5.6 ölümü hariç tutar). |
| Ç12 | Tempo | **Evet** | Erken: kutular ve keşif (ocak soğuk). Orta: takas ve ilk tabancalar (2. gün). Geç: pahalı, belirleyici tek atışlar; ama yaşayan ≤ 4 iken yeni parça yok (S10.5). Kutu kırmak her zaman yapılabilen bir fiil olduğu için ölü zamanı azaltır (S1.1). |

**Sonuç:** Ç1–Ç4 Evet (Ç2, 020b sıralamasına bağlı). Diğer 8'den **7 Evet** (Ç6 Hayır, gerekçeli). **Geçer.** Koşul:
Parça B 020b, 022 ve 023'ten önce başlamaz; aksi hâlde Ç2 ve Ç8 Hayır'a döner ve mekanik reddedilir.

---

## 13. Riskler, açık sorular ve onay listesi

**Riskler**
1. **Kasaba'ya fazla güç** (§4.3 kaba modeli +1.5–2.5 puan). İzleme L4; düğme `P(N)`.
2. **S2.1 üst sınırı (%80):** atışlar 60 m'den duyulur, tabanca cinayetlerinin neredeyse hepsi tanıklıdır. Tabanca
   ölümleri ~%4–8 olduğu için S2.1'i ~+2–3 puan kaydırır.
3. **S10.5:** son eleme tabancayla olursa "asma ya da rol yeteneği" payı düşer. Oyun sonu kesintisi (yaşayan ≤ 4)
   bunu sınırlar; İnfazcı'nın rol tabancası "rol yeteneği" sayılır.
4. **Bugünkü `LootSeed = 0`:** kap ganimeti maçtan maça aynı. Parça A'da düzelir (§3.1).
5. **Doğrulama borcu:** FPArms2 ve yeni klipler gerçek oturumda görülmeli; borç > 10 ise sprint beklemeli (LoopContract).
6. **Seri dosyalar** (`KGCharacter.cpp`, `KGBotController.cpp`): Parça B paralel koşamaz.
7. **Sanat:** tabanca mesh'i yoksa Blender başsız hattında yapılır; indirme gerekirse önce kullanıcıya sorulur.

**Onay listesi (kullanıcı için)**
- [ ] L-1: Parçadan zanaat, GDD §4'teki "doğrudan nadir loot tabanca"nın yerine geçsin mi?
- [ ] L-2: Ganimet Yönetmeni (günlük bütçe + tavan) kabul mü, yoksa saf kap olasılıkları mı? (Öneri: yönetmen; G4.)
- [ ] L-3: N ≤ 7'de tabanca varsayılan kapalı (lobi seçeneğiyle açık) mı?
- [ ] L-4: Masum cezası sadece **ölümde** (yaralamada değil) mi?
- [ ] L-5: Av tüfeği ve harpon (GDD §4) kaldırılsın mı?
- [ ] L-6: Charter §0.3'e sabit önerileri: `KG_HEAR_SHOT` 60 m, `KG_HEAR_FORGE` 35 m, `KG_HEAR_BREAK` 20 m,
  `KG_HEAR_COCK` 8 m, `KG_HEAR_RELOAD` 10 m.
- [ ] L-7: Charter §0.4 karar fiillerine "silah çek" (Risk), "ateş et" (Risk), "parça ver" (Güven) eklensin mi?
- [ ] L-8: Parça A (S) 020a'dan hemen sonra; Parça B (L) 020b/021/022/023 ve FPArms2 doğrulamasından sonra.

---

## Ek A: Model varsayımları (tabanca sayıları, §4.2)
10.000 tohumlu Monte Carlo (tasarım aracı, oyun kodu değil; sprintte `KillGodot.Loot.Director` mantık testine dönüşür):
- C(N) aktif kap; karışım %50 sandık, %30 fıçı, %20 küp; kırılan kaplar şafakta bütün.
- Yönetmen: tavan altındayken bulunmamış stoğu P(N)'e tamamlar, en nadir tür önce, kap ağırlıkları 3/2/1.
- Yaşayan oyuncu günde U{2..4} kap açar (N ≥ 16'da U{2..3}, rekabet); en fazla 0.9·C.
- Ölümler: her gece 1 kasabalı; 2. günden itibaren toplantı %60 olasılıkla asar (asılanın tehdit olma olasılığı %45).
  Ölenin parçaları düşer; ertesi gün %60 olasılıkla biri alır.
- Takas (akşam): en çok farklı türü olan oyuncu toplayıcıdır. Kasabalılar eksik türü %50 olasılıkla verir (toplayıcının
  Sabırsız olabileceğini bilmeden). Sabırsızlar kimseye vermez.
- Zanaat: 2. günden itibaren (N = 6 seçeneğinde 3. gün). Seti tamamlanan kasabalı ertesi gün %85, aynı gün %50;
  Sabırsız %50 olasılıkla döver (inkâr/çerçeve), Sabırsız tabancası günde %30 olasılıkla batırılır.
- Sınırlar: atış, isabet ve korumalar modellenmedi (§4.3'teki D ayrı tahmindir). Oyuncuların gerçek takas isteği
  modelden yüksekse sayılar artar: L1 bu yüzden [L] + gece soak'u raporuyla izlenir.

## Ek B: Lore satırları (ST_Lore önerisi, EN / TR)
| Key | EN | TR |
|---|---|---|
| Lore.Loot.Dawn | At dawn the broken crates are whole again. The village wakes to the same morning; what is inside never is. | Şafakta kırık sandıklar yine bütündür. Köy aynı sabaha uyanır; içlerindeki hiç aynı değildir. |
| Lore.Hodge.Cold | Forge's cold. Come back tomorrow. Everyone does. | Ocak soğuk. Yarın gel. Herkes gelir. |
| Lore.Hodge.Cap | The village smells of enough powder already. | Köyde yeterince barut kokusu var. |
| Lore.Gun.Tooltip | One shot. Kill an innocent and it falls from your hand. | Tek atım. Masumu öldürürsen elinden düşer. |
| Lore.Load.Gun | Tip: a wet flintlock only sparks. The sea is closer than you think. | İpucu: ıslak çakmaklı sadece kıvılcım çıkarır. Deniz sandığından yakın. |
